#pragma once
#include "binary.hpp"

#include <portable-executable/image.hpp>

namespace binwrite
{
	class pe_relocation_t : public relocation_t
	{
	public:
		pe_relocation_t() = default;

		explicit pe_relocation_t(std::shared_ptr<rva_t> target, const portable_executable::relocation_type_t type)
				:	relocation_t(std::move(target)),
					type_(type) { }

		[[nodiscard]] reloc_type type() const override
		{
			return static_cast<reloc_type>(type_);
		}

	protected:
		portable_executable::relocation_type_t type_;
	};

	class portable_executable_t final : public binary_t
	{
	public:
		explicit portable_executable_t(std::vector<std::uint8_t> buffer)
				:	binary_t(std::move(buffer)) {}

		[[nodiscard]] std::uint64_t image_base() const override;
		[[nodiscard]] rva_t entry_point() const override;

		void decompress() override;
		void compress() override;

		[[nodiscard]] portable_executable::image_t* image();
		[[nodiscard]] const portable_executable::image_t* image() const;

	protected:
		void find_sections() override;
		void update_section_headers() override;
		void update_relocations() override;

		void copy_sections(std::vector<std::uint8_t>& to, bool decompress);

		void add_misc_rvas(const portable_executable::nt_headers_t* nt_headers);
		void add_data_directory_rvas(const portable_executable::nt_headers_t* nt_headers);
		void add_import_rvas(const portable_executable::nt_headers_t* nt_headers);
		void add_delay_import_rvas(const portable_executable::nt_headers_t* nt_headers);
		void parse_import_thunk_rvas(const portable_executable::thunk_data_t* original_thunk);
		void add_debug_rvas(const portable_executable::nt_headers_t* nt_headers);
		void add_resource_rvas(const portable_executable::nt_headers_t* nt_headers);
		void parse_resource_directory_rvas(const portable_executable::resource_directory_t* root_directory, const portable_executable::resource_directory_t* directory, std::uint16_t depth = 0);
		void add_export_rvas(const portable_executable::nt_headers_t* nt_headers);
		void add_relocation_rvas(const portable_executable::nt_headers_t* nt_headers);

		template <class T>
		std::shared_ptr<data_rva_ref_t> add_relocation_ref(const T* const value)
		{
			const auto data_reference = static_cast<rva_t::value_type>(reinterpret_cast<const std::uint8_t*>(value) - data());
			const auto data_rva = add_rva(static_cast<rva_t::value_type>(*value));

			const auto ref = std::make_shared<data_rva_ref_t>(data_rva, rva_t{ data_reference }, static_cast<data_rva_ref_t::size_type>(sizeof(T)));

			add_rva_ref(ref);

			return ref;
		}

		void find_data_rvas() override;
	};
}
