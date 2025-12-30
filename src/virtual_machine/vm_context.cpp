#include "vm_context.hpp"

void vm_context_t::enter_virtualized_state(binwrite::binary_t& binary, const binwrite::rva_t rva)
{
	std::vector<binwrite::instruction_t> instructions;

	push_registers(instructions);

	entry_block_ = previous_block_ = binary.create_basic_block(rva, instructions);
	virtualized_state_ = true;
}

void vm_context_t::exit_virtualized_state(binwrite::binary_t& binary)
{
	free_instruction();

	if (!virtualized_state_)
	{
		return;
	}

	std::vector<binwrite::instruction_t> instructions;

	pop_registers(instructions);

	previous_block_->push(binary, instructions, false, true);
	previous_block_->push(binary, ret_instruction().value(), false, true);

	virtualized_state_ = false;
	previous_block_ = { };
	entry_block_ = { };

	shuffle_registers();
}

void vm_context_t::process_instruction(const binwrite::disassembled_instruction_t& instruction_disassembly)
{
	const auto& operands = instruction_disassembly.visible_operands();

	load_instruction(operands);

	handle_instruction(instruction_disassembly, operands);

	unload_instruction(instruction_disassembly, operands);
}

void vm_context_t::compile_instruction(binwrite::binary_t& binary, const binwrite::rva_t rva)
{
	if (!virtualized_state_)
	{
		enter_virtualized_state(binary, rva);
	}

	previous_block_->push(binary, current_instruction_.load, false, true);

	const auto handler_block = binary.create_basic_block(rva, current_instruction_.handler);

	push_jump_to_block(binary, previous_block_, handler_block);

	previous_block_ = binary.create_basic_block(rva, current_instruction_.unload);

	push_jump_to_block(binary, handler_block, previous_block_);

	free_instruction();
}

hardware_register_t vm_context_t::random_hardware_register()
{
	if (free_registers_.empty())
	{
		return hardware_register_t({}, binwrite::register_family_t::none);
	}

	const auto free_register = free_registers_.back();

	free_registers_.pop_back();

	return hardware_register_t(shared_from_this(), free_register);
}

void vm_context_t::free_hardware_register(const hardware_register_t& hardware_register)
{
	free_registers_.push_front(hardware_register.value());
}

void vm_context_t::shuffle_registers()
{
	binwrite::math::shuffle<binwrite::register_family_t>(stack_registers_);
}

void vm_context_t::push_registers(std::vector<binwrite::instruction_t>& instructions) const
{
	push_register(instructions, binwrite::register_family_t::flags);

	for (const auto register_family : stack_registers_)
	{
		push_register(instructions, register_family);
	}
}

void vm_context_t::pop_registers(std::vector<binwrite::instruction_t>& instructions) const
{
	for (const auto register_family : stack_registers_ | std::views::reverse)
	{
		pop_register(instructions, register_family);
	}

	pop_register(instructions, binwrite::register_family_t::flags);
}

std::optional<vm_context_t::offset_type> vm_context_t::register_stack_offset(const binwrite::register_t reg) const
{
	offset_type offset = 0;

	for (const auto stack_register : stack_registers_ | std::views::reverse)
	{
		if (reg.in_same_family(stack_register.qword))
		{
			return offset;
		}

		offset += stack_register_size;
	}

	return std::nullopt;
}

void vm_context_t::free_instruction()
{
	current_instruction_ = { };
}

void vm_context_t::load_instruction(const std::span<const binwrite::decoded_operand_t> operands)
{
	std::vector<binwrite::instruction_t>& instructions = current_instruction_.load;

	allocate_operands(instructions, operands.size());

	const offset_type operand_offset = calculate_operand_offset(operands.size());

	for (size_type i = 0; i < operands.size(); i++)
	{
		const auto& original_operand = operands[i];

		const auto redirected_operand = redirect_operand(instructions, original_operand, operand_offset);

		const hardware_register_t holder = random_hardware_register();
		const std::uint16_t operand_width = original_operand.size();

		const auto sized_holder = original_operand.is_reg() || original_operand.is_imm()
			                          ? holder->qword
			                          : holder->of_size(operand_width);

		instructions.push_back(mov_instruction(redirected_operand, sized_holder).value());

		write_operand(instructions, holder, i);
	}
}

void vm_context_t::handle_instruction(const binwrite::disassembled_instruction_t& instruction_disassembly,
	const std::span<const binwrite::decoded_operand_t> original_operands)
{
	std::vector<hardware_register_t> holding_registers(original_operands.size());
	std::vector<binwrite::encoder_operand_t> operands(original_operands.size());

	std::vector<binwrite::instruction_t>& instructions = current_instruction_.handler;

	for (size_type i = 0; i < operands.size(); i++)
	{
		const auto& original_operand = original_operands[i];

		const auto operand_width = static_cast<std::uint16_t>(instruction_disassembly.operand_width());

		hardware_register_t operand_register = read_operand(instructions, i);

		operands[i] = operand_register->of_size(original_operand.is_imm() ? operand_width : original_operand.size());
		holding_registers[i] = std::move(operand_register);
	}

	const auto mnemonic = instruction_disassembly.mnemonic();
	const bool uses_flags = instruction_disassembly.reads_rflags() || instruction_disassembly.writes_rflags();

	const offset_type operand_offset = calculate_operand_offset(original_operands.size());

	if (uses_flags)
	{
		load_flags(instructions, operand_offset);
	}

	instructions.push_back(compile_assembler_instruction(mnemonic, operands).value());

	if (uses_flags)
	{
		save_flags(instructions, operand_offset);
	}

	if (instruction_disassembly.writes_result())
	{
		const hardware_register_t& result_holder = holding_registers[0];

		write_result_operand(instructions, result_holder);
	}
}

void vm_context_t::unload_instruction(const binwrite::disassembled_instruction_t& instruction_disassembly,
	const std::span<const binwrite::decoded_operand_t> operands)
{
	std::vector<binwrite::instruction_t>& instructions = current_instruction_.unload;

	const bool has_result = instruction_disassembly.writes_result();

	hardware_register_t result_holder;

	if (has_result)
	{
		result_holder = read_operand(instructions, 0);
	}

	free_operands(instructions, operands.size());

	if (has_result)
	{
		const auto& original_destination = operands[0];
		const auto& destination_redirected = redirect_operand(instructions, original_destination);

		const auto operand_width = static_cast<std::uint16_t>(instruction_disassembly.operand_width());
		const auto result_register = original_destination.is_reg() ? result_holder->qword : result_holder->of_size(operand_width);

		instructions.push_back(mov_instruction(result_register, destination_redirected).value());
	}
}

vm_context_t::offset_type vm_context_t::calculate_operand_offset(const size_type index)
{
	return static_cast<offset_type>(operand_size * index);
}

binwrite::encoder_operand_t vm_context_t::redirect_operand(std::vector<binwrite::instruction_t>& instructions,
	const binwrite::encoder_operand_t& operand, const std::int64_t stack_offset)
{
	if (operand.is_reg())
	{
		const auto reg = operand.reg().value;
		const auto family = reg.family();

		return register_to_stack(family.qword, 64, stack_offset);
	}

	if (operand.is_mem())
	{
		const auto mem = operand.mem();
		const auto base_width = mem.base.width();

		const binwrite::encoder_operand_t base = register_to_stack(mem.base, base_width, stack_offset);
		const auto base_holder = random_hardware_register();

		const binwrite::register_t base_register = base_holder->of_size(base_width);

		instructions.push_back(mov_instruction(base, base_register).value());

		binwrite::register_t index_register = binwrite::register_t::none;

		if (mem.index != binwrite::register_t::none)
		{
			const auto index_width = mem.index.width();

			const binwrite::encoder_operand_t index = register_to_stack(mem.index, index_width, stack_offset);
			const auto index_holder = random_hardware_register();

			index_register = index_holder->of_size(index_width);

			instructions.push_back(mov_instruction(index, index_register).value());
		}

		return encode_mem_operand(base_register, mem.displacement, mem.size, index_register, mem.scale);
	}

	return operand;
}

binwrite::encoder_operand_t vm_context_t::register_to_stack(const binwrite::register_t reg,
	const std::uint16_t operand_width, std::int64_t additional_displacement) const
{
	const auto stack_offset = register_stack_offset(reg);

	if (!stack_offset)
	{
		return reg;
	}

	if (reg.is_high_byte())
	{
		additional_displacement += 1;
	}

	return encode_stack_mem_operand(*stack_offset + additional_displacement, operand_width / 8);
}

binwrite::encoder_operand_t vm_context_t::flags_to_stack(const std::int64_t additional_displacement) const
{
	const size_type flags_index = stack_registers_.size();
	const offset_type stack_offset = static_cast<offset_type>(flags_index * stack_register_size);

	return encode_stack_mem_operand(stack_offset + additional_displacement, 8);
}

hardware_register_t vm_context_t::read_operand(std::vector<binwrite::instruction_t>& instructions,
	const size_type index)
{
	const binwrite::encoder_operand_t stack_memory = encode_stack_mem_operand(static_cast<std::int64_t>(operand_size * index), operand_size);

	hardware_register_t holder = random_hardware_register();

	instructions.push_back(mov_instruction(stack_memory, holder->qword).value());

	return holder;
}

void vm_context_t::load_flags(std::vector<binwrite::instruction_t>& instructions, const std::int64_t operand_offset)
{
	const hardware_register_t holder = random_hardware_register();

	instructions.push_back(mov_instruction(flags_to_stack(operand_offset), holder->qword).value());
	instructions.push_back(push_instruction(holder->qword).value());
	instructions.push_back(popfq_instruction().value());
}

void vm_context_t::save_flags(std::vector<binwrite::instruction_t>& instructions, const std::int64_t operand_offset)
{
	const hardware_register_t holder = random_hardware_register();

	instructions.push_back(pushfq_instruction().value());
	instructions.push_back(mov_instruction(encode_stack_mem_operand(0, 8), holder->qword).value());
	instructions.push_back(popfq_instruction().value());

	instructions.push_back(mov_instruction(holder->qword, flags_to_stack(operand_offset)).value());
}
