#include <format>
#include <stdexcept>
#include <string>
#include <vector>

#include "tokenizer.hpp"

static bool is_whitespace(const char c) {
    return c == ' ' or c == '\n';
}

/**
 * Checks if character is a token delimiter.
 */
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

/**
 * A numeric character is either 0-9 or `.`.
 */
static bool is_numeric(const char c) {
    return is_digit(c) or c == '.';
}

/**
 * Checks if character is a special initial character of an identifier.
 */
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

/**
 * Checks if character is the initial character of an identifier.
 */
static bool is_identifier_initial(const char c) {
    return (
        (c >= 'A' and c <= 'Z')
        or (c >= 'a' and c <= 'z')
        or is_special_initial(c)
    );
}

/**
 * Checks if character is a subsequent character of an identifier (i.e. any position but the
 * initial).
 */
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

void tokenizer::push_expression() {
    if (tokens.size() > 1 and tokens[tokens.size() - 2].type == token_type::single_quote)
        return;

    expression_sequence_stack.back().emplace_back(tokens.size() - 1);
}

void tokenizer::push_expression_sequence() {
    push_expression();
    expression_sequence_stack.emplace_back();
}

void tokenizer::pop_expression_sequence() {
    if (expression_sequence_stack.empty())
        throw std::runtime_error("can't pop from empty expression sequence stack");

    const auto& expression_sequence = expression_sequence_stack.back();
    if (expression_sequence.empty())
        throw std::runtime_error("no expressions in expression sequence");

    tokens[expression_sequence.back()].is_final = true;
    expression_sequence_stack.pop_back();
}

tokenizer::tokenizer(const char* const source) {
    current_ptr = source;
    expression_sequence_stack.emplace_back();

    while (*current_ptr != 0) {
        const char current_char = *current_ptr;

        if (is_whitespace(current_char)) {
            current_ptr++;
            continue;
        }

        switch (current_char) {
            case '(':
                add_token(1, token_type::left_paren);
                push_expression_sequence();
                break;
            case ')':
                add_token(1, token_type::right_paren);
                pop_expression_sequence();
                break;
            case '\'':
                add_token(1, token_type::single_quote);
                push_expression();
                break;
            case '.':
                add_token(1, token_type::dot);
                break;
            case '#':
                add_hash_token();
                push_expression();
                break;
            case '"':
                add_string_token();
                push_expression();
                break;
            case '-':
            case '+':
                add_minus_or_plus_token();
                push_expression();
                break;
            case ';':
                while (*current_ptr != '\n' and *current_ptr != 0)
                    current_ptr++;
                break;
            default:
                if (is_numeric(current_char))
                    add_number_token();
                else if (is_identifier_initial(current_char))
                    add_identifier_token();
                else
                    throw std::runtime_error("unexpected first character of token");
                push_expression();
        }
    }

    if (expression_sequence_stack.size() != 1)
        throw std::runtime_error("unexpected expression stack size");

    pop_expression_sequence();

    tokens.emplace_back(current_ptr, current_ptr, token_type::eof);
}

std::string tokenizer::to_string() const {
    std::string str = "tokens: [";

    for (const auto& t : tokens)
        str += std::format("{{\"{}\", {}}}, ", t.value, static_cast<int>(t.type));

    str += "]";
    return str;
}
