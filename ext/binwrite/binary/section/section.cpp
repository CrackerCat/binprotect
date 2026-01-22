#include "section.hpp"
#include "../binary.hpp"

void binwrite::section_t::process_disruption(const rva_t disruption_rva, const rva_t::size_type disruption_size)
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

	buffer.insert_range(buffer.begin() + insertion_rva.value(), data);

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

binwrite::section_t::size_type binwrite::section_t::padding() const
{
	return padding_;
}

void binwrite::section_t::set_padding(const size_type padding)
{
	padding_ = padding;
}

void binwrite::section_t::remove_padding(const size_type size)
{
	padding_ -= size;
}

bool binwrite::section_t::contains(const rva_t rva) const
{
	const auto section_start = rva_;
	const auto section_end = end_rva();

	return section_start <= rva && rva < section_end;
}

bool binwrite::section_t::code() const
{
	return code_;
}
