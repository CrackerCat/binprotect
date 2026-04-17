#include "pe.hpp"
#include "../rva/rva.hpp"
#include "../../util/serialize.hpp"

#include <portable-executable/image.hpp>

namespace binwrite
{
	void portable_executable_t::add_runtime_function(const runtime_function_params_t& params,
													 const std::shared_ptr<rva_t>& exception_directory_rva,
	                                                 const std::shared_ptr<rva_t>& unwind_insertion_rva)
	{
		const auto unwind_code_count = static_cast<std::uint8_t>(params.unwind_codes.size());
		const std::uint32_t aligned_code_count = (unwind_code_count + 1) & ~1;
		const std::uint32_t unwind_info_size = offsetof(portable_executable::unwind_info_t, codes)
			+ aligned_code_count * sizeof(portable_executable::unwind_code_t);

		std::vector<std::uint8_t> unwind_info_bytes(unwind_info_size, 0);
		auto* unwind_info = reinterpret_cast<portable_executable::unwind_info_t*>(unwind_info_bytes.data());

		unwind_info->version = 1;
		unwind_info->flags = params.flags;
		unwind_info->size_of_prolog = params.prolog_size;
		unwind_info->unwind_code_count = unwind_code_count;
		unwind_info->frame_register = params.frame_register;
		unwind_info->frame_offset = params.frame_offset;

		for (std::uint8_t i = 0; i < unwind_code_count; i++)
		{
			unwind_info->codes[i] = params.unwind_codes[i];
		}

		const rva_t unwind_insert_rva(*unwind_insertion_rva);
		insert(unwind_insert_rva, unwind_info_bytes, true);

		const auto unwind_rva = add_rva(unwind_insert_rva.value());

		portable_executable::runtime_function_t runtime_function;

		runtime_function.begin_address = params.begin_address;
		runtime_function.end_address = params.end_address;
		runtime_function.unwind_info_rva = unwind_rva->value();

		auto* img = reinterpret_cast<portable_executable::image_t*>(data());

		auto& exception_directory = img->nt_headers()->optional_header.data_directories.exception_directory;
		const std::uint32_t existing_count = exception_directory.size / sizeof(portable_executable::runtime_function_t);

		const auto* existing_table = reinterpret_cast<portable_executable::runtime_function_t*>(data() + exception_directory_rva->value());

		std::uint32_t insert_index = existing_count;

		for (std::uint32_t i = 0; i < existing_count; i++)
		{
			if (runtime_function.begin_address < existing_table[i].begin_address)
			{
				insert_index = i;

				break;
			}
		}

		exception_directory.size += sizeof(portable_executable::runtime_function_t);

		const rva_t runtime_insert_rva(exception_directory_rva->value() + insert_index * sizeof(portable_executable::runtime_function_t));
		const auto runtime_bytes = std::span(reinterpret_cast<const std::uint8_t*>(&runtime_function), sizeof(runtime_function));
		insert(runtime_insert_rva, { runtime_bytes.begin(), runtime_bytes.end() }, true);

		const auto* inserted = reinterpret_cast<portable_executable::runtime_function_t*>(data() + runtime_insert_rva.value());
		const auto begin_rva = add_rva(inserted->begin_address);
		const auto end_rva = add_rva(inserted->end_address);
		const auto uw_rva = add_rva(inserted->unwind_info_rva);

		add_rva_ref(std::make_shared<data_rva_ref_t>(begin_rva,
			rva_t{ static_cast<rva_t::value_type>(runtime_insert_rva.value() + offsetof(portable_executable::runtime_function_t, begin_address)) },
			static_cast<data_rva_ref_t::size_type>(sizeof(inserted->begin_address))));

		add_rva_ref(std::make_shared<data_rva_ref_t>(end_rva,
			rva_t{ static_cast<rva_t::value_type>(runtime_insert_rva.value() + offsetof(portable_executable::runtime_function_t, end_address)) },
			static_cast<data_rva_ref_t::size_type>(sizeof(inserted->end_address))));

		add_rva_ref(std::make_shared<data_rva_ref_t>(uw_rva,
			rva_t{ static_cast<rva_t::value_type>(runtime_insert_rva.value() + offsetof(portable_executable::runtime_function_t, unwind_info_rva)) },
			static_cast<data_rva_ref_t::size_type>(sizeof(inserted->unwind_info_rva))));
	}
}
