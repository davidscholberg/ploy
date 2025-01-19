#pragma once

#include <string>
#include <string_view>
#include <vector>

enum class token_type {
    left_paren,
    right_paren,
    single_quote,
    dot,
    boolean_true,
    boolean_false,
    character,
    string,
    number,
    identifier,
    eof,
};

struct token {
    std::string_view value;
    token_type type;

    token(const char* const first, size_t size, const token_type type)
        : value{first, size}, type{type} {}

    token(const char* const first, const char* const last, const token_type type)
        : value{first, last}, type{type} {}
};

struct tokenizer {
    std::vector<token> tokens;

    tokenizer(const char* const source);

    std::string to_string() const;

    protected:
    const char* current_ptr;

    void add_token(size_t size, token_type type);
    void add_hash_token();
    void add_minus_or_plus_token();
    void add_identifier_token();
    void add_number_token();
    void add_string_token();
};
