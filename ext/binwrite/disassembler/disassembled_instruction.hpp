#pragma once
#include "decoded_operand.hpp"

#include <string>
#include <vector>

namespace binwrite
{
	class mnemonic_t;

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

		[[nodiscard]] bool reads_register_family(register_family_t family) const;
		[[nodiscard]] bool writes_register_family(register_family_t family) const;

		[[nodiscard]] bool reads_flags() const;
		[[nodiscard]] bool writes_flags() const;

		[[nodiscard]] bool writes_stack_pointer() const;
		[[nodiscard]] bool reads_stack_pointer() const;

		[[nodiscard]] bool has_lock() const;

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

		[[nodiscard]] bool is_control_flow() const;

		[[nodiscard]] bool is_jump() const;
		[[nodiscard]] bool is_conditional_jump() const;
		[[nodiscard]] bool is_unconditional_jump() const;
		[[nodiscard]] bool is_call() const;
		[[nodiscard]] bool is_int() const;
		[[nodiscard]] bool is_ret() const;
		[[nodiscard]] bool is_mov() const;
		[[nodiscard]] bool is_movzx() const;
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

		[[nodiscard]] explicit operator ZydisDecodedInstruction& ();
		[[nodiscard]] explicit operator const ZydisDecodedInstruction& () const;

	protected:
		ZydisDecodedInstruction decoded_instruction_ = { };
		operand_list_t operands_ = { };
	};
}
