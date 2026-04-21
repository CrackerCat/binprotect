#pragma once
#include <binwrite/binary/binary.hpp>
#include <functional>

namespace binprotect::mba
{
	using should_skip_memory_operands_fn = std::function<bool(const binwrite::disassembled_instruction_t& instruction, binwrite::rva_t rva)>;

	void do_pass(binwrite::binary_t& binary, binwrite::basic_block_t& basic_block, bool flag_dependant, const should_skip_memory_operands_fn& should_skip_memory_operands = { });
}
