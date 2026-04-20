#include "function.hpp"
#include "../binary.hpp"

void binwrite::function_t::add_basic_block(std::shared_ptr<basic_block_t> basic_block)
{
	bb_index_[basic_block->rva()->value()] = basic_block;
	basic_blocks_.push_back(std::move(basic_block));
}

void binwrite::function_t::unlink_basic_block(std::shared_ptr<basic_block_t> basic_block)
{
	std::erase(basic_blocks_, basic_block);

	bb_index_dirty_ = true;
}

void binwrite::function_t::set_basic_blocks_skip(const bool state) const
{
	for (const auto& basic_block : basic_blocks_)
	{
		basic_block->set_skip(state);
	}
}

void binwrite::function_t::set_basic_blocks_dirty(const bool state)
{
	bb_index_dirty_ = state;
}

std::shared_ptr<binwrite::basic_block_t> binwrite::function_t::find_basic_block(const rva_t rva) const
{
	if (bb_index_dirty_)
	{
		reindex_basic_blocks();
	}

	const auto it = bb_index_.find(rva.value());

	if (it == bb_index_.end())
	{
		return { };
	}

	return it->second;
}

void binwrite::function_t::reindex_basic_blocks() const
{
	bb_index_.clear();
	bb_index_.reserve(basic_blocks_.size());

	for (const auto& basic_block : basic_blocks_)
	{
		bb_index_[basic_block->rva()->value()] = basic_block;
	}

	bb_index_dirty_ = false;
}

std::shared_ptr<binwrite::basic_block_t> binwrite::function_t::entry_block() const
{
	return find_basic_block(*rva_);
}

[[nodiscard]] std::vector<std::shared_ptr<binwrite::basic_block_t>> binwrite::function_t::exit_blocks(const binary_t& binary) const
{
	std::vector<std::shared_ptr<basic_block_t>> blocks = { };

	for (const auto& basic_block : basic_blocks_)
	{
		const auto& last_instruction = basic_block->last_instruction();
		const auto& last_instruction_disassembly = last_instruction.disassemble();

		if (!last_instruction_disassembly.is_ret() && (!last_instruction_disassembly.is_jump() || target_block(binary, basic_block)))
		{
			continue;
		}

		blocks.push_back(basic_block);
	}

	return blocks;
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
