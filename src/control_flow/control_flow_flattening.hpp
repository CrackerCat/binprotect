#pragma once
#include <binwrite/binary/binary.hpp>

namespace binprotect::control_flow::flattening
{
	void do_pass(binwrite::binary_t& binary, binwrite::function_t& function);
}
