#include <binwrite/binary/portable_executable.hpp>
#include <binwrite/binary/symbols/map_parsing.hpp>

#include <spdlog/spdlog.h>

#include <cstdint>
#include <fstream>
#include <string>
#include <random>

#include "binwrite/math/random.hpp"
#include "virtual_machine/virtual_machine.hpp"
#include "control_flow/control_flow_flattening.hpp"
#include "control_flow/control_flow_obfuscation.hpp"
#include "linear_substitution/linear_substitution.hpp"
#include "mba/mba.hpp"

std::vector<std::uint8_t> read_file_from_disk(const std::string& path)
{
	std::ifstream file(path, std::ios::binary);

	if (file.is_open())
	{
		return { std::istreambuf_iterator(file), { } };
	}

	return { };
}

void write_file_to_disk(const std::string& path, const std::vector<std::uint8_t>& buffer)
{
	std::ofstream file(path, std::ios::binary);

	if (file.is_open())
	{
		file.write(reinterpret_cast<const char*>(buffer.data()), static_cast<std::streamsize>(buffer.size()));
	}
}

void erase_unused_data_directories(binwrite::portable_executable_t& pe)
{
	const auto nt_headers = pe.image()->nt_headers();

	auto& exception_directory = nt_headers->optional_header.data_directories.exception_directory;

	if (exception_directory.present())
	{
		exception_directory.virtual_address = 0;
		exception_directory.size = 0;

		spdlog::warn("exception directory present when exceptions are not currently supported");
	}
}

void fix_security_directory(binwrite::portable_executable_t& pe)
{
	const auto image = pe.image();
	const auto nt_headers = image->nt_headers();

	auto& security_directory = nt_headers->optional_header.data_directories.security_directory;

	if (!security_directory.present())
	{
		return;
	}

	if (const auto resolved_rva = image->ptr_to_rva(security_directory.virtual_address))
	{
		security_directory.virtual_address = *resolved_rva;
	}
	else
	{
		spdlog::warn("unable to redirect raw ptr of security directory");
	}
}

void mutate_basic_block(binwrite::binary_t& binary, binwrite::basic_block_t& basic_block)
{
	binprotect::linear_substitution::do_pass(binary, basic_block);

	constexpr std::uint32_t mba_passes = 1;

	bool is_first_pass = true;

	for (std::uint32_t j = 0; j < mba_passes; j++)
	{
		binprotect::mba::do_pass(binary, basic_block, is_first_pass);

		is_first_pass = false;
	}
}

std::int32_t main()
{
	std::vector<std::uint8_t> buffer = read_file_from_disk("input.exe");

	if (buffer.empty())
	{
		spdlog::error("unable to read input file");

		return 1;
	}

	binwrite::portable_executable_t pe(std::move(buffer));

	erase_unused_data_directories(pe);
	//fix_security_directory(pe);

	pe.decompress();
	pe.parse();

	if (!binwrite::symbols::map::parse(pe, "input.map"))
	{
		spdlog::warn("unable to find or parse .map file");
	}

	pe.disassemble();

	const auto code_section = pe.code_section();

	for (const auto& function : pe.functions())
	{
		binprotect::control_flow::flattening::do_pass(pe, *function);
	}

	const auto original_basic_blocks = pe.basic_blocks();
	const std::vector basic_blocks(original_basic_blocks.begin(), original_basic_blocks.end());

	std::vector<std::shared_ptr<binwrite::basic_block_t>> virtual_machine_blocks;

	for (std::uint64_t i = 0; i < basic_blocks.size(); i++)
	{
		const auto& basic_block = basic_blocks[i];

		mutate_basic_block(pe, *basic_block);

		binprotect::vm::do_pass(pe, *basic_block, code_section->rva(), virtual_machine_blocks);

		binprotect::control_flow::obfuscation::do_pass(pe, *basic_block);

		spdlog::info("obfuscated {}/{} blocks", i + 1, basic_blocks.size());
	}

	binwrite::math::shuffle<std::shared_ptr<binwrite::basic_block_t>>(virtual_machine_blocks);

	for (std::uint64_t i = 0; i < virtual_machine_blocks.size(); i++)
	{
		const auto& basic_block = virtual_machine_blocks[i];

		basic_block->move_entire(pe, code_section->rva());

		mutate_basic_block(pe, *basic_block);

		spdlog::info("obfuscated {}/{} virtual machine blocks", i + 1, virtual_machine_blocks.size());
	}

	pe.update_rva_references();

	// we aren't dealing with compression right now
	//pe.compress();

	write_file_to_disk("output.exe", pe.buffer());

	return 0;
}
