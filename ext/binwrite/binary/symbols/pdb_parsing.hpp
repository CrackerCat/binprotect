#pragma once
#include <filesystem>

namespace binwrite
{
	class binary_t;

	namespace symbols::pdb
	{
		bool parse(binary_t& binary, const std::filesystem::path& pdb_file_path);
	}
}
