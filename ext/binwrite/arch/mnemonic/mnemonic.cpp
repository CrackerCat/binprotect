#include "mnemonic.hpp"

const binwrite::mnemonic_t binwrite::mnemonic_t::invalid = mnemonic_t(ZYDIS_MNEMONIC_INVALID);
const binwrite::mnemonic_t binwrite::mnemonic_t::call = mnemonic_t(ZYDIS_MNEMONIC_CALL);
const binwrite::mnemonic_t binwrite::mnemonic_t::pushfq = mnemonic_t(ZYDIS_MNEMONIC_PUSHFQ);
const binwrite::mnemonic_t binwrite::mnemonic_t::popfq = mnemonic_t(ZYDIS_MNEMONIC_POPFQ);
const binwrite::mnemonic_t binwrite::mnemonic_t::push = mnemonic_t(ZYDIS_MNEMONIC_PUSH);
const binwrite::mnemonic_t binwrite::mnemonic_t::pop = mnemonic_t(ZYDIS_MNEMONIC_POP);
const binwrite::mnemonic_t binwrite::mnemonic_t::shl = mnemonic_t(ZYDIS_MNEMONIC_SHL);
const binwrite::mnemonic_t binwrite::mnemonic_t::shr = mnemonic_t(ZYDIS_MNEMONIC_SHR);
const binwrite::mnemonic_t binwrite::mnemonic_t::imul = mnemonic_t(ZYDIS_MNEMONIC_IMUL);
const binwrite::mnemonic_t binwrite::mnemonic_t::mul = mnemonic_t(ZYDIS_MNEMONIC_MUL);
const binwrite::mnemonic_t binwrite::mnemonic_t::add = mnemonic_t(ZYDIS_MNEMONIC_ADD);
const binwrite::mnemonic_t binwrite::mnemonic_t::neg = mnemonic_t(ZYDIS_MNEMONIC_NEG);
const binwrite::mnemonic_t binwrite::mnemonic_t::mov = mnemonic_t(ZYDIS_MNEMONIC_MOV);
const binwrite::mnemonic_t binwrite::mnemonic_t::jmp = mnemonic_t(ZYDIS_MNEMONIC_JMP);
const binwrite::mnemonic_t binwrite::mnemonic_t::jz = mnemonic_t(ZYDIS_MNEMONIC_JZ);
const binwrite::mnemonic_t binwrite::mnemonic_t::jnz = mnemonic_t(ZYDIS_MNEMONIC_JNZ);
const binwrite::mnemonic_t binwrite::mnemonic_t::jb = mnemonic_t(ZYDIS_MNEMONIC_JB);
const binwrite::mnemonic_t binwrite::mnemonic_t::jnb = mnemonic_t(ZYDIS_MNEMONIC_JNB);
const binwrite::mnemonic_t binwrite::mnemonic_t::cmp = mnemonic_t(ZYDIS_MNEMONIC_CMP);
const binwrite::mnemonic_t binwrite::mnemonic_t::cmpxchg = mnemonic_t(ZYDIS_MNEMONIC_CMPXCHG);
const binwrite::mnemonic_t binwrite::mnemonic_t::lea = mnemonic_t(ZYDIS_MNEMONIC_LEA);
const binwrite::mnemonic_t binwrite::mnemonic_t::test = mnemonic_t(ZYDIS_MNEMONIC_TEST);
const binwrite::mnemonic_t binwrite::mnemonic_t::sub = mnemonic_t(ZYDIS_MNEMONIC_SUB);
const binwrite::mnemonic_t binwrite::mnemonic_t::and_ = mnemonic_t(ZYDIS_MNEMONIC_AND);
const binwrite::mnemonic_t binwrite::mnemonic_t::or_ = mnemonic_t(ZYDIS_MNEMONIC_OR);
const binwrite::mnemonic_t binwrite::mnemonic_t::xor_ = mnemonic_t(ZYDIS_MNEMONIC_XOR);
const binwrite::mnemonic_t binwrite::mnemonic_t::not_ = mnemonic_t(ZYDIS_MNEMONIC_NOT);
const binwrite::mnemonic_t binwrite::mnemonic_t::nop = mnemonic_t(ZYDIS_MNEMONIC_NOP);
const binwrite::mnemonic_t binwrite::mnemonic_t::ret = mnemonic_t(ZYDIS_MNEMONIC_RET);
const binwrite::mnemonic_t binwrite::mnemonic_t::setnbe = mnemonic_t(ZYDIS_MNEMONIC_SETNBE);
const binwrite::mnemonic_t binwrite::mnemonic_t::setb = mnemonic_t(ZYDIS_MNEMONIC_SETB);
const binwrite::mnemonic_t binwrite::mnemonic_t::sets = mnemonic_t(ZYDIS_MNEMONIC_SETS);

binwrite::mnemonic_t::value_type binwrite::mnemonic_t::value() const
{
	return value_;
}

bool binwrite::mnemonic_t::operator==(const mnemonic_t& other) const
{
	return value_ == other.value_;
}

bool binwrite::mnemonic_t::operator!=(const mnemonic_t& other) const
{
	return value_ != other.value_;
}

binwrite::mnemonic_t::operator ZydisMnemonic_() const
{
	return static_cast<ZydisMnemonic_>(value_);
}
