#include "linear_substitution.hpp"
#include "../assembler/assembler.hpp"
#include "../math/random.hpp"

#include <binwrite/disassembler/mnemonic.hpp>

std::int64_t generate_random_signed_constant(const std::uint32_t bit_count)
{
	switch (bit_count)
	{
	case 8:
		return binprotect::math::random_integral<std::int64_t>(-127, 128);
	case 16:
		return binprotect::math::random_integral<std::int16_t>();
	case 32:
	case 64:
		return binprotect::math::random_integral<std::int32_t>();
	default: ;
	}

	return 0;
}

std::uint64_t generate_random_unsigned_constant(const std::uint32_t bit_count)
{
	switch (bit_count)
	{
	case 8:
		return binprotect::math::random_integral<std::uint64_t>(0, 255);
	case 16:
		return binprotect::math::random_integral<std::uint16_t>();
	case 32:
	case 64:
		return binprotect::math::random_integral<std::uint32_t>();
	default:;
	}

	return 0;
}

std::vector<binwrite::instruction_t> substitute_single_instruction(binwrite::disassembled_instruction_t& instruction, const std::optional<std::shared_ptr<binwrite::rva_ref_t>>& rva_reference)
{
	if (rva_reference || instruction.rip_relative() || instruction.mnemonic() == binwrite::mnemonic_t::shl || instruction.mnemonic() == binwrite::mnemonic_t::shr)
	{
		return { };
	}

	std::vector<binwrite::instruction_t> instructions = { };

	const binwrite::register_family_t unused_register_family = instruction.find_unused_register();

	const auto operands = instruction.visible_operands();

	if (operands.empty())
	{
		return { };
	}

	const auto& destination_operand = operands[0];

	if (destination_operand.is_reg() && destination_operand.reg().value == binwrite::register_t::rsp)
	{
		return { };
	}

	const binwrite::decoded_operand_t::size_type operand_size = destination_operand.size();

	const binwrite::encoder_operand_t unused_register_qword(unused_register_family.qword);
	const binwrite::register_t unused_register = unused_register_family.of_size(operand_size);

	for (auto& operand : operands)
	{
		if (operand.is_imm())
		{
			const auto imm = operand.imm();

			binwrite::encoder_operand_t first;
			binwrite::encoder_operand_t second;

			if (imm.is_signed)
			{
				const std::int64_t value = imm.value.s;

				const std::int64_t random = generate_random_signed_constant(operand_size);

				first = encode_signed_imm_operand(value + random);
				second = encode_signed_imm_operand(random);
			}
			else
			{
				const std::uint64_t value = imm.value.u;

				const std::uint64_t random = generate_random_unsigned_constant(operand_size);

				first = encode_unsigned_imm_operand(value + random);
				second = encode_unsigned_imm_operand(random);
			}

			// instruction will be reassembled with this operand as a register instead
			operand.set_reg({ .value = unused_register });

			instructions.push_back(*push_instruction(unused_register_qword));

			instructions.push_back(*mov_instruction(first, unused_register));
			instructions.push_back(*sub_instruction(second, unused_register));

			const auto reassembled_instruction = make_assembler_instruction(instruction);

			instructions.push_back(*reassembled_instruction->compile());
			instructions.push_back(*pop_instruction(unused_register_qword));

			break;
		}
		
		if (operand.is_mem())
		{
			auto mem = operand.mem();

			if (mem.has_displacement)
			{
				const std::int64_t value = mem.displacement;

				const std::int64_t random = generate_random_signed_constant(operand_size);

				const auto first = encode_signed_imm_operand(value + random);
				const auto second = encode_signed_imm_operand(random);

				instructions.push_back(*push_instruction(unused_register_qword));

				const binwrite::register_t base = mem.base;

				if (base != binwrite::register_t::none)
				{
					instructions.push_back(*mov_instruction(base, unused_register_qword));
					instructions.push_back(*add_instruction(first, unused_register_qword));
				}
				else
				{
					instructions.push_back(*mov_instruction(first, unused_register_qword));
				}

				instructions.push_back(*sub_instruction(second, unused_register_qword));

				mem.base = unused_register_family.qword;
				mem.has_displacement = false;
				operand.set_mem(mem);

				const auto reassembled_instruction = make_assembler_instruction(instruction);

				instructions.push_back(*reassembled_instruction->compile());
				instructions.push_back(*pop_instruction(unused_register_qword));
			}
		}
	}

	return instructions;
}

void binprotect::linear_substitution::do_pass(binwrite::binary_t& binary, binwrite::basic_block_t& basic_block)
{
	const std::span<const binwrite::instruction_t> original_instructions = basic_block.instructions();
	std::vector instructions(original_instructions.begin(), original_instructions.end());

	std::uint32_t added = 0;

	for (std::uint32_t i = 0; i < instructions.size(); i++)
	{
		auto& instruction = instructions[i];
		auto& disassembled_instruction = instruction.disassemble();

		const std::uint32_t basic_block_index = i + added;
		const binwrite::rva_t instruction_rva = basic_block.instruction_rva(basic_block_index);

		const auto rva_reference = binary.find_rva_ref(instruction_rva);

		std::vector<binwrite::instruction_t> obfuscated_instructions = substitute_single_instruction(disassembled_instruction, rva_reference);

		if (obfuscated_instructions.empty())
		{
			continue;
		}

		basic_block.erase(binary, basic_block_index);
		basic_block.insert(binary, obfuscated_instructions, i + added);

		added += static_cast<std::uint32_t>(obfuscated_instructions.size()) - 1;
	}
}
