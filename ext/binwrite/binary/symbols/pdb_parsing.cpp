#include "pdb_parsing.hpp"
#include "../../util/file.hpp"
#include "../binary.hpp"

#include <spdlog/spdlog.h>
#include <raw_pdb/PDB.h>
#include <raw_pdb/PDB_RawFile.h>
#include <raw_pdb/PDB_InfoStream.h>
#include <raw_pdb/PDB_DBIStream.h>
#include <raw_pdb/PDB_PublicSymbolStream.h>
#include <raw_pdb/PDB_GlobalSymbolStream.h>
#include <raw_pdb/PDB_ImageSectionStream.h>
#include <fstream>

using symbol_kind = PDB::CodeView::DBI::SymbolRecordKind;

void process_local_symbols(binwrite::binary_t& binary, const PDB::RawFile& raw_file, const PDB::DBIStream& dbi_stream,
                            const PDB::CoalescedMSFStream& symbol_records,
                            const PDB::ImageSectionStream& sections)
{
	if (dbi_stream.HasValidPublicSymbolStream(raw_file) != PDB::ErrorCode::Success)
	{
		return;
	}

	const auto stream = dbi_stream.CreatePublicSymbolStream(raw_file);

	for (const auto& hash_record : stream.GetRecords())
	{
		const auto record = stream.GetRecord(symbol_records, hash_record);

		if (!record || record->header.kind != symbol_kind::S_PUB32)
		{
			continue;
		}

		const auto& pub = record->data.S_PUB32;
		const auto underlying_flags = PDB_AS_UNDERLYING(pub.flags);

		const auto rva = binwrite::rva_t{ sections.ConvertSectionOffsetToRVA(pub.section, pub.offset) };

		if (!rva.value())
		{
			continue;
		}

		const bool is_function = (underlying_flags & PDB_AS_UNDERLYING(PDB::CodeView::DBI::PublicSymbolFlags::Function));
		const bool is_data = !(underlying_flags & PDB_AS_UNDERLYING(PDB::CodeView::DBI::PublicSymbolFlags::Code));

		if (is_function)
		{
			binary.create_function(pub.name, rva);
		}
		else if (is_data)
		{
			binary.add_data_symbol(rva);
		}
	}
}

void process_global_symbols(binwrite::binary_t& binary, const PDB::RawFile& raw_file, const PDB::DBIStream& dbi_stream,
                            const PDB::CoalescedMSFStream& symbol_records,
                            const PDB::ImageSectionStream& sections)
{
	if (dbi_stream.HasValidGlobalSymbolStream(raw_file) != PDB::ErrorCode::Success)
	{
		return;
	}

	const auto stream = dbi_stream.CreateGlobalSymbolStream(raw_file);

	for (const auto& hash_record : stream.GetRecords())
	{
		const auto record = stream.GetRecord(symbol_records, hash_record);

		if (!record)
		{
			continue;
		}

		const auto kind = record->header.kind;

		const bool is_proc = kind == symbol_kind::S_GPROC32 || kind == symbol_kind::S_LPROC32 ||
			kind == symbol_kind::S_GPROC32_ID || kind == symbol_kind::S_LPROC32_ID;

		const bool is_data = kind == symbol_kind::S_GDATA32 || kind == symbol_kind::S_LDATA32;

		if (is_proc)
		{
			const auto& proc = record->data.S_LPROC32;

			if (const auto rva = sections.ConvertSectionOffsetToRVA(proc.section, proc.offset))
			{
				binary.create_function(proc.name, binwrite::rva_t{ rva });
			}
		}
		else if (is_data)
		{
			const auto& data = record->data.S_LDATA32;

			if (const auto rva = sections.ConvertSectionOffsetToRVA(data.section, data.offset))
			{
				binary.add_data_symbol(binwrite::rva_t{ rva });
			}
		}
	}
}

bool binwrite::symbols::pdb::parse(binary_t& binary, const std::filesystem::path& pdb_file_path)
{
	const std::vector<std::uint8_t> pdb_file = util::read_file(pdb_file_path);

	if (pdb_file.empty())
	{
		return false;
	}

	const auto error_code = PDB::ValidateFile(pdb_file.data(), pdb_file.size());

	if (error_code != PDB::ErrorCode::Success)
	{
		return false;
	}

	const auto raw_file = PDB::CreateRawFile(pdb_file.data());

	if (PDB::HasValidDBIStream(raw_file) != PDB::ErrorCode::Success)
	{
		return false;
	}

	const auto dbi_stream = PDB::CreateDBIStream(raw_file);

	if (dbi_stream.HasValidSymbolRecordStream(raw_file) != PDB::ErrorCode::Success)
	{
		return false;
	}

	if (dbi_stream.HasValidImageSectionStream(raw_file) != PDB::ErrorCode::Success)
	{
		return false;
	}

	const auto symbol_records = dbi_stream.CreateSymbolRecordStream(raw_file);
	const auto sections = dbi_stream.CreateImageSectionStream(raw_file);

	process_local_symbols(binary, raw_file, dbi_stream, symbol_records, sections);
	process_global_symbols(binary, raw_file, dbi_stream, symbol_records, sections);

	return true;
}
