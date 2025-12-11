#include "control_flow_obfuscation.hpp"
#include "../assembler/assembler.hpp"

#include <binwrite/math/random.hpp>

binwrite::instruction_t create_forward_jump()
{
	constexpr std::array bytes = { static_cast<std::uint8_t>(0xEB) };
	const binwrite::disassembled_instruction_t disassembly{ };

	return binwrite::instruction_t{ bytes, disassembly };
}

void binprotect::control_flow::obfuscation::do_pass(binwrite::binary_t& binary, binwrite::basic_block_t& basic_block)
{
	const std::span<const binwrite::instruction_t> original_instructions = basic_block.instructions();
	const std::vector instructions(original_instructions.begin(), original_instructions.end());

	for (std::uint32_t i = 0; i < instructions.size(); i++)
	{
		const auto& instruction = instructions[i];
		const auto& bytes = instruction.bytes();

		if (bytes.empty())
		{
			continue;
		}

		if (bytes[0] == 0xFF)
		{
			const binwrite::instruction_t forward_jump_instruction = create_forward_jump();

			basic_block.insert(binary, forward_jump_instruction, i);
		}
	}
}
