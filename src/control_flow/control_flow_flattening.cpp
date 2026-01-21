#include "control_flow_flattening.hpp"
#include "../assembler/assembler.hpp"

#include <binwrite/math/random.hpp>
#include <spdlog/spdlog.h>

struct cff_block_t
{
	std::shared_ptr<binwrite::basic_block_t> basic_block;

	std::shared_ptr<binwrite::basic_block_t> fallthrough_block;
	std::shared_ptr<binwrite::basic_block_t> target_block;

	std::uint32_t id = 0;
};

static std::vector<cff_block_t> collect_cff_blocks(const binwrite::binary_t& binary, binwrite::function_t& function)
{
	std::vector<cff_block_t> cff_blocks;

	for (const auto& basic_block : function.basic_blocks())
	{
		std::uint32_t id;

		do
		{
			id = binwrite::math::random_integral<std::uint16_t>();
		} while (std::ranges::any_of(cff_blocks, [id](const cff_block_t& cff_block) { return cff_block.id == id; }));

		const auto fallthrough_block = function.fallthrough_block(basic_block);
		const auto target_block = function.target_block(binary, basic_block);

		cff_blocks.emplace_back(basic_block, fallthrough_block, target_block, id);
	}

	return cff_blocks;
}

static std::vector<cff_block_t>::iterator find_cff_block(std::vector<cff_block_t>& cff_blocks, const binwrite::rva_t target_rva)
{
	const auto it = std::ranges::find_if(cff_blocks,
		[target_rva](const cff_block_t& cff_block)
		{
			return *cff_block.basic_block->rva() == target_rva;
		}
	);

	return it;
}

static std::vector<binwrite::instruction_t> set_block_id_instructions(const cff_block_t& target_cff_block,
                                                                      const binwrite::register_family_t  id_register_family)
{
	const auto id_operand = encode_unsigned_imm_operand(target_cff_block.id);

	std::vector<binwrite::instruction_t> instructions = { };

	instructions.push_back(pushfq_instruction().value());
	instructions.push_back(push_instruction(id_register_family.qword).value());
	instructions.push_back(mov_instruction(id_operand, id_register_family.dword).value());

	return instructions;
}

static std::shared_ptr<binwrite::rva_t> insert_entry_block_stub(binwrite::binary_t& binary,
	binwrite::function_t& function,
	const std::shared_ptr<binwrite::basic_block_t>& entry_block,
	const binwrite::register_family_t id_register_family,
	std::vector<cff_block_t>& cff_blocks)
{
	const auto entry_cff_block = find_cff_block(cff_blocks, *entry_block->rva());

	if (entry_cff_block == cff_blocks.end())
	{
		return { };
	}

	const auto marker_instruction = nop_instruction().value();

	std::vector<binwrite::instruction_t> set_id_instructions = set_block_id_instructions(*entry_cff_block, id_register_family);

	set_id_instructions.push_back(marker_instruction);

	entry_block->insert(binary, set_id_instructions, 0);

	const auto split_block = binary.split_basic_block(*entry_block,
	                                                  static_cast<binwrite::basic_block_t::size_type>(
		                                                  set_id_instructions.size()));

	function.add_basic_block(split_block);
	entry_cff_block->basic_block = split_block;

	return binary.add_rva(binwrite::rva_t{ entry_block->end_rva().value() - marker_instruction.size() });
}

static void insert_block_jump_stub(binwrite::binary_t& binary,
                                   const std::shared_ptr<binwrite::basic_block_t>& stub_basic_block,
                                   const cff_block_t& cff_block,
                                   const binwrite::register_family_t id_register_family,
                                   const binwrite::basic_block_t::size_type entry_stub_size)
{
	const auto jmp_destination_operand = encode_unsigned_imm_operand(1);
	const auto id_operand = encode_unsigned_imm_operand(cff_block.id);

	const auto jump_instruction = jmp_instruction(jmp_destination_operand).value();
	const auto jump_nz_instruction = jnz_instruction(jmp_destination_operand).value();

	const std::array instructions = {
		cmp_instruction(id_operand, id_register_family.dword).value(),
		jump_nz_instruction,
		pop_instruction(id_register_family.qword).value(),
		popfq_instruction().value(),
		jump_instruction
	};

	stub_basic_block->insert(binary, instructions, entry_stub_size, true);

	const binwrite::rva_t jnz_rva = stub_basic_block->instruction_rva(entry_stub_size + 1);
	const binwrite::rva_t block_jmp_rva = stub_basic_block->instruction_rva(entry_stub_size + 4);

	const binwrite::rva_t next_branch_rva = stub_basic_block->instruction_rva(entry_stub_size + 5);

	binary.add_rva_ref(std::make_shared<binwrite::code_rva_ref_t>(binary.add_rva(next_branch_rva), jnz_rva, jump_nz_instruction.size()));
	binary.add_rva_ref(std::make_shared<binwrite::code_rva_ref_t>(cff_block.basic_block->rva(), block_jmp_rva, jump_instruction.size()));
}

static void insert_fallthrough_block_stub(binwrite::binary_t& binary,
                                          const std::shared_ptr<binwrite::basic_block_t>& basic_block,
                                          const std::shared_ptr<binwrite::rva_t>& stub_insert_rva,
                                          const cff_block_t& fallthrough_cff_block,
                                          const binwrite::register_family_t id_register_family)
{
	const auto jmp_destination_operand = encode_unsigned_imm_operand(1);
	const auto jump_instruction = jmp_instruction(jmp_destination_operand).value();

	const auto set_id_instructions = set_block_id_instructions(fallthrough_cff_block, id_register_family);

	basic_block->push(binary, set_id_instructions, false, true);
	basic_block->push(binary, jump_instruction, false, true);

	const binwrite::rva_t jmp_rva = basic_block->last_instruction_rva();

	binary.add_rva_ref(std::make_shared<binwrite::code_rva_ref_t>(stub_insert_rva, jmp_rva, jump_instruction.size()));
}

static void insert_target_block_stub(binwrite::binary_t& binary,
                                     const std::shared_ptr<binwrite::basic_block_t>& basic_block,
                                     const std::shared_ptr<binwrite::rva_t>& stub_insert_rva,
                                     const cff_block_t& target_cff_block,
                                     const binwrite::register_family_t id_register_family,
                                     const binwrite::rva_t last_instruction_rva)
{
	const binwrite::rva_t end_rva = basic_block->end_rva();

	insert_fallthrough_block_stub(binary, basic_block, stub_insert_rva, target_cff_block, id_register_family);

	binary.redirect_rva_ref(last_instruction_rva, end_rva);
}

static void process_cff_fallthrough_block(binwrite::binary_t& binary,
                                      const cff_block_t& cff_block,
                                      std::vector<cff_block_t>& cff_blocks,
                                      const std::shared_ptr<binwrite::rva_t>& stub_insert_rva,
                                      const binwrite::register_family_t id_register_family)
{
	if (const auto& fallthrough_block = cff_block.fallthrough_block)
	{
		const auto fallthrough_cff_block = find_cff_block(cff_blocks, *fallthrough_block->rva());

		if (fallthrough_cff_block == cff_blocks.end())
		{
			spdlog::warn("couldn't find fallthrough cff block for control flow flattening");

			return;
		}

		insert_fallthrough_block_stub(binary, cff_block.basic_block, stub_insert_rva, *fallthrough_cff_block, id_register_family);
	}
}

static void process_cff_target_block(binwrite::binary_t& binary, const cff_block_t& cff_block,
                                      std::vector<cff_block_t>& cff_blocks,
                                      const std::shared_ptr<binwrite::rva_t>& stub_insert_rva,
                                      const binwrite::register_family_t id_register_family,
                                      const binwrite::rva_t last_instruction_rva)
{
	if (const auto& target_block = cff_block.target_block)
	{
		const auto target_cff_block = find_cff_block(cff_blocks, *target_block->rva());

		if (target_cff_block == cff_blocks.end())
		{
			spdlog::warn("couldn't find target cff block for control flow flattening");

			return;
		}

		insert_target_block_stub(binary, cff_block.basic_block, stub_insert_rva, *target_cff_block, id_register_family,
		                         last_instruction_rva);
	}
}

static void flatten_blocks(binwrite::binary_t& binary, const std::shared_ptr<binwrite::rva_t>& stub_insert_rva,
                           const std::shared_ptr<binwrite::basic_block_t>& stub_basic_block,
                           const binwrite::register_family_t id_register_family,
                           std::vector<cff_block_t>& cff_blocks)
{
	const binwrite::basic_block_t::size_type entry_stub_size = stub_basic_block->count();

	for (const auto& cff_block : cff_blocks)
	{
		const auto& basic_block = cff_block.basic_block;
		const auto last_block_instruction = basic_block->last_instruction();

		basic_block->move_entire(binary, stub_basic_block->end_rva());

		insert_block_jump_stub(binary, stub_basic_block, cff_block, id_register_family, entry_stub_size);

		const auto last_instruction_rva = basic_block->last_instruction_rva();

		process_cff_fallthrough_block(binary, cff_block, cff_blocks, stub_insert_rva, id_register_family);

		process_cff_target_block(binary, cff_block, cff_blocks, stub_insert_rva, id_register_family, last_instruction_rva);
	}
}

void binprotect::control_flow::flattening::do_pass(binwrite::binary_t& binary, binwrite::function_t& function)
{
	if (function.basic_blocks().size() <= 1)
	{
		return;
	}

	std::vector<cff_block_t> cff_blocks = collect_cff_blocks(binary, function);

	const binwrite::register_family_t id_register_family = binwrite::register_family_t::random();

	const auto entry_block = function.entry_block();

	const auto stub_insert_rva = insert_entry_block_stub(binary, function, entry_block, id_register_family, cff_blocks);

	if (!stub_insert_rva)
	{
		spdlog::error("unable to create entry block stub for control flow flattening");

		return;
	}

	binwrite::math::shuffle<cff_block_t>(cff_blocks);

	flatten_blocks(binary, stub_insert_rva, entry_block, id_register_family, cff_blocks);

	entry_block->push(binary, int3_instruction(), false, true);
}
