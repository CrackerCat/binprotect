#pragma once
#include <cstdint>

namespace binwrite::cfh3
{
	inline constexpr std::uint32_t magic_mask = 0x1FFFFFFF;
	inline constexpr std::uint32_t magic_v1 = 0x19930520;
	inline constexpr std::uint32_t magic_v2 = 0x19930521;
	inline constexpr std::uint32_t magic_v3 = 0x19930522;

	[[nodiscard]] inline bool is_fh3_magic(const std::uint32_t word)
	{
		const auto m = word & magic_mask;
		return m == magic_v1 || m == magic_v2 || m == magic_v3;
	}

#pragma pack(push, 4)
	struct func_info_t
	{
		std::uint32_t magic_and_bbt;
		std::int32_t max_state;
		std::int32_t disp_unwind_map;
		std::uint32_t n_try_blocks;
		std::int32_t disp_try_block_map;
		std::uint32_t n_ip_map_entries;
		std::int32_t disp_ip_to_state_map;
		std::int32_t disp_unwind_help;
		std::int32_t disp_es_type_list;
		std::int32_t eh_flags;
	};

	struct unwind_map_entry_t
	{
		std::int32_t to_state;
		std::int32_t action;
	};

	struct try_block_map_entry_t
	{
		std::int32_t try_low;
		std::int32_t try_high;
		std::int32_t catch_high;
		std::int32_t n_catches;
		std::int32_t disp_handler_array;
	};

	struct handler_type_t
	{
		std::uint32_t adjectives;
		std::int32_t disp_type;
		std::int32_t disp_catch_obj;
		std::int32_t disp_of_handler;
		std::int32_t disp_frame;
	};

	struct ip_to_state_entry_t
	{
		std::uint32_t ip;
		std::int32_t state;
	};
#pragma pack(pop)

	static_assert(sizeof(func_info_t) == 40);
	static_assert(sizeof(unwind_map_entry_t) == 8);
	static_assert(sizeof(try_block_map_entry_t) == 20);
	static_assert(sizeof(handler_type_t) == 20);
	static_assert(sizeof(ip_to_state_entry_t) == 8);
}
