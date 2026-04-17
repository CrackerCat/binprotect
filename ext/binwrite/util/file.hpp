#pragma once
#include <filesystem>
#include <fstream>
#include <vector>

namespace binwrite::util
{
	static std::vector<std::uint8_t> read_file(const std::filesystem::path& path)
	{
		std::ifstream file(path, std::ios::binary);

		if (file.is_open())
		{
			return { std::istreambuf_iterator(file), { } };
		}

		return { };
	}
}
