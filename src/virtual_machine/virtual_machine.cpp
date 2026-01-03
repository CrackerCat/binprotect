#include "virtual_machine.hpp"

#include <spdlog/spdlog.h>

#include "vm_context.hpp"

static void insert_call_to_block(binwrite::binary_t& binary,
	binwrite::basic_block_t& source_block,
	const binwrite::basic_block_t::size_type index,
	const binwrite::basic_block_t& target_block)
{
	const auto destination_placeholder = encode_unsigned_imm_operand(1);
	const auto instruction = call_instruction(destination_placeholder).value();

	source_block.insert(binary, instruction, index);

	const binwrite::rva_t instruction_rva = source_block.instruction_rva(index);

	binary.add_rva_ref(std::make_shared<binwrite::code_rva_ref_t>(target_block.rva(), instruction_rva, instruction.size()));
}

void binprotect::vm::do_pass(binwrite::binary_t& binary, binwrite::basic_block_t& basic_block, const binwrite::rva_t rva)
{
	const auto context = std::make_shared<vm_context_t>(binwrite::register_family_t::general_purpose);

	const std::span<const binwrite::instruction_t> original_instructions = basic_block.instructions();
	const std::vector instructions(original_instructions.begin(), original_instructions.end());

	std::uint32_t erased = 0;

	for (std::uint32_t i = 0; i < instructions.size(); i++)
	{
		const auto& instruction = instructions[i];
		const auto& disassembled_instruction = instruction.disassemble();
		const auto& operands = disassembled_instruction.visible_operands();

		const auto basic_block_index = i - erased;
		const binwrite::rva_t instruction_rva = basic_block.instruction_rva(basic_block_index);

		if (disassembled_instruction.rip_relative() || disassembled_instruction.rsp_relative() || binary.find_rva_ref(instruction_rva) || disassembled_instruction.is_jump() || disassembled_instruction.is_lea() || disassembled_instruction.is_nop())
		{
			context->exit_virtualized_state(binary);

			continue;
		}

		try
		{
			const bool requires_entry = !context->in_virtualized_state();

			context->process_instruction(disassembled_instruction);
			context->compile_instruction(binary, rva);

			basic_block.erase(binary, basic_block_index);

			if (requires_entry)
			{
				const auto entry_block = context->entry_block();

				insert_call_to_block(binary, basic_block, basic_block_index, *entry_block);
			}
			else
			{
				erased++;
			}
		}
		catch (const std::exception&)
		{
			context->exit_virtualized_state(binary);

			spdlog::error("unable to do virtualization pass on '{}'", disassembled_instruction.to_string());
		}
	}

	context->exit_virtualized_state(binary);
}
