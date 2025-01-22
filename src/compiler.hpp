#pragma once

#include <stdint.h>
#include <vector>

#include "bytecode.hpp"
#include "tokenizer.hpp"

struct compiler {
    bytecode program;

    compiler(const std::vector<token>& tokens);

    protected:
    const token* current_token_ptr;

    void compile_boolean();
    void compile_builtin_procedure();
    void compile_expression();
    void compile_identifier();
    void compile_if();
    void compile_number();
    void consume_token(const token_type type);
    bool eof();
};
