#pragma once

#include <string>
#include <string_view>
#include <vector>

/**
 * Identifies the type of a token. A token is a semantic piece of the source
 * code, similar to words and punctuation in human language.
 */
enum class token_type {

    /**
     * Generally denotes the beginning of a compound expression or pair literal.
     */
    left_paren,

    /**
     * Generally denotes the end of a compound expression or pair literal.
     */
    right_paren,

    /**
     * Used as a shorthand to denote literals. 'x expands to (quote x).
     */
    single_quote,

    /**
     * Used to delimit the car and cdr values of a pair literal: '(1 . 2).
     */
    dot,

    /**
     * Boolean true literal.
     */
    boolean_true,

    /**
     * Boolean false literal.
     */
    boolean_false,

    /**
     * Character literal.
     */
    character,

    /**
     * String literal.
     */
    string,

    /**
     * Number literal.
     */
    number,

    /**
     * Identifier, which can be a syntactic keyword, builtin procedure name, or variable name.
     */
    identifier,

    /**
     * Denotes the end of the token array.
     */
    eof,
};

/**
 * Holds the type and value of a token.
 */
struct token {

    /**
     * The string value of this token. The value points to its location in the source code rather
     * than containing a copy.
     */
    std::string_view value;

    /**
     * The type of this token.
     */
    token_type type;

    /**
     * Signifies whether or not this token is the beginning of the final expression in an expression
     * sequence. Used by the compiler to properly handle continuation arity of lambda bodies.
     */
    bool is_final = false;

    /**
     * Initializes token value based on size.
     */
    token(const char* const first, size_t size, const token_type type)
        : value{first, size}, type{type} {}

    /**
     * Initializes token value based on first and last character positions.
     */
    token(const char* const first, const char* const last, const token_type type)
        : value{first, last}, type{type} {}
};

/**
 * Tokenizes! Takes a source string representing a scheme program and generates
 * an array of semantic tokens representative of the input program. Must be
 * constructed with a source string, after which the tokens field can be passed
 * to the compiler. All add* functions add a token to the token vector and
 * advance to the next character in the source.
 */
struct tokenizer {

    /**
     * Array of tokens generated from the source string. Note that this relies on the source string
     * not being destroyed during the lifetime of this object.
     */
    std::vector<token> tokens;

    /**
     * Generate tokens from source string.
     */
    tokenizer(const char* const source);

    /**
     * Prints token array (output is currently ass since I haven't bothered with making string
     * values for the token types).
     */
    std::string to_string() const;

    protected:

    /**
     * Holds the current position of the source file during tokenization. Should only be used inside
     * the constructor.
     */
    const char* current_ptr;

    /**
     * Tracks the locations of initial tokens of expression sequences.
     */
    std::vector<std::vector<size_t>> expression_sequence_stack;

    /**
     * Adds given token of given size to token vector. Useful for tokens of fixed value.
     */
    void add_token(size_t size, token_type type);

    /**
     * Adds token beginning with a hash (either boolean or character literal) to token vector.
     */
    void add_hash_token();

    /**
     * Adds token beginning with - or + (can be either a number or identifier) to token vector.
     */
    void add_minus_or_plus_token();

    /**
     * Adds identifier token to token vector.
     */
    void add_identifier_token();

    /**
     * Adds number token to token vector.
     */
    void add_number_token();

    /**
     * Adds string token to token vector.
     */
    void add_string_token();

    /**
     * Pops an expression sequence, marking the token that starts the final expression.
     */
    void pop_expression_sequence();

    /**
     * Pushes the location of the current token to the current expression sequence.
     */
    void push_expression();

    /**
     * Pushes a new expression sequence (which is itself an expression and therefore gets pushed as
     * well.
     */
    void push_expression_sequence();
};

/**
 * Standalone string_view meant to be used in place of a token value. Normally
 * token values point directly to the source, but currently the tokenizer
 * doesn't expand 'x into (quote x), so the compiler needs this string_view to
 * create the quote symbol for this expansion.
 */
inline constexpr std::string_view quote_symbol = "quote";
