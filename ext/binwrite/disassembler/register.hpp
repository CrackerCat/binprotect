#pragma once
#include <Zydis/Zydis.h>

#include <array>
#include <span>

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

		[[nodiscard]] value_type value() const;

		[[nodiscard]] size_type width() const;

		[[nodiscard]] bool in_same_family(const register_t& other) const;
		[[nodiscard]] register_family_t family() const;

		[[nodiscard]] bool is_high_byte() const;
		[[nodiscard]] bool is_general_purpose() const;

		bool operator==(const register_t& other) const;
		bool operator!=(const register_t& other) const;

		[[nodiscard]] operator ZydisRegister_() const;

		static const register_t none;
		static const register_t rip;

		static const register_t rflags;
		static const register_t eflags;
		static const register_t flags;

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

		static const register_t rsi;
		static const register_t esi;
		static const register_t si;
		static const register_t sil;

		static const register_t rdi;
		static const register_t edi;
		static const register_t di;
		static const register_t dil;

		static const register_t rbp;
		static const register_t ebp;
		static const register_t bp;
		static const register_t bpl;

		static const register_t rsp;
		static const register_t esp;
		static const register_t sp;
		static const register_t spl;

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

		[[nodiscard]] register_t of_size(register_t::size_type size) const;

		bool operator==(const register_family_t& other) const;

		static [[nodiscard]] register_family_t find(register_t qword);
		static [[nodiscard]] register_family_t random(std::span<const register_family_t> excluding);

		static const register_family_t none;

		static const register_family_t ax;
		static const register_family_t cx;
		static const register_family_t dx;
		static const register_family_t bx;
		static const register_family_t si;
		static const register_family_t di;
		static const register_family_t bp;
		static const register_family_t sp;
		static const register_family_t eight;
		static const register_family_t nine;
		static const register_family_t ten;
		static const register_family_t eleven;
		static const register_family_t twelve;
		static const register_family_t thirteen;
		static const register_family_t fourteen;
		static const register_family_t fifteen;

		static const register_family_t flags;

		static const std::array<register_family_t, 15> general_purpose;
		static const std::array<register_family_t, 16> families;
	};
}
