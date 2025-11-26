#include "register.hpp"

const binwrite::register_t binwrite::register_t::none = register_t(ZYDIS_REGISTER_NONE);
const binwrite::register_t binwrite::register_t::rip = register_t(ZYDIS_REGISTER_RIP);
const binwrite::register_t binwrite::register_t::rsp = register_t(ZYDIS_REGISTER_RSP);

const binwrite::register_t binwrite::register_t::rax = register_t(ZYDIS_REGISTER_RAX);
const binwrite::register_t binwrite::register_t::eax = register_t(ZYDIS_REGISTER_EAX);
const binwrite::register_t binwrite::register_t::ax = register_t(ZYDIS_REGISTER_AX);
const binwrite::register_t binwrite::register_t::al = register_t(ZYDIS_REGISTER_AL);
const binwrite::register_t binwrite::register_t::ah = register_t(ZYDIS_REGISTER_AH);

const binwrite::register_t binwrite::register_t::rcx = register_t(ZYDIS_REGISTER_RCX);
const binwrite::register_t binwrite::register_t::ecx = register_t(ZYDIS_REGISTER_ECX);
const binwrite::register_t binwrite::register_t::cx = register_t(ZYDIS_REGISTER_CX);
const binwrite::register_t binwrite::register_t::cl = register_t(ZYDIS_REGISTER_CL);
const binwrite::register_t binwrite::register_t::ch = register_t(ZYDIS_REGISTER_CH);

const binwrite::register_t binwrite::register_t::rdx = register_t(ZYDIS_REGISTER_RDX);
const binwrite::register_t binwrite::register_t::edx = register_t(ZYDIS_REGISTER_EDX);
const binwrite::register_t binwrite::register_t::dx = register_t(ZYDIS_REGISTER_DX);
const binwrite::register_t binwrite::register_t::dl = register_t(ZYDIS_REGISTER_DL);
const binwrite::register_t binwrite::register_t::dh = register_t(ZYDIS_REGISTER_DH);

const binwrite::register_family_t binwrite::register_family_t::none = { .qword = register_t::none, .dword = register_t::none, .word = register_t::none, .byte = register_t::none, .high_byte = register_t::none };
const binwrite::register_family_t binwrite::register_family_t::ax = { .qword = register_t::rax, .dword = register_t::eax, .word = register_t::ax, .byte = register_t::al, .high_byte = register_t::ah };
const binwrite::register_family_t binwrite::register_family_t::cx = { .qword = register_t::rcx, .dword = register_t::ecx, .word = register_t::cx, .byte = register_t::cl, .high_byte = register_t::ch };
const binwrite::register_family_t binwrite::register_family_t::dx = { .qword = register_t::rdx, .dword = register_t::edx, .word = register_t::dx, .byte = register_t::dl, .high_byte = register_t::dh };

const std::array<binwrite::register_family_t, 3> binwrite::register_family_t::general_purpose = { ax, cx, dx };

binwrite::register_family_t binwrite::register_t::family() const
{
	constexpr auto mode = ZYDIS_MACHINE_MODE_LONG_64;

	const register_t enclosing_qword(ZydisRegisterGetLargestEnclosing(mode, static_cast<ZydisRegister>(value_)));

	return register_family_t::find(enclosing_qword);
}

binwrite::register_family_t binwrite::register_family_t::find(const register_t qword)
{
	const auto family = std::ranges::find_if(general_purpose,
		[qword](const register_family_t& current_family)
		{
			return current_family.qword == qword;
		}
	);

	return family != general_purpose.end() ? *family : none;
}
