#pragma once
#include <algorithm>
#include <random>
#include <ranges>
#include <span>

namespace binwrite::util
{
	inline std::mt19937_64& make_mersenne()
	{
		thread_local std::random_device random_device = { };
		thread_local std::mt19937_64 mersenne(random_device());

		return mersenne;
	}

	template <class T>
	T random_integral(const T min, const T max)
	{
		auto& mersenne = make_mersenne();

		std::uniform_int_distribution<T> distribution(min, max);

		return distribution(mersenne);
	}

	template <class T>
	T random_integral()
	{
		const T min = std::numeric_limits<T>::min();
		const T max = std::numeric_limits<T>::max();

		return random_integral(min, max);
	}

	inline bool random_bool()
	{
		return random_integral(0, 1) == 1;
	}

	template <class T>
	T& random_entry(const std::span<T> list)
	{
		assert(!list.empty());

		const std::uint64_t size = list.size();
		const std::uint64_t index = random_integral<std::uint64_t>(0, size - 1);

		return list[index];
	}

	template <class T>
	const T& random_entry(const std::span<const T> list)
	{
		assert(!list.empty());

		const std::uint64_t size = list.size();
		const std::uint64_t index = random_integral<std::uint64_t>(0, size - 1);

		return list[index];
	}

	template <class T>
	void shuffle(const std::span<T> list)
	{
		auto& mersenne = make_mersenne();

		std::ranges::shuffle(list, mersenne);
	}
}
