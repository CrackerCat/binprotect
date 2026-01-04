#pragma once
#include <cstdint>
#include <optional>
#include <span>

#include <Zydis/Zydis.h>

#include <vector>
#include <algorithm>
#include <string>

#include "register.hpp"

namespace binwrite
{
	class mnemonic_t;
	class rva_t;

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
				: value_(value) { }

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

		operator ZydisDecodedOperand&();
		operator const ZydisDecodedOperand&() const;

	protected:
		ZydisDecodedOperand value_;
	};

	class disassembled_instruction_t
	{
	public:
		using size_type = std::uint32_t;
		using operand_list_t = std::vector<decoded_operand_t>;

		disassembled_instruction_t() = default;

		explicit disassembled_instruction_t(const ZydisDecodedInstruction& decoded_instruction, operand_list_t operands)
				:	decoded_instruction_(decoded_instruction),
					operands_(std::move(operands)) { }

		[[nodiscard]] bool relative() const;
		[[nodiscard]] bool rip_relative() const;
		[[nodiscard]] bool rsp_relative() const;

		[[nodiscard]] bool reads_flags() const;
		[[nodiscard]] bool writes_flags() const;

		[[nodiscard]] bool writes_stack_pointer() const;

		[[nodiscard]] size_type size() const;
		[[nodiscard]] size_type operand_width() const;

		[[nodiscard]] mnemonic_t mnemonic() const;

		[[nodiscard]] std::span<decoded_operand_t> operands();
		[[nodiscard]] std::span<const decoded_operand_t> operands() const;

		[[nodiscard]] std::span<decoded_operand_t> visible_operands();
		[[nodiscard]] std::span<const decoded_operand_t> visible_operands() const;

		[[nodiscard]] std::span<decoded_operand_t> hidden_operands();
		[[nodiscard]] std::span<const decoded_operand_t> hidden_operands() const;

		[[nodiscard]] register_family_t find_unused_register(std::span<const register_family_t> excluding = { }) const;
		[[nodiscard]] register_family_t find_unused_register(register_family_t excluding) const;

		[[nodiscard]] bool is_jump() const;
		[[nodiscard]] bool is_conditional_jump() const;
		[[nodiscard]] bool is_unconditional_jump() const;
		[[nodiscard]] bool is_call() const;
		[[nodiscard]] bool is_ret() const;
		[[nodiscard]] bool is_mov() const;
		[[nodiscard]] bool is_lea() const;
		[[nodiscard]] bool is_rol() const;
		[[nodiscard]] bool is_ror() const;
		[[nodiscard]] bool is_add() const;
		[[nodiscard]] bool is_sub() const;
		[[nodiscard]] bool is_and() const;
		[[nodiscard]] bool is_nop() const;
		[[nodiscard]] bool is_movsb() const;
		[[nodiscard]] bool is_movsxd() const;
		[[nodiscard]] bool is_cmp() const;

		[[nodiscard]] std::string to_string() const;

		[[nodiscard]] explicit operator ZydisDecodedInstruction&();
		[[nodiscard]] explicit operator const ZydisDecodedInstruction&() const;

	protected:
		ZydisDecodedInstruction decoded_instruction_ = { };
		operand_list_t operands_ = { };
	};

	class disassembler_t
	{
	public:
		disassembler_t();

		[[nodiscard]] std::optional<disassembled_instruction_t> disassemble(const std::uint8_t* instruction) const;

	protected:
		[[nodiscard]] bool decode_instruction(const std::uint8_t* instruction, ZydisDecoderContext* context, ZydisDecodedInstruction* decoded_instruction) const;
		[[nodiscard]] bool decode_operands(const ZydisDecoderContext* context, const ZydisDecodedInstruction* instruction, std::span<ZydisDecodedOperand> operands) const;

		ZydisDecoder decoder_;
	};

	std::optional<std::uint32_t> resolve_instruction_rva(const disassembled_instruction_t& instruction, rva_t instruction_rva);
}
