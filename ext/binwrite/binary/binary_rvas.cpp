#include "binary.hpp"

#include <spdlog/spdlog.h>
#include <ranges>

void binwrite::binary_t::update_rva_references()
{
	for (const auto& rva_ref : rva_refs_)
	{
		rva_ref->update_reference(*this);
	}

	update_section_headers();

	for (std::uint64_t i = 0; i < rva_refs_.size();)
	{
		const auto& rva_ref = rva_refs_[i];

		const auto result = rva_ref->update_reference(*this);

		if (!result)
		{
			spdlog::warn("rva reference at 0x{:X} had instruction length change", rva_ref->self().value());

			if (result.error() == rva_ref_t::error_t::instruction_length_changed)
			{
				update_section_headers();

				i = 0;

				continue;
			}

			spdlog::error("unable to update rva reference at 0x{:X}", rva_ref->self().value());
		}

		i++;
	}

	update_relocations();
	update_section_headers();
}

void binwrite::binary_t::update_section_rvas(const rva_t disruption_rva, const rva_t::size_type disruption_size)
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

std::shared_ptr<binwrite::rva_t> binwrite::binary_t::add_relocation_rva(const rva_t::value_type target)
{
	return add_rva(target, true);
}

std::shared_ptr<binwrite::rva_t> binwrite::binary_t::add_relocation_rva(const rva_t target)
{
	return add_relocation_rva(target.value());
}
