#include <charconv>
#include <format>
#include <limits>
#include <stdexcept>
#include <system_error>

#include "bytecode.hpp"
#include "compiler.hpp"
#include "virtual_machine.hpp"

compiler::compiler(const std::vector<token>& tokens)
    : current_token_ptr{tokens.data()} {
        while (!eof()) {
            compile_expression();
        }

    program.code.emplace_back(static_cast<uint8_t>(opcode::ret));
}

void compiler::compile_builtin_procedure() {
    current_token_ptr++;
    const auto* procedure_expression_start = current_token_ptr;

    // skip over procedure expression for now
    if (current_token_ptr->type == token_type::left_paren) {
        size_t paren_depth = 1;

        current_token_ptr++;
        while (!eof() and paren_depth != 0) {
            if (current_token_ptr->type == token_type::left_paren)
                paren_depth++;
            else if (current_token_ptr->type == token_type::right_paren)
                paren_depth--;
            current_token_ptr++;
        }
    } else {
        current_token_ptr++;
    }

    if (eof())
        throw std::runtime_error("unexpected eof in procedure call expression");

    // compile procedure args
    uint8_t argc = 0;
    while (!eof() and current_token_ptr->type != token_type::right_paren) {
        compile_expression();
        if (argc == std::numeric_limits<uint8_t>::max())
            throw std::runtime_error("exceeded max arg count for procedure");
        argc++;
    }

    if (eof())
        throw std::runtime_error("unexpected eof in procedure call expression");

    current_token_ptr++;
    const auto* post_expression_ptr = current_token_ptr;

    // go back and compile procedure expression
    current_token_ptr = procedure_expression_start;
    compile_expression();
    current_token_ptr = post_expression_ptr;

    program.code.emplace_back(static_cast<uint8_t>(opcode::call));
    program.code.emplace_back(argc);
}

void compiler::compile_expression() {
    switch (current_token_ptr->type) {
        case token_type::number:
            compile_number();
            break;
        case token_type::identifier:
            compile_identifier();
            break;
        case token_type::left_paren:
            // TODO: expand to handle special forms and user defined procedure calls.
            compile_builtin_procedure();
            break;
        default:
            throw std::runtime_error(std::format("unexpected token: {}", static_cast<uint8_t>(current_token_ptr->type)));
    }
}

void compiler::compile_identifier() {
    // TODO: handle user defined shiz.
    if (!bp_name_to_ptr.contains(current_token_ptr->value))
        throw std::runtime_error("identifier not found");

    uint8_t constant_index = program.add_constant(bp_name_to_ptr.at(current_token_ptr->value));

    program.code.emplace_back(static_cast<uint8_t>(opcode::push_constant));
    program.code.emplace_back(constant_index);

    current_token_ptr++;
}

void compiler::compile_number() {
    const std::string_view& sv = current_token_ptr->value;

    uint8_t constant_index;
    if (sv.find(".") == std::string::npos) {
        int64_t int_value;
        auto [ptr, ec] = std::from_chars(sv.data(), sv.data() + sv.size(), int_value);

        if (ec != std::errc())
            throw std::runtime_error("couldn't parse int");

        constant_index = program.add_constant(int_value);
    } else {
        double double_value;
        auto [ptr, ec] = std::from_chars(sv.data(), sv.data() + sv.size(), double_value);

        if (ec != std::errc())
            throw std::runtime_error("couldn't parse double");

        constant_index = program.add_constant(double_value);
    }

    program.code.emplace_back(static_cast<uint8_t>(opcode::push_constant));
    program.code.emplace_back(constant_index);

    current_token_ptr++;
}

void compiler::consume_token(const token_type type) {
    if (current_token_ptr->type != type)
        throw std::runtime_error(std::format("unexpected token: {}", static_cast<uint8_t>(current_token_ptr->type)));

    current_token_ptr++;
}

bool compiler::eof() {
    return current_token_ptr->type == token_type::eof;
}
