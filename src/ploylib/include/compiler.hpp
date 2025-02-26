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

/**
 * Represents the compilation context for a lambda currently being compiled.
 * TODO: in debug mode, bytecode should get these objects so that variable names can be resolved in
 * disassembly (note: last time I thought about this it seemed a little sussy, gonna have to thonk
 * on it some more).
 */
struct lambda_context {

    /**
     * Maps a stack variable's name to its id used in the bytecode.
     */
    std::unordered_map<std::string_view, uint8_t> stack_vars;

    /**
     * Maps a shared (i.e. captured) variable's name to its id used in the bytecode.
     */
    std::unordered_map<std::string_view, uint8_t> shared_vars;

    /**
     * Stack of coarity_types that tell the compiler which coarity toggle to add to the bytecode.
     */
     std::vector<coarity_type> coarity_stack;
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

    /**
     * Check if current token is a particular sentinel type.
     */
    template <token_type TokenType>
    bool at_sentinel() {
        return current_token_ptr->type == TokenType;
    }

    void compile_boolean();
    void compile_procedure_call();
    void compile_expression();
    void compile_external_representation();
    void compile_external_representation_abbr();
    void compile_identifier();
    void compile_if();
    void compile_lambda();
    void compile_number();
    void compile_pair();
    void compile_set();

    /**
     * Compiles a sequence of expressions. If FinalCoarity is one, results from all but the last
     * expression in the sequence will be discarded, otherwise results from all expressions will be
     * discarded. Used for expressions in the global scope, lambda bodies, begin expressions, etc.
     */
    template <coarity_type FinalCoarity, auto... SentinelChecks>
    void compile_expression_sequence() {
        if constexpr (FinalCoarity == coarity_type::one) {
            if (current_token_ptr->is_final)
                push_coarity(coarity_type::one);
            else
                push_coarity(coarity_type::any);
        } else {
            push_coarity(coarity_type::any);
        }

        compile_expression();

        while ((!(this->*SentinelChecks)() and ...)) {
            if constexpr (FinalCoarity == coarity_type::one) {
                if (current_token_ptr->is_final)
                    set_coarity(coarity_type::one);
            }

            compile_expression();
        }

        pop_coarity();
    }

    void consume_token(const token_type type);

    /**
     * Check if current token is the eof token type.
     */
    bool eof();

    scheme_constant generate_boolean_constant();
    scheme_constant generate_number_constant();

    /**
     * Get the lambda_context for the currently compiling lambda.
     */
    lambda_context& get_current_lambda();

    std::pair<variable_type, uint8_t> get_var_type_and_id(const std::string_view& name);
    std::pair<variable_type, uint8_t> get_var_type_and_id(const std::string_view& name, size_t scope_depth);

    /**
     * Pop value from coarity stack.
     */
    void pop_coarity();

    /**
     * Push value to the coarity stack. If the value passed is the same as the stack top, no
     * bytecode will be emitted.
     */
    void push_coarity(coarity_type type);

    void pop_lambda();
    void push_lambda();

    /**
     * Set value of the coarity stack top. If the value passed is the same as the stack top, no
     * bytecode will be emitted.
     */
    void set_coarity(coarity_type type);
};
