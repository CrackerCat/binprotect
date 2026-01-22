#include "binary.hpp"

static std::shared_ptr<binwrite::basic_block_t> previous_basic_block(const binwrite::binary_t& binary, const binwrite::basic_block_t& basic_block)
{
	const binwrite::rva_t current_block_rva = *basic_block.rva();
	const binwrite::rva_t last_block_rva(current_block_rva.value() - 1);

	return binary.is_inside_basic_block(last_block_rva);
}

static std::int32_t estimate_jump_table_count(const binwrite::binary_t& binary, const binwrite::basic_block_t& basic_block)
{
	const auto last_block = previous_basic_block(binary, basic_block);

	if (!last_block || last_block->count() < 2)
	{
		return -1;
	}

	const auto& index_instruction = last_block->at(last_block->count() - 2);
	const auto& index_disassembly = index_instruction.disassemble();

	if (!index_disassembly.is_sub() && !index_disassembly.is_cmp())
	{
		return -1;
	}

	const auto index_operands = index_disassembly.visible_operands();

	if (!index_operands.empty() && index_operands[1].is_imm())
	{
		const auto imm = index_operands[1].imm();

		return static_cast<std::int32_t>(imm.value.s) + 1;
	}

	return -1;
}

static std::optional<binwrite::decoded_operand_t::mem_t> extract_jump_table_mov_operand(const binwrite::disassembled_instruction_t& mov_disassembly, const bool ignore_scale = false)
{
	const auto& mov_operands = mov_disassembly.visible_operands();

	if (mov_operands.size() < 2)
	{
		return { };
	}

	const auto& mov_operand = mov_operands[1];

	if (!mov_operand.is_mem())
	{
		return { };
	}

	const auto mem = mov_operand.mem();

	if ((!ignore_scale && mem.scale != 4) ||
		mem.index == binwrite::register_t::none ||
		mem.base == binwrite::register_t::none)
	{
		return { };
	}

	return mem;
}

static std::optional<binwrite::disassembled_instruction_t> extract_jump_table_lea_disassembly(
	const binwrite::basic_block_t& basic_block, const binwrite::basic_block_t::size_type lea_index,
	const binwrite::decoded_operand_t::mem_t mem)
{
	const auto& lea_instruction = basic_block.at(lea_index);
	const auto& lea_disassembly = lea_instruction.disassemble();

	const auto lea_operands = lea_disassembly.visible_operands();

	if (lea_operands.empty())
	{
		return { };
	}

	const auto reg = lea_operands[0];

	if (!reg.is_reg() || reg.reg().value != mem.base)
	{
		return { };
	}

	return lea_disassembly;
}

bool binwrite::binary_t::process_multi_level_jump_table(const basic_block_t& basic_block, const rva_t entry_table_base,
	const basic_block_t::size_type mov_index)
{
	if (mov_index == 0)
	{
		return false;
	}

	const basic_block_t::size_type previous_index = mov_index - 1;

	const auto& movzx_instruction = basic_block.at(previous_index);
	const auto& movzx_disassembly = movzx_instruction.disassemble();

	if (!movzx_disassembly.is_movzx())
	{
		return false;
	}

	const auto mem = extract_jump_table_mov_operand(movzx_disassembly, true);

	if (!mem)
	{
		return false;
	}

	const rva_t movzx_rva = basic_block.instruction_rva(previous_index);
	const auto index_table_base = add_rva(static_cast<rva_t::value_type>(mem->displacement));

	const std::uint32_t inner_table_size = index_table_base->value() - entry_table_base.value();
	const std::int32_t inner_table_count = static_cast<std::int32_t>(inner_table_size / 4);

	add_rva_ref(std::make_shared<msvc_jmp_table_ref_t>(index_table_base, movzx_rva, movzx_disassembly.size()));
	add_msvc_jmp_table_ref(entry_table_base, inner_table_count);

	return true;
}

void binwrite::binary_t::process_jump_table_instruction(const basic_block_t& basic_block,
	const disassembled_instruction_t& mov_disassembly,
	const basic_block_t::size_type mov_index,
	const basic_block_t::size_type lea_index)
{
	const auto mem = extract_jump_table_mov_operand(mov_disassembly);

	if (!mem)
	{
		return;
	}

	const auto lea_disassembly = extract_jump_table_lea_disassembly(basic_block, lea_index, *mem);

	if (!lea_disassembly)
	{
		return;
	}

	const std::int32_t count = estimate_jump_table_count(*this, basic_block);

	if (mem->has_displacement) // has displacement = is MSVC
	{
		const auto table_base = add_rva(static_cast<rva_t::value_type>(mem->displacement));
		const rva_t mov_disassembly_rva = basic_block.instruction_rva(mov_index);

		add_rva_ref(std::make_shared<msvc_jmp_table_ref_t>(table_base, mov_disassembly_rva, mov_disassembly.size()));

		if (!process_multi_level_jump_table(basic_block, *table_base, mov_index))
		{
			add_msvc_jmp_table_ref(*table_base, count);
		}
	}
	else if (const auto table_base = resolve_instruction_rva(*lea_disassembly, basic_block.instruction_rva(lea_index)))
	{
		add_llvm_jmp_table_ref(rva_t{ *table_base }, count);
	}
}

void binwrite::binary_t::find_jump_tables(const basic_block_t& basic_block)
{
	const auto& instructions = basic_block.instructions();

	basic_block_t::size_type latest_lea = -1;

	for (std::uint32_t i = 0; i < instructions.size(); i++)
	{
		const auto& instruction = instructions[i];
		const auto& disassembled_instruction = instruction.disassemble();

		if (disassembled_instruction.is_lea() && disassembled_instruction.rip_relative())
		{
			latest_lea = i;
		}
		else if (latest_lea != -1 && disassembled_instruction.is_mov())
		{
			process_jump_table_instruction(basic_block, disassembled_instruction, i, latest_lea);
		}
	}
}

void binwrite::binary_t::add_llvm_jmp_table_ref(const rva_t table_base, const std::int32_t count)
{
	const auto table_base_rva = add_rva(table_base);

	rva_t table_entry = table_base;

	std::int32_t i = 0;

	while (count == -1 || i++ < count)
	{
		const llvm_jmp_table_entry_t::value_type offset = *reinterpret_cast<const llvm_jmp_table_entry_t::value_type*>(data() + table_entry.value());

		const auto target_rva = add_rva(table_base.value() + offset);

		if (!is_in_code_section(*target_rva))
		{
			break;
		}

		const auto ref = std::make_shared<llvm_jmp_table_entry_t>(target_rva, table_entry, table_base_rva);

		add_to_disassembly_queue(target_rva);
		add_rva_ref(ref);

		table_entry.set_value(table_entry.value() + sizeof(llvm_jmp_table_entry_t::size_type));
	}
}

void binwrite::binary_t::add_msvc_jmp_table_ref(const rva_t table_base, const std::int32_t count)
{
	rva_t table_entry = table_base;

	std::int32_t i = 0;

	while (count == -1 || i++ < count)
	{
		const auto entry = reinterpret_cast<const rva_t::value_type*>(data() + table_entry.value());

		if (!is_in_code_section(rva_t{ *entry }))
		{
			break;
		}

		const auto ref = add_data_rva_ref(entry);

		add_to_disassembly_queue(ref->target());

		table_entry.set_value(table_entry.value() + sizeof(llvm_jmp_table_entry_t::size_type));
	}
}
