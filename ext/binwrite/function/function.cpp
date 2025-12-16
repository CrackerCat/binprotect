#include "function.hpp"
#include "../binary/binary.hpp"

void binwrite::function_t::add_basic_block(std::shared_ptr<basic_block_t> basic_block)
{
	basic_blocks_.push_back(std::move(basic_block));
}

std::shared_ptr<binwrite::basic_block_t> binwrite::function_t::find_basic_block(const rva_t rva) const
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

std::shared_ptr<binwrite::basic_block_t> binwrite::function_t::entry_block() const
{
	return find_basic_block(*rva_);
}

std::shared_ptr<binwrite::basic_block_t> binwrite::function_t::fallthrough_block(
	const std::shared_ptr<basic_block_t>& basic_block) const
{
	const auto& last_instruction = basic_block->last_instruction();
	const auto& last_instruction_disassembly = last_instruction.disassemble();

	if (last_instruction_disassembly.is_unconditional_jump() || last_instruction_disassembly.is_ret())
	{
		return { };
	}

	const auto fallthrough_rva = basic_block->end_rva();

	return find_basic_block(fallthrough_rva);
}

std::shared_ptr<binwrite::basic_block_t> binwrite::function_t::target_block(const binary_t& binary,
                                                                                           const std::shared_ptr<basic_block_t>& basic_block) const
{
	const auto& last_instruction = basic_block->last_instruction();
	const auto& last_instruction_disassembly = last_instruction.disassemble();

	if (!last_instruction_disassembly.is_jump())
	{
		return { };
	}

	const auto& last_instruction_rva = basic_block->last_instruction_rva();

	const auto code_rva_ref = binary.find_rva_ref(last_instruction_rva);

	if (!code_rva_ref)
	{
		return { };
	}

	return find_basic_block(*code_rva_ref->target());
}

std::span<std::shared_ptr<binwrite::basic_block_t>> binwrite::function_t::basic_blocks()
{
	return basic_blocks_;
}

std::span<const std::shared_ptr<binwrite::basic_block_t>> binwrite::function_t::basic_blocks() const
{
	return basic_blocks_;
}

std::shared_ptr<binwrite::rva_t> binwrite::function_t::rva() const
{
	return rva_;
}

std::string_view binwrite::function_t::name() const
{
	return name_;
}
