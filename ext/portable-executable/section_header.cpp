#include "section_header.hpp"
#include <string.h>

std::string portable_executable::section_header_t::to_str() const
{
	std::string str(this->name, this->name + sizeof(this->name));

	const auto null_terminator = str.find('\0');

	if (null_terminator != std::string::npos)
	{
		str.erase(null_terminator);
	}

	return str;
}
