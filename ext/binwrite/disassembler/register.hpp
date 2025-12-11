#pragma once
#include <Zydis/Zydis.h>

#include <array>

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

		static const register_t rbx;
		static const register_t ebx;
		static const register_t bx;
		static const register_t bl;
		static const register_t bh;

		static const register_t r8;
		static const register_t r8d;
		static const register_t r8w;
		static const register_t r8b;

		static const register_t r9;
		static const register_t r9d;
		static const register_t r9w;
		static const register_t r9b;

		static const register_t r10;
		static const register_t r10d;
		static const register_t r10w;
		static const register_t r10b;

		static const register_t r11;
		static const register_t r11d;
		static const register_t r11w;
		static const register_t r11b;

		static const register_t r12;
		static const register_t r12d;
		static const register_t r12w;
		static const register_t r12b;

		static const register_t r13;
		static const register_t r13d;
		static const register_t r13w;
		static const register_t r13b;

		static const register_t r14;
		static const register_t r14d;
		static const register_t r14w;
		static const register_t r14b;

		static const register_t r15;
		static const register_t r15d;
		static const register_t r15w;
		static const register_t r15b;

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

		static [[nodiscard]] register_family_t find(register_t qword);
		static [[nodiscard]] register_family_t random();

		static const register_family_t none;
		static const register_family_t ax;
		static const register_family_t cx;
		static const register_family_t dx;
		static const register_family_t bx;
		static const register_family_t eight;
		static const register_family_t nine;
		static const register_family_t ten;
		static const register_family_t eleven;
		static const register_family_t twelve;
		static const register_family_t thirteen;
		static const register_family_t fourteen;
		static const register_family_t fifteen;

		static const std::array<register_family_t, 12> general_purpose;
	};
}
