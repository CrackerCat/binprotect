#include "exception_directory.hpp"

bool portable_executable::unwind_info_t::has_frame_pointer() const
{
	for (const auto unwind_code : *this)
	{
		if (unwind_code.opcode == unwind_opcode_t::set_frame_register)
		{
			return true;
		}
	}

	return false;
}

bool portable_executable::unwind_info_t::has_handler() const
{
	return flags & 3;
}

bool portable_executable::unwind_info_t::has_chained_function() const
{
	return flags & 4;
}

std::span<portable_executable::unwind_code_t> portable_executable::unwind_info_t::unwind_codes()
{
	return { codes, codes + unwind_code_count };
}

std::span<const portable_executable::unwind_code_t> portable_executable::unwind_info_t::unwind_codes() const
{
	return { codes, codes + unwind_code_count };
}

portable_executable::unwind_code_iterator_t portable_executable::unwind_info_t::begin() const
{
	return unwind_code_iterator_t{ codes };
}

portable_executable::unwind_code_iterator_t portable_executable::unwind_info_t::end() const
{
	return unwind_code_iterator_t{ codes + unwind_code_count };
}

portable_executable::unwind_code_iterator_t::reference portable_executable::unwind_code_iterator_t::operator*()
{
	return *const_cast<unwind_code_t*>(m_current_code);
}

portable_executable::unwind_code_iterator_t::const_reference portable_executable::unwind_code_iterator_t::operator*() const
{
	return *m_current_code;
}

portable_executable::unwind_code_iterator_t& portable_executable::unwind_code_iterator_t::operator++()
{
	++m_current_code;

	return *this;
}

bool portable_executable::unwind_code_iterator_t::operator==(const unwind_code_iterator_t& other) const
{
	return m_current_code == other.m_current_code;
}

bool portable_executable::unwind_code_iterator_t::operator!=(const unwind_code_iterator_t& other) const
{
	return m_current_code != other.m_current_code;
}

portable_executable::runtime_functions_iterator_t::value_type portable_executable::runtime_functions_iterator_t::operator*() const
{
	const auto unwind_info = reinterpret_cast<unwind_info_t*>(const_cast<std::uint8_t*>(m_module + m_current_function->unwind_info_rva));

	return value_type{ m_current_function->begin_address, m_current_function->end_address, unwind_info };
}

portable_executable::runtime_functions_iterator_t& portable_executable::runtime_functions_iterator_t::operator++()
{
	++m_current_function;

	return *this;
}

bool portable_executable::runtime_functions_iterator_t::operator==(const runtime_functions_iterator_t& other) const
{
	return m_current_function == other.m_current_function;
}

bool portable_executable::runtime_functions_iterator_t::operator!=(const runtime_functions_iterator_t& other) const
{
	return m_current_function != other.m_current_function;
}
