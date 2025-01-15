#pragma once

#include <array>
#include <stdint.h>
#include <string>
#include <variant>
#include <vector>

#include "opcodes.hpp"
#include "tokenizer.hpp"

using result_variant = std::variant<int64_t>;

struct bytecode {
    std::vector<result_variant> constants;
    std::vector<uint8_t> code;

    std::string to_string() const;
};

struct compiler {
    bytecode program;

    compiler(const std::vector<token>& tokens);

    protected:
    const token* current_token_ptr;

    void compile_combination();
    void compile_forward_slash();
    void compile_int();
    void compile_minus();
    void compile_plus();
    void compile_star();

    void compile_binary_op(const opcode op);
    void compile_unary_op(const opcode op);

    void compile_error();
    void compile_expression();

    void consume_token(const token_type type);
    bool eof();

    static constexpr auto token_compile_map = std::to_array({
        &compiler::compile_combination,
        &compiler::compile_error,
        &compiler::compile_plus,
        &compiler::compile_minus,
        &compiler::compile_star,
        &compiler::compile_forward_slash,
        &compiler::compile_error,
        &compiler::compile_error,
        &compiler::compile_error,
        &compiler::compile_error,
        &compiler::compile_error,
        &compiler::compile_error,
        &compiler::compile_error,
        &compiler::compile_int,
        &compiler::compile_error,
        &compiler::compile_error,
        &compiler::compile_error,
    });
};
