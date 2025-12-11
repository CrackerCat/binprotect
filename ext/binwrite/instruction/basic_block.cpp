#include "basic_block.hpp"

#include <spdlog/spdlog.h>

#include "../binary/binary.hpp"

binwrite::rva_t binwrite::basic_block_t::end_rva() const
{
	return instruction_rva(count());
}

[[nodiscard]] binwrite::rva_t binwrite::basic_block_t::instruction_rva(const size_type index) const
{
	rva_t::value_type rva = rva_->value();

	for (size_type i = 0; i < count() && i < index; i++)
	{
		const auto instruction = instructions_[i];

		rva += instruction.size();
	}

	return rva_t{ rva };
}

binwrite::basic_block_t::size_type binwrite::basic_block_t::instruction_index(const rva_t target_rva) const
{
	rva_t::value_type rva = rva_->value();

	for (size_type i = 0; i < count(); i++)
	{
		if (rva == target_rva.value())
		{
			return i;
		}

		const auto instruction = instructions_[i];

		rva += instruction.size();
	}

	return -1;
}

void binwrite::basic_block_t::move_entire(binary_t& binary, rva_t destination) const
{
	const rva_t original_block_rva = *rva_;
	const rva_t::size_type block_size = static_cast<std::int32_t>(end_rva().value() - original_block_rva.value() - 1);

	std::vector<std::pair<std::shared_ptr<rva_ref_t>, rva_t::value_type>> references_in_block = { };

	for (const auto& rva_ref : binary.rva_refs())
	{
		if (contains(rva_ref->self()))
		{
			const auto offset = rva_ref->self().value() - original_block_rva.value();

			references_in_block.emplace_back(rva_ref, offset);
		}
	}

	std::vector<std::pair<std::shared_ptr<rva_t>, rva_t::value_type>> rvas_pointing_to_block = { };

	for (const auto& rva : binary.rvas())
	{
		if (contains(*rva))
		{
			const auto offset = rva->value() - original_block_rva.value();

			rvas_pointing_to_block.emplace_back(rva, offset);
		}
	}

	binary.insert(destination, block_size, true);

	if (destination < original_block_rva)
	{
		binary.erase(rva_t{ original_block_rva.value() + block_size }, block_size, true);
	}
	else
	{
		binary.erase(original_block_rva, block_size, true);

		destination = rva_t{ destination.value() - block_size };
	}

	for (const auto& [reference, value] : references_in_block)
	{
		rva_t self = reference->self();

		self.set_value(destination.value() + value);

		reference->set_self(self);
	}

	for (const auto& [rva_finger, value] : rvas_pointing_to_block)
	{
		rva_finger->set_value(destination.value() + value);
	}

	*rva_ = destination;
}

void binwrite::basic_block_t::push(binary_t& binary, const instruction_t& instruction, const bool pre_existing)
{
	if (!pre_existing)
	{
		const rva_t rva = instruction_rva(count());

		binary.insert(rva, instruction.size());	
	}

	instructions_.push_back(instruction);
}

void binwrite::basic_block_t::push(binary_t& binary, const std::span<const instruction_t> instructions, const bool pre_existing)
{
	for (const auto& instruction : instructions)
	{
		push(binary, instruction, pre_existing);
	}
}

void binwrite::basic_block_t::insert(binary_t& binary, const instruction_t& instruction, const size_type index)
{
	const rva_t rva = instruction_rva(index);

	binary.insert(rva, instruction.bytes());

	instructions_.insert(instructions_.begin() + index, instruction);
}

void binwrite::basic_block_t::insert(binary_t& binary, const std::span<const instruction_t> instructions, const size_type index)
{
	for (std::uint32_t i = 0; i < instructions.size(); i++)
	{
		const auto& instruction = instructions[i];

		insert(binary, instruction, index + i);
	}
}

void binwrite::basic_block_t::erase(binary_t& binary, const size_type index, const size_type count, const bool affects_buffer)
{
	const auto first_instruction = instructions_.begin() + index;

	if (affects_buffer)
	{
		const rva_t rva = instruction_rva(index);

		for (size_type i = 0; i < count; i++)
		{
			const auto instruction = first_instruction + i;

			binary.erase(rva, instruction->size());
		}
	}

	instructions_.erase(first_instruction, first_instruction + count);
}

void binwrite::basic_block_t::erase(binary_t& binary, const size_type index, const bool affects_buffer)
{
	return erase(binary, index, 1, affects_buffer);
}
