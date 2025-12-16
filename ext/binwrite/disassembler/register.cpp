#include "register.hpp"
#include "../math/random.hpp"

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

const binwrite::register_t binwrite::register_t::rbx = register_t(ZYDIS_REGISTER_RBX);
const binwrite::register_t binwrite::register_t::ebx = register_t(ZYDIS_REGISTER_EBX);
const binwrite::register_t binwrite::register_t::bx = register_t(ZYDIS_REGISTER_BX);
const binwrite::register_t binwrite::register_t::bl = register_t(ZYDIS_REGISTER_BL);
const binwrite::register_t binwrite::register_t::bh = register_t(ZYDIS_REGISTER_BH);

const binwrite::register_t binwrite::register_t::r8 = register_t(ZYDIS_REGISTER_R8);
const binwrite::register_t binwrite::register_t::r8d = register_t(ZYDIS_REGISTER_R8D);
const binwrite::register_t binwrite::register_t::r8w = register_t(ZYDIS_REGISTER_R8W);
const binwrite::register_t binwrite::register_t::r8b = register_t(ZYDIS_REGISTER_R8B);

const binwrite::register_t binwrite::register_t::r9 = register_t(ZYDIS_REGISTER_R9);
const binwrite::register_t binwrite::register_t::r9d = register_t(ZYDIS_REGISTER_R9D);
const binwrite::register_t binwrite::register_t::r9w = register_t(ZYDIS_REGISTER_R9W);
const binwrite::register_t binwrite::register_t::r9b = register_t(ZYDIS_REGISTER_R9B);

const binwrite::register_t binwrite::register_t::r10 = register_t(ZYDIS_REGISTER_R10);
const binwrite::register_t binwrite::register_t::r10d = register_t(ZYDIS_REGISTER_R10D);
const binwrite::register_t binwrite::register_t::r10w = register_t(ZYDIS_REGISTER_R10W);
const binwrite::register_t binwrite::register_t::r10b = register_t(ZYDIS_REGISTER_R10B);

const binwrite::register_t binwrite::register_t::r11 = register_t(ZYDIS_REGISTER_R11);
const binwrite::register_t binwrite::register_t::r11d = register_t(ZYDIS_REGISTER_R11D);
const binwrite::register_t binwrite::register_t::r11w = register_t(ZYDIS_REGISTER_R11W);
const binwrite::register_t binwrite::register_t::r11b = register_t(ZYDIS_REGISTER_R11B);

const binwrite::register_t binwrite::register_t::r12 = register_t(ZYDIS_REGISTER_R12);
const binwrite::register_t binwrite::register_t::r12d = register_t(ZYDIS_REGISTER_R12D);
const binwrite::register_t binwrite::register_t::r12w = register_t(ZYDIS_REGISTER_R12W);
const binwrite::register_t binwrite::register_t::r12b = register_t(ZYDIS_REGISTER_R12B);

const binwrite::register_t binwrite::register_t::r13 = register_t(ZYDIS_REGISTER_R13);
const binwrite::register_t binwrite::register_t::r13d = register_t(ZYDIS_REGISTER_R13D);
const binwrite::register_t binwrite::register_t::r13w = register_t(ZYDIS_REGISTER_R13W);
const binwrite::register_t binwrite::register_t::r13b = register_t(ZYDIS_REGISTER_R13B);

const binwrite::register_t binwrite::register_t::r14 = register_t(ZYDIS_REGISTER_R14);
const binwrite::register_t binwrite::register_t::r14d = register_t(ZYDIS_REGISTER_R14D);
const binwrite::register_t binwrite::register_t::r14w = register_t(ZYDIS_REGISTER_R14W);
const binwrite::register_t binwrite::register_t::r14b = register_t(ZYDIS_REGISTER_R14B);

const binwrite::register_t binwrite::register_t::r15 = register_t(ZYDIS_REGISTER_R15);
const binwrite::register_t binwrite::register_t::r15d = register_t(ZYDIS_REGISTER_R15D);
const binwrite::register_t binwrite::register_t::r15w = register_t(ZYDIS_REGISTER_R15W);
const binwrite::register_t binwrite::register_t::r15b = register_t(ZYDIS_REGISTER_R15B);

const binwrite::register_family_t binwrite::register_family_t::none = { .qword = register_t::none, .dword = register_t::none, .word = register_t::none, .byte = register_t::none, .high_byte = register_t::none };
const binwrite::register_family_t binwrite::register_family_t::ax = { .qword = register_t::rax, .dword = register_t::eax, .word = register_t::ax, .byte = register_t::al, .high_byte = register_t::ah };
const binwrite::register_family_t binwrite::register_family_t::cx = { .qword = register_t::rcx, .dword = register_t::ecx, .word = register_t::cx, .byte = register_t::cl, .high_byte = register_t::ch };
const binwrite::register_family_t binwrite::register_family_t::dx = { .qword = register_t::rdx, .dword = register_t::edx, .word = register_t::dx, .byte = register_t::dl, .high_byte = register_t::dh };
const binwrite::register_family_t binwrite::register_family_t::bx = { .qword = register_t::rbx, .dword = register_t::ebx, .word = register_t::bx, .byte = register_t::bl, .high_byte = register_t::bh };
const binwrite::register_family_t binwrite::register_family_t::eight = { .qword = register_t::r8, .dword = register_t::r8d, .word = register_t::r8w, .byte = register_t::r8b, .high_byte = register_t::none };
const binwrite::register_family_t binwrite::register_family_t::nine = { .qword = register_t::r9, .dword = register_t::r9d, .word = register_t::r9w, .byte = register_t::r9b, .high_byte = register_t::none };
const binwrite::register_family_t binwrite::register_family_t::ten = { .qword = register_t::r10, .dword = register_t::r10d, .word = register_t::r10w, .byte = register_t::r10b, .high_byte = register_t::none };
const binwrite::register_family_t binwrite::register_family_t::eleven = { .qword = register_t::r11, .dword = register_t::r11d, .word = register_t::r11w, .byte = register_t::r11b, .high_byte = register_t::none };
const binwrite::register_family_t binwrite::register_family_t::twelve = { .qword = register_t::r12, .dword = register_t::r12d, .word = register_t::r12w, .byte = register_t::r12b, .high_byte = register_t::none };
const binwrite::register_family_t binwrite::register_family_t::thirteen = { .qword = register_t::r13, .dword = register_t::r13d, .word = register_t::r13w, .byte = register_t::r13b, .high_byte = register_t::none };
const binwrite::register_family_t binwrite::register_family_t::fourteen = { .qword = register_t::r14, .dword = register_t::r14d, .word = register_t::r14w, .byte = register_t::r14b, .high_byte = register_t::none };
const binwrite::register_family_t binwrite::register_family_t::fifteen = { .qword = register_t::r15, .dword = register_t::r15d, .word = register_t::r15w, .byte = register_t::r15b, .high_byte = register_t::none };

const std::array<binwrite::register_family_t, 12> binwrite::register_family_t::general_purpose = { ax, cx, dx, bx, eight, nine, ten, eleven, twelve, thirteen, fourteen, fifteen };

binwrite::register_t::value_type binwrite::register_t::value() const
{
	return value_;
}

bool binwrite::register_t::in_same_family(const register_t& other) const
{
	constexpr auto mode = ZYDIS_MACHINE_MODE_LONG_64;

	const auto enclosing = ZydisRegisterGetLargestEnclosing(mode, static_cast<ZydisRegister>(value_));
	const auto other_enclosing = ZydisRegisterGetLargestEnclosing(mode, static_cast<ZydisRegister>(other.value_));

	return enclosing == other_enclosing;
}

binwrite::register_family_t binwrite::register_t::family() const
{
	constexpr auto mode = ZYDIS_MACHINE_MODE_LONG_64;

	const register_t enclosing_qword(ZydisRegisterGetLargestEnclosing(mode, static_cast<ZydisRegister>(value_)));

	return register_family_t::find(enclosing_qword);
}

bool binwrite::register_t::operator==(const register_t& other) const
{
	return value_ == other.value_ || in_same_family(other);
}

bool binwrite::register_t::operator!=(const register_t& other) const
{
	return value_ != other.value_ && !in_same_family(other);
}

binwrite::register_t::operator ZydisRegister_() const
{
	return static_cast<ZydisRegister>(value_);
}

binwrite::register_t binwrite::register_family_t::of_size(const register_t::size_type size) const
{
	switch (size)
	{
	case 64:
		return qword;
	case 32:
		return dword;
	case 16:
		return word;
	case 8:
		return byte;
	default:
		return register_t::none;
	}
}

bool binwrite::register_family_t::operator==(const register_family_t& other) const
{
	return qword == other.qword;
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

binwrite::register_family_t binwrite::register_family_t::random()
{
	return binwrite::math::random_entry<const register_family_t>(general_purpose);
}
