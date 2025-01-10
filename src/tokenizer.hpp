#pragma once

#include <string>
#include <string_view>
#include <vector>

enum class token_type {
    left_paren,
    right_paren,
    plus,
    minus,
    star,
    forward_slash,
    equal,
    bang,
    less,
    greater,
    bang_equal,
    less_equal,
    greater_equal,
    number,
    string,
    identifier,
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

    std::string to_string();

    protected:
    const char* current_ptr;

    void add_static_token(size_t size, token_type type);

    template <auto IsTokenChar>
    void add_dynamic_token(token_type type);

    void add_string_token();
};

std::vector<token> tokenize(const char* const source);
