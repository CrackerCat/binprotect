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

		[[nodiscard]] value_type value() const;

		bool operator==(const mnemonic_t& other) const;
		bool operator!=(const mnemonic_t& other) const;

		[[nodiscard]] operator ZydisMnemonic() const;

		static const mnemonic_t invalid;
		static const mnemonic_t call;
		static const mnemonic_t pushfq;
		static const mnemonic_t popfq;
		static const mnemonic_t push;
		static const mnemonic_t pop;
		static const mnemonic_t shl;
		static const mnemonic_t shr;
		static const mnemonic_t imul;
		static const mnemonic_t mul;
		static const mnemonic_t add;
		static const mnemonic_t neg;
		static const mnemonic_t mov;
		static const mnemonic_t jmp;
		static const mnemonic_t jz;
		static const mnemonic_t jnz;
		static const mnemonic_t cmp;
		static const mnemonic_t lea;
		static const mnemonic_t test;
		static const mnemonic_t sub;
		static const mnemonic_t and_;
		static const mnemonic_t or_;
		static const mnemonic_t xor_;
		static const mnemonic_t not_;
		static const mnemonic_t nop;

	protected:
		value_type value_ = 0;
	};
}
