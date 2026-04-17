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

		if ((PDB_AS_UNDERLYING(pub.flags) & PDB_AS_UNDERLYING(PDB::CodeView::DBI::PublicSymbolFlags::Function)) == 0)
		{
			continue;
		}

		if (const auto rva = sections.ConvertSectionOffsetToRVA(pub.section, pub.offset))
		{
			binary.create_function(pub.name, binwrite::rva_t{ rva });
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

		if (kind != symbol_kind::S_GPROC32 && kind != symbol_kind::S_LPROC32 &&
			kind != symbol_kind::S_GPROC32_ID && kind != symbol_kind::S_LPROC32_ID)
		{
			continue;
		}

		const auto& proc = record->data.S_LPROC32;
	
		if (const auto rva = sections.ConvertSectionOffsetToRVA(proc.section, proc.offset))
		{
			binary.create_function(proc.name, binwrite::rva_t{ rva });
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
