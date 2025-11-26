#include "mnemonic.hpp"

const binwrite::mnemonic_t binwrite::mnemonic_t::invalid = mnemonic_t(ZYDIS_MNEMONIC_INVALID);
const binwrite::mnemonic_t binwrite::mnemonic_t::push = mnemonic_t(ZYDIS_MNEMONIC_PUSH);
const binwrite::mnemonic_t binwrite::mnemonic_t::pop = mnemonic_t(ZYDIS_MNEMONIC_POP);
const binwrite::mnemonic_t binwrite::mnemonic_t::shl = mnemonic_t(ZYDIS_MNEMONIC_SHL);
const binwrite::mnemonic_t binwrite::mnemonic_t::shr = mnemonic_t(ZYDIS_MNEMONIC_SHR);
const binwrite::mnemonic_t binwrite::mnemonic_t::imul = mnemonic_t(ZYDIS_MNEMONIC_IMUL);
const binwrite::mnemonic_t binwrite::mnemonic_t::mul = mnemonic_t(ZYDIS_MNEMONIC_MUL);
const binwrite::mnemonic_t binwrite::mnemonic_t::add = mnemonic_t(ZYDIS_MNEMONIC_ADD);
const binwrite::mnemonic_t binwrite::mnemonic_t::neg = mnemonic_t(ZYDIS_MNEMONIC_NEG);
const binwrite::mnemonic_t binwrite::mnemonic_t::mov = mnemonic_t(ZYDIS_MNEMONIC_MOV);
const binwrite::mnemonic_t binwrite::mnemonic_t::sub = mnemonic_t(ZYDIS_MNEMONIC_SUB);
const binwrite::mnemonic_t binwrite::mnemonic_t::and_ = mnemonic_t(ZYDIS_MNEMONIC_AND);
const binwrite::mnemonic_t binwrite::mnemonic_t::or_ = mnemonic_t(ZYDIS_MNEMONIC_OR);
const binwrite::mnemonic_t binwrite::mnemonic_t::xor_ = mnemonic_t(ZYDIS_MNEMONIC_XOR);
const binwrite::mnemonic_t binwrite::mnemonic_t::not_ = mnemonic_t(ZYDIS_MNEMONIC_NOT);
