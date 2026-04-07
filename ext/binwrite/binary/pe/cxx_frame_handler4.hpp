#pragma once
#include <cstdint>
#include <cstring>
#include <vector>

namespace binwrite::cfh4
{
	using ehstate_t = std::int32_t;

	inline constexpr std::int8_t neg_length_table[16] =
	{
		-1, -2, -1, -3,
		-1, -2, -1, -4,
		-1, -2, -1, -3,
		-1, -2, -1, -5,
	};

	inline constexpr std::uint8_t shift_table[16] =
	{
		32 - 7 * 1, 32 - 7 * 2, 32 - 7 * 1, 32 - 7 * 3,
		32 - 7 * 1, 32 - 7 * 2, 32 - 7 * 1, 32 - 7 * 4,
		32 - 7 * 1, 32 - 7 * 2, 32 - 7 * 1, 32 - 7 * 3,
		32 - 7 * 1, 32 - 7 * 2, 32 - 7 * 1, 0,
	};

	[[nodiscard]] inline std::int32_t read_int32(std::uint8_t** buffer)
	{
		std::int32_t value;
		std::memcpy(&value, *buffer, sizeof(std::int32_t));
		*buffer += sizeof(std::int32_t);
		return value;
	}

	[[nodiscard]] inline std::uint32_t read_unsigned(std::uint8_t** buffer)
	{
		const std::uint32_t length_bits = **buffer & 0x0F;
		const auto neg_length = neg_length_table[length_bits];
		const std::uint32_t shift = shift_table[length_bits];

		std::uint32_t result;
		std::memcpy(&result, *buffer - neg_length - 4, sizeof(std::uint32_t));
		result >>= shift;
		*buffer -= neg_length;

		return result;
	}

	[[nodiscard]] inline std::uint32_t encode_unsigned(std::uint8_t* buffer, std::uint32_t value)
	{
		std::uint32_t i = 0;

		if (value < 128)
		{
			buffer[i++] = static_cast<std::uint8_t>((value << 1) + 0);
		}
		else if (value < 128 * 128)
		{
			buffer[i++] = static_cast<std::uint8_t>((value << 2) + 1);
			buffer[i++] = static_cast<std::uint8_t>(value >> 6);
		}
		else if (value < 128 * 128 * 128)
		{
			buffer[i++] = static_cast<std::uint8_t>((value << 3) + 3);
			buffer[i++] = static_cast<std::uint8_t>(value >> 5);
			buffer[i++] = static_cast<std::uint8_t>(value >> 13);
		}
		else if (value < 128u * 128 * 128 * 128)
		{
			buffer[i++] = static_cast<std::uint8_t>((value << 4) + 7);
			buffer[i++] = static_cast<std::uint8_t>(value >> 4);
			buffer[i++] = static_cast<std::uint8_t>(value >> 12);
			buffer[i++] = static_cast<std::uint8_t>(value >> 20);
		}
		else
		{
			buffer[i++] = 15;
			buffer[i++] = static_cast<std::uint8_t>(value);
			buffer[i++] = static_cast<std::uint8_t>(value >> 8);
			buffer[i++] = static_cast<std::uint8_t>(value >> 16);
			buffer[i++] = static_cast<std::uint8_t>(value >> 24);
		}

		return i;
	}

	[[nodiscard]] inline std::uint8_t* image_rel_to_buffer(std::uintptr_t image_base, std::int32_t disp)
	{
		return reinterpret_cast<std::uint8_t*>(image_base + disp);
	}

	// reference to a field in the binary that holds an rva
	struct field_ref_t
	{
		std::uint8_t* ptr = nullptr;
		std::uint32_t size = 0;
	};

	struct func_info_header_t
	{
		union
		{
#pragma warning(push)
#pragma warning(disable: 4201)
			struct
			{
				std::uint8_t is_catch : 1;
				std::uint8_t is_separated : 1;
				std::uint8_t bbt : 1;
				std::uint8_t has_unwind_map : 1;
				std::uint8_t has_try_block_map : 1;
				std::uint8_t ehs : 1;
				std::uint8_t no_except : 1;
				std::uint8_t reserved : 1;
			};
#pragma warning(pop)
			std::uint8_t value = 0;
		};
	};

	static_assert(sizeof(func_info_header_t) == 1);

	struct func_info4_t
	{
		func_info_header_t header;
		std::uint32_t bbt_flags = 0;
		std::int32_t disp_unwind_map = 0;
		std::int32_t disp_try_block_map = 0;
		std::int32_t disp_ip_to_state_map = 0;
		std::uint32_t disp_frame = 0;

		field_ref_t unwind_map_ref;
		field_ref_t try_block_map_ref;
		field_ref_t ip_to_state_map_ref;
		field_ref_t disp_frame_ref;
	};

	[[nodiscard]] inline std::ptrdiff_t decompress_func_info(
		std::uint8_t* buffer,
		func_info4_t& out,
		std::uintptr_t image_base,
		std::int32_t image_size,
		std::int32_t function_start)
	{
		std::uint8_t* start = buffer;
		out.header.value = buffer[0];
		++buffer;

		if (out.header.bbt)
		{
			out.bbt_flags = read_unsigned(&buffer);
		}

		if (out.header.has_unwind_map)
		{
			out.unwind_map_ref = { buffer, sizeof(std::int32_t) };
			out.disp_unwind_map = read_int32(&buffer);

			if (image_size < out.disp_unwind_map || out.disp_unwind_map <= 0)
			{
				return -1;
			}
		}

		if (out.header.has_try_block_map)
		{
			out.try_block_map_ref = { buffer, sizeof(std::int32_t) };
			out.disp_try_block_map = read_int32(&buffer);

			if (image_size < out.disp_try_block_map || out.disp_try_block_map <= 0)
			{
				return -1;
			}
		}

		if (out.header.is_separated)
		{
			out.ip_to_state_map_ref = { buffer, sizeof(std::int32_t) };
			std::int32_t disp_to_seg_map = read_int32(&buffer);

			if (image_size < disp_to_seg_map || disp_to_seg_map <= 0)
			{
				return -1;
			}

			std::uint8_t* seg_buffer = image_rel_to_buffer(image_base, disp_to_seg_map);
			std::uint32_t num_seg_entries = read_unsigned(&seg_buffer);

			out.disp_ip_to_state_map = 0;
			for (std::uint32_t i = 0; i < num_seg_entries; i++)
			{
				std::int32_t seg_rva = read_int32(&seg_buffer);
				std::int32_t disp_seg_table = read_int32(&seg_buffer);

				if (seg_rva == function_start)
				{
					out.disp_ip_to_state_map = disp_seg_table;
					break;
				}
			}
		}
		else
		{
			out.ip_to_state_map_ref = { buffer, sizeof(std::int32_t) };
			out.disp_ip_to_state_map = read_int32(&buffer);

			if (image_size < out.disp_ip_to_state_map || out.disp_ip_to_state_map <= 0)
			{
				return -1;
			}
		}

		if (out.header.is_catch)
		{
			out.disp_frame_ref = { buffer, 0 };
			out.disp_frame = read_unsigned(&buffer);
			out.disp_frame_ref.size = static_cast<std::uint32_t>(buffer - out.disp_frame_ref.ptr);

			if (image_size < static_cast<std::int32_t>(out.disp_frame))
			{
				return -1;
			}
		}

		return buffer - start;
	}

	struct unwind_map_entry4_t
	{
		enum class type_t : std::uint32_t
		{
			no_unwind = 0b00,
			dtor_with_obj = 0b01,
			dtor_with_ptr_to_obj = 0b10,
			rva = 0b11
		};

		std::uint32_t next_offset = 0;
		type_t type = type_t::no_unwind;
		std::int32_t action = 0;
		std::uint32_t object = 0;

		field_ref_t action_ref;
	};

	inline constexpr std::uint32_t max_cont_addresses = 2;

	struct handler_type_header_t
	{
		enum class cont_type_t : std::uint8_t
		{
			none = 0b00,
			one = 0b01,
			two = 0b10,
			reserved = 0b11
		};

		union
		{
#pragma warning(push)
#pragma warning(disable: 4201)
			struct
			{
				std::uint8_t adjectives : 1;
				std::uint8_t disp_type : 1;
				std::uint8_t disp_catch_obj : 1;
				std::uint8_t cont_is_rva : 1;
				std::uint8_t cont_addr : 2;
				std::uint8_t unused : 2;
			};
#pragma warning(pop)
			std::uint8_t value = 0;
		};

		[[nodiscard]] cont_type_t cont_type() const
		{
			return static_cast<cont_type_t>(cont_addr);
		}
	};

	static_assert(sizeof(handler_type_header_t) == 1);

	struct handler_type4_t
	{
		handler_type_header_t header;
		std::uint32_t adjectives = 0;
		std::int32_t disp_type = 0;
		std::uint32_t disp_catch_obj = 0;
		std::int32_t disp_of_handler = 0;
		std::uintptr_t continuation_address[max_cont_addresses] = {};

		field_ref_t disp_type_ref;
		field_ref_t disp_of_handler_ref;
		field_ref_t cont_ref[max_cont_addresses];
	};

	struct try_block_map_entry4_t
	{
		ehstate_t try_low = 0;
		ehstate_t try_high = 0;
		ehstate_t catch_high = 0;
		std::int32_t disp_handler_array = 0;

		field_ref_t disp_handler_array_ref;
	};

	struct ip_to_state_entry4_t
	{
		std::int32_t ip = 0;
		ehstate_t state = 0;

		field_ref_t ip_ref;
	};

	class unwind_map4_t
	{
	public:
		unwind_map4_t(const func_info4_t* func_info, std::uintptr_t image_base)
		{
			if (func_info->disp_unwind_map != 0)
			{
				std::uint8_t* buffer = image_rel_to_buffer(image_base, func_info->disp_unwind_map);
				num_entries_ = read_unsigned(&buffer);
				buffer_start_ = buffer;
			}
		}

		class iterator
		{
		public:
			iterator(unwind_map4_t& map, std::uint8_t* pos)
				:	map_(map), pos_(pos) {}

			iterator& operator++()
			{
				map_.read_entry(&pos_);
				return *this;
			}

			[[nodiscard]] unwind_map_entry4_t operator*()
			{
				std::uint8_t* saved = pos_;
				map_.read_entry(&pos_);
				pos_ = saved;
				return map_.current_;
			}

			[[nodiscard]] bool operator!=(const iterator& other) const { return pos_ != other.pos_; }

		private:
			unwind_map4_t& map_;
			std::uint8_t* pos_;
		};

		[[nodiscard]] iterator begin() { return iterator(*this, buffer_start_); }

		[[nodiscard]] iterator end()
		{
			std::uint8_t* pos = buffer_start_;
			for (std::uint32_t i = 0; i < num_entries_; i++)
			{
				read_entry(&pos);
			}
			return iterator(*this, pos);
		}

		[[nodiscard]] std::uint32_t count() const { return num_entries_; }

	private:
		std::uint32_t num_entries_ = 0;
		std::uint8_t* buffer_start_ = nullptr;
		unwind_map_entry4_t current_;

		void read_entry(std::uint8_t** pos)
		{
			current_.next_offset = read_unsigned(pos);
			current_.type = static_cast<unwind_map_entry4_t::type_t>(current_.next_offset & 0b11);
			current_.next_offset >>= 2;

			current_.action_ref = { *pos, sizeof(std::int32_t) };
			current_.action = 0;

			if (current_.type == unwind_map_entry4_t::type_t::dtor_with_obj ||
				current_.type == unwind_map_entry4_t::type_t::dtor_with_ptr_to_obj)
			{
				current_.action = read_int32(pos);
				current_.object = read_unsigned(pos);
			}
			else if (current_.type == unwind_map_entry4_t::type_t::rva)
			{
				current_.action = read_int32(pos);
			}
		}
	};

	class try_block_map4_t
	{
	public:
		try_block_map4_t(const func_info4_t* func_info, std::uintptr_t image_base)
		{
			if (func_info->disp_try_block_map != 0)
			{
				buffer_ = image_rel_to_buffer(image_base, func_info->disp_try_block_map);
				num_entries_ = read_unsigned(&buffer_);
				buffer_start_ = buffer_;
				read_entry();
			}
		}

		class iterator
		{
		public:
			iterator(try_block_map4_t& map, std::uint32_t index)
				:	map_(map), index_(index) {}

			iterator& operator++()
			{
				map_.read_entry();
				index_++;
				return *this;
			}

			[[nodiscard]] try_block_map_entry4_t operator*() { return map_.current_; }
			[[nodiscard]] bool operator!=(const iterator& other) const { return index_ != other.index_; }

		private:
			try_block_map4_t& map_;
			std::uint32_t index_;
		};

		[[nodiscard]] iterator begin() { return iterator(*this, 0); }
		[[nodiscard]] iterator end() { return iterator(*this, num_entries_); }
		[[nodiscard]] std::uint32_t count() const { return num_entries_; }

	private:
		std::uint32_t num_entries_ = 0;
		std::uint8_t* buffer_ = nullptr;
		std::uint8_t* buffer_start_ = nullptr;
		try_block_map_entry4_t current_;

		void read_entry()
		{
			current_.try_low = read_unsigned(&buffer_);
			current_.try_high = read_unsigned(&buffer_);
			current_.catch_high = read_unsigned(&buffer_);

			current_.disp_handler_array_ref = { buffer_, sizeof(std::int32_t) };
			current_.disp_handler_array = read_int32(&buffer_);
		}
	};

	class handler_map4_t
	{
	public:
		handler_map4_t(const try_block_map_entry4_t* try_block, std::uintptr_t image_base, std::int32_t function_start)
			:	image_base_(image_base), function_start_(function_start)
		{
			if (try_block->disp_handler_array != 0)
			{
				buffer_ = image_rel_to_buffer(image_base_, try_block->disp_handler_array);
				num_entries_ = read_unsigned(&buffer_);
				buffer_start_ = buffer_;
				read_entry();
			}
		}

		class iterator
		{
		public:
			iterator(handler_map4_t& map, std::uint32_t index)
				:	map_(map), index_(index) {}

			iterator& operator++()
			{
				map_.read_entry();
				index_++;
				return *this;
			}

			[[nodiscard]] handler_type4_t operator*() { return map_.current_; }
			[[nodiscard]] bool operator!=(const iterator& other) const { return index_ != other.index_; }

		private:
			handler_map4_t& map_;
			std::uint32_t index_;
		};

		[[nodiscard]] iterator begin() { return iterator(*this, 0); }
		[[nodiscard]] iterator end() { return iterator(*this, num_entries_); }
		[[nodiscard]] std::uint32_t count() const { return num_entries_; }

	private:
		std::uint32_t num_entries_ = 0;
		std::uint8_t* buffer_ = nullptr;
		std::uint8_t* buffer_start_ = nullptr;
		handler_type4_t current_;
		std::uintptr_t image_base_;
		std::int32_t function_start_;

		void read_entry()
		{
			current_ = handler_type4_t{};

			current_.header.value = buffer_[0];
			++buffer_;

			if (current_.header.adjectives)
			{
				current_.adjectives = read_unsigned(&buffer_);
			}

			current_.disp_type_ref = { buffer_, sizeof(std::int32_t) };
			if (current_.header.disp_type)
			{
				current_.disp_type = read_int32(&buffer_);
			}

			if (current_.header.disp_catch_obj)
			{
				current_.disp_catch_obj = read_unsigned(&buffer_);
			}

			current_.disp_of_handler_ref = { buffer_, sizeof(std::int32_t) };
			current_.disp_of_handler = read_int32(&buffer_);

			read_continuation_addresses();
		}

		void read_continuation_addresses()
		{
			const auto cont = current_.header.cont_type();

			if (current_.header.cont_is_rva)
			{
				if (cont == handler_type_header_t::cont_type_t::one)
				{
					current_.cont_ref[0] = { buffer_, sizeof(std::int32_t) };
					current_.continuation_address[0] = read_int32(&buffer_);
				}
				else if (cont == handler_type_header_t::cont_type_t::two)
				{
					current_.cont_ref[0] = { buffer_, sizeof(std::int32_t) };
					current_.continuation_address[0] = read_int32(&buffer_);
					current_.cont_ref[1] = { buffer_, sizeof(std::int32_t) };
					current_.continuation_address[1] = read_int32(&buffer_);
				}
			}
			else
			{
				if (cont == handler_type_header_t::cont_type_t::one)
				{
					std::uint8_t* before = buffer_;
					current_.continuation_address[0] = function_start_ + read_unsigned(&buffer_);
					current_.cont_ref[0] = { before, static_cast<std::uint32_t>(buffer_ - before) };
				}
				else if (cont == handler_type_header_t::cont_type_t::two)
				{
					std::uint8_t* before = buffer_;
					current_.continuation_address[0] = function_start_ + read_unsigned(&buffer_);
					current_.cont_ref[0] = { before, static_cast<std::uint32_t>(buffer_ - before) };

					before = buffer_;
					current_.continuation_address[1] = function_start_ + read_unsigned(&buffer_);
					current_.cont_ref[1] = { before, static_cast<std::uint32_t>(buffer_ - before) };
				}
			}
		}
	};

	class ip_to_state_map4_t
	{
	public:
		ip_to_state_map4_t(const func_info4_t* func_info, std::uintptr_t image_base, std::uint32_t func_start)
			:	func_start_(func_start)
		{
			if (func_info->disp_ip_to_state_map)
			{
				std::uint8_t* buffer = image_rel_to_buffer(image_base, func_info->disp_ip_to_state_map);
				num_entries_ = read_unsigned(&buffer);
				buffer_start_ = buffer;
			}
		}

		class iterator
		{
		public:
			iterator(ip_to_state_map4_t& map, std::uint32_t index, std::uint8_t* pos)
				:	map_(map), index_(index), pos_(pos) {}

			iterator& operator++()
			{
				auto entry = map_.read_entry(&pos_, prev_ip_);
				prev_ip_ = entry.ip - map_.func_start_;
				index_++;
				return *this;
			}

			[[nodiscard]] ip_to_state_entry4_t operator*()
			{
				std::uint8_t* saved = pos_;
				auto entry = map_.read_entry(&pos_, prev_ip_);
				pos_ = saved;
				return entry;
			}

			[[nodiscard]] bool operator!=(const iterator& other) const { return index_ != other.index_; }

		private:
			ip_to_state_map4_t& map_;
			std::uint32_t index_;
			std::uint8_t* pos_;
			std::uint32_t prev_ip_ = 0;
		};

		[[nodiscard]] iterator begin() { return iterator(*this, 0, buffer_start_); }
		[[nodiscard]] iterator end() { return iterator(*this, num_entries_, nullptr); }
		[[nodiscard]] std::uint32_t count() const { return num_entries_; }

	private:
		std::uint32_t num_entries_ = 0;
		std::uint8_t* buffer_start_ = nullptr;
		std::uint32_t func_start_;

		ip_to_state_entry4_t read_entry(std::uint8_t** pos, std::uint32_t prev_ip)
		{
			ip_to_state_entry4_t entry;

			entry.ip_ref.ptr = *pos;
			entry.ip = func_start_ + prev_ip + read_unsigned(pos);
			entry.ip_ref.size = static_cast<std::uint32_t>(*pos - entry.ip_ref.ptr);

			// states are encoded as state+1 to avoid negatives
			entry.state = read_unsigned(pos) - 1;

			return entry;
		}
	};
}
