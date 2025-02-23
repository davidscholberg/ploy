#pragma once

#include <string>
#include <string_view>
#include <unordered_map>

#include "bytecode.hpp"

void builtin_car(void* vm_void_ptr, uint8_t argc);
void builtin_cdr(void* vm_void_ptr, uint8_t argc);
void builtin_cons(void* vm_void_ptr, uint8_t argc);
void builtin_divide(void* vm_void_ptr, uint8_t argc);
void builtin_minus(void* vm_void_ptr, uint8_t argc);
void builtin_multiply(void* vm_void_ptr, uint8_t argc);
void builtin_odd(void* vm_void_ptr, uint8_t argc);
void builtin_plus(void* vm_void_ptr, uint8_t argc);

inline const std::unordered_map<std::string_view, builtin_procedure> bp_name_to_ptr{
    {"cons", builtin_cons},
    {"car", builtin_car},
    {"cdr", builtin_cdr},
    {"/", builtin_divide},
    {"-", builtin_minus},
    {"*", builtin_multiply},
    {"odd?", builtin_odd},
    {"+", builtin_plus},
};

inline auto& get_bp_ptr_to_name() {
    static std::unordered_map<builtin_procedure, std::string_view> bp_ptr_to_name;
    static bool initialized = false;

    if (initialized)
        return bp_ptr_to_name;

    for (const auto& [k, v] : bp_name_to_ptr)
        bp_ptr_to_name[v] = k;

    initialized = true;
    return bp_ptr_to_name;
}

/**
 * Maps the names of hand-rolled lambdas to their bytecode arrays.
 */
inline const std::unordered_map<std::string_view, std::vector<uint8_t>> hr_lambda_name_to_code{
    {
        "call/cc",
        {
            static_cast<uint8_t>(opcode::expect_argc), 1,
            static_cast<uint8_t>(opcode::push_continuation),
            static_cast<uint8_t>(opcode::set_coarity_one),
            static_cast<uint8_t>(opcode::push_frame_index),
            static_cast<uint8_t>(opcode::push_stack_var), 0,
            static_cast<uint8_t>(opcode::push_stack_var), 1,
            static_cast<uint8_t>(opcode::call),
            static_cast<uint8_t>(opcode::delete_stack_var), 1,
            static_cast<uint8_t>(opcode::ret),
        },
    },
};

struct virtual_machine {
    std::vector<call_frame> call_frame_stack;
    std::vector<stack_value> stack;

    /**
     * Current coarity state, which tells the vm how to handle return values and pushes to the value
     * stack.
     */
    coarity_type coarity_state;

    /**
     * Removes all values belonging in the call frame from the value stack.
     */
    void clear_call_frame();

    void execute(const bytecode& p);
    void execute_cons(size_t dest_from_top);
    void pop_excess(const size_t return_value_count);

    /**
     * Prints top value of vm stack to a string.
     */
    std::string stack_top_to_string() const;

    /**
     * Prints current state of vm stack to a string.
     */
    std::string stack_to_string() const;

    /**
     * Prints current state of vm to a string.
     */
    std::string to_string() const;

    protected:
    const uint8_t* begin_instruction_ptr;
    const uint8_t* instruction_ptr;

    void execute_call();
    void execute_capture_stack_var();
    void execute_capture_shared_var();
    void execute_cons();
    void execute_expect_argc();

    /**
     * Pushes the current continuation to the value stack.
     */
    void execute_push_continuation();

    void execute_push_stack_var();
    void execute_push_shared_var();
    void execute_ret();
    call_frame& get_executing_call_frame();
    lambda_ptr& get_executing_lambda();
};
