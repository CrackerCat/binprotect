#include "mba.hpp"
#include "flag_behaviour.hpp"

#include "../assembler/assembler.hpp"

#include <binwrite/disassembler/mnemonic.hpp>
#include <functional>
#include <spdlog/spdlog.h>

static std::vector<binwrite::instruction_t> mba_stub(const binwrite::disassembled_instruction_t& instruction,
                                                     const std::function<void(
	                                                     std::vector<binwrite::instruction_t>& instructions,
	                                                     const binwrite::encoder_operand_t& x,
	                                                     const binwrite::encoder_operand_t& y,
	                                                     const binwrite::encoder_operand_t& unused_register,
	                                                     const binwrite::encoder_operand_t& second_unused_register)>&
                                                     callback)
{
	const auto& visible_operands = instruction.visible_operands();

	if (visible_operands.size() < 2 || instruction.rsp_relative())
	{
		return { };
	}

	const auto& decoded_x = visible_operands[0];

	const binwrite::encoder_operand_t x(decoded_x);
	const binwrite::encoder_operand_t y(visible_operands[1]);

	const binwrite::register_family_t unused_register_family = instruction.find_unused_register();
	const binwrite::register_family_t second_unused_register_family = instruction.find_unused_register(unused_register_family);

	const binwrite::encoder_operand_t unused_register(unused_register_family.of_size(decoded_x.size()));
	const binwrite::encoder_operand_t unused_register_qword(unused_register_family.qword);

	const binwrite::encoder_operand_t second_unused_register(second_unused_register_family.of_size(decoded_x.size()));
	const binwrite::encoder_operand_t second_unused_register_qword(second_unused_register_family.qword);

	std::vector<binwrite::instruction_t> instructions = { };

	instructions.push_back(push_instruction(unused_register_qword).value());
	instructions.push_back(push_instruction(second_unused_register_qword).value());
	instructions.push_back(pushfq_instruction().value());

	instructions.push_back(mov_instruction(x, unused_register).value());
	instructions.push_back(mov_instruction(y, second_unused_register).value());

	// todo: only do if needed
	instructions.push_back(push_instruction(unused_register_qword).value());

	callback(instructions, x, y, unused_register, second_unused_register);

	instructions.push_back(pop_instruction(unused_register_qword).value()); // restore 'x' value into unused register
	instructions.push_back(popfq_instruction().value());

	const auto flag_emulation_instructions = binprotect::mba::emulate_flag_behaviour(instruction, x, unused_register_family, unused_register, y, second_unused_register_family, second_unused_register);

	instructions.insert(instructions.end(), flag_emulation_instructions.begin(), flag_emulation_instructions.end());

	instructions.push_back(pop_instruction(second_unused_register_qword).value());
	instructions.push_back(pop_instruction(unused_register_qword).value());

	return instructions;
}

static std::vector<binwrite::instruction_t> mba_obfuscate_add(const binwrite::disassembled_instruction_t& instruction);
static std::vector<binwrite::instruction_t> mba_obfuscate_sub(const binwrite::disassembled_instruction_t& instruction);
static std::vector<binwrite::instruction_t> mba_obfuscate_and(const binwrite::disassembled_instruction_t& instruction);
static std::vector<binwrite::instruction_t> mba_obfuscate_or(const binwrite::disassembled_instruction_t& instruction);
static std::vector<binwrite::instruction_t> mba_obfuscate_xor(const binwrite::disassembled_instruction_t& instruction);

void binprotect::mba::do_pass(binwrite::binary_t& binary, binwrite::basic_block_t& basic_block)
{
	const std::span<const binwrite::instruction_t> original_instructions = basic_block.instructions();
	const std::vector instructions(original_instructions.begin(), original_instructions.end());

	std::uint32_t added = 0;

	for (std::uint32_t i = 0; i < instructions.size(); i++)
	{
		const auto& instruction = instructions[i];
		const auto& disassembled_instruction = instruction.disassemble();

		std::vector<binwrite::instruction_t> obfuscated_instructions = { };

		try
		{
			const binwrite::mnemonic_t mnemonic = disassembled_instruction.mnemonic();

			if (mnemonic == binwrite::mnemonic_t::add)
			{
				obfuscated_instructions = mba_obfuscate_add(disassembled_instruction);
			}
			else if (mnemonic == binwrite::mnemonic_t::sub)
			{
				obfuscated_instructions = mba_obfuscate_sub(disassembled_instruction);
			}
			else if (mnemonic == binwrite::mnemonic_t::and_)
			{
				obfuscated_instructions = mba_obfuscate_and(disassembled_instruction);
			}
			else if (mnemonic == binwrite::mnemonic_t::or_)
			{
				obfuscated_instructions = mba_obfuscate_or(disassembled_instruction);
			}
			else if (mnemonic == binwrite::mnemonic_t::xor_)
			{
				obfuscated_instructions = mba_obfuscate_xor(disassembled_instruction);
			}
		}
		catch (const std::exception&)
		{
			spdlog::error("unable to do mixed boolean arithmetic pass on '{}'", disassembled_instruction.to_string());
		}

		if (obfuscated_instructions.empty())
		{
			continue;
		}

		const std::uint32_t basic_block_index = i + added;

		basic_block.insert(binary, obfuscated_instructions, basic_block_index);
		basic_block.erase(binary, basic_block_index + static_cast<std::uint32_t>(obfuscated_instructions.size()));

		added += static_cast<std::uint32_t>(obfuscated_instructions.size()) - 1;
	}
}

std::vector<binwrite::instruction_t> mba_obfuscate_add(const binwrite::disassembled_instruction_t& instruction)
{
	const auto callback =
		[](std::vector<binwrite::instruction_t>& instructions, const binwrite::encoder_operand_t& x, const binwrite::encoder_operand_t& y, const binwrite::encoder_operand_t& unused_register, const binwrite::encoder_operand_t& second_unused_register) -> void
		{
			// (x + y) can be obfuscated to ((x & y) + (x | y))

			// unused register = (x & y)
			instructions.push_back(and_instruction(y, unused_register).value());

			// x = (x | y)
			instructions.push_back(or_instruction(y, x).value());

			// x = x + unused register
			instructions.push_back(add_instruction(unused_register, x).value());
		};

	return mba_stub(instruction, callback);
}

std::vector<binwrite::instruction_t> mba_obfuscate_and(const binwrite::disassembled_instruction_t& instruction)
{
	const auto callback =
		[](std::vector<binwrite::instruction_t>& instructions, const binwrite::encoder_operand_t& x, const binwrite::encoder_operand_t& y, const binwrite::encoder_operand_t& unused_register, const binwrite::encoder_operand_t& second_unused_register) -> void
		{
			instructions.push_back(add_instruction(y, x).value());
			instructions.push_back(or_instruction(y, unused_register).value());
			instructions.push_back(sub_instruction(unused_register, x).value());
		};

	return mba_stub(instruction, callback);
}

std::vector<binwrite::instruction_t> mba_obfuscate_xor(const binwrite::disassembled_instruction_t& instruction)
{
	const auto callback =
		[](std::vector<binwrite::instruction_t>& instructions, const binwrite::encoder_operand_t& x, const binwrite::encoder_operand_t& y, const binwrite::encoder_operand_t& unused_register, const binwrite::encoder_operand_t& second_unused_register) -> void
		{
			instructions.push_back(and_instruction(y, unused_register).value());
			instructions.push_back(or_instruction(y, x).value());
			instructions.push_back(sub_instruction(unused_register, x).value());
		};

	return mba_stub(instruction, callback);
}

std::vector<binwrite::instruction_t> mba_obfuscate_sub(const binwrite::disassembled_instruction_t& instruction)
{
	const auto callback =
		[](std::vector<binwrite::instruction_t>& instructions, const binwrite::encoder_operand_t& x, const binwrite::encoder_operand_t& y, const binwrite::encoder_operand_t& unused_register, const binwrite::encoder_operand_t& second_unused_register) -> void
		{
			instructions.push_back(neg_instruction(second_unused_register).value());

			instructions.push_back(xor_instruction(second_unused_register, unused_register).value());
			instructions.push_back(and_instruction(second_unused_register, x).value());

			instructions.push_back(shl_instruction(x, 1).value());
			instructions.push_back(add_instruction(unused_register, x).value());
		};

	return mba_stub(instruction, callback);
}

std::vector<binwrite::instruction_t> mba_obfuscate_or(const binwrite::disassembled_instruction_t& instruction)
{
	const auto callback =
		[](std::vector<binwrite::instruction_t>& instructions, const binwrite::encoder_operand_t& x, const binwrite::encoder_operand_t& y, const binwrite::encoder_operand_t& unused_register, const binwrite::encoder_operand_t& second_unused_register) -> void
		{
			const binwrite::encoder_operand_t constant = encode_unsigned_imm_operand(1);

			instructions.push_back(add_instruction(y, unused_register).value());
			instructions.push_back(add_instruction(constant, unused_register).value());

			instructions.push_back(not_instruction(second_unused_register).value());
			instructions.push_back(not_instruction(x).value());

			instructions.push_back(or_instruction(second_unused_register, x).value());

			instructions.push_back(add_instruction(unused_register, x).value());
		};

	return mba_stub(instruction, callback);
}
