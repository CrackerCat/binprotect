#pragma once
#include <span>

namespace binwrite::util
{
	template <class T>
	std::span<const std::uint8_t> serialize_bytes(const T& info)
	{
		const auto start = reinterpret_cast<const std::uint8_t*>(&info);

		return { start, start + sizeof(T) };
	}
}
