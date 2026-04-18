#include "rva.hpp"
#include <memory>
#include <spdlog/spdlog.h>

#include "../../disassembler/disassembler.hpp"
#include "../binary.hpp"
#include "../pe/cxx_frame_handler4.hpp"

binwrite::rva_t::value_type binwrite::rva_t::value() const
{
	return value_;
}

void binwrite::rva_t::set_value(const value_type value)
{
	value_ = value;
}

bool binwrite::rva_t::force_inclusive() const
{
	return force_inclusive_;
}

void binwrite::rva_t::set_force_inclusive(const bool force_inclusive)
{
	force_inclusive_ = force_inclusive;
}

void binwrite::rva_t::process_disruption(const rva_t disruption_rva, const size_type disruption_size,
                                         const bool inclusive)
{
	if (inclusive || force_inclusive_)
	{
		if (disruption_rva.value() <= value_)
		{
			value_ += disruption_size;
		}
	}
	else if (disruption_rva.value() < value_)
	{
		value_ += disruption_size;
	}
}

binwrite::rva_t binwrite::rva_ref_t::self() const
{
	return self_;
}

void binwrite::rva_ref_t::set_self(const rva_t self)
{
	self_ = self;
}

std::shared_ptr<binwrite::rva_t> binwrite::rva_ref_t::target() const
{
	return target_;
}

void binwrite::rva_ref_t::set_target(std::shared_ptr<rva_t> target)
{
	target_ = std::move(target);
}

void binwrite::rva_ref_t::process_disruption(const rva_t disruption_rva, const rva_t::size_type disruption_size)
{
	self_.process_disruption(disruption_rva, disruption_size, true);
}

std::expected<void, binwrite::rva_ref_t::error_t> binwrite::code_rva_ref_t::update_reference(binary_t& binary)
{
	const auto buffer = binary.data() + self_.value();

	auto assembler_instruction = make_assembler_instruction(buffer);

	if (!assembler_instruction)
	{
		return std::unexpected(error_t::cant_make_assembler_instruction);
	}

	if (!update_rva_in_assembler_instruction(*assembler_instruction))
	{
		return std::unexpected(error_t::cant_update_operand);
	}

	return compile_and_patch(binary, *assembler_instruction);
}

bool binwrite::code_rva_ref_t::update_rva_in_assembler_instruction(assembler_instruction_t& instruction) const
{
	const rva_t::value_type rip = self_.value() + size_;
	const auto difference = static_cast<std::int64_t>(target_->value()) - static_cast<std::int64_t>(rip);

	bool updated = false;

	for (auto& operand : instruction.operands())
	{
		if (operand.is_imm() && (instruction.is_call() || instruction.is_jump()))
		{
			const encoder_operand_t::imm_t imm = { .s = difference };

			operand.set_imm(imm);

			updated = true;
		}
		else if (operand.is_mem())
		{
			if (encoder_operand_t::mem_t mem = operand.mem(); mem.base == register_t::rip)
			{
				mem.displacement = difference;

				operand.set_mem(mem);

				updated = true;
			}
		}
	}

	return updated;
}

std::expected<void, binwrite::rva_ref_t::error_t> binwrite::code_rva_ref_t::compile_and_patch(binary_t& binary, const assembler_instruction_t& instruction)
{
	const auto compilation = instruction.compile_bytes();

	if (!compilation)
	{
		return std::unexpected(error_t::cant_compile);
	}

	const auto& [full_bytes, compiled_size] = *compilation;

	const auto self_rva = self_;

	if (compiled_size != size_)
	{
		binary.insert(self_rva, static_cast<rva_t::size_type>(compiled_size));
		binary.erase(self_rva, size_);

		self_ = self_rva;
		size_ = static_cast<size_type>(compiled_size);

		std::memcpy(binary.data() + self_rva.value(), full_bytes.data(), compiled_size);

		return std::unexpected(error_t::instruction_length_changed);
	}

	std::memcpy(binary.data() + self_rva.value(), full_bytes.data(), compiled_size);

	return { };
}

std::expected<void, binwrite::rva_ref_t::error_t> binwrite::data_rva_ref_t::update_reference(binary_t& binary)
{
	const rva_t::value_type target_rva = target_->value();

	std::uint8_t* const destination = binary.data() + self_.value();
	const auto source = reinterpret_cast<const std::uint8_t*>(&target_rva);

	const size_type copy_size = std::min(size_, static_cast<size_type>(sizeof(target_rva)));

	std::memset(destination, 0, size_);
	std::memcpy(destination, source, copy_size);

	return { };
}

bool binwrite::msvc_jmp_table_ref_t::update_rva_in_assembler_instruction(assembler_instruction_t& instruction) const
{
	for (auto& operand : instruction.operands())
	{
		if (operand.is_mem())
		{
			encoder_operand_t::mem_t mem = operand.mem();

			mem.displacement = target_->value();

			operand.set_mem(mem);
		}
	}

	return true;
}

std::expected<void, binwrite::rva_ref_t::error_t> binwrite::llvm_jmp_table_entry_t::update_reference(binary_t& binary)
{
	const rva_t::value_type target_rva = target_->value();
	const value_type offset = static_cast<value_type>(target_rva - table_base_->value());

	std::uint8_t* const destination = binary.data() + self_.value();
	const auto source = reinterpret_cast<const std::uint8_t*>(&offset);

	std::memcpy(destination, source, sizeof(size_type));

	return { };
}

std::expected<void, binwrite::rva_ref_t::error_t> binwrite::pe_dir64_reloc_t::update_reference(binary_t& binary)
{
	const rva_t::value_type target_rva = target_->value();
	const rva_t::value_type original_rva = original_target_value_.value();

	if (const rva_t::size_type difference = static_cast<std::int32_t>(target_rva - original_rva))
	{
		const auto destination = reinterpret_cast<std::uint64_t*>(binary.data() + self_.value());

		*destination += difference;

		original_target_value_ = *target_;
	}

	return { };
}

std::expected<void, binwrite::rva_ref_t::error_t> binwrite::pe_fh4_encoded_entry_t::update_reference(binary_t& binary)
{
	const rva_t::value_type target_rva = target_->value();
	const rva_t::value_type previous_rva = previous_entry_target_->value(); // if this is the first entry, the function rva is used

	const rva_t::size_type ip = static_cast<std::int32_t>(target_rva - previous_rva);

	std::array<std::uint8_t, 5> encoded_bytes = { };

	const std::uint32_t encoded_size = binwrite::cfh4::encode_unsigned(encoded_bytes.data(), ip);

	const auto self_rva = self_;

	if (size_ != encoded_size)
	{
		binary.insert(self_rva, static_cast<rva_t::size_type>(encoded_size));
		binary.erase(self_rva, size_);

		self_ = self_rva;
		size_ = static_cast<size_type>(encoded_size);

		std::memcpy(binary.data() + self_rva.value(), encoded_bytes.data(), encoded_size);

		return std::unexpected(error_t::instruction_length_changed);
	}

	std::memcpy(binary.data() + self_rva.value(), encoded_bytes.data(), encoded_size);

	size_ = static_cast<size_type>(encoded_size);

	return { };
}
