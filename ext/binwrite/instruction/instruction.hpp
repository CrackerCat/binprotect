#pragma once
#include <array>
#include <span>

namespace binwrite
{
	class instruction_t
	{
	public:
		using size_type = std::uint8_t;
		using value_type = std::span<std::uint8_t>;
		using const_value_type = std::span<const std::uint8_t>;

		constexpr static size_type max_size = 15;

		explicit instruction_t(const const_value_type bytes, disassembled_instruction_t disassembly)
				:	bytes_({}),
					size_(static_cast<size_type>(bytes.size())),
					disassembly_(std::move(disassembly))
		{
			std::memcpy(bytes_.data(), bytes.data(), bytes.size());
		}

		explicit instruction_t(const const_value_type bytes)
				:	bytes_({}),
					size_(static_cast<size_type>(bytes.size())),
					disassembly_(disassembled_instruction_t())
		{
			disassembler_t diassembler;

			disassembly_ = *diassembler.disassemble(bytes.data());

			std::memcpy(bytes_.data(), bytes.data(), bytes.size());
		}

		[[nodiscard]] value_type bytes()
		{
			return { bytes_.begin(), size_ };
		}

		[[nodiscard]] const_value_type bytes() const
		{
			return { bytes_.begin(), size_ };
		}

		[[nodiscard]] size_type size() const
		{
			return size_;
		}

		[[nodiscard]] disassembled_instruction_t& disassemble()
		{
			return disassembly_;
		}

		[[nodiscard]] const disassembled_instruction_t& disassemble() const
		{
			return disassembly_;
		}

	protected:
		std::array<std::uint8_t, max_size> bytes_;
		size_type size_;

		disassembled_instruction_t disassembly_;
	};
}
