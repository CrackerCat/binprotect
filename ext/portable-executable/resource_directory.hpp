#pragma once

#include <cstdint>
#include <string>

namespace portable_executable
{
	struct resource_directory_t;

	enum class resource_id_t : std::uint16_t
    {
        cursor = 1,
        bitmap = 2,
        icon = 3,
        menu = 4,
        dialog = 5,
        string = 6,
        font_directory = 7,
        font = 8,
        accelerator = 9,
        rc_data = 10,
        message_table = 11,
        version = 16,
        dlg_include = 17,
        plug_and_play = 19,
        vxd = 20,
        animated_cursor = 21,
        animated_icon = 22,
        html = 23,
        manifest = 24
    };

    struct resource_directory_string_t
    {
        std::uint16_t length;
        char name[1];

        [[nodiscard]] std::string to_str() const
        {
            return { this->name, this->name + length };
        }
    };

    struct resource_directory_wide_string_t
    {
        std::uint16_t length;
        wchar_t name[1];

        [[nodiscard]] std::wstring to_str() const
        {
            return { this->name, this->name + length };
        }
    };

    struct resource_data_entry_t
    {
        std::uint32_t data_rva;
        std::uint32_t size;
        std::uint32_t code_page;
        std::uint32_t reserved;
    };

    struct resource_directory_entry_t
    {
        union
        {
	        struct
	        {
                std::uint32_t name_offset : 31;
                std::uint32_t is_name_ : 1;
	        };

            resource_id_t id;
        };

        union
        {
            struct
            {
                std::uint32_t directory_offset : 31;
                std::uint32_t is_directory_ : 1;
            };

            std::uint32_t data_offset;
        };

        [[nodiscard]] bool is_name() const
        {
            return is_name_;
        }

        [[nodiscard]] bool is_id() const
        {
            return !is_name_;
        }

        [[nodiscard]] bool is_directory() const
        {
            return is_directory_;
        }

        [[nodiscard]] bool is_data() const
        {
            return !is_directory_;
        }

        [[nodiscard]] resource_directory_string_t* as_name(resource_directory_t* const directory)
        {
            return reinterpret_cast<resource_directory_string_t*>(reinterpret_cast<std::uint8_t*>(directory) + name_offset);
        }

        [[nodiscard]] const resource_directory_string_t* as_name(const resource_directory_t* const directory) const
        {
            return reinterpret_cast<const resource_directory_string_t*>(reinterpret_cast<const std::uint8_t*>(directory) + name_offset);
        }

        [[nodiscard]] resource_data_entry_t* as_data(resource_directory_t* const directory)
        {
            return reinterpret_cast<resource_data_entry_t*>(reinterpret_cast<std::uint8_t*>(directory) + directory_offset);
        }

        [[nodiscard]] const resource_data_entry_t* as_data(const resource_directory_t* const directory) const
        {
            return reinterpret_cast<const resource_data_entry_t*>(reinterpret_cast<const std::uint8_t*>(directory) + directory_offset);
        }

        [[nodiscard]] resource_directory_t* as_directory(resource_directory_t* const directory)
        {
            return reinterpret_cast<resource_directory_t*>(reinterpret_cast<std::uint8_t*>(directory) + directory_offset);
        }

        [[nodiscard]] const resource_directory_t* as_directory(const resource_directory_t* const directory) const
        {
            return reinterpret_cast<const resource_directory_t*>(reinterpret_cast<const std::uint8_t*>(directory) + directory_offset);
        }
    };

    struct resource_directory_t
    {
        using size_type = std::uint64_t;

        std::uint32_t characteristics;
        std::uint32_t time_date_stamp;
        std::uint16_t major_version;
        std::uint16_t minor_version;
        std::uint16_t named_entry_count;
        std::uint16_t id_entry_count;
        resource_directory_entry_t entries[1];

        [[nodiscard]] resource_directory_entry_t* begin()
        {
            return entries;
        }

        [[nodiscard]] resource_directory_entry_t* end()
        {
            return entries + entry_count();
        }

        [[nodiscard]] const resource_directory_entry_t* begin() const
        {
            return entries;
        }

        [[nodiscard]] const resource_directory_entry_t* end() const
        {
            return entries + entry_count();
        }

        [[nodiscard]] size_type entry_count() const
        {
            return named_entry_count + id_entry_count;
        }
    };
}