#pragma once
#include "../rva/rva.hpp"
#include "../function/function.hpp"
#include "../instruction/basic_block.hpp"

#include <unordered_map>
#include <vector>
#include <string>
#include <memory>
#include <optional>
#include <span>
#include <queue>

namespace binwrite
{
	class section_t
	{
	public:
		using size_type = std::uint32_t;

		section_t() = default;

		explicit section_t(const rva_t rva, const size_type size, const bool code_section)
				:	rva_(rva),
					size_(size),
					code_(code_section) { }

		void process_disruption(rva_t disruption_rva, rva_t::size_type disruption_size);

		void insert(binary_t& binary, rva_t section_offset, std::span<const std::uint8_t> data);

		[[nodiscard]] rva_t rva() const;
		[[nodiscard]] rva_t end_rva() const;

		[[nodiscard]] bool contains(rva_t rva) const;
		[[nodiscard]] bool code() const;

		[[nodiscard]] size_type size() const;
		void set_size(size_type size);

	protected:
		rva_t rva_;
		size_type size_;
		bool code_;
	};

	class relocation_t
	{
	public:
		using reloc_type = std::uint16_t;

		relocation_t() = default;

		explicit relocation_t(std::shared_ptr<rva_t> target)
				:	target_(std::move(target)) { }

		[[nodiscard]] rva_t target() const;
		[[nodiscard]] virtual reloc_type type() const = 0;

	protected:
		std::shared_ptr<rva_t> target_;
	};

	class binary_t
	{
	public:
		explicit binary_t(std::vector<std::uint8_t> buffer)
				:	buffer_(std::move(buffer)) { }

		virtual ~binary_t() = default;

		void parse();
		void disassemble();

		void insert(rva_t rva, std::span<const std::uint8_t> data, bool inclusive = false);
		void insert(rva_t rva, rva_t::size_type size, bool inclusive = false);

		void erase(rva_t rva, rva_t::size_type size, bool inclusive = false);

		virtual void decompress() = 0;
		virtual void compress() = 0;

		[[nodiscard]] virtual std::uint64_t image_base() const = 0;
		[[nodiscard]] virtual rva_t entry_point() const = 0;

		[[nodiscard]] std::span<std::shared_ptr<function_t>> functions();
		[[nodiscard]] std::span<const std::shared_ptr<function_t>> functions() const;

		std::shared_ptr<function_t> create_function(const std::string& name, rva_t rva);

		std::shared_ptr<basic_block_t> create_basic_block(rva_t rva, std::span<const instruction_t> instructions);
		std::shared_ptr<basic_block_t> create_basic_block(rva_t rva);

		[[nodiscard]] std::span<std::shared_ptr<basic_block_t>> basic_blocks();
		[[nodiscard]] std::span<const std::shared_ptr<basic_block_t>> basic_blocks() const;

		[[nodiscard]] std::shared_ptr<basic_block_t> find_basic_block(rva_t rva) const;
		[[nodiscard]] std::shared_ptr<basic_block_t> is_inside_basic_block(rva_t rva) const;
		std::shared_ptr<basic_block_t> split_basic_block(const std::shared_ptr<basic_block_t>& basic_block, basic_block_t::size_type index);

		[[nodiscard]] std::vector<std::shared_ptr<rva_t>> rvas();
		[[nodiscard]] std::vector<std::shared_ptr<rva_ref_t>> rva_refs();

		[[nodiscard]] std::unordered_map<std::string, std::shared_ptr<section_t>>& sections();
		[[nodiscard]] const std::unordered_map<std::string, std::shared_ptr<section_t>>& sections() const;

		[[nodiscard]] std::vector<std::shared_ptr<section_t>> ordered_sections() const;

		[[nodiscard]] std::shared_ptr<section_t> find_section(const std::string& name) const;
		[[nodiscard]] std::shared_ptr<section_t> code_section() const;

		[[nodiscard]] std::vector<std::uint8_t>& buffer();
		[[nodiscard]] const std::vector<std::uint8_t>& buffer() const;

		[[nodiscard]] std::uint8_t* data();
		[[nodiscard]] const std::uint8_t* data() const;

		void update_rva_references();

		void update_rvas(rva_t disruption_rva, rva_t::size_type disruption_size, bool inclusive = false, bool update_sections = true);

		[[nodiscard]] std::shared_ptr<rva_ref_t> find_rva_ref(rva_t ref_rva, bool must_be_code_reference = false) const;

		std::shared_ptr<rva_t> add_rva(rva_t::value_type value, bool force_inclusive = false);
		std::shared_ptr<rva_t> add_rva(rva_t rva, bool force_inclusive = false);

		void add_rva_ref(std::shared_ptr<rva_ref_t> ref);
		void redirect_rva_ref(rva_t self, rva_t new_target);

	protected:
		virtual void find_data_rvas() = 0;
		virtual void find_sections() = 0;
		virtual void update_section_headers() = 0;
		virtual void update_relocations() = 0;

		void find_jump_tables(const basic_block_t& basic_block);
		void assign_function_basic_blocks() const;

		void update_section_rvas(rva_t disruption_rva, rva_t::size_type disruption_size);

		bool is_in_code_section(rva_t rva);

		[[nodiscard]] bool is_inside_disassembly_queue(rva_t rva) const;
		void add_to_disassembly_queue(const std::shared_ptr<rva_t>& rva);

		std::shared_ptr<rva_t> add_relocation_rva(rva_t::value_type target);
		std::shared_ptr<rva_t> add_relocation_rva(rva_t target);

		template <class T>
		std::shared_ptr<data_rva_ref_t> add_data_rva_ref(const T* const value)
		{
			const auto data_reference = static_cast<rva_t::value_type>(reinterpret_cast<const std::uint8_t*>(value) - data());
			const auto data_rva = add_rva(static_cast<rva_t::value_type>(*value));

			const auto ref = std::make_shared<data_rva_ref_t>(data_rva, rva_t{ data_reference }, static_cast<data_rva_ref_t::size_type>(sizeof(T)));

			add_rva_ref(ref);

			return ref;
		}

		void add_llvm_jmp_table_ref(rva_t table_base);
		void add_msvc_jmp_table_ref(rva_t table_base);

		std::vector<std::uint8_t> buffer_;
		std::unordered_map<std::string, std::shared_ptr<section_t>> sections_;

		std::vector<std::shared_ptr<rva_t>> rvas_;
		std::vector<std::shared_ptr<rva_ref_t>> rva_refs_;

		std::vector<std::shared_ptr<basic_block_t>> basic_blocks_;
		std::deque<std::shared_ptr<rva_t>> disassembly_queue_;

		std::vector<std::shared_ptr<function_t>> functions_;

		std::vector<std::shared_ptr<relocation_t>> relocations_;
	};
}
