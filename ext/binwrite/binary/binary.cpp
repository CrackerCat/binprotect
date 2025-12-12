#include "binary.hpp"
#include <spdlog/spdlog.h>
#include <ranges>

#include "../disassembler/disassembler.hpp"

void binwrite::section_t::process_disruption(const rva_t disruption_rva,
	const rva_t::size_type disruption_size)
{
	if (contains(disruption_rva))
	{
		size_ += disruption_size;
	}
}

void binwrite::section_t::insert(binary_t& binary, const rva_t section_offset, const std::span<const std::uint8_t> data)
{
	if (size_ < section_offset.value())
	{
		return;
	}

	auto& buffer = binary.buffer();

	const rva_t insertion_rva(rva_->value() + section_offset.value());

	buffer.insert(buffer.begin() + insertion_rva.value(), data.begin(), data.end());

	binary.update_rvas(insertion_rva, static_cast<rva_t::size_type>(data.size()), true, false);

	size_ += static_cast<size_type>(data.size());
}

binwrite::rva_t binwrite::section_t::rva() const
{
	return *rva_;
}

binwrite::rva_t binwrite::section_t::end_rva() const
{
	return rva_t{ rva_->value() + size_ };
}

binwrite::section_t::size_type binwrite::section_t::size() const
{
	return size_;
}

void binwrite::section_t::set_size(const size_type size)
{
	size_ = size;
}

bool binwrite::section_t::contains(const rva_t rva) const
{
	const auto section_start = rva_t{ rva_->value() };
	const auto section_end = rva_t{ section_start.value() + size_ };

	return section_start <= rva && rva < section_end;
}

bool binwrite::section_t::code() const
{
	return code_;
}

binwrite::rva_t binwrite::relocation_t::target() const
{
	return *target_;
}

void binwrite::binary_t::parse()
{
	find_sections();
	find_data_rvas();

	disassemble();
}

void binwrite::binary_t::insert(const rva_t rva, const std::span<const std::uint8_t> data, const bool inclusive)
{
	buffer_.insert(buffer_.begin() + rva.value(), data.begin(), data.end());

	update_rvas(rva, static_cast<rva_t::size_type>(data.size()), inclusive);
}

void binwrite::binary_t::insert(const rva_t rva, const rva_t::size_type size, const bool inclusive)
{
	buffer_.insert(buffer_.begin() + rva.value(), size, 0);

	update_rvas(rva, size, inclusive);
}

void binwrite::binary_t::erase(const rva_t rva, const rva_t::size_type size, const bool inclusive)
{
	const auto start = buffer_.begin() + rva.value();

	buffer_.erase(start, start + size);

	update_rvas(rva, -size, inclusive);
}

std::span<std::shared_ptr<binwrite::basic_block_t>> binwrite::binary_t::basic_blocks()
{
	return basic_blocks_;
}

std::span<const std::shared_ptr<binwrite::basic_block_t>> binwrite::binary_t::basic_blocks() const
{
	return basic_blocks_;
}

std::vector<std::shared_ptr<binwrite::rva_t>> binwrite::binary_t::rvas()
{
	return rvas_;
}

std::vector<std::shared_ptr<binwrite::rva_ref_t>> binwrite::binary_t::rva_refs()
{
	return rva_refs_;
}

std::unordered_map<std::string, binwrite::section_t>& binwrite::binary_t::sections()
{
	return sections_;
}

const std::unordered_map<std::string, binwrite::section_t>& binwrite::binary_t::sections() const
{
	return sections_;
}

std::vector<std::uint8_t>& binwrite::binary_t::buffer()
{
	return buffer_;
}

const std::vector<std::uint8_t>& binwrite::binary_t::buffer() const
{
	return buffer_;
}

std::uint8_t* binwrite::binary_t::data()
{
	return buffer_.data();
}

const std::uint8_t* binwrite::binary_t::data() const
{
	return buffer_.data();
}

void binwrite::binary_t::update_rva_references()
{
	update_section_headers();

	for (std::uint64_t i = 0; i < rva_refs_.size();)
	{
		const auto& rva_ref = rva_refs_[i];

		const auto result = rva_ref->update_reference(*this);

		if (!result)
		{
			if (result.error() == rva_ref_t::error_t::instruction_length_changed)
			{
				spdlog::warn("rva reference at 0x{:X} had instruction length change", rva_ref->self().value());

				update_section_headers();

				i = 0;

				continue;
			}

			spdlog::error("unable to update rva reference at 0x{:X}", rva_ref->self().value());
		}

		i++;
	}

	update_relocations();

	// just in case relocations change section size
	update_section_headers();
}

void binwrite::binary_t::find_jump_tables(const basic_block_t& basic_block)
{
	const auto& instructions = basic_block.instructions();

	std::optional<disassembled_instruction_t> latest_lea = std::nullopt;
	rva_t latest_lea_rva = { };

	for (std::uint32_t i = 0; i < instructions.size(); i++)
	{
		const auto& root_instruction = instructions[i].disassemble();

		if (root_instruction.is_lea() && root_instruction.rip_relative())
		{
			latest_lea_rva = basic_block.instruction_rva(i);
			latest_lea = root_instruction;
		}

		if (!latest_lea && !root_instruction.is_mov())
		{
			continue;
		}

		for (const auto& root_operand : root_instruction.visible_operands())
		{
			if (!root_operand.is_mem())
			{
				continue;
			}

			const auto mem = root_operand.mem();

			if (mem.scale != 4 || mem.index == register_t::none || mem.base == register_t::none)
			{
				continue;
			}

			const bool is_msvc = mem.has_displacement;

			const auto lea_operands = latest_lea->visible_operands();

			if (lea_operands.empty())
			{
				continue;
			}

			const auto result_operand = lea_operands[0];

			if (!result_operand.is_reg() || result_operand.reg().value != mem.base)
			{
				continue;
			}

			if (is_msvc)
			{
				const auto table_base = add_rva(static_cast<rva_t::value_type>(mem.displacement));

				add_msvc_jmp_table_ref(*table_base);

				const rva_t root_instruction_rva = basic_block.instruction_rva(i);

				rva_refs_.push_back(std::make_shared<msvc_jmp_table_ref_t>(table_base, root_instruction_rva, root_instruction.size()));
			}
			else if (const auto table_base = resolve_instruction_rva(*latest_lea, latest_lea_rva))
			{
				add_llvm_jmp_table_ref(rva_t{ *table_base });
			}
		}
	}
}

void binwrite::binary_t::disassemble()
{
	const disassembler_t disassembler;

	while (!disassembly_queue_.empty())
	{
		const auto block_rva = disassembly_queue_.front();
		disassembly_queue_.pop_front();

		basic_block_t basic_block(block_rva);

		rva_t instruction_rva = *block_rva;

		while (true)
		{
			const auto instruction_address = reinterpret_cast<const std::uint8_t*>(buffer_.data() + instruction_rva.value());
			const auto disassembled_instruction = disassembler.disassemble(instruction_address);

			if (!disassembled_instruction)
			{
				break;
			}

			if (const auto overstepped_basic_block = is_inside_basic_block(rva_t{ instruction_rva }))
			{
				const auto index = (*overstepped_basic_block)->instruction_index(rva_t{ instruction_rva });

				split_basic_block(*overstepped_basic_block, index);

				break;
			}

			const auto next_instruction_rva = instruction_rva.value() + disassembled_instruction->size();

			const instruction_t::const_value_type instruction_bytes(instruction_address, disassembled_instruction->size());

			if (const auto raw_target_rva = resolve_instruction_rva(*disassembled_instruction, instruction_rva))
			{
				const auto target_rva = add_rva(*raw_target_rva);

				rva_refs_.push_back(std::make_shared<code_rva_ref_t>(target_rva, rva_t{ instruction_rva }, disassembled_instruction->size()));

				if (is_in_code_section(*target_rva))
				{
					if (disassembled_instruction->is_conditional_jump())
					{
						const auto fallthrough_rva = add_rva(next_instruction_rva);

						add_to_disassembly_queue(fallthrough_rva);
					}

					add_to_disassembly_queue(target_rva);
				}
			}

			basic_block.push(*this, instruction_t{ instruction_bytes, *disassembled_instruction }, true);

			if (disassembled_instruction->is_jump() || disassembled_instruction->is_ret())
			{
				break;
			}

			instruction_rva.set_value(next_instruction_rva);
		}

		if (basic_block.count())
		{
			find_jump_tables(basic_block);
				
			basic_blocks_.push_back(std::make_shared<basic_block_t>(std::move(basic_block)));
		}
	}

	spdlog::info("basic block count: {}", basic_blocks_.size());
}

void binwrite::binary_t::update_section_rvas(const rva_t disruption_rva,
	const rva_t::size_type disruption_size, const bool inclusive)
{
	for (auto& section : sections_ | std::views::values)
	{
		section.process_disruption(disruption_rva, disruption_size);
	}
}

void binwrite::binary_t::update_rvas(const rva_t disruption_rva, const rva_t::size_type disruption_size,
                                     const bool inclusive, const bool update_sections)
{
	if (update_sections)
	{
		update_section_rvas(disruption_rva, disruption_size, inclusive);
	}

	for (const auto& rva : rvas_)
	{
		rva->process_disruption(disruption_rva, disruption_size, inclusive);
	}

	for (const auto& rva_ref : rva_refs_)
	{
		rva_ref->process_disruption(disruption_rva, disruption_size);
	}
}

bool binwrite::binary_t::split_basic_block(const std::shared_ptr<basic_block_t>& basic_block, const basic_block_t::size_type index)
{
	if (index == 0)
	{
		return true;
	}

	const auto split_rva = basic_block->instruction_rva(index);

	const basic_block_t::size_type split_count = basic_block->count() - index;

	const auto original_block_instructions = basic_block->instructions();

	const auto start = original_block_instructions.begin() + index;
	const auto end = start + split_count;

	const std::vector new_block_instructions(start, end);
	const auto offset_rva = add_rva(split_rva);

	basic_block->erase(*this, index, split_count, false);

	const auto new_basic_block = std::make_shared<basic_block_t>(offset_rva);

	for (const auto& instruction : new_block_instructions)
	{
		new_basic_block->push(*this, instruction, true);
	}

	basic_blocks_.push_back(new_basic_block);

	return true;
}

std::optional<std::shared_ptr<binwrite::basic_block_t>> binwrite::binary_t::is_inside_basic_block(const rva_t rva) const
{
	for (const auto& basic_block : basic_blocks_)
	{
		if (basic_block->contains(rva))
		{
			return basic_block;
		}
	}

	return std::nullopt;
}

bool binwrite::binary_t::is_inside_disassembly_queue(const rva_t rva) const
{
	for (const auto& queued_rva : disassembly_queue_)
	{
		if (*queued_rva == rva)
		{
			return true;
		}
	}

	return false;
}

bool binwrite::binary_t::is_in_code_section(const rva_t rva)
{
	for (const auto& section : sections_ | std::views::values)
	{
		if (section.code() && section.contains(rva))
		{
			return true;
		}
	}

	return false;
}

void binwrite::binary_t::add_to_disassembly_queue(const std::shared_ptr<rva_t>& rva)
{
	if (!is_inside_disassembly_queue(*rva) && !is_inside_basic_block(*rva))
	{
		disassembly_queue_.push_back(rva);
	}
}

std::shared_ptr<binwrite::rva_t> binwrite::binary_t::add_rva(const rva_t::value_type value, const bool force_inclusive)
{
	const auto found = std::ranges::find_if(rvas_,
		[value, force_inclusive](const std::shared_ptr<rva_t>& rva) -> bool
		{
			return rva->value() == value && rva->force_inclusive() == force_inclusive;
		}
	);

	if (found != rvas_.end())
	{
		return *found;
	}

	const auto rva = std::make_shared<rva_t>(value, force_inclusive);

	rvas_.push_back(rva);

	return rva;
}

std::shared_ptr<binwrite::rva_t> binwrite::binary_t::add_rva(const rva_t rva, const bool force_inclusive)
{
	return add_rva(rva.value(), force_inclusive);
}

std::shared_ptr<binwrite::rva_t> binwrite::binary_t::add_relocation_rva(const rva_t::value_type target)
{
	return add_rva(target, true);
}

std::shared_ptr<binwrite::rva_t> binwrite::binary_t::add_relocation_rva(const rva_t target)
{
	return add_relocation_rva(target.value());
}

void binwrite::binary_t::add_llvm_jmp_table_ref(const rva_t table_base)
{
	const auto table_base_rva = add_rva(table_base);

	rva_t table_entry = table_base;

	while (true)
	{
		const llvm_jmp_table_entry_t::value_type offset = *reinterpret_cast<const llvm_jmp_table_entry_t::value_type*>(data() + table_entry.value());

		const auto target_rva = add_rva(table_base.value() + offset);

		if (!is_in_code_section(*target_rva))
		{
			break;
		}

		const auto ref = std::make_shared<llvm_jmp_table_entry_t>(target_rva, table_entry, table_base_rva);

		add_to_disassembly_queue(target_rva);
		rva_refs_.push_back(ref);

		table_entry.set_value(table_entry.value() + sizeof(llvm_jmp_table_entry_t::size_type));
	}
}

void binwrite::binary_t::add_msvc_jmp_table_ref(const rva_t table_base)
{
	const auto table_base_rva = add_rva(table_base);

	rva_t table_entry = table_base;

	while (true)
	{
		const auto entry = reinterpret_cast<const rva_t::value_type*>(data() + table_entry.value());

		if (!is_in_code_section(rva_t{ *entry }))
		{
			break;
		}

		const auto ref = add_data_rva_ref(entry);

		add_to_disassembly_queue(ref->target());

		table_entry.set_value(table_entry.value() + sizeof(llvm_jmp_table_entry_t::size_type));
	}
}
