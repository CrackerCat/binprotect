#include "decoded_operand.hpp"

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

binwrite::decoded_operand_t::operator ZydisDecodedOperand_& ()
{
	return value_;
}

binwrite::decoded_operand_t::operator const ZydisDecodedOperand_& () const
{
	return value_;
}
