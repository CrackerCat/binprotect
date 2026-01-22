#pragma once
#include <Zydis/Zydis.h>

#include "../arch/register/register.hpp"

namespace binwrite
{
	enum class operand_width_t : std::uint8_t
	{
		rel8,
		rel32
	};

	class decoded_operand_t
	{
	public:
		using size_type = std::uint16_t;

		explicit decoded_operand_t(const ZydisDecodedOperand& value)
				:	value_(value) { }

		struct imm_t
		{
			union
			{
				std::uint64_t u;
				std::int64_t s;
			} value;

			bool is_relative;
			bool is_signed;
		};

		struct mem_t
		{
			std::int64_t displacement;
			std::uint8_t scale;
			bool has_displacement;
			register_t base;
			register_t index;
			register_t segment;
		};

		struct reg_t
		{
			register_t value;
		};

		[[nodiscard]] bool is_imm() const;
		[[nodiscard]] imm_t imm() const;
		void set_imm(imm_t imm);

		[[nodiscard]] bool is_mem() const;
		[[nodiscard]] mem_t mem() const;
		void set_mem(mem_t mem);

		[[nodiscard]] bool is_reg() const;
		[[nodiscard]] reg_t reg() const;
		void set_reg(reg_t reg);

		[[nodiscard]] bool is_read_action() const;
		[[nodiscard]] bool is_write_action() const;

		[[nodiscard]] size_type size() const;

		operator ZydisDecodedOperand& ();
		operator const ZydisDecodedOperand& () const;

	protected:
		ZydisDecodedOperand value_;
	};
}
