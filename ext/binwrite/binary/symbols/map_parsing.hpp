#pragma once
#include <filesystem>

namespace binwrite
{
	class binary_t;

	namespace symbols::map
	{
		bool parse(binary_t& binary, const std::filesystem::path& map_file_path);
	}
}
