#pragma once

#include <array>
#include <stdint.h>
#include <string>
#include <string.h>
#include <unordered_map>
#include <vector>

#include "scheme_value.hpp"

enum class opcode : uint8_t {
    call,
    capture_shared_var,
    capture_stack_var,
    expect_argc,
    halt,
    jump_forward,
    jump_forward_if_not,
    push_constant,
    push_frame_index,
    push_shared_var,
    push_stack_var,
    ret,
};

struct opcode_no_arg {
    uint8_t opcode_value;
};

struct opcode_one_arg {
    uint8_t opcode_value;
    uint8_t arg;
};

using jump_size_type = uint32_t;

struct opcode_jump {
    uint8_t opcode_value;
    uint8_t jump_size[sizeof(jump_size_type)];
};

struct opcode_info {
    const char* const name;
    uint8_t size;
};

inline constexpr auto opcode_infos = std::to_array<opcode_info>({
    {"call", sizeof(opcode_no_arg)},
    {"capture_shared_var", sizeof(opcode_one_arg)},
    {"capture_stack_var", sizeof(opcode_one_arg)},
    {"expect_argc", sizeof(opcode_one_arg)},
    {"halt", sizeof(opcode_no_arg)},
    {"jump_forward", sizeof(opcode_jump)},
    {"jump_forward_if_not", sizeof(opcode_jump)},
    {"push_constant", sizeof(opcode_one_arg)},
    {"push_frame_index", sizeof(opcode_no_arg)},
    {"push_shared_var", sizeof(opcode_one_arg)},
    {"push_stack_var", sizeof(opcode_one_arg)},
    {"ret", sizeof(opcode_no_arg)},
});

struct lambda_code {
    std::vector<uint8_t> code;
    uint8_t lambda_constant_id;
};

struct bytecode {
    std::vector<uint8_t> code;

    uint8_t add_constant(const scheme_constant& new_constant);
    void append_byte(uint8_t value);
    void append_byte(uint8_t value, size_t scope_depth);
    void append_opcode(opcode value);
    void append_opcode(opcode value, size_t scope_depth);
    void backpatch_jump(const size_t jump_size_index);
    void concat_blocks();
    std::string disassemble() const;
    const scheme_constant& get_constant(uint8_t index) const;
    size_t prepare_backpatch_jump(const opcode jump_type);
    void pop_lambda();
    void push_lambda(uint8_t lambda_constant_index);

    template <typename T>
    static constexpr T read_value(const uint8_t* const ptr) {
        if constexpr (sizeof(T) == 1)
            return *ptr;

        T value;
        memcpy(&value, ptr, sizeof(T));
        return value;
    }

    std::string to_string() const;

    template <typename T>
    static constexpr void write_value(T value, uint8_t* const ptr) {
        if constexpr (sizeof(T) == 1) {
            *ptr = value;
            return;
        }

        memcpy(ptr, &value, sizeof(T));
    }

    protected:
    std::vector<scheme_constant> constants;
    std::unordered_map<scheme_constant, uint8_t> constant_to_index_map;
    std::vector<lambda_code> compiling_blocks;
    std::vector<lambda_code> compiled_blocks;
};
