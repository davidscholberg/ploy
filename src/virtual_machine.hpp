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

struct call_frame {
    lambda_ptr executing_lambda;
    size_t frame_index;
    const uint8_t* return_ptr;
    uint8_t argc;
};

struct virtual_machine {
    std::vector<call_frame> call_frame_stack;
    std::vector<stack_value> stack;

    void execute(const bytecode& p);
    void pop_excess(const size_t return_value_count);
    std::string to_string() const;

    protected:
    const uint8_t* begin_instruction_ptr;
    const uint8_t* instruction_ptr;

    void execute_call();
    void execute_capture_stack_var();
    void execute_capture_shared_var();
    void execute_expect_argc();
    void execute_push_stack_var();
    void execute_push_shared_var();
    void execute_ret();
    call_frame& get_executing_call_frame();
    lambda_ptr& get_executing_lambda();
};
