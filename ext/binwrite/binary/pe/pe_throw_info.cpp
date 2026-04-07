#include "pe_exceptions.hpp"
#include "../../disassembler/disassembler.hpp"

#include <spdlog/spdlog.h>

struct msvc_throw_info_t
{
	std::uint32_t attributes;
	std::int32_t pmfn_unwind;
	std::int32_t forward_compat;
	std::int32_t catchable_type_array;
};

struct catchable_type_array_t
{
	std::uint32_t count;
	std::uint32_t type_rvas[1];
};

struct catchable_type_t
{
	std::uint32_t attributes;
	std::uint32_t rva_type;
	std::uint32_t mdisp;
	std::uint32_t pdisp;
	std::uint32_t vdisp;
	std::uint32_t size_of_thrown_object;
	std::uint32_t optional_copy_constructor_rva;
};

static void register_throw_info_refs(binwrite::portable_executable_t& pe,
                                     const binwrite::rva_t::value_type throw_info_rva)
{
	const auto throw_info = reinterpret_cast<const msvc_throw_info_t*>(pe.data() + throw_info_rva);

	if (throw_info->pmfn_unwind)
	{
		pe.add_data_rva_ref(&throw_info->pmfn_unwind);
	}

	if (throw_info->forward_compat)
	{
		pe.add_data_rva_ref(&throw_info->forward_compat);
	}

	if (!throw_info->catchable_type_array)
	{
		return;
	}

	pe.add_data_rva_ref(&throw_info->catchable_type_array);

	const auto array = reinterpret_cast<const catchable_type_array_t*>(
		pe.data() + throw_info->catchable_type_array);

	for (std::uint32_t j = 0; j < array->count; j++)
	{
		const auto type_rva = &array->type_rvas[j];

		pe.add_data_rva_ref(type_rva);

		const auto type = reinterpret_cast<const catchable_type_t*>(pe.data() + *type_rva);

		if (type->rva_type)
		{
			pe.add_data_rva_ref(&type->rva_type);
		}

		if (type->optional_copy_constructor_rva)
		{
			pe.add_data_rva_ref(&type->optional_copy_constructor_rva);
		}
	}
}

void binwrite::process_throw_info(portable_executable_t& pe)
{
	std::shared_ptr<function_t> cxx_throw_exception;

	for (const auto& function : pe.functions())
	{
		if (function->name().contains("CxxThrowException"))
		{
			cxx_throw_exception = function;
			break;
		}
	}

	if (!cxx_throw_exception)
	{
		return;
	}

	const auto& refs = pe.find_all_targetted_rva_refs(*cxx_throw_exception->rva());

	for (const auto& ref : refs)
	{
		if (!ref->is_code_reference())
		{
			continue;
		}

		const auto& holding_block = pe.find_containing_basic_block(ref->self());
		const auto instruction_index = holding_block->instruction_index(ref->self());

		if (instruction_index == 0)
		{
			continue;
		}

		const auto& call_instruction = holding_block->at(instruction_index);

		if (!call_instruction.disassemble().is_call())
		{
			continue;
		}

		for (std::int64_t i = instruction_index; i != 0; i--)
		{
			const auto& instruction = holding_block->at(i - 1);
			const auto& disassembly = instruction.disassemble();

			if (disassembly.writes_register_family(register_family_t::dx))
			{
				const auto instruction_rva = holding_block->instruction_rva(i - 1);

				if (const auto throw_info_rva = resolve_instruction_rva(disassembly, instruction_rva);
					throw_info_rva && disassembly.is_lea())
				{
					register_throw_info_refs(pe, *throw_info_rva);
				}

				break;
			}
		}
	}
}
