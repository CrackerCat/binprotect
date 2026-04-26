#include "map_parsing.hpp"
#include "../binary.hpp"

#include <fstream>
#include <regex>
#include <spdlog/spdlog.h>

static std::uint64_t hex_to_int(const std::string& hex)
{
	constexpr std::int32_t base = 16;

	return std::stoull(hex, nullptr, base);
}

bool binwrite::symbols::map::parse(binary_t& binary, const std::filesystem::path& map_file_path)
{
	std::ifstream map_file(map_file_path);

	if (!map_file.is_open())
	{
		return false;
	}

	const std::regex regex(R"(([a-fA-F0-9]+):(\w+)\s+(\S+)\s+([a-fA-F0-9]+))");

	const std::uint64_t image_base = binary.image_base();
	const auto ordered_sections = binary.ordered_sections();

	bool started = false;

	std::string line;

	while (std::getline(map_file, line))
	{
		if (line.contains("Rva+Base"))
		{
			started = true;

			continue;
		}

		if (!started)
		{
			continue;
		}

		std::smatch regex_matches = { };

		if (!std::regex_search(line, regex_matches, regex))
		{
			continue;
		}

		const std::uint64_t section_index = hex_to_int(regex_matches[1]) - 1;

		if (ordered_sections.size() <= section_index)
		{
			continue;
		}

		const auto& section = ordered_sections[section_index];

		if (!section->code())
		{
			continue;
		}

		const std::uint64_t symbol_address = hex_to_int(regex_matches[4]);

		if (symbol_address < image_base)
		{
			spdlog::warn("corrupted symbol address found whilst parsing .map");

			continue;
		}

		const auto symbol_name = regex_matches[3].str();

		if (symbol_name.empty() || symbol_name[0] == '.' ||
			symbol_name[0] == '$' ||
			symbol_name.contains("__IMPORT_DESCRIPTOR_") ||
			symbol_name == "__NULL_IMPORT_DESCRIPTOR")
		{
			continue;
		}

		const auto symbol_rva = rva_t{ static_cast<std::uint32_t>(symbol_address - image_base) };

		if (binary.find_function(symbol_rva) || binary.is_data_symbol(symbol_rva))
		{
			continue;
		}

		if (const bool is_string = symbol_name.starts_with("??_C@_"))
		{
			binary.add_data_symbol(symbol_rva);
		}
		else
		{
			binary.create_function(symbol_name, symbol_rva);
		}
	}

	return true;
}
