#include "linear_substitution.hpp"
#include "../assembler/assembler.hpp"

#include <binwrite/disassembler/mnemonic.hpp>
#include <binwrite/math/random.hpp>
#include <spdlog/spdlog.h>

std::int64_t generate_random_signed_constant(const std::uint32_t bit_count)
{
	switch (bit_count)
	{
	case 8:
		return binwrite::math::random_integral<std::int64_t>(std::numeric_limits<std::int8_t>::min(), std::numeric_limits<std::int8_t>::max());
	case 16:
		return binwrite::math::random_integral<std::int16_t>();
	case 32:
	case 64:
		return binwrite::math::random_integral<std::int32_t>();
	default: ;
	}

	return 0;
}

std::int64_t overflow_signed_constant(const std::int64_t value, const std::uint32_t bit_count)
{
	switch (bit_count)
	{
	case 8:
		return static_cast<std::int8_t>(value);
	case 16:
		return static_cast<std::int16_t>(value);
	case 32:
		return static_cast<std::int32_t>(value);
	default: ;
	}

	return value;
}

std::uint64_t generate_random_unsigned_constant(const std::uint32_t bit_count)
{
	switch (bit_count)
	{
	case 8:
		return binwrite::math::random_integral<std::uint64_t>(std::numeric_limits<std::uint8_t>::min(), std::numeric_limits<std::uint8_t>::max());
	case 16:
		return binwrite::math::random_integral<std::uint16_t>();
	case 32:
	case 64:
		return binwrite::math::random_integral<std::uint32_t>();
	default: ;
	}

	return 0;
}

std::uint64_t overflow_unsigned_constant(const std::uint64_t value, const std::uint32_t bit_count)
{
	switch (bit_count)
	{
	case 8:
		return static_cast<std::uint8_t>(value);
	case 16:
		return static_cast<std::uint16_t>(value);
	case 32:
		return static_cast<std::uint32_t>(value);
	default: ;
	}

	return value;
}

std::vector<binwrite::instruction_t> substitute_single_instruction(binwrite::disassembled_instruction_t& instruction)
{
	const binwrite::register_family_t unused_register_family = instruction.find_unused_register();

	const auto operands = instruction.visible_operands();

	if (operands.empty())
	{
		return { };
	}

	const auto& destination_operand = operands[0];

	const binwrite::decoded_operand_t::size_type operand_size = destination_operand.size();

	const binwrite::encoder_operand_t unused_register_qword(unused_register_family.qword);
	const binwrite::register_t unused_register = unused_register_family.of_size(operand_size);

	std::vector<binwrite::instruction_t> instructions = { };

	for (auto& operand : operands)
	{
		if (operand.is_reg())
		{
			const auto reg = operand.reg().value;
			const auto family = reg.family();

			if (family == binwrite::register_family_t::sp)
			{
				return { };
			}
		}
		else if (operand.is_imm())
		{
			const auto imm = operand.imm();

			binwrite::encoder_operand_t first;
			binwrite::encoder_operand_t second;

			if (imm.is_signed)
			{
				const std::int64_t value = imm.value.s;

				const std::int64_t random = generate_random_signed_constant(operand_size);

				first = encode_signed_imm_operand(overflow_signed_constant(value + random, operand_size));
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

			instructions.push_back(push_instruction(unused_register_qword).value());
			instructions.push_back(pushfq_instruction().value());

			instructions.push_back(mov_instruction(first, unused_register).value());
			instructions.push_back(sub_instruction(second, unused_register).value());

			instructions.push_back(popfq_instruction().value());

			const auto reassembled_instruction = make_assembler_instruction(instruction);

			instructions.push_back(reassembled_instruction->compile().value());
			instructions.push_back(pop_instruction(unused_register_qword).value());

			break;
		}
		else if (operand.is_mem())
		{
			auto mem = operand.mem();

			if (mem.has_displacement)
			{
				std::int64_t value = mem.displacement;

				constexpr std::uint32_t displacement_bit_count = 32;

				const std::int64_t random = generate_random_signed_constant(displacement_bit_count);

				if (mem.base == binwrite::register_t::rsp)
				{
					value += 16;
				}

				const auto first = encode_signed_imm_operand(overflow_signed_constant(value + random, displacement_bit_count));
				const auto second = encode_signed_imm_operand(random);

				instructions.push_back(push_instruction(unused_register_qword).value());
				instructions.push_back(pushfq_instruction().value());

				const binwrite::register_t base = mem.base;

				if (base != binwrite::register_t::none)
				{
					instructions.push_back(mov_instruction(base, unused_register_qword).value());
					instructions.push_back(add_instruction(first, unused_register_qword).value());
				}
				else
				{
					instructions.push_back(mov_instruction(first, unused_register_qword).value());
				}

				instructions.push_back(sub_instruction(second, unused_register_qword).value());

				mem.base = unused_register_family.qword;
				mem.has_displacement = false;
				operand.set_mem(mem);

				instructions.push_back(popfq_instruction().value());

				const auto reassembled_instruction = make_assembler_instruction(instruction);

				instructions.push_back(reassembled_instruction->compile().value());
				instructions.push_back(pop_instruction(unused_register_qword).value());
			}

			break;
		}
	}

	return instructions;
}

void binprotect::linear_substitution::do_pass(binwrite::binary_t& binary, binwrite::basic_block_t& basic_block)
{
	const std::span<const binwrite::instruction_t> original_instructions = basic_block.instructions();
	std::vector instructions(original_instructions.begin(), original_instructions.end());

	std::uint32_t added = 0;

	for (std::uint32_t i = 1; i < instructions.size(); i++)
	{
		auto& instruction = instructions[i];
		auto& disassembled_instruction = instruction.disassemble();

		const std::uint32_t basic_block_index = i + added;
		const binwrite::rva_t instruction_rva = basic_block.instruction_rva(basic_block_index);

		if (disassembled_instruction.rip_relative() || disassembled_instruction.writes_stack_pointer() ||
			disassembled_instruction.has_lock() || binary.find_rva_ref(instruction_rva))
		{
			continue;
		}

		std::vector<binwrite::instruction_t> obfuscated_instructions;

		try
		{
			obfuscated_instructions = substitute_single_instruction(disassembled_instruction);
		}
		catch (const std::exception&)
		{
			spdlog::error("unable to linearly substitute '{}'", disassembled_instruction.to_string());

			continue;
		}

		if (obfuscated_instructions.empty())
		{
			continue;
		}

		basic_block.insert(binary, obfuscated_instructions, basic_block_index);
		basic_block.erase(binary, basic_block_index + static_cast<std::uint32_t>(obfuscated_instructions.size()));

		added += static_cast<std::uint32_t>(obfuscated_instructions.size()) - 1;
	}
}
