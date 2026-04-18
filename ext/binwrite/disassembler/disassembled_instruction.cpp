#include "disassembled_instruction.hpp"
#include "../arch/mnemonic/mnemonic.hpp"

bool binwrite::disassembled_instruction_t::relative() const
{
	return decoded_instruction_.attributes & ZYDIS_ATTRIB_IS_RELATIVE;
}

bool binwrite::disassembled_instruction_t::rip_relative() const
{
	for (const auto& operand : operands_)
	{
		if ((operand.is_reg() && operand.reg().value == register_t::rip) ||
			(operand.is_mem() && operand.mem().base == register_t::rip))
		{
			return true;
		}
	}

	return false;
}

bool binwrite::disassembled_instruction_t::rsp_relative() const
{
	return reads_stack_pointer() || writes_stack_pointer();
}

bool binwrite::disassembled_instruction_t::reads_register_family(const register_family_t family) const
{
	for (const auto& operand : operands_)
	{
		if (operand.is_reg() && operand.is_read_action())
		{
			const auto reg = operand.reg();

			if (family == reg.value.family())
			{
				return true;
			}
		}
		else if (operand.is_mem())
		{
			const auto& mem = operand.mem();

			if (mem.base != register_t::none && family == mem.base.family())
			{
				return true;
			}

			if (mem.index != register_t::none && family == mem.index.family())
			{
				return true;
			}
		}
	}

	return false;
}

bool binwrite::disassembled_instruction_t::writes_register_family(const register_family_t family) const
{
	for (const auto& operand : operands_)
	{
		if (operand.is_reg() && operand.is_write_action())
		{
			const auto reg = operand.reg();

			if (family == reg.value.family())
			{
				return true;
			}
		}
	}

	return false;
}

bool binwrite::disassembled_instruction_t::reads_flags() const
{
	return reads_register_family(register_family_t::flags);
}

bool binwrite::disassembled_instruction_t::writes_flags() const
{
	return writes_register_family(register_family_t::flags);
}

bool binwrite::disassembled_instruction_t::writes_stack_pointer() const
{
	return writes_register_family(register_family_t::sp);
}

bool binwrite::disassembled_instruction_t::reads_stack_pointer() const
{
	return reads_register_family(register_family_t::sp);
}

bool binwrite::disassembled_instruction_t::has_lock() const
{
	return decoded_instruction_.attributes & ZYDIS_ATTRIB_HAS_LOCK;
}

binwrite::disassembled_instruction_t::size_type binwrite::disassembled_instruction_t::size() const
{
	return decoded_instruction_.length;
}

binwrite::disassembled_instruction_t::size_type binwrite::disassembled_instruction_t::operand_width() const
{
	return decoded_instruction_.operand_width;
}

binwrite::mnemonic_t binwrite::disassembled_instruction_t::mnemonic() const
{
	return mnemonic_t(decoded_instruction_.mnemonic);
}

std::span<binwrite::decoded_operand_t> binwrite::disassembled_instruction_t::operands()
{
	return operands_;
}

std::span<const binwrite::decoded_operand_t> binwrite::disassembled_instruction_t::operands() const
{
	return operands_;
}

std::span<binwrite::decoded_operand_t> binwrite::disassembled_instruction_t::visible_operands()
{
	const ZyanU8 visible_operand_count = decoded_instruction_.operand_count_visible;

	return { operands_.begin(), operands_.begin() + visible_operand_count };
}

std::span<const binwrite::decoded_operand_t> binwrite::disassembled_instruction_t::visible_operands() const
{
	const ZyanU8 visible_operand_count = decoded_instruction_.operand_count_visible;

	return { operands_.begin(), operands_.begin() + visible_operand_count };
}

std::span<binwrite::decoded_operand_t> binwrite::disassembled_instruction_t::hidden_operands()
{
	const ZyanU8 visible_operand_count = decoded_instruction_.operand_count_visible;
	const ZyanU8 hidden_operand_count = decoded_instruction_.operand_count - visible_operand_count;

	const auto hidden_operands_begin = operands_.begin() + visible_operand_count;

	return { hidden_operands_begin, hidden_operands_begin + hidden_operand_count };
}

std::span<const binwrite::decoded_operand_t> binwrite::disassembled_instruction_t::hidden_operands() const
{
	const ZyanU8 visible_operand_count = decoded_instruction_.operand_count_visible;
	const ZyanU8 hidden_operand_count = decoded_instruction_.operand_count - visible_operand_count;

	const auto hidden_operands_begin = operands_.begin() + visible_operand_count;

	return { hidden_operands_begin, hidden_operands_begin + hidden_operand_count };
}

binwrite::register_family_t binwrite::disassembled_instruction_t::find_unused_register(
	const std::span<const register_family_t> excluding) const
{
	std::vector used_registers(excluding.begin(), excluding.end());

	for (const auto& operand : operands_)
	{
		if (operand.is_reg())
		{
			const auto& reg = operand.reg();

			used_registers.push_back(reg.value.family());
		}
		else if (operand.is_mem())
		{
			const auto& mem = operand.mem();

			if (mem.base != register_t::none)
			{
				used_registers.push_back(mem.base.family());
			}

			if (mem.index != register_t::none)
			{
				used_registers.push_back(mem.index.family());
			}
		}
	}

	return register_family_t::random(used_registers);
}

binwrite::register_family_t binwrite::disassembled_instruction_t::find_unused_register(
	const register_family_t excluding) const
{
	return find_unused_register(std::array{ excluding });
}

bool binwrite::disassembled_instruction_t::is_control_flow() const
{
	return is_jump() || is_call() || is_ret();
}

bool binwrite::disassembled_instruction_t::is_jump() const
{
	return is_conditional_jump() || is_unconditional_jump();
}

bool binwrite::disassembled_instruction_t::is_conditional_jump() const
{
	return decoded_instruction_.meta.category == ZYDIS_CATEGORY_COND_BR;
}

bool binwrite::disassembled_instruction_t::is_unconditional_jump() const
{
	return decoded_instruction_.meta.category == ZYDIS_CATEGORY_UNCOND_BR;
}

bool binwrite::disassembled_instruction_t::is_call() const
{
	return decoded_instruction_.meta.category == ZYDIS_CATEGORY_CALL;
}

bool binwrite::disassembled_instruction_t::is_int() const
{
	return decoded_instruction_.meta.category == ZYDIS_CATEGORY_INTERRUPT;
}

bool binwrite::disassembled_instruction_t::is_ret() const
{
	return decoded_instruction_.meta.category == ZYDIS_CATEGORY_RET;
}

bool binwrite::disassembled_instruction_t::is_mov() const
{
	return ZYDIS_MNEMONIC_MOV <= decoded_instruction_.mnemonic && decoded_instruction_.mnemonic <= ZYDIS_MNEMONIC_MOVZX;
}

bool binwrite::disassembled_instruction_t::is_movzx() const
{
	return decoded_instruction_.mnemonic == ZYDIS_MNEMONIC_MOVZX;
}

bool binwrite::disassembled_instruction_t::is_lea() const
{
	return decoded_instruction_.mnemonic == ZYDIS_MNEMONIC_LEA;
}

bool binwrite::disassembled_instruction_t::is_rol() const
{
	return decoded_instruction_.mnemonic == ZYDIS_MNEMONIC_ROL;
}

bool binwrite::disassembled_instruction_t::is_ror() const
{
	return decoded_instruction_.mnemonic == ZYDIS_MNEMONIC_ROR;
}

bool binwrite::disassembled_instruction_t::is_add() const
{
	return decoded_instruction_.mnemonic == ZYDIS_MNEMONIC_ADD;
}

bool binwrite::disassembled_instruction_t::is_sub() const
{
	return decoded_instruction_.mnemonic == ZYDIS_MNEMONIC_SUB;
}

bool binwrite::disassembled_instruction_t::is_and() const
{
	return decoded_instruction_.mnemonic == ZYDIS_MNEMONIC_AND;
}

bool binwrite::disassembled_instruction_t::is_nop() const
{
	return decoded_instruction_.mnemonic == ZYDIS_MNEMONIC_NOP;
}

bool binwrite::disassembled_instruction_t::is_movsb() const
{
	return decoded_instruction_.mnemonic == ZYDIS_MNEMONIC_MOVSB;
}

bool binwrite::disassembled_instruction_t::is_movsxd() const
{
	return decoded_instruction_.mnemonic == ZYDIS_MNEMONIC_MOVSXD;
}

bool binwrite::disassembled_instruction_t::is_cmp() const
{
	return decoded_instruction_.mnemonic == ZYDIS_MNEMONIC_CMP;
}

std::string binwrite::disassembled_instruction_t::to_string() const
{
	if (is_nop())
	{
		return "nop";
	}

	ZydisFormatter formatter;
	ZydisFormatterInit(&formatter, ZYDIS_FORMATTER_STYLE_INTEL_MASM);

	std::array<char, 256> buffer = { };

	const auto zydis_operands = reinterpret_cast<const ZydisDecodedOperand*>(operands_.data());

	ZydisFormatterFormatInstruction(&formatter, &decoded_instruction_, zydis_operands, decoded_instruction_.operand_count_visible, buffer.data(), sizeof(buffer), 0, nullptr);

	return { buffer.data() };
}

binwrite::disassembled_instruction_t::operator ZydisDecodedInstruction_& ()
{
	return decoded_instruction_;
}

binwrite::disassembled_instruction_t::operator const ZydisDecodedInstruction_& () const
{
	return decoded_instruction_;
}
