#pragma once

#include <stdexcept>
#include <stdint.h>
#include <string>
#include <variant>
#include <type_traits>
#include <unordered_map>
#include <vector>

using builtin_procedure = void (*)(void*, uint8_t);

using result_variant = std::variant<
    int64_t,
    double,
    bool,
    builtin_procedure
>;

enum class opcode : uint8_t {
    call,
    jump_back,
    jump_back_if_not,
    jump_forward,
    jump_forward_if_not,
    push_constant,
    ret,
};

template <typename T>
inline constexpr void check_alignment() {
    static_assert(
        std::alignment_of<T>() <= __STDCPP_DEFAULT_NEW_ALIGNMENT__,
        "bytecode vector alignment not guaranteed for this type"
    );
}

template <typename T>
inline constexpr size_t get_aligned_offset(const uint8_t* const ptr) {
    uintptr_t addr = reinterpret_cast<uintptr_t>(ptr);
    uintptr_t aligned_mod = addr % std::alignment_of<T>();
    return (aligned_mod == 0) ? 0 : (std::alignment_of<T>() - aligned_mod);
}

template <typename T>
inline size_t get_aligned_access_size(const uint8_t* const ptr) {
    return get_aligned_offset<T>(ptr) + sizeof(T);
}

struct bytecode {
    using jump_size_type = uint32_t;

    std::vector<uint8_t> code;

    uint8_t add_constant(const result_variant& new_constant);
    template <typename T>
    T aligned_read(const uint8_t* const ptr) const {
        return *get_aligned_ptr<const T>(ptr);
    }
    template <typename T>
    void aligned_write(T value, uint8_t* const ptr) {
        *get_aligned_ptr<T>(ptr) = value;
    }
    void backpatch_jump(const size_t jump_size_index);
    const result_variant& get_constant(uint8_t index) const;
    size_t prepare_backpatch_jump(const opcode jump_type);
    std::string to_string() const;

    protected:
    std::vector<result_variant> constants;
    std::unordered_map<result_variant, uint8_t> constant_to_index_map;

    template <typename T, typename U>
    requires (
        (!std::is_const_v<T> && std::is_same_v<U, uint8_t>)
        || (std::is_const_v<T> && std::is_same_v<U, const uint8_t>)
    )
    T* get_aligned_ptr(U* const ptr) const {
        check_alignment<T>();

        U* const aligned_ptr = ptr + get_aligned_offset<T>(ptr);

        if ((aligned_ptr - code.data()) + sizeof(T) > code.size())
            throw std::runtime_error("accessing aligned pointer would be out of range");

        return reinterpret_cast<T*>(aligned_ptr);
    }
};
