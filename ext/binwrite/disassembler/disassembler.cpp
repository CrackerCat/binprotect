#include "disassembler.hpp"
#include "mnemonic.hpp"
#include "../rva/rva.hpp"

bool binwrite::decoded_operand_t::is_imm() const
{
	return value_.type == ZYDIS_OPERAND_TYPE_IMMEDIATE;
}

binwrite::decoded_operand_t::imm_t binwrite::decoded_operand_t::imm() const
{
	imm_t imm;

	imm.value.u = value_.imm.value.u;
	imm.is_relative = value_.imm.is_relative;
	imm.is_signed = value_.imm.is_signed;

	return imm;
}

void binwrite::decoded_operand_t::set_imm(const imm_t imm)
{
	value_.imm.value.u = imm.value.u;
	value_.imm.is_relative = imm.is_relative;
	value_.imm.is_signed = imm.is_signed;

	value_.type = ZYDIS_OPERAND_TYPE_IMMEDIATE;
}

bool binwrite::decoded_operand_t::is_mem() const
{
	return value_.type == ZYDIS_OPERAND_TYPE_MEMORY;
}

binwrite::decoded_operand_t::mem_t binwrite::decoded_operand_t::mem() const
{
	mem_t mem;

	mem.has_displacement = value_.mem.disp.has_displacement;
	mem.displacement = value_.mem.disp.value;
	mem.scale = value_.mem.scale;

	mem.base = register_t(value_.mem.base);
	mem.index = register_t(value_.mem.index);
	mem.segment = register_t(value_.mem.segment);

	return mem;
}

void binwrite::decoded_operand_t::set_mem(const mem_t mem)
{
	value_.mem.segment = mem.segment;
	value_.mem.base = mem.base;
	value_.mem.index = mem.index;
	value_.mem.scale = mem.scale;
	value_.mem.disp.value = mem.displacement;
	value_.mem.disp.has_displacement = mem.has_displacement;

	value_.type = ZYDIS_OPERAND_TYPE_MEMORY;
}

bool binwrite::decoded_operand_t::is_reg() const
{
	return value_.type == ZYDIS_OPERAND_TYPE_REGISTER;
}

binwrite::decoded_operand_t::reg_t binwrite::decoded_operand_t::reg() const
{
	reg_t reg;

	reg.value = register_t(value_.reg.value);

	return reg;
}

void binwrite::decoded_operand_t::set_reg(const reg_t reg)
{
	value_.reg.value = reg.value;

	value_.type = ZYDIS_OPERAND_TYPE_REGISTER;
}

bool binwrite::decoded_operand_t::is_read_action() const
{
	return value_.actions & ZYDIS_OPERAND_ACTION_READ || value_.actions & ZYDIS_OPERAND_ACTION_CONDREAD;
}

bool binwrite::decoded_operand_t::is_write_action() const
{
	return value_.actions & ZYDIS_OPERAND_ACTION_WRITE || value_.actions & ZYDIS_OPERAND_ACTION_CONDWRITE;
}

binwrite::decoded_operand_t::size_type binwrite::decoded_operand_t::size() const
{
	return value_.size;
}

binwrite::decoded_operand_t::operator ZydisDecodedOperand_&()
{
	return value_;
}

binwrite::decoded_operand_t::operator const ZydisDecodedOperand_&() const
{
	return value_;
}

bool binwrite::disassembled_instruction_t::relative() const
{
	return decoded_instruction_.attributes & ZYDIS_ATTRIB_IS_RELATIVE;
}

bool binwrite::disassembled_instruction_t::rip_relative() const
{
	if (!relative())
	{
		return false;
	}

	for (const auto& operand : operands_)
	{
		if (operand.is_imm() || (operand.is_mem() && operand.mem().base == register_t::rip))
		{
			return true;
		}
	}

	return false;
}

bool binwrite::disassembled_instruction_t::rsp_relative() const
{
	for (const auto& operand : operands_)
	{
		if (operand.is_reg())
		{
			const auto reg = operand.reg();

			if (reg.value == register_t::rsp)
			{
				return true;
			}
		}

		if (operand.is_mem())
		{
			const auto mem = operand.mem();

			if (mem.base == register_t::rsp)
			{
				return true;
			}
		}
	}

	return false;
}

bool binwrite::disassembled_instruction_t::reads_rflags() const
{
	for (const auto& operand : hidden_operands())
	{
		if (operand.is_reg())
		{
			const auto reg = operand.reg();

			if (reg.value == register_t::rflags && operand.is_read_action())
			{
				return true;
			}
		}
	}

	return false;
}

bool binwrite::disassembled_instruction_t::writes_rflags() const
{
	for (const auto& operand : hidden_operands())
	{
		if (operand.is_reg())
		{
			const auto reg = operand.reg();

			if (reg.value == register_t::rflags && operand.is_write_action())
			{
				return true;
			}
		}
	}

	return false;
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
	std::vector<register_t> used_registers = { };

	for (const auto& visible_operand : visible_operands())
	{
		if (visible_operand.is_reg())
		{
			const auto& reg = visible_operand.reg();

			used_registers.push_back(reg.value);
		}
		else if (visible_operand.is_mem())
		{
			const auto& mem = visible_operand.mem();

			used_registers.push_back(mem.base);
			used_registers.push_back(mem.index);
		}
	}

	const auto unused_register = std::ranges::find_if(
		register_family_t::general_purpose,
		[&used_registers, excluding](const register_family_t family)
		{
			if (std::ranges::contains(excluding, family))
			{
				return false;
			}

			return std::ranges::none_of(used_registers,
				[family, excluding](const register_t used_register)
				{
					return family == used_register.family();
				}
			);
		}
	);

	return *unused_register;
}

binwrite::register_family_t binwrite::disassembled_instruction_t::find_unused_register(
	const register_family_t excluding) const
{
	return find_unused_register(std::array{ excluding });
}

bool binwrite::disassembled_instruction_t::is_jump() const
{
	return ZYDIS_MNEMONIC_JB <= decoded_instruction_.mnemonic && decoded_instruction_.mnemonic <= ZYDIS_MNEMONIC_JZ;
}

bool binwrite::disassembled_instruction_t::is_conditional_jump() const
{
	return is_jump() && decoded_instruction_.mnemonic != ZYDIS_MNEMONIC_JMP;
}

bool binwrite::disassembled_instruction_t::is_unconditional_jump() const
{
	return decoded_instruction_.mnemonic == ZYDIS_MNEMONIC_JMP;
}

bool binwrite::disassembled_instruction_t::is_call() const
{
	return decoded_instruction_.mnemonic == ZYDIS_MNEMONIC_CALL;
}

bool binwrite::disassembled_instruction_t::is_ret() const
{
	return decoded_instruction_.mnemonic == ZYDIS_MNEMONIC_RET;
}

bool binwrite::disassembled_instruction_t::is_mov() const
{
	return ZYDIS_MNEMONIC_MOV <= decoded_instruction_.mnemonic && decoded_instruction_.mnemonic <= ZYDIS_MNEMONIC_MOVZX;
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
	ZydisFormatter formatter;
	ZydisFormatterInit(&formatter, ZYDIS_FORMATTER_STYLE_INTEL_MASM);

	std::array<char, 256> buffer = { };

	const auto zydis_operands = reinterpret_cast<const ZydisDecodedOperand*>(operands_.data());

	ZydisFormatterFormatInstruction(&formatter, &decoded_instruction_, zydis_operands, decoded_instruction_.operand_count_visible, buffer.data(), sizeof(buffer), 0, nullptr);

	return { buffer.data() };
}

binwrite::disassembled_instruction_t::operator ZydisDecodedInstruction_&()
{
	return decoded_instruction_;
}

binwrite::disassembled_instruction_t::operator const ZydisDecodedInstruction_&() const
{
	return decoded_instruction_;
}

binwrite::disassembler_t::disassembler_t()
{
	ZydisDecoderInit(&decoder_, ZYDIS_MACHINE_MODE_LONG_64, ZYDIS_STACK_WIDTH_64);
}

std::optional<binwrite::disassembled_instruction_t> binwrite::disassembler_t::disassemble(
	const std::uint8_t* const instruction) const
{
	ZydisDecoderContext context;
	ZydisDecodedInstruction decoded_instruction;

	if (!decode_instruction(instruction, &context, &decoded_instruction))
	{
		return std::nullopt;
	}

	std::vector<ZydisDecodedOperand> decoded_operands(decoded_instruction.operand_count);

	if (!decode_operands(&context, &decoded_instruction, decoded_operands))
	{
		return std::nullopt;
	}

	std::vector<decoded_operand_t> wrapped_operands(decoded_operands.begin(), decoded_operands.end());

	return disassembled_instruction_t{ decoded_instruction, std::move(wrapped_operands) };
}

bool binwrite::disassembler_t::decode_instruction(const std::uint8_t* const instruction,
	ZydisDecoderContext* const context, ZydisDecodedInstruction* const decoded_instruction) const
{
	constexpr ZyanUSize instruction_length = ZYDIS_MAX_INSTRUCTION_LENGTH;

	const auto status = ZydisDecoderDecodeInstruction(&decoder_, context, instruction, instruction_length, decoded_instruction);

	return ZYAN_SUCCESS(status);
}

bool binwrite::disassembler_t::decode_operands(const ZydisDecoderContext* const context,
                                               const ZydisDecodedInstruction* const instruction, const std::span<ZydisDecodedOperand> operands) const
{
	const ZyanU8 operand_count = static_cast<ZyanU8>(operands.size());

	const auto status = ZydisDecoderDecodeOperands(&decoder_, context, instruction, operands.data(), operand_count);

	return ZYAN_SUCCESS(status);
}

std::optional<std::uint32_t> binwrite::resolve_instruction_rva(const disassembled_instruction_t& instruction,
                                                               const rva_t instruction_rva)
{
	if (!instruction.relative())
	{
		return std::nullopt;
	}

	const std::uint32_t rip = instruction_rva.value() + instruction.size();

	for (auto& operand : instruction.visible_operands())
	{
		if (operand.is_imm() && operand.imm().is_relative)
		{
			const auto imm = operand.imm();

			return rip + (imm.is_signed ? static_cast<std::int32_t>(imm.value.s) : static_cast<std::uint32_t>(imm.value.u));
		}

		if (operand.is_mem() && operand.mem().base == register_t::rip)
		{
			return rip + static_cast<std::int32_t>(operand.mem().displacement);
		}
	}

	return std::nullopt;
}
