#include "mba.hpp"
#include "flag_behaviour.hpp"

#include "../assembler/assembler.hpp"

#include <binwrite/disassembler/mnemonic.hpp>
#include <functional>
#include <spdlog/spdlog.h>

#include "binwrite/math/random.hpp"

using mba_callback_t = std::function<void(std::vector<binwrite::instruction_t>& instructions,
	const binwrite::encoder_operand_t& x,
	const binwrite::encoder_operand_t& y,
	const binwrite::encoder_operand_t& unused_register,
	const binwrite::encoder_operand_t& second_unused_register)>;

#define MBA_CALLBACK(callback) (mba_callback_t)[](std::vector<binwrite::instruction_t>& instructions,\
                                  const binwrite::encoder_operand_t& x, const binwrite::encoder_operand_t& y, const binwrite::encoder_operand_t& unused_register,\
								  const binwrite::encoder_operand_t& second_unused_register) -> void\
								  {\
									  callback\
								  }

static std::vector<binwrite::instruction_t> mba_stub(const binwrite::disassembled_instruction_t& instruction,
													const bool should_emulate_flags, const std::span<const mba_callback_t> callbacks)
{
	const auto& visible_operands = instruction.visible_operands();

	if (callbacks.empty() || visible_operands.size() < 2)
	{
		return { };
	}

	const auto& decoded_x = visible_operands[0];
	const auto& decoded_y = visible_operands[1];

	const binwrite::encoder_operand_t x(decoded_x);
	const binwrite::encoder_operand_t y(decoded_y);

	const binwrite::register_family_t unused_register_family = instruction.find_unused_register();
	const binwrite::register_family_t second_unused_register_family = instruction.find_unused_register(unused_register_family);

	const binwrite::encoder_operand_t unused_register(unused_register_family.of_size(decoded_x.size()));
	const binwrite::encoder_operand_t unused_register_qword(unused_register_family.qword);

	const binwrite::encoder_operand_t second_unused_register(second_unused_register_family.of_size(decoded_x.size()));
	const binwrite::encoder_operand_t second_unused_register_qword(second_unused_register_family.qword);

	std::vector<binwrite::instruction_t> instructions = { };

	instructions.push_back(push_instruction(unused_register_qword).value());
	instructions.push_back(push_instruction(second_unused_register_qword).value());

	instructions.push_back(mov_instruction(x, unused_register).value());
	instructions.push_back(mov_instruction(y, second_unused_register).value());

	const auto& callback = binwrite::math::random_entry<mba_callback_t>(callbacks);

	callback(instructions, x, y, unused_register, second_unused_register);

	if (should_emulate_flags)
	{
		instructions.insert(instructions.begin() + 4, push_instruction(unused_register_qword).value());
		instructions.insert(instructions.begin() + 4, pushfq_instruction().value());

		instructions.push_back(pop_instruction(unused_register_qword).value()); // restore 'x' value into unused register
		instructions.push_back(popfq_instruction().value());

		const auto flag_emulation_instructions = binprotect::mba::emulate_flag_behaviour(
			instruction, x, unused_register_family,
 unused_register, y, second_unused_register_family,
			second_unused_register);

		instructions.insert_range(instructions.end(), flag_emulation_instructions);
	}

	instructions.push_back(pop_instruction(second_unused_register_qword).value());
	instructions.push_back(pop_instruction(unused_register_qword).value());

	return instructions;
}

static std::vector<binwrite::instruction_t> mba_obfuscate_add(const binwrite::disassembled_instruction_t& instruction, bool should_emulate_flags);
static std::vector<binwrite::instruction_t> mba_obfuscate_sub(const binwrite::disassembled_instruction_t& instruction, bool should_emulate_flags);
static std::vector<binwrite::instruction_t> mba_obfuscate_and(const binwrite::disassembled_instruction_t& instruction, bool should_emulate_flags);
static std::vector<binwrite::instruction_t> mba_obfuscate_or(const binwrite::disassembled_instruction_t& instruction, bool should_emulate_flags);
static std::vector<binwrite::instruction_t> mba_obfuscate_xor(const binwrite::disassembled_instruction_t& instruction, bool should_emulate_flags);

void binprotect::mba::do_pass(binwrite::binary_t& binary, binwrite::basic_block_t& basic_block, const bool is_first_pass)
{
	const std::span<const binwrite::instruction_t> original_instructions = basic_block.instructions();
	const std::vector instructions(original_instructions.begin(), original_instructions.end());

	auto flag_dependants = is_first_pass ? find_flag_dependent_instructions(instructions) : std::deque<flag_dependant_t>{ };

	std::uint32_t added = 0;

	for (std::uint32_t i = 0; i < instructions.size(); i++)
	{
		const auto& instruction = instructions[i];
		const auto& disassembled_instruction = instruction.disassemble();

		if (disassembled_instruction.rip_relative() || disassembled_instruction.rsp_relative() || disassembled_instruction.has_lock())
		{
			continue;
		}

		std::vector<binwrite::instruction_t> obfuscated_instructions = { };

		try
		{
			const bool should_emulate_flags = should_instruction_emulate_flags(flag_dependants, i, obfuscated_instructions);

			const binwrite::mnemonic_t mnemonic = disassembled_instruction.mnemonic();

			// switch statement isn't compatible with the mnemonic_t members as they aren't constexpr
			if (mnemonic == binwrite::mnemonic_t::add)
			{
				obfuscated_instructions = mba_obfuscate_add(disassembled_instruction, should_emulate_flags);
			}
			else if (mnemonic == binwrite::mnemonic_t::sub)
			{
				obfuscated_instructions = mba_obfuscate_sub(disassembled_instruction, should_emulate_flags);
			}
			else if (mnemonic == binwrite::mnemonic_t::and_)
			{
				obfuscated_instructions = mba_obfuscate_and(disassembled_instruction, should_emulate_flags);
			}
			else if (mnemonic == binwrite::mnemonic_t::or_)
			{
				obfuscated_instructions = mba_obfuscate_or(disassembled_instruction, should_emulate_flags);
			}
			else if (mnemonic == binwrite::mnemonic_t::xor_)
			{
				obfuscated_instructions = mba_obfuscate_xor(disassembled_instruction, should_emulate_flags);
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

std::vector<binwrite::instruction_t> mba_obfuscate_add(const binwrite::disassembled_instruction_t& instruction, const bool should_emulate_flags)
{
	const std::array callbacks = {
		MBA_CALLBACK(
			// (x + y) can be obfuscated to ((x & y) + (x | y))

			// unused register = (x & y)
			instructions.push_back(and_instruction(y, unused_register).value());

			// x = (x | y)
			instructions.push_back(or_instruction(y, x).value());

			// x = x + unused register
			instructions.push_back(add_instruction(unused_register, x).value());
		),
		MBA_CALLBACK(
			// (x + y) = (x ^ y) + 2(x & y)
			instructions.push_back(and_instruction(y, unused_register).value());
			instructions.push_back(xor_instruction(y, x).value());
			instructions.push_back(shl_instruction(unused_register, 1).value());

			instructions.push_back(add_instruction(unused_register, x).value());
		),
		MBA_CALLBACK(
			// (x + y) = 2(x | y) - (x ^ y)
			instructions.push_back(xor_instruction(y, unused_register).value());

			instructions.push_back(or_instruction(second_unused_register, x).value());
			instructions.push_back(shl_instruction(x, 1).value());

			instructions.push_back(sub_instruction(unused_register, x).value());
		)
	};

	return mba_stub(instruction, should_emulate_flags, callbacks);
}

std::vector<binwrite::instruction_t> mba_obfuscate_and(const binwrite::disassembled_instruction_t& instruction, const bool should_emulate_flags)
{
	const std::array callbacks = {
		MBA_CALLBACK(
			// (x & y) = (x + y) - (x | y)

			instructions.push_back(or_instruction(y, unused_register).value());
			instructions.push_back(add_instruction(y, x).value());
			instructions.push_back(sub_instruction(unused_register, x).value());
		),
		MBA_CALLBACK(
			// (x & y) = (~x | y) - (~x)

			instructions.push_back(not_instruction(unused_register).value());
			instructions.push_back(not_instruction(x).value());

			instructions.push_back(or_instruction(second_unused_register, x).value());

			instructions.push_back(sub_instruction(unused_register, x).value());
		)
	};

	return mba_stub(instruction, should_emulate_flags, callbacks);
}

std::vector<binwrite::instruction_t> mba_obfuscate_xor(const binwrite::disassembled_instruction_t& instruction, const bool should_emulate_flags)
{
	const std::array callbacks = {
		MBA_CALLBACK(
			// (x ^ y) = (x | y) - (x & y)

			instructions.push_back(and_instruction(y, unused_register).value());
			instructions.push_back(or_instruction(y, x).value());
			instructions.push_back(sub_instruction(unused_register, x).value());
		)
	};

	return mba_stub(instruction, should_emulate_flags, callbacks);
}

std::vector<binwrite::instruction_t> mba_obfuscate_sub(const binwrite::disassembled_instruction_t& instruction, const bool should_emulate_flags)
{
	const std::array callbacks = {
		MBA_CALLBACK(
			// (x - y) = 2(x & ~y) - (x ^ y) 
			instructions.push_back(xor_instruction(y, unused_register).value());

			instructions.push_back(not_instruction(second_unused_register).value());
			instructions.push_back(and_instruction(second_unused_register, x).value());
			instructions.push_back(shl_instruction(x, 1).value());

			instructions.push_back(sub_instruction(unused_register, x).value());
		),
		MBA_CALLBACK(
			// (x - y) = (x & ~y) - (~x & y)
			instructions.push_back(not_instruction(unused_register).value());
			instructions.push_back(not_instruction(second_unused_register).value());

			instructions.push_back(and_instruction(y, unused_register).value());
			instructions.push_back(and_instruction(second_unused_register, x).value());

			instructions.push_back(sub_instruction(unused_register, x).value());
		),
		MBA_CALLBACK(
			// (x - y) = (x ^ y) - 2(~x & y)
			instructions.push_back(not_instruction(unused_register).value());
			instructions.push_back(and_instruction(y, unused_register).value());
			instructions.push_back(shl_instruction(unused_register, 1).value());

			instructions.push_back(xor_instruction(y, x).value());

			instructions.push_back(sub_instruction(unused_register, x).value());
		)
	};

	return mba_stub(instruction, should_emulate_flags, callbacks);
}

std::vector<binwrite::instruction_t> mba_obfuscate_or(const binwrite::disassembled_instruction_t& instruction, const bool should_emulate_flags)
{
	const std::array callbacks = {
		MBA_CALLBACK(
			// (x | y) = (~x | ~y) + (x + y + 1)
			const binwrite::encoder_operand_t constant = encode_unsigned_imm_operand(1);

			instructions.push_back(add_instruction(y, unused_register).value());
			instructions.push_back(add_instruction(constant, unused_register).value());

			instructions.push_back(not_instruction(second_unused_register).value());
			instructions.push_back(not_instruction(x).value());

			instructions.push_back(or_instruction(second_unused_register, x).value());

			instructions.push_back(add_instruction(unused_register, x).value());
		),
		MBA_CALLBACK(
			// (x | y) = (x & ~y) + y

			instructions.push_back(not_instruction(second_unused_register).value());
			instructions.push_back(and_instruction(second_unused_register, x).value());
			instructions.push_back(not_instruction(second_unused_register).value());

			instructions.push_back(add_instruction(second_unused_register, x).value());
		)
	};

	return mba_stub(instruction, should_emulate_flags, callbacks);
}
