#pragma once

#include <stdint.h>
#include <utility>
#include <vector>

#include "bytecode.hpp"
#include "tokenizer.hpp"

enum class variable_type {
    stack,
    shared,
};

// TODO: in debug mode, bytecode should get these objects so that variable names can be resolved in disassembly.
struct lambda_context {
    std::unordered_map<std::string_view, uint8_t> stack_vars;
    std::unordered_map<std::string_view, uint8_t> shared_vars;
};

struct compiler {
    bytecode program;

    compiler(const std::vector<token>& tokens);

    protected:
    std::vector<lambda_context> lambda_stack;
    const token* current_token_ptr;
    size_t lambda_offset_placeholder;

    uint8_t add_shared_var(const std::string_view& var_name, size_t scope_depth);
    void add_stack_var(const std::string_view& var_name);
    void compile_boolean();
    void compile_procedure_call();
    void compile_expression();
    void compile_identifier();
    void compile_if();
    void compile_lambda();
    void compile_number();
    void consume_token(const token_type type);
    bool eof();
    std::pair<variable_type, uint8_t> get_var_type_and_id(const std::string_view& name);
    std::pair<variable_type, uint8_t> get_var_type_and_id(const std::string_view& name, size_t scope_depth);
    void pop_lambda();
    void push_lambda();
};
