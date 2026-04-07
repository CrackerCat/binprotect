#pragma once

#include <cstdint>
#include <span>
#include <string>

namespace portable_executable
{
    class unwind_code_iterator_t;

    enum class unwind_register_t : std::uint8_t
    {
        rax = 0,
        rcx,
        rdx,
        rbx,
        rsp,
        rbp,
        rsi,
        rdi,
        r8,
        r9,
        r10,
        r11,
        r12,
        r13,
        r14,
        r15
    };

    enum class unwind_opcode_t : std::uint8_t
    {
        push_non_volatile = 0,
        stack_allocate_large,
        stack_allocate_small,
        set_frame_register,
        save_non_volatile,
        save_non_volatile_far,
        save_xmm128 = 8,
        same_xmm128_far,
        push_machine_frame
    };

    union unwind_code_t
    {
        using size_type = std::uint32_t;

        unwind_code_t() = default;

        unwind_code_t(const std::uint16_t value_)
    			:   value(value_) { }

        unwind_code_t(const std::uint8_t offset_, const unwind_opcode_t opcode_, const std::uint8_t info_)
    			:   offset(offset_), opcode(opcode_), info(info_) { }

        [[nodiscard]] size_type node_count() const
        {
	        switch (opcode)
	        {
            case unwind_opcode_t::push_machine_frame:
            case unwind_opcode_t::set_frame_register:
            case unwind_opcode_t::stack_allocate_small:
	        case unwind_opcode_t::push_non_volatile:
                return 1;
            case unwind_opcode_t::save_non_volatile:
            case unwind_opcode_t::save_xmm128:
                return 2;
            case unwind_opcode_t::save_non_volatile_far:
            case unwind_opcode_t::same_xmm128_far:
                return 3;
            case unwind_opcode_t::stack_allocate_large:
                return info == 0 ? 2 : 3;
	        }

            return -1;
        }

        std::uint16_t value;

        struct
        {
            std::uint8_t offset;
            unwind_opcode_t opcode : 4;
            std::uint8_t info : 4;
        };
    };

    struct unwind_info_t
    {
        std::uint8_t version : 3;
        std::uint8_t flags : 5;
        std::uint8_t size_of_prolog;
        std::uint8_t unwind_code_count;
        unwind_register_t frame_register : 4;
        std::uint8_t frame_offset : 4;
        unwind_code_t codes[1];

        template <class T>
        [[nodiscard]] T* language_specific_data()
        {
            return reinterpret_cast<T*>(&codes[(unwind_code_count + 1) & ~1]);
        }

        template <class T>
        [[nodiscard]] const T* language_specific_data() const
        {
            return const_cast<unwind_info_t*>(this)->language_specific_data<T>();
        }

        // frame_register and frame_offset could both be 0
        [[nodiscard]] bool has_frame_pointer() const;
        [[nodiscard]] bool has_handler() const;
        [[nodiscard]] bool has_chained_function() const;

        [[nodiscard]] std::span<unwind_code_t> unwind_codes();
        [[nodiscard]] std::span<const unwind_code_t> unwind_codes() const;

        [[nodiscard]] unwind_code_iterator_t begin() const;
        [[nodiscard]] unwind_code_iterator_t end() const;
    };

    struct runtime_function_t
    {
        std::uint32_t begin_address;
        std::uint32_t end_address;
        std::uint32_t unwind_info_rva;
    };

    struct runtime_function_descriptor_t
    {
        std::uint32_t function_begin_rva;
        std::uint32_t function_end_rva;

        unwind_info_t* unwind_info;
    };

    class unwind_code_iterator_t
    {
        const unwind_code_t* m_current_code = nullptr;

    public:
        unwind_code_iterator_t() = default;

        unwind_code_iterator_t(const unwind_code_t* unwind_code) :
            m_current_code(unwind_code)
        {
            
        }

        using iterator_category = std::forward_iterator_tag;
        using difference_type = std::ptrdiff_t;
        using value_type = unwind_code_t;
        using pointer = value_type*;
        using reference = value_type&;
        using const_reference = const value_type&;

        reference operator*();
        const_reference operator*() const;

        unwind_code_iterator_t& operator++();

        bool operator==(const unwind_code_iterator_t& other) const;
        bool operator!=(const unwind_code_iterator_t& other) const;
    };

    class runtime_functions_iterator_t
    {
        const std::uint8_t* m_module = nullptr;
        const runtime_function_t* m_current_function = nullptr;

    public:
        runtime_functions_iterator_t() = default;

        runtime_functions_iterator_t(const std::uint8_t* const module, const runtime_function_t* runtime_function) :
            m_module(module),
			m_current_function(runtime_function)
        {

        }

        using iterator_category = std::forward_iterator_tag;
        using difference_type = std::ptrdiff_t;
        using value_type = runtime_function_descriptor_t;
        using pointer = value_type*;
        using reference = value_type&;

        value_type operator*() const;

        runtime_functions_iterator_t& operator++();

        bool operator==(const runtime_functions_iterator_t& other) const;
        bool operator!=(const runtime_functions_iterator_t& other) const;
    };

    template <typename T>
    class runtime_functions_range_t
    {
    private:
        using pointer_type = std::conditional_t<std::is_const_v<T>, const std::uint8_t*, std::uint8_t*>;
        using runtime_function_type = std::conditional_t<std::is_const_v<T>, const runtime_function_t*, runtime_function_t*>;

        pointer_type m_module = nullptr;

        runtime_function_type m_runtime_functions = nullptr;
        runtime_function_type m_end_runtime_functions = nullptr;

    public:
        runtime_functions_range_t() = default;

        runtime_functions_range_t(const pointer_type module, const std::uint32_t exception_directory_rva, const std::uint32_t exception_directory_size) :
            m_module(module),
            m_runtime_functions(reinterpret_cast<runtime_function_type>(module + exception_directory_rva)),
            m_end_runtime_functions(reinterpret_cast<runtime_function_type>(module + exception_directory_rva + exception_directory_size))
        {

        }

        [[nodiscard]] T begin() const
        {
            return { m_module, m_runtime_functions };
        }

        [[nodiscard]] T end() const
        {
            return { m_module, m_end_runtime_functions };
        }
    };
}