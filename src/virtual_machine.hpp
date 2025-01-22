#pragma once

#include <string>
#include <string_view>
#include <unordered_map>

#include "bytecode.hpp"

void builtin_divide(void* vm_void_ptr, uint8_t argc);
void builtin_minus(void* vm_void_ptr, uint8_t argc);
void builtin_multiply(void* vm_void_ptr, uint8_t argc);
void builtin_odd(void* vm_void_ptr, uint8_t argc);
void builtin_plus(void* vm_void_ptr, uint8_t argc);

inline const std::unordered_map<std::string_view, builtin_procedure> bp_name_to_ptr{
    {"/", builtin_divide},
    {"-", builtin_minus},
    {"*", builtin_multiply},
    {"odd?", builtin_odd},
    {"+", builtin_plus},
};

struct virtual_machine {
    std::vector<result_variant> stack;

    void execute(const bytecode& p);
    std::string to_string() const;

    protected:
    const uint8_t* instruction_ptr;

    void execute_call();
};
