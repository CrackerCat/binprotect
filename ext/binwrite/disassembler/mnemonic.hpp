#pragma once
#include <Zydis/Zydis.h>

#include <cstdint>

namespace binwrite
{
	class mnemonic_t
	{
	public:
		using value_type = std::uint16_t;

		constexpr mnemonic_t() = default;
		explicit constexpr mnemonic_t(const value_type value)
				:	value_(value) { }

		[[nodiscard]] value_type value() const
		{
			return value_;
		}

		bool operator==(const mnemonic_t& other) const
		{
			return value_ == other.value_;
		}

		bool operator!=(const mnemonic_t& other) const
		{
			return value_ != other.value_;
		}

		[[nodiscard]] operator ZydisMnemonic() const
		{
			return static_cast<ZydisMnemonic_>(value_);
		}

		static const mnemonic_t invalid;
		static const mnemonic_t push;
		static const mnemonic_t pop;
		static const mnemonic_t shl;
		static const mnemonic_t shr;
		static const mnemonic_t imul;
		static const mnemonic_t mul;
		static const mnemonic_t add;
		static const mnemonic_t neg;
		static const mnemonic_t mov;
		static const mnemonic_t sub;
		static const mnemonic_t and_;
		static const mnemonic_t or_;
		static const mnemonic_t xor_;
		static const mnemonic_t not_;

	protected:
		value_type value_ = 0;
	};
}
