#pragma once
#include "../../arch/instruction/basic_block.hpp"

namespace binwrite
{
	class binary_t;

	class function_t
	{
	public:
		function_t() = default;

		explicit function_t(std::string name, std::shared_ptr<rva_t> rva)
				:	name_(std::move(name)),
					rva_(std::move(rva)) { }

		void add_basic_block(std::shared_ptr<basic_block_t> basic_block);

		[[nodiscard]] std::shared_ptr<basic_block_t> find_basic_block(rva_t rva) const;
		[[nodiscard]] std::shared_ptr<basic_block_t> entry_block() const;

		[[nodiscard]] std::shared_ptr<basic_block_t> fallthrough_block(const std::shared_ptr<basic_block_t>& basic_block) const;
		[[nodiscard]] std::shared_ptr<basic_block_t> target_block(const binary_t& binary, const std::shared_ptr<basic_block_t>& basic_block) const;

		[[nodiscard]] std::span<std::shared_ptr<basic_block_t>> basic_blocks();
		[[nodiscard]] std::span<const std::shared_ptr<basic_block_t>> basic_blocks() const;

		[[nodiscard]] std::shared_ptr<rva_t> rva() const;

		[[nodiscard]] std::string_view name() const;

	protected:
		std::string name_ = { };
		std::shared_ptr<rva_t> rva_ = { };

		std::vector<std::shared_ptr<basic_block_t>> basic_blocks_;
	};
}
