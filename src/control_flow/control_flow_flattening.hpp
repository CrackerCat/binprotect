#pragma once
#include <binwrite/binary/binary.hpp>
#include <functional>

namespace binprotect::control_flow::flattening
{
		void do_pass(binwrite::binary_t& binary, binwrite::function_t& function,
			const std::function<bool(binwrite::rva_t::value_type)>& is_block_fixed = {});
}
