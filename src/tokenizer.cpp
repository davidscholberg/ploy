#include <format>
#include <stdexcept>
#include <stdint.h>
#include <string>
#include <vector>

#include "tokenizer.hpp"

static bool is_whitespace(const char c) {
    return c == ' ' or c == '\n';
}

static bool is_delimiter(const char c) {
    return (
        is_whitespace(c)
        or c == '('
        or c == ')'
        or c == '"'
        or c == ';'
    );
}

static bool is_eof(const char c) {
    return c == 0;
}

static bool is_digit(const char c) {
    return c >= '0' and c <= '9';
}

static bool is_numeric(const char c) {
    return is_digit(c) or c == '.';
}

static bool is_special_initial(const char c) {
    switch (c) {
        case '!':
        case '$':
        case '%':
        case '&':
        case '*':
        case '/':
        case ':':
        case '<':
        case '=':
        case '>':
        case '?':
        case '^':
        case '_':
        case '~':
            return true;
    };

    return false;
}

static bool is_identifier_initial(const char c) {
    return (
        (c >= 'A' and c <= 'Z')
        or (c >= 'a' and c <= 'z')
        or is_special_initial(c)
    );
}

static bool is_identifier_subsequent(const char c) {
    return (
        is_identifier_initial(c)
        or is_digit(c)
        or c == '+'
        or c == '-'
        or c == '.'
        or c == '@'
    );
}

void tokenizer::add_token(size_t size, token_type type) {
    tokens.emplace_back(current_ptr, size, type);
    current_ptr += size;
}

void tokenizer::add_hash_token() {
    const char* next_ptr = current_ptr + 1;

    switch (*next_ptr) {
        case 't':
            add_token(2, token_type::boolean_true);
            break;
        case 'f':
            add_token(2, token_type::boolean_false);
            break;
        case '\\':
            current_ptr += 2;

            if (is_eof(*current_ptr))
                throw std::runtime_error("unexpected eof");

            // TODO: expand this to include space and newline.
            add_token(1, token_type::character);
            break;
        case 0:
            throw std::runtime_error("unexpected eof");
        default:
            throw std::runtime_error("invalid character after #");
    }
}

void tokenizer::add_minus_or_plus_token() {
    const char* const next_ptr = current_ptr + 1;

    if (is_numeric(*next_ptr))
        add_number_token();
    else if (is_delimiter(*next_ptr))
        add_token(1, token_type::identifier);
    else
        throw std::runtime_error("invalid character after - or +");
}

void tokenizer::add_identifier_token() {
    const char* token_start = current_ptr;
    current_ptr++;

    while (is_identifier_subsequent(*current_ptr))
        current_ptr++;

    if (is_eof(*current_ptr))
        throw std::runtime_error("unexpected eof after identifier");

    tokens.emplace_back(token_start, current_ptr, token_type::identifier);
}

void tokenizer::add_number_token() {
    const char* token_start = current_ptr;
    if (*token_start == '+')
        token_start++;
    current_ptr++;

    while (is_numeric(*current_ptr))
        current_ptr++;

    if (is_eof(*current_ptr))
        throw std::runtime_error("unexpected eof after number");

    tokens.emplace_back(token_start, current_ptr, token_type::number);
}

void tokenizer::add_string_token() {
    current_ptr++;
    const char* const token_start = current_ptr;

    // TODO: handle escaped quotes.
    while (*current_ptr != 0 && *current_ptr != '"')
        current_ptr++;

    if (is_eof(*current_ptr))
        throw std::runtime_error("source ended with no closing quote");

    tokens.emplace_back(token_start, current_ptr, token_type::string);
    current_ptr++;
}

tokenizer::tokenizer(const char* const source) {
    current_ptr = source;

    while (*current_ptr != 0) {
        const char current_char = *current_ptr;

        if (is_whitespace(current_char)) {
            current_ptr++;
            continue;
        }

        switch (current_char) {
            case '(':
                add_token(1, token_type::left_paren); break;
            case ')':
                add_token(1, token_type::right_paren); break;
            case '\'':
                add_token(1, token_type::single_quote); break;
            case '.':
                add_token(1, token_type::dot); break;
            case '#':
                add_hash_token(); break;
            case '"':
                add_string_token(); break;
            case '-':
            case '+':
                add_minus_or_plus_token(); break;
            default:
                if (is_numeric(current_char))
                    add_number_token();
                else if (is_identifier_initial(current_char))
                    add_identifier_token();
                else
                    throw std::runtime_error("unexpected first character of token");
        }
    }

    tokens.emplace_back(current_ptr, current_ptr, token_type::eof);
}

std::string tokenizer::to_string() const {
    std::string str = "tokens: [";

    for (const auto& t : tokens)
        str += std::format("{{\"{}\", {}}}, ", t.value, static_cast<int>(t.type));

    str += "]";
    return str;
}
