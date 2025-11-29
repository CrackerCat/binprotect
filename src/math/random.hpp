#pragma once
#include <random>

namespace binprotect::math
{
	template <class T>
	T random_integral(const T min, const T max)
	{
		std::random_device random_device = { };
		std::mt19937_64 mersenne(random_device());

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
}
