#pragma once
#include <binwrite/binary/binary.hpp>

namespace binprotect::vm
{
	void do_pass(binwrite::binary_t& binary, binwrite::basic_block_t& basic_block, binwrite::rva_t rva);
}
