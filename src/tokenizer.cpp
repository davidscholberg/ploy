#include <cctype>
#include <format>
#include <stdexcept>
#include <stdint.h>
#include <string>
#include <string_view>
#include <vector>

#include "tokenizer.hpp"

static bool is_number_char(const char c) {
    return c >= '0' and c <= '9';
}

static bool is_identifier_char(const char c) {
    return (
        (c >= 'A' && c <= 'Z')
        || (c >= 'a' && c <= 'z')
    );
}

void tokenizer::add_static_token(size_t size, token_type type) {
    tokens.emplace_back(current_ptr, size, type);
    current_ptr += size;
}

template <auto IsTokenChar>
void tokenizer::add_dynamic_token(token_type type) {
    const char* const token_start = current_ptr;
    current_ptr++;

    while (*current_ptr != 0 && IsTokenChar(*current_ptr))
        current_ptr++;

    tokens.emplace_back(token_start, current_ptr, type);
}

void tokenizer::add_string_token() {
    current_ptr++;
    const char* const token_start = current_ptr;

    // TODO: handle escaped quotes.
    while (*current_ptr != 0 && *current_ptr != '"')
        current_ptr++;

    if (*current_ptr == 0)
        throw std::runtime_error("source ended with no closing quote");

    tokens.emplace_back(token_start, current_ptr, token_type::string);
    current_ptr++;
}

tokenizer::tokenizer(const char* const source) {
    current_ptr = source;

    while (*current_ptr != 0) {
        const char current_char = *current_ptr;

        if (std::isspace(current_char)) {
            current_ptr++;
            continue;
        }

        if (is_number_char(current_char)) {
            add_dynamic_token<is_number_char>(token_type::number);
            continue;
        }

        if (is_identifier_char(current_char)) {
            add_dynamic_token<is_identifier_char>(token_type::identifier);
            continue;
        }

        switch (current_char) {
            case '(':
				add_static_token(1, token_type::left_paren); break;
            case ')':
				add_static_token(1, token_type::right_paren); break;
            case '+':
				add_static_token(1, token_type::plus); break;
            case '-':
				add_static_token(1, token_type::minus); break;
            case '*':
				add_static_token(1, token_type::star); break;
            case '/':
				add_static_token(1, token_type::forward_slash); break;
            case '=':
				add_static_token(1, token_type::equal); break;
            case '!':
                if (*(current_ptr + 1) == '=')
                    add_static_token(2, token_type::bang_equal);
                else
                    add_static_token(1, token_type::bang);
                break;
            case '<':
                if (*(current_ptr + 1) == '=')
                    add_static_token(2, token_type::bang_equal);
                else
                    add_static_token(1, token_type::bang);
                break;
            case '>':
                if (*(current_ptr + 1) == '=')
                    add_static_token(2, token_type::greater_equal);
                else
                    add_static_token(1, token_type::greater);
                break;
            case '"':
                add_string_token(); break;
        }
    }
}

std::string tokenizer::to_string() {
    std::string str = "[";

    for (const auto& t : tokens)
        str += std::format("{{\"{}\", {}}}, ", t.value, static_cast<int>(t.type));

    str += "]";
    return str;
}
