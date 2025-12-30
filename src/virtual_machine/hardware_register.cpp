#include "hardware_register.hpp"
#include "vm_context.hpp"

hardware_register_t::~hardware_register_t()
{
	if (vm_context_ && value_ != value_type::none)
	{
		vm_context_->free_hardware_register(*this);
	}
}

binwrite::register_t hardware_register_t::of_size(const size_type size) const
{
	return value_.of_size(size);
}

binwrite::register_t hardware_register_t::of_size(const binwrite::decoded_operand_t& operand) const
{
	return of_size(operand.size());
}
