#pragma once
#include "../rva/rva.hpp"

#include <memory>

namespace binwrite
{
	class relocation_t
	{
	public:
		using reloc_type = std::uint16_t;

		relocation_t() = default;

		explicit relocation_t(std::shared_ptr<rva_t> target)
				:	target_(std::move(target)) { }

		[[nodiscard]] rva_t target() const
		{
			return *target_;
		}

		[[nodiscard]] virtual reloc_type type() const = 0;

	protected:
		std::shared_ptr<rva_t> target_;
	};
}
