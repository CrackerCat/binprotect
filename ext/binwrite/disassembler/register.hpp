#pragma once
#include <Zydis/Zydis.h>

#include <algorithm>
#include <vector>
#include <array>
#include <unordered_map>

namespace binwrite
{
	struct register_family_t;

	class register_t
	{
	public:
		using value_type = std::uint16_t;
		using size_type = std::uint16_t;

		constexpr register_t() = default;
		explicit constexpr register_t(const value_type value)
				:	value_(value) {}

		[[nodiscard]] value_type value() const
		{
			return value_;
		}

		bool operator==(const register_t& other) const
		{
			return value_ == other.value_ || in_same_family(other);
		}

		bool operator!=(const register_t& other) const
		{
			return value_ != other.value_;
		}

		[[nodiscard]] operator ZydisRegister() const
		{
			return static_cast<ZydisRegister>(value_);
		}

		[[nodiscard]] bool in_same_family(const register_t& other) const
		{
			constexpr auto mode = ZYDIS_MACHINE_MODE_LONG_64;

			const auto enclosing = ZydisRegisterGetLargestEnclosing(mode, static_cast<ZydisRegister>(value_));
			const auto other_enclosing = ZydisRegisterGetLargestEnclosing(mode, static_cast<ZydisRegister>(other.value_));

			return enclosing == other_enclosing;
		}

		[[nodiscard]] register_family_t family() const;

		static const register_t none;
		static const register_t rip;
		static const register_t rsp;

		static const register_t rax;
		static const register_t eax;
		static const register_t ax;
		static const register_t al;
		static const register_t ah;

		static const register_t rcx;
		static const register_t ecx;
		static const register_t cx;
		static const register_t cl;
		static const register_t ch;

		static const register_t rdx;
		static const register_t edx;
		static const register_t dx;
		static const register_t dl;
		static const register_t dh;

	protected:
		value_type value_ = 0;
	};

	struct register_family_t
	{
		register_t qword;
		register_t dword;
		register_t word;
		register_t byte;
		register_t high_byte;

		bool operator==(const register_family_t& other) const
		{
			return qword == other.qword;
		}

		[[nodiscard]] register_t of_size(const register_t::size_type size) const
		{
			switch (size)
			{
			case 64:
				return qword;
			case 32:
				return dword;
			case 16:
				return word;
			case 8:
				return byte;
			default:
				return register_t::none;
			}
		}

		static register_family_t find(register_t qword);

		static const register_family_t none;
		static const register_family_t ax;
		static const register_family_t cx;
		static const register_family_t dx;

		static const std::array<register_family_t, 3> general_purpose;
	};
}
