#include "binary.hpp"
#include <spdlog/spdlog.h>
#include <ranges>

#include "../disassembler/disassembler.hpp"

void binwrite::binary_t::parse()
{
	find_sections();
	find_data_rvas();
}

void binwrite::binary_t::insert(const rva_t rva, const std::span<const std::uint8_t> data, const bool inclusive)
{
	buffer_.insert_range(buffer_.begin() + rva.value(), data);

	update_rvas(rva, static_cast<rva_t::size_type>(data.size()), inclusive);
}

void binwrite::binary_t::insert(const rva_t rva, const rva_t::size_type size, const bool inclusive)
{
	buffer_.insert(buffer_.begin() + rva.value(), size, 0);

	update_rvas(rva, size, inclusive);
}

void binwrite::binary_t::erase(const rva_t rva, const rva_t::size_type size, const bool inclusive)
{
	const auto start = buffer_.begin() + rva.value();

	buffer_.erase(start, start + size);

	update_rvas(rva, -size, inclusive);
}

std::span<std::shared_ptr<binwrite::function_t>> binwrite::binary_t::functions()
{
	return functions_;
}

std::span<const std::shared_ptr<binwrite::function_t>> binwrite::binary_t::functions() const
{
	return functions_;
}

std::shared_ptr<binwrite::function_t> binwrite::binary_t::find_function(const rva_t rva) const
{
	for (const auto& function : functions_)
	{
		if (*function->rva() == rva)
		{
			return function;
		}
	}

	return { };
}

std::shared_ptr<binwrite::function_t> binwrite::binary_t::create_function(const std::string& name, const rva_t rva)
{
	const auto added_rva = add_rva(rva);
	const auto function = std::make_shared<function_t>(name, added_rva);

	functions_.push_back(function);

	add_to_disassembly_queue(added_rva);

	return function;
}

std::shared_ptr<binwrite::basic_block_t> binwrite::binary_t::create_basic_block(const rva_t rva, const std::span<const instruction_t> instructions)
{
	const auto bytes = group_instruction_bytes(instructions);

	insert(rva, bytes, true);

	const auto added_rva = add_rva(rva);
	const auto basic_block = std::make_shared<basic_block_t>(added_rva);

	basic_blocks_.push_back(basic_block);

	basic_block->push(*this, instructions, true);

	return basic_block;
}

std::shared_ptr<binwrite::basic_block_t> binwrite::binary_t::create_basic_block(const rva_t rva)
{
	const auto added_rva = add_rva(rva);
	const auto basic_block = std::make_shared<basic_block_t>(added_rva);

	basic_blocks_.push_back(basic_block);

	return basic_block;
}

std::span<std::shared_ptr<binwrite::basic_block_t>> binwrite::binary_t::basic_blocks()
{
	return basic_blocks_;
}

std::span<const std::shared_ptr<binwrite::basic_block_t>> binwrite::binary_t::basic_blocks() const
{
	return basic_blocks_;
}

std::shared_ptr<binwrite::basic_block_t> binwrite::binary_t::find_basic_block(const rva_t rva) const
{
	for (const auto& basic_block : basic_blocks_)
	{
		if (*basic_block->rva() == rva)
		{
			return basic_block;
		}
	}

	return { };
}

std::shared_ptr<binwrite::basic_block_t> binwrite::binary_t::is_inside_basic_block(const rva_t rva) const
{
	for (const auto& basic_block : basic_blocks_)
	{
		if (basic_block->contains(rva))
		{
			return basic_block;
		}
	}

	return { };
}

std::shared_ptr<binwrite::basic_block_t> binwrite::binary_t::split_basic_block(basic_block_t& basic_block, const basic_block_t::size_type index)
{
	if (index == 0)
	{
		return { };
	}

	const auto split_rva = basic_block.instruction_rva(index);

	const basic_block_t::size_type split_count = basic_block.count() - index;

	const auto original_block_instructions = basic_block.instructions();

	const auto start = original_block_instructions.begin() + index;
	const auto end = start + split_count;

	const std::vector new_block_instructions(start, end);
	const auto offset_rva = add_rva(split_rva);

	basic_block.erase(*this, index, split_count, false);

	auto new_basic_block = std::make_shared<basic_block_t>(offset_rva);

	new_basic_block->push(*this, new_block_instructions, true);

	basic_blocks_.push_back(new_basic_block);

	return new_basic_block;
}

std::vector<std::shared_ptr<binwrite::rva_t>> binwrite::binary_t::rvas()
{
	return rvas_;
}

std::vector<std::shared_ptr<binwrite::rva_ref_t>> binwrite::binary_t::rva_refs()
{
	return rva_refs_;
}

std::unordered_map<std::string, std::shared_ptr<binwrite::section_t>>& binwrite::binary_t::sections()
{
	return sections_;
}

const std::unordered_map<std::string, std::shared_ptr<binwrite::section_t>>& binwrite::binary_t::sections() const
{
	return sections_;
}

std::vector<std::shared_ptr<binwrite::section_t>> binwrite::binary_t::ordered_sections() const
{
	const auto sections_view = sections_ | std::views::values;

	std::vector ordered_sections(sections_view.begin(), sections_view.end());

	std::ranges::sort(ordered_sections,
		[](const std::shared_ptr<section_t>& left, const std::shared_ptr<section_t>& right)
		{
			return left->rva() < right->rva();
		}
	);

	return ordered_sections;
}

std::shared_ptr<binwrite::section_t> binwrite::binary_t::find_section(const std::string& name) const
{
	const auto it = sections_.find(name);

	if (it == sections_.end())
	{
		return { };
	}

	return it->second;
}

std::shared_ptr<binwrite::section_t> binwrite::binary_t::code_section() const
{
	for (const auto& section : sections_ | std::views::values)
	{
		if (section->code())
		{
			return section;
		}
	}

	return { };
}

bool binwrite::binary_t::is_in_code_section(const rva_t rva) const
{
	for (const auto& section : sections_ | std::views::values)
	{
		if (section->code() && section->contains(rva))
		{
			return true;
		}
	}

	return false;
}


std::vector<std::uint8_t>& binwrite::binary_t::buffer()
{
	return buffer_;
}

const std::vector<std::uint8_t>& binwrite::binary_t::buffer() const
{
	return buffer_;
}

std::uint8_t* binwrite::binary_t::data()
{
	return buffer_.data();
}

const std::uint8_t* binwrite::binary_t::data() const
{
	return buffer_.data();
}
