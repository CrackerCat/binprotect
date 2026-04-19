#pragma once
#include "pe.hpp"

namespace binwrite
{
	struct rtti_info_t
	{
		std::unordered_set<rva_t::value_type> type_descriptor_rvas;
	};

	rtti_info_t parse_rtti(portable_executable_t& pe);
}
