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

	rva_.process_disruption(disruption_rva, disruption_size, false);
}

void binwrite::section_t::insert(binary_t& binary, const rva_t section_offset, const std::span<const std::uint8_t> data)
{
	if (size_ < section_offset.value())
	{
		return;
	}

	auto& buffer = binary.buffer();

	const rva_t insertion_rva(rva_.value() + section_offset.value());

	buffer.insert(buffer.begin() + insertion_rva.value(), data.begin(), data.end());

	binary.update_rvas(insertion_rva, static_cast<rva_t::size_type>(data.size()), true, false);

	size_ += static_cast<size_type>(data.size());
}

binwrite::rva_t binwrite::section_t::rva() const
{
	return rva_;
}

binwrite::rva_t binwrite::section_t::end_rva() const
{
	return rva_t{ rva_.value() + size_ };
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
	const auto section_start = rva_t{ rva_.value() };
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

std::span<std::shared_ptr<binwrite::function_t>> binwrite::binary_t::functions()
{
	return functions_;
}

std::span<const std::shared_ptr<binwrite::function_t>> binwrite::binary_t::functions() const
{
	return functions_;
}

std::shared_ptr<binwrite::function_t> binwrite::binary_t::create_function(const std::string& name, const rva_t rva)
{
	const auto added_rva = add_rva(rva);
	const auto function = std::make_shared<function_t>(name, added_rva);

	functions_.push_back(function);

	add_to_disassembly_queue(added_rva);

	return function;
}

std::shared_ptr<binwrite::basic_block_t> binwrite::binary_t::create_basic_block(const rva_t rva, const std::span<const instruction_t> instructions)
{
	rva_t instruction_rva = rva;

	for (const auto& instruction : instructions)
	{
		insert(instruction_rva, instruction.bytes(), true);

		instruction_rva.set_value(instruction_rva.value() + instruction.size());
	}

	const auto added_rva = add_rva(rva);
	const auto basic_block = std::make_shared<basic_block_t>(added_rva);

	basic_blocks_.push_back(basic_block);

	for (const auto& instruction : instructions | std::views::reverse)
	{
		basic_block->push(*this, instruction, true);
	}

	return basic_block;
}

std::shared_ptr<binwrite::basic_block_t> binwrite::binary_t::create_basic_block(const rva_t rva)
{
	const auto added_rva = add_rva(rva);
	const auto basic_block = std::make_shared<basic_block_t>(added_rva);

	basic_blocks_.push_back(basic_block);

	return basic_block;
}

std::span<std::shared_ptr<binwrite::basic_block_t>> binwrite::binary_t::basic_blocks()
{
	return basic_blocks_;
}

std::span<const std::shared_ptr<binwrite::basic_block_t>> binwrite::binary_t::basic_blocks() const
{
	return basic_blocks_;
}

std::shared_ptr<binwrite::basic_block_t> binwrite::binary_t::find_basic_block(const rva_t rva) const
{
	for (const auto& basic_block : basic_blocks_)
	{
		if (*basic_block->rva() == rva)
		{
			return basic_block;
		}
	}

	return { };
}

std::shared_ptr<binwrite::basic_block_t> binwrite::binary_t::is_inside_basic_block(const rva_t rva) const
{
	for (const auto& basic_block : basic_blocks_)
	{
		if (basic_block->contains(rva))
		{
			return basic_block;
		}
	}

	return { };
}

std::shared_ptr<binwrite::basic_block_t> binwrite::binary_t::split_basic_block(const std::shared_ptr<basic_block_t>& basic_block, const basic_block_t::size_type index)
{
	if (index == 0)
	{
		return { };
	}

	const auto split_rva = basic_block->instruction_rva(index);

	const basic_block_t::size_type split_count = basic_block->count() - index;

	const auto original_block_instructions = basic_block->instructions();

	const auto start = original_block_instructions.begin() + index;
	const auto end = start + split_count;

	const std::vector new_block_instructions(start, end);
	const auto offset_rva = add_rva(split_rva);

	basic_block->erase(*this, index, split_count, false);

	auto new_basic_block = std::make_shared<basic_block_t>(offset_rva);

	for (const auto& instruction : new_block_instructions)
	{
		new_basic_block->push(*this, instruction, true);
	}

	basic_blocks_.push_back(new_basic_block);

	return new_basic_block;
}

std::vector<std::shared_ptr<binwrite::rva_t>> binwrite::binary_t::rvas()
{
	return rvas_;
}

std::vector<std::shared_ptr<binwrite::rva_ref_t>> binwrite::binary_t::rva_refs()
{
	return rva_refs_;
}

std::unordered_map<std::string, std::shared_ptr<binwrite::section_t>>& binwrite::binary_t::sections()
{
	return sections_;
}

const std::unordered_map<std::string, std::shared_ptr<binwrite::section_t>>& binwrite::binary_t::sections() const
{
	return sections_;
}

std::vector<std::shared_ptr<binwrite::section_t>> binwrite::binary_t::ordered_sections() const
{
	const auto sections_view = sections_ | std::views::values;

	std::vector ordered_sections(sections_view.begin(), sections_view.end());

	std::ranges::sort(ordered_sections,
		[](const std::shared_ptr<section_t>& left, const std::shared_ptr<section_t>& right)
		{
			return left->rva() < right->rva();
		}
	);

	return ordered_sections;
}

std::shared_ptr<binwrite::section_t> binwrite::binary_t::find_section(const std::string& name) const
{
	const auto it = sections_.find(name);

	if (it == sections_.end())
	{
		return { };
	}

	return it->second;
}

std::shared_ptr<binwrite::section_t> binwrite::binary_t::code_section() const
{
	for (const auto& section : sections_ | std::views::values)
	{
		if (section->code())
		{
			return section;
		}
	}

	return { };
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
				//spdlog::warn("rva reference at 0x{:X} had instruction length change", rva_ref->self().value());

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

		if (!latest_lea || !root_instruction.is_mov())
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

				add_rva_ref(std::make_shared<msvc_jmp_table_ref_t>(table_base, root_instruction_rva, root_instruction.size()));
			}
			else if (const auto table_base = resolve_instruction_rva(*latest_lea, latest_lea_rva))
			{
				add_llvm_jmp_table_ref(rva_t{ *table_base });
			}
		}
	}
}

static void process_function_basic_block(const binwrite::binary_t& binary,
                                         const std::shared_ptr<binwrite::function_t>& function,
                                         const std::shared_ptr<binwrite::basic_block_t>& current_block);

static void process_basic_block_target_branch(const binwrite::binary_t& binary,
                                       const std::shared_ptr<binwrite::function_t>& function,
                                       const std::shared_ptr<binwrite::basic_block_t>& current_block,
                                       const binwrite::disassembled_instruction_t& last_instruction_disassembly)
{
	const auto last_instruction_rva = current_block->last_instruction_rva();

	const auto code_rva_ref = binary.find_rva_ref(last_instruction_rva);

	if (!code_rva_ref)
	{
		spdlog::error("unable to find code rva ref of conditional jump");

		return;
	}

	const auto target_rva = *code_rva_ref->target();

	const auto target_basic_block = binary.find_basic_block(target_rva);

	if (!target_basic_block)
	{
		spdlog::error("unable to find target basic block");

		return;
	}

	if (!function->find_basic_block(target_rva))
	{
		function->add_basic_block(target_basic_block);

		process_function_basic_block(binary, function, target_basic_block);
	}
}

static void process_basic_block_fallthrough_branch(const binwrite::binary_t& binary,
                                            const std::shared_ptr<binwrite::function_t>& function,
                                            const std::shared_ptr<binwrite::basic_block_t>& current_block)
{
	const auto fallthrough_rva = current_block->end_rva();

	const auto fallthrough_basic_block = binary.find_basic_block(fallthrough_rva);

	if (!fallthrough_basic_block)
	{
		spdlog::error("unable to find fallthrough basic block");

		return;
	}

	if (!function->find_basic_block(fallthrough_rva))
	{
		function->add_basic_block(fallthrough_basic_block);

		process_function_basic_block(binary, function, fallthrough_basic_block);
	}
}

static void process_function_basic_block(const binwrite::binary_t& binary,
                                  const std::shared_ptr<binwrite::function_t>& function,
                                  const std::shared_ptr<binwrite::basic_block_t>& current_block)
{
	const auto& last_instruction = current_block->last_instruction();
	const auto& last_instruction_disassembly = last_instruction.disassemble();

	if (last_instruction_disassembly.is_ret() || last_instruction_disassembly.is_unconditional_jump())
	{
		return;
	}

	if (last_instruction_disassembly.is_conditional_jump())
	{
		process_basic_block_target_branch(binary, function, current_block, last_instruction_disassembly);
	}

	process_basic_block_fallthrough_branch(binary, function, current_block);
}

void binwrite::binary_t::assign_function_basic_blocks() const
{
	for (const auto& function : functions_)
	{
		const auto basic_block = find_basic_block(*function->rva());

		if (!basic_block)
		{
			return;
		}

		function->add_basic_block(basic_block);

		process_function_basic_block(*this, function, basic_block);

		spdlog::info("{} has {} basic block(s)", function->name(), function->basic_blocks().size());
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
				const auto index = overstepped_basic_block->instruction_index(rva_t{ instruction_rva });

				split_basic_block(overstepped_basic_block, index);

				break;
			}

			const auto next_instruction_rva = instruction_rva.value() + disassembled_instruction->size();

			const instruction_t::const_value_type instruction_bytes(instruction_address, disassembled_instruction->size());

			if (const auto raw_target_rva = resolve_instruction_rva(*disassembled_instruction, instruction_rva))
			{
				const auto target_rva = add_rva(*raw_target_rva);

				add_rva_ref(std::make_shared<code_rva_ref_t>(target_rva, rva_t{ instruction_rva }, disassembled_instruction->size()));

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

	assign_function_basic_blocks();
}

void binwrite::binary_t::update_section_rvas(const rva_t disruption_rva,
	const rva_t::size_type disruption_size)
{
	for (const auto& section : sections_ | std::views::values)
	{
		section->process_disruption(disruption_rva, disruption_size);
	}
}

void binwrite::binary_t::update_rvas(const rva_t disruption_rva, const rva_t::size_type disruption_size,
                                     const bool inclusive, const bool update_sections)
{
	if (update_sections)
	{
		update_section_rvas(disruption_rva, disruption_size);
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

std::shared_ptr<binwrite::rva_ref_t> binwrite::binary_t::find_rva_ref(const rva_t ref_rva,
	const bool must_be_code_reference) const
{
	const auto found = std::ranges::find_if(rva_refs_,
		[ref_rva, must_be_code_reference](const std::shared_ptr<rva_ref_t>& ref)
		{
			if (must_be_code_reference && !ref->is_code_reference())
			{
				return false;
			}

			return ref->self() == ref_rva;
		}
	);

	if (found == rva_refs_.end())
	{
		return { };
	}

	return *found;
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

void binwrite::binary_t::add_rva_ref(std::shared_ptr<rva_ref_t> ref)
{
	rva_refs_.push_back(std::move(ref));
}

void binwrite::binary_t::redirect_rva_ref(const rva_t self, const rva_t new_target)
{
	const auto added_rva = add_rva(new_target);

	for (const auto& rva_ref : rva_refs_)
	{
		if (rva_ref->self() == self)
		{
			rva_ref->set_target(added_rva);
		}
	}
}

bool binwrite::binary_t::is_in_code_section(const rva_t rva)
{
	for (const auto& section : sections_ | std::views::values)
	{
		if (section->code() && section->contains(rva))
		{
			return true;
		}
	}

	return false;
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

void binwrite::binary_t::add_to_disassembly_queue(const std::shared_ptr<rva_t>& rva)
{
	if (!is_inside_disassembly_queue(*rva) && !find_basic_block(*rva))
	{
		disassembly_queue_.push_back(rva);
	}
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
		add_rva_ref(ref);

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
