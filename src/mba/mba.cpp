#include "mba.hpp"
#include "../assembler/assembler.hpp"

#include <binwrite/disassembler/mnemonic.hpp>
#include <functional>

std::vector<binwrite::instruction_t> mba_stub(const binwrite::disassembled_instruction_t& instruction, std::function<void(std::vector<binwrite::instruction_t>& instructions, const binwrite::encoder_operand_t& x, const binwrite::encoder_operand_t& y, const binwrite::encoder_operand_t& unused_register)> callback)
{
	const auto& visible_operands = instruction.visible_operands();

	if (visible_operands.size() < 2)
	{
		return { };
	}

	const auto& decoded_x = visible_operands[0];

	const binwrite::encoder_operand_t& x(decoded_x);
	const binwrite::encoder_operand_t& y(visible_operands[1]);

	const binwrite::register_family_t unused_register_family = instruction.find_unused_register();

	const binwrite::encoder_operand_t unused_register(unused_register_family.of_size(decoded_x.size()));
	const binwrite::encoder_operand_t unused_register_qword(unused_register_family.qword);

	if (x.is_mem() || y.is_mem() || (x.is_reg() && x.reg().value == binwrite::register_t::rsp) || (y.is_reg() && y.reg().value == binwrite::register_t::rsp))
	{
		return { };
	}

	std::vector<binwrite::instruction_t> instructions = { };

	instructions.push_back(*push_instruction(unused_register_qword));
	instructions.push_back(*mov_instruction(x, unused_register));

	callback(instructions, x, y, unused_register);

	instructions.push_back(*pop_instruction(unused_register_qword));

	return instructions;
}

std::vector<binwrite::instruction_t> mba_obfuscate_add(const binwrite::disassembled_instruction_t& instruction);
std::vector<binwrite::instruction_t> mba_obfuscate_sub(const binwrite::disassembled_instruction_t& instruction);
std::vector<binwrite::instruction_t> mba_obfuscate_and(const binwrite::disassembled_instruction_t& instruction);
std::vector<binwrite::instruction_t> mba_obfuscate_or(const binwrite::disassembled_instruction_t& instruction);
std::vector<binwrite::instruction_t> mba_obfuscate_xor(const binwrite::disassembled_instruction_t& instruction);

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

		if (obfuscated_instructions.empty())
		{
			continue;
		}

		basic_block.erase(binary, i + added);
		basic_block.insert(binary, obfuscated_instructions, i + added);

		added += static_cast<std::uint32_t>(obfuscated_instructions.size()) - 1;
	}
}

std::vector<binwrite::instruction_t> mba_obfuscate_add(const binwrite::disassembled_instruction_t& instruction)
{
	const auto callback =
		[](std::vector<binwrite::instruction_t>& instructions, const binwrite::encoder_operand_t& x, const binwrite::encoder_operand_t& y, const binwrite::encoder_operand_t& unused_register) -> void
		{
			// (x + y) can be obfuscated to ((x & y) + (x | y))

			// unused register = (x & y)
			instructions.push_back(*and_instruction(y, unused_register));

			// x = (x | y)
			instructions.push_back(*or_instruction(y, x));

			// x = x + unused register
			instructions.push_back(*add_instruction(unused_register, x));
		};

	return mba_stub(instruction, callback);
}

std::vector<binwrite::instruction_t> mba_obfuscate_and(const binwrite::disassembled_instruction_t& instruction)
{
	const auto callback =
		[](std::vector<binwrite::instruction_t>& instructions, const binwrite::encoder_operand_t& x, const binwrite::encoder_operand_t& y, const binwrite::encoder_operand_t& unused_register) -> void
		{
			instructions.push_back(*add_instruction(y, x));
			instructions.push_back(*or_instruction(y, unused_register));
			instructions.push_back(*sub_instruction(unused_register, x));
		};

	return mba_stub(instruction, callback);
}

std::vector<binwrite::instruction_t> mba_obfuscate_xor(const binwrite::disassembled_instruction_t& instruction)
{
	const auto callback =
		[](std::vector<binwrite::instruction_t>& instructions, const binwrite::encoder_operand_t& x, const binwrite::encoder_operand_t& y, const binwrite::encoder_operand_t& unused_register) -> void
		{
			instructions.push_back(*and_instruction(y, unused_register));
			instructions.push_back(*or_instruction(y, x));
			instructions.push_back(*sub_instruction(unused_register, x));
		};

	return mba_stub(instruction, callback);
}

std::vector<binwrite::instruction_t> mba_obfuscate_sub(const binwrite::disassembled_instruction_t& instruction)
{
	const auto callback =
		[](std::vector<binwrite::instruction_t>& instructions, const binwrite::encoder_operand_t& x, const binwrite::encoder_operand_t& y, const binwrite::encoder_operand_t& unused_register) -> void
		{
			if (y.is_imm())
			{
				const binwrite::encoder_operand_t neg_y = encode_unsigned_imm_operand(-y.imm().s);

				instructions.push_back(*xor_instruction(neg_y, unused_register));
				instructions.push_back(*and_instruction(neg_y, x));
			}
			else
			{
				instructions.push_back(*neg_instruction(y));

				instructions.push_back(*xor_instruction(y, unused_register));
				instructions.push_back(*and_instruction(y, x));

				instructions.push_back(*neg_instruction(y));
			}

			instructions.push_back(*shl_instruction(x, 1));
			instructions.push_back(*add_instruction(unused_register, x));
		};

	return mba_stub(instruction, callback);
}

std::vector<binwrite::instruction_t> mba_obfuscate_or(const binwrite::disassembled_instruction_t& instruction)
{
	const auto callback =
		[](std::vector<binwrite::instruction_t>& instructions, const binwrite::encoder_operand_t& x, const binwrite::encoder_operand_t& y, const binwrite::encoder_operand_t& unused_register) -> void
		{
			const binwrite::encoder_operand_t constant = encode_unsigned_imm_operand(1);

			instructions.push_back(*add_instruction(y, unused_register));
			instructions.push_back(*add_instruction(constant, unused_register));

			if (y.is_imm())
			{
				const binwrite::encoder_operand_t not_y = encode_unsigned_imm_operand(~y.imm().u);

				instructions.push_back(*not_instruction(x));

				instructions.push_back(*or_instruction(not_y, x));

				instructions.push_back(*add_instruction(unused_register, x));
			}
			else
			{
				instructions.push_back(*not_instruction(y));
				instructions.push_back(*not_instruction(x));

				instructions.push_back(*or_instruction(y, x));

				instructions.push_back(*not_instruction(y));

				instructions.push_back(*add_instruction(unused_register, x));
			}
		};

	return mba_stub(instruction, callback);
}
