#pragma once
#include <binwrite/binary/binary.hpp>

namespace binprotect::mba
{
	std::vector<binwrite::instruction_t> emulate_flag_behaviour(const binwrite::disassembled_instruction_t& instruction, const binwrite::encoder_operand_t& result, const binwrite::register_family_t& x_copy_register_family, const binwrite::encoder_operand_t& x_copy_register, const binwrite::encoder_operand_t& y, const binwrite::register_family_t& unused_register_family, const binwrite::encoder_operand_t& unused_register);
}
