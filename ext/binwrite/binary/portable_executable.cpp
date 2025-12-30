#include "portable_executable.hpp"
#include <spdlog/spdlog.h>
#include <ranges>

#include "../instruction/basic_block.hpp"

std::uint64_t binwrite::portable_executable_t::image_base() const
{
	const auto img = image();

	return img->nt_headers()->optional_header.image_base;
}

binwrite::rva_t binwrite::portable_executable_t::entry_point() const
{
	const auto img = image();

	return rva_t{ img->nt_headers()->optional_header.address_of_entry_point };
}

void binwrite::portable_executable_t::decompress()
{
	const auto img = image();
	const auto nt_headers = img->nt_headers();

	std::vector<std::uint8_t> new_buffer(nt_headers->optional_header.size_of_image);

	copy_sections(new_buffer, true);

	buffer_ = std::move(new_buffer);
}

void binwrite::portable_executable_t::compress()
{
	const auto img = image();
	const auto nt_headers = img->nt_headers();

	const auto section_headers = nt_headers->section_headers();
	const std::uint16_t section_count = nt_headers->file_header.number_of_sections;

	const auto last_section = &section_headers[section_count - 1];
	const std::uint32_t raw_size = last_section->pointer_to_raw_data + last_section->size_of_raw_data;

	std::vector<std::uint8_t> new_buffer(raw_size);

	copy_sections(new_buffer, false);

	buffer_ = std::move(new_buffer);
}

portable_executable::image_t* binwrite::portable_executable_t::image()
{
	return reinterpret_cast<portable_executable::image_t*>(data());
}

const portable_executable::image_t* binwrite::portable_executable_t::image() const
{
	return reinterpret_cast<const portable_executable::image_t*>(data());
}

void binwrite::portable_executable_t::find_sections()
{
	const auto img = image();

	const auto nt_headers = img->nt_headers();
	const auto section_headers = nt_headers->section_headers();
	const auto section_count = nt_headers->file_header.number_of_sections;

	for (std::uint16_t i = 0; i < section_count; i++)
	{
		const auto section = &section_headers[i];
		const auto next_section = section + 1;

		const auto virtual_address = section->virtual_address;
		const auto next_virtual_address = i + 1 < section_count
			                                  ? next_section->virtual_address
			                                  : nt_headers->optional_header.size_of_image;

		const auto section_name = section->to_str();
		const bool code_section = section->characteristics.cnt_code && !section->characteristics.cnt_uninit_data;

		sections_[section_name] = std::make_shared<section_t>(rva_t{ virtual_address },
															  next_virtual_address - virtual_address,
		                                                      code_section);
	}
}

void binwrite::portable_executable_t::update_section_headers()
{
	const std::int64_t headers_size = std::min(0x1000ll, static_cast<std::int64_t>(buffer_.size()));

	std::vector headers_buffer(buffer_.begin(), buffer_.begin() + headers_size);

	const auto img = reinterpret_cast<portable_executable::image_t*>(headers_buffer.data());
	const auto nt_headers = img->nt_headers();

	std::uint32_t section_virtual_address = 0;

	for (auto& section_header : img->sections())
	{
		if (!section_virtual_address)
		{
			section_virtual_address = section_header.virtual_address;
		}

		const auto section_name = section_header.to_str();
		auto& info = sections_[section_name];

		const auto size = info->size();

		const auto unaligned_section_end = section_virtual_address + size;
		const auto aligned_section_end = img->calculate_alignment(unaligned_section_end, nt_headers->optional_header.section_alignment);

		const auto padding_size = static_cast<std::int32_t>(aligned_section_end - unaligned_section_end);

		if (padding_size)
		{
			const auto offset = rva_t{ unaligned_section_end };
			const std::uint8_t padding_value = info->code() ? 0xCC : 0x00;

			if (buffer_.size() <= offset.value())
			{
				buffer_.insert(buffer_.end(), padding_size, padding_value);
			}
			else
			{
				buffer_.insert(buffer_.begin() + offset.value(), padding_size, padding_value);
			}

			update_rvas(offset, static_cast<rva_t::size_type>(padding_size), true, false);
		}

		const std::uint32_t new_size = size + padding_size;

		section_header.virtual_address = section_virtual_address;
		section_header.virtual_size = new_size;

		info->set_size(new_size);

		section_header.pointer_to_raw_data = section_header.virtual_address;
		section_header.size_of_raw_data = section_header.virtual_size;

		section_virtual_address = aligned_section_end;
	}

	nt_headers->optional_header.size_of_image = section_virtual_address;

	std::memcpy(data(), headers_buffer.data(), headers_buffer.size());
}

template <class T>
std::span<const std::uint8_t> serialize_bytes(const T* const info)
{
	const auto start = reinterpret_cast<const std::uint8_t*>(info);

	return { start, start + sizeof(T) };
}

void binwrite::portable_executable_t::update_relocations()
{
	const auto relocation_directory_header = image()->nt_headers()->optional_header.data_directories.basereloc_directory;

	if (!relocation_directory_header.present())
	{
		return;
	}

	const rva_t directory_rva(relocation_directory_header.virtual_address);

	// <pfn, list of relocs for that page>
	std::map<std::uint32_t, std::vector<portable_executable::relocation_entry_descriptor_t>> reloc_pages = { };

	for (const auto& relocation : relocations_)
	{
		const auto target = relocation->target();
		const auto pfn = target.value() >> 12;

		auto& page = reloc_pages[pfn];

		const std::uint16_t offset = target.value() & 0xFFF;

		page.emplace_back(offset, static_cast<portable_executable::relocation_type_t>(relocation->type()));
	}

	std::vector<std::uint8_t> new_directory = { };

	for (auto& [pfn, entry_descriptors] : reloc_pages)
	{
		const std::uint16_t count = static_cast<std::uint16_t>(entry_descriptors.size());
		const std::uint32_t block_size = sizeof(portable_executable::raw_relocation_block_descriptor_t) + (count * sizeof(portable_executable::relocation_entry_descriptor_t));
		const std::uint32_t block_virtual_address = pfn << 12;

		const portable_executable::raw_relocation_block_descriptor_t block_descriptor = {
			.virtual_address = block_virtual_address,
			.size_of_block = block_size
		};

		const auto serialized_block_descriptor = serialize_bytes(&block_descriptor);

		new_directory.insert(new_directory.end(), serialized_block_descriptor.begin(), serialized_block_descriptor.end());

		const bool needs_padding = (entry_descriptors.size() % 2) != 0;

		if (needs_padding)
		{
			constexpr portable_executable::relocation_entry_descriptor_t padding = { .offset = 0, .type = portable_executable::relocation_type_t::absolute };

			entry_descriptors.push_back(padding);
		}

		for (const auto& entry_descriptor : entry_descriptors)
		{
			const auto serialized_entry_descriptor = serialize_bytes(&entry_descriptor);

			new_directory.insert(new_directory.end(), serialized_entry_descriptor.begin(), serialized_entry_descriptor.end());
		}
	}

	insert(directory_rva, static_cast<rva_t::size_type>(new_directory.size()));
	erase(directory_rva, static_cast<rva_t::size_type>(relocation_directory_header.size));

	image()->nt_headers()->optional_header.data_directories.basereloc_directory.size = static_cast<std::uint32_t>(new_directory.size());

	std::memcpy(data() + directory_rva.value(), new_directory.data(), new_directory.size());
}

void binwrite::portable_executable_t::copy_sections(std::vector<std::uint8_t>& to, const bool decompress)
{
	const auto img = image();

	for (const auto& section : img->sections())
	{
		const std::uint32_t raw_offset = section.pointer_to_raw_data;
		const std::uint32_t virtual_offset = section.virtual_address;
		const std::uint32_t size = section.size_of_raw_data;

		const auto destination_offset = decompress ? virtual_offset : raw_offset;
		const auto source_offset = decompress ? raw_offset : virtual_offset;

		std::memcpy(to.data() + destination_offset, buffer_.data() + source_offset, size);
	}

	const std::uint32_t size_of_headers = img->nt_headers()->optional_header.size_of_headers;

	std::memcpy(to.data(), buffer_.data(), size_of_headers);
}

void binwrite::portable_executable_t::add_misc_rvas(const portable_executable::nt_headers_t* const nt_headers)
{
	const auto entry_point_ptr = &nt_headers->optional_header.address_of_entry_point;

	if (*entry_point_ptr)
	{
		add_to_disassembly_queue(add_rva(*entry_point_ptr));

		add_data_rva_ref(entry_point_ptr);
	}
}

void binwrite::portable_executable_t::add_data_directory_rvas(const portable_executable::nt_headers_t* const nt_headers)
{
	for (const auto& directory : nt_headers->optional_header.data_directories.entries)
	{
		if (!directory.present())
		{
			continue;
		}

		add_data_rva_ref(&directory.virtual_address);
	}
}

void binwrite::portable_executable_t::add_resource_rvas(const portable_executable::nt_headers_t* const nt_headers)
{
	const auto resource_directory_header = nt_headers->optional_header.data_directories.resource_directory;

	if (!resource_directory_header.present())
	{
		return;
	}

	const auto directory = reinterpret_cast<const portable_executable::resource_directory_t*>(data() + resource_directory_header.virtual_address);

	parse_resource_directory_rvas(directory, directory);
}

void binwrite::portable_executable_t::parse_resource_directory_rvas(const portable_executable::resource_directory_t* const root_directory, const portable_executable::resource_directory_t* const directory, const std::uint16_t depth)
{
	for (const portable_executable::resource_directory_entry_t* entry = directory->begin(); entry != directory->end(); entry++)
	{
		if (entry->is_directory())
		{
			parse_resource_directory_rvas(root_directory, entry->as_directory(root_directory), depth + 1);
		}
		else // is data
		{
			const auto data_entry = entry->as_data(root_directory);

			add_data_rva_ref(&data_entry->data_rva);
		}
	}
}

void binwrite::portable_executable_t::add_import_rvas(const portable_executable::nt_headers_t* const nt_headers)
{
	const auto import_directory_header = nt_headers->optional_header.data_directories.import_directory;

	if (!import_directory_header.present())
	{
		return;
	}

	auto import_descriptor = reinterpret_cast<const portable_executable::import_descriptor_t*>(data() + import_directory_header.virtual_address);

	while (import_descriptor->misc.characteristics)
	{
		add_data_rva_ref(&import_descriptor->misc.original_first_thunk);
		add_data_rva_ref(&import_descriptor->first_thunk);
		add_data_rva_ref(&import_descriptor->name);

		const auto original_thunk = reinterpret_cast<const portable_executable::thunk_data_t*>(data() + import_descriptor->misc.original_first_thunk);

		parse_import_thunk_rvas(original_thunk);

		import_descriptor++;
	}
}

void binwrite::portable_executable_t::add_delay_import_rvas(const portable_executable::nt_headers_t* const nt_headers)
{
	const auto delay_import_directory_header = nt_headers->optional_header.data_directories.delay_import_directory;

	if (!delay_import_directory_header.present())
	{
		return;
	}

	auto import_descriptor = reinterpret_cast<const portable_executable::delay_load_descriptor_t*>(data() + delay_import_directory_header.virtual_address);

	while (import_descriptor->import_address_table_rva)
	{
		add_data_rva_ref(&import_descriptor->dll_name_rva);
		add_data_rva_ref(&import_descriptor->module_handle_rva);
		add_data_rva_ref(&import_descriptor->import_address_table_rva);
		add_data_rva_ref(&import_descriptor->import_name_table_rva);
		add_data_rva_ref(&import_descriptor->bound_import_address_table_rva);
		add_data_rva_ref(&import_descriptor->unload_information_table_rva);

		const auto original_thunk = reinterpret_cast<const portable_executable::thunk_data_t*>(data() + import_descriptor->import_name_table_rva);

		parse_import_thunk_rvas(original_thunk);

		import_descriptor++;
	}
}

void binwrite::portable_executable_t::parse_import_thunk_rvas(const portable_executable::thunk_data_t* original_thunk)
{
	while (original_thunk->function)
	{
		if (original_thunk->is_ordinal)
		{
			const auto ordinal = reinterpret_cast<const std::uint16_t*>(&original_thunk->address);

			add_data_rva_ref(ordinal);
		}
		else
		{
			add_data_rva_ref(&original_thunk->address);
		}

		original_thunk++;
	}
}

void binwrite::portable_executable_t::add_debug_rvas(const portable_executable::nt_headers_t* const nt_headers)
{
	const auto debug_directory_header = nt_headers->optional_header.data_directories.debug_directory;

	if (!debug_directory_header.present())
	{
		return;
	}

	const std::uint32_t entry_count = debug_directory_header.size / sizeof(portable_executable::debug_directory_t);

	auto entry = reinterpret_cast<portable_executable::debug_directory_t*>(data() + debug_directory_header.virtual_address);
	const auto end = entry + entry_count;

	while (entry < end)
	{
		add_data_rva_ref(&entry->virtual_address);

		// todo: remove when starting to use compression
		entry->pointer_to_raw_data = entry->virtual_address;
		add_data_rva_ref(&entry->pointer_to_raw_data);

		entry++;
	}
}

void binwrite::portable_executable_t::add_export_rvas(const portable_executable::nt_headers_t* const nt_headers)
{
	const auto export_directory_header = nt_headers->optional_header.data_directories.export_directory;

	if (!export_directory_header.present())
	{
		return;
	}

	const auto export_directory = reinterpret_cast<const portable_executable::export_directory_t*>(data() + export_directory_header.virtual_address);

	if (export_directory->name)
	{
		add_data_rva_ref(&export_directory->name);
	}

	add_data_rva_ref(&export_directory->address_of_functions);
	add_data_rva_ref(&export_directory->address_of_names);
	add_data_rva_ref(&export_directory->address_of_name_ordinals);

	const auto functions = reinterpret_cast<const std::uint32_t*>(data() + export_directory->address_of_functions);
	const auto names = reinterpret_cast<const std::uint32_t*>(data() + export_directory->address_of_names);
	const auto name_ordinals = reinterpret_cast<const std::uint16_t*>(data() + export_directory->address_of_name_ordinals);

	for (std::uint32_t i = 0; i < export_directory->number_of_names; i++)
	{
		const auto current_ordinal = name_ordinals[i];
		const auto current_function_ptr = &functions[current_ordinal];

		add_data_rva_ref(current_function_ptr);
		add_data_rva_ref(&names[i]);

		add_to_disassembly_queue(add_rva(*current_function_ptr));
	}
}

void binwrite::portable_executable_t::add_relocation_rvas(const portable_executable::nt_headers_t* const nt_headers)
{
	const auto relocation_directory_header = nt_headers->optional_header.data_directories.basereloc_directory;

	if (!relocation_directory_header.present())
	{
		return;
	}

	auto block_descriptor = reinterpret_cast<const portable_executable::raw_relocation_block_descriptor_t*>(data() + relocation_directory_header.virtual_address);

	while (block_descriptor->virtual_address)
	{
		const std::uint32_t size = block_descriptor->size_of_block - sizeof(*block_descriptor);
		const std::uint32_t entry_count = size / sizeof(portable_executable::relocation_entry_descriptor_t);

		auto entry_descriptor = reinterpret_cast<const portable_executable::relocation_entry_descriptor_t*>(block_descriptor + 1);

		for (std::uint32_t i = 0; i < entry_count; i++, entry_descriptor++)
		{
			const auto rva = add_relocation_rva(block_descriptor->virtual_address + entry_descriptor->offset);
			const auto type = entry_descriptor->type;

			if (type == portable_executable::relocation_type_t::absolute)
			{
				continue;
			}

			relocations_.push_back(std::make_shared<pe_relocation_t>(rva, type));

			if (type == portable_executable::relocation_type_t::dir64)
			{
				const auto image_offsetted_target = *reinterpret_cast<std::uint64_t*>(data() + rva->value());

				const auto target_rva = add_rva(static_cast<std::uint32_t>(image_offsetted_target - image_base()));

				if (is_in_code_section(*target_rva))
				{
					add_to_disassembly_queue(target_rva);
				}

				add_rva_ref(std::make_shared<pe_dir64_reloc_t>(target_rva, *rva));
			}
			else
			{
				spdlog::warn("non-dir64 relocation found");
			}
		}

		block_descriptor = reinterpret_cast<decltype(block_descriptor)>(reinterpret_cast<const std::uint8_t*>(block_descriptor) + block_descriptor->size_of_block);
	}
}

void binwrite::portable_executable_t::find_data_rvas()
{
	const auto img = image();
	const auto nt_headers = img->nt_headers();

	add_data_directory_rvas(nt_headers);
	add_import_rvas(nt_headers);
	add_delay_import_rvas(nt_headers);
	add_debug_rvas(nt_headers);
	add_export_rvas(nt_headers);
	add_relocation_rvas(nt_headers);
	add_resource_rvas(nt_headers);

	add_misc_rvas(nt_headers);
}
