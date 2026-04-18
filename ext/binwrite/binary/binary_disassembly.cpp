#include "binary.hpp"

#include <spdlog/spdlog.h>

static void process_function_basic_block(const binwrite::binary_t& binary,
                                         const std::shared_ptr<binwrite::function_t>& function,
                                         const std::shared_ptr<binwrite::basic_block_t>& current_block);

static void process_basic_block_target_branch(const binwrite::binary_t& binary,
                                              const std::shared_ptr<binwrite::function_t>& function,
                                              const std::shared_ptr<binwrite::basic_block_t>& current_block,
                                              const binwrite::disassembled_instruction_t& last_instruction_disassembly)
{
	const auto last_instruction_rva = current_block->last_instruction_rva();

	const auto code_rva_ref = binary.find_rva_ref(last_instruction_rva);

	if (!code_rva_ref)
	{
		const auto jt_targets = binary.jump_table_targets(last_instruction_rva);

		if (jt_targets.empty())
		{
			spdlog::error("unable to find code rva ref of conditional jump at 0x{:X}", last_instruction_rva.value());
			return;
		}

		for (const auto& jt_target : jt_targets)
		{
			const auto target_block = binary.find_basic_block(*jt_target);

			if (!target_block)
			{
				continue;
			}

			if (!binary.find_function(*jt_target) && !function->find_basic_block(*jt_target))
			{
				function->add_basic_block(target_block);

				process_function_basic_block(binary, function, target_block);
			}
		}

		return;
	}

	const auto target_rva = *code_rva_ref->target();

	const auto target_basic_block = binary.find_basic_block(target_rva);

	if (!target_basic_block)
	{
		spdlog::error("unable to find target basic block");

		return;
	}

	if (!binary.find_function(target_rva) && !function->find_basic_block(target_rva))
	{
		function->add_basic_block(target_basic_block);

		process_function_basic_block(binary, function, target_basic_block);
	}
}

static void process_basic_block_fallthrough_branch(const binwrite::binary_t& binary,
                                                   const std::shared_ptr<binwrite::function_t>& function,
                                                   const std::shared_ptr<binwrite::basic_block_t>& current_block)
{
	const auto fallthrough_rva = current_block->end_rva();

	const auto fallthrough_basic_block = binary.find_basic_block(fallthrough_rva);

	if (!fallthrough_basic_block)
	{
		spdlog::error("unable to find fallthrough basic block");

		return;
	}

	if (!binary.find_function(fallthrough_rva) && !function->find_basic_block(fallthrough_rva))
	{
		function->add_basic_block(fallthrough_basic_block);

		process_function_basic_block(binary, function, fallthrough_basic_block);
	}
}

static void process_function_basic_block(const binwrite::binary_t& binary,
                                         const std::shared_ptr<binwrite::function_t>& function,
                                         const std::shared_ptr<binwrite::basic_block_t>& current_block)
{
	const auto& last_instruction = current_block->last_instruction();
	const auto& last_instruction_disassembly = last_instruction.disassemble();

	if (last_instruction_disassembly.is_ret())
	{
		return;
	}

	if (last_instruction_disassembly.is_jump())
	{
		process_basic_block_target_branch(binary, function, current_block, last_instruction_disassembly);
	}

	if (!last_instruction_disassembly.is_unconditional_jump())
	{
		process_basic_block_fallthrough_branch(binary, function, current_block);
	}
}

void binwrite::binary_t::assign_function_basic_blocks()
{
	std::vector<std::shared_ptr<function_t>> removal_functions = { };

	for (const auto& function : functions_)
	{
		const auto basic_block = find_basic_block(*function->rva());

		if (!basic_block)
		{
			removal_functions.push_back(function);

			continue;
		}

		assign_basic_block_to_function(function, basic_block);

		spdlog::info("{} has {} basic block(s)", function->name(), function->basic_blocks().size());
	}

	for (const auto& function : removal_functions)
	{
		remove_function(function);
	}
}

void binwrite::binary_t::assign_basic_block_to_function(const std::shared_ptr<function_t>& function,
                                                        const std::shared_ptr<basic_block_t>& basic_block) const
{
	function->add_basic_block(basic_block);

	process_function_basic_block(*this, function, basic_block);
}

void binwrite::binary_t::process_instruction_rip_relativity(const disassembled_instruction_t& disassembled_instruction,
                                                            const rva_t instruction_rva, const rva_t next_instruction_rva,
	                                                        std::vector<std::shared_ptr<rva_t>>& risky_references)
{
	if (const auto raw_target_rva = resolve_instruction_rva(disassembled_instruction, instruction_rva))
	{
		const auto target_rva = add_rva(*raw_target_rva);

		add_rva_ref(std::make_shared<code_rva_ref_t>(target_rva, rva_t{ instruction_rva }, disassembled_instruction.size()));

		const bool is_risky_reference = disassembled_instruction.is_lea();

		if (disassembled_instruction.is_control_flow() && is_in_code_section(*target_rva))
		{
			if (disassembled_instruction.is_conditional_jump())
			{
				const auto fallthrough_rva = add_rva(next_instruction_rva);

				add_to_disassembly_queue(fallthrough_rva);
			}

			add_to_disassembly_queue(target_rva);
		}
		else if (is_risky_reference && is_in_code_section(*target_rva) && is_definitely_in_code_range(*target_rva))
		{
			risky_references.push_back(target_rva);
		}
	}
}

bool binwrite::binary_t::collect_basic_block_instructions(const disassembler_t& disassembler,
                                                          basic_block_t& basic_block, const bool is_risky,
                                                          std::vector<std::shared_ptr<rva_t>>& risky_references)
{
	rva_t instruction_rva = *basic_block.rva();

	while (true)
	{
		constexpr std::size_t max_padding_count = 16;

		if (instruction_rva.value() + max_padding_count <= buffer_.size())
		{
			const auto* p = buffer_.data() + instruction_rva.value();
			bool all_zero = true;
			bool all_cc = true;

			for (std::uint32_t k = 0; k < max_padding_count; k++)
			{
				if (p[k] != 0x00) all_zero = false;
				if (p[k] != 0xCC) all_cc = false;
			}

			if (all_zero || all_cc)
			{
				break;
			}
		}

		const auto instruction_address = reinterpret_cast<const std::uint8_t*>(buffer_.data() + instruction_rva.value());
		const auto disassembled_instruction = disassembler.disassemble(instruction_address);

		if (!disassembled_instruction)
		{
			if (is_risky)
			{
				return false;
			}

			break;
		}

		if (const auto overstepped_basic_block = find_containing_basic_block(rva_t{ instruction_rva }))
		{
			const auto index = overstepped_basic_block->instruction_index(rva_t{ instruction_rva });

			split_basic_block(*overstepped_basic_block, index);

			break;
		}

		const rva_t next_instruction_rva(instruction_rva.value() + disassembled_instruction->size());

		const instruction_t::const_value_type instruction_bytes(instruction_address, disassembled_instruction->size());

		process_instruction_rip_relativity(*disassembled_instruction, instruction_rva, next_instruction_rva, risky_references);

		basic_block.push(*this, instruction_t{ instruction_bytes, *disassembled_instruction }, true);

		if (disassembled_instruction->is_jump() || disassembled_instruction->is_ret() ||
			disassembled_instruction->is_int())
		{
			break;
		}

		instruction_rva = next_instruction_rva;
	}

	return true;
}

void binwrite::binary_t::process_disassembly_queue()
{
	const disassembler_t disassembler;

	bb_index_dirty_ = false;
	fn_index_dirty_ = false;

	while (!disassembly_queue_.empty())
	{
		const auto entry = disassembly_queue_.front();
		disassembly_queue_.pop_front();

		basic_block_t basic_block(entry.rva);

		std::vector<std::shared_ptr<rva_t>> risky_references = { };

		if (!collect_basic_block_instructions(disassembler, basic_block, entry.risky, risky_references))
		{
			continue;
		}

		if (basic_block.count())
		{
			find_jump_tables(basic_block);

			for (const auto& risky_rva : risky_references)
			{
				if (find_rva_ref(*risky_rva))
				{
					continue;
				}

				add_to_disassembly_queue(risky_rva, true);
			}

			auto shared_block = std::make_shared<basic_block_t>(std::move(basic_block));

			basic_blocks_.push_back(shared_block);
			bb_index_[shared_block->rva()->value()] = shared_block;
			bb_interval_index_[shared_block->rva()->value()] = shared_block;
		}
	}
}

void binwrite::binary_t::disassemble()
{
	process_disassembly_queue();
	assign_function_basic_blocks();

	spdlog::info("basic block count: {}", basic_blocks_.size());
}

bool binwrite::binary_t::is_inside_disassembly_queue(const rva_t rva) const
{
	return disassembly_queue_set_.contains(rva.value());
}

void binwrite::binary_t::add_to_disassembly_queue(const std::shared_ptr<rva_t>& rva, const bool risky)
{
	if (!is_in_code_section(*rva))
	{
		return;
	}

	if (!disassembly_queue_set_.contains(rva->value()) && !find_basic_block(*rva))
	{
		disassembly_queue_.emplace_back(rva, risky);
		disassembly_queue_set_.insert(rva->value());
	}
}
