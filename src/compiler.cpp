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

    program.code.emplace_back(static_cast<uint8_t>(opcode::halt));
}

void compiler::compile_boolean() {
    uint8_t constant_index = program.add_constant(current_token_ptr->type == token_type::boolean_true);

    program.code.emplace_back(static_cast<uint8_t>(opcode::push_constant));
    program.code.emplace_back(constant_index);

    current_token_ptr++;
}

void compiler::compile_builtin_procedure() {
    program.code.emplace_back(static_cast<uint8_t>(opcode::push_frame_index));

    // compile procedure expression
    compile_expression();

    // compile procedure args
    while (!eof() and current_token_ptr->type != token_type::right_paren)
        compile_expression();

    if (eof())
        throw std::runtime_error("unexpected eof in procedure call expression");

    current_token_ptr++;

    program.code.emplace_back(static_cast<uint8_t>(opcode::call));
}

void compiler::compile_expression() {
    switch (current_token_ptr->type) {
        case token_type::number:
            compile_number();
            break;
        case token_type::identifier:
            compile_identifier();
            break;
        case token_type::boolean_true:
        case token_type::boolean_false:
            compile_boolean();
            break;
        case token_type::left_paren:
            current_token_ptr++;

            // TODO: maybe use a map for special forms if there's enough of them.
            if (current_token_ptr->value == "if")
                compile_if();
            else
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

void compiler::compile_if() {
    current_token_ptr++;

    // compile test
    compile_expression();

    // compile conditional jump and leave space for jump offset
    const size_t first_backpatch_index = program.prepare_backpatch_jump(opcode::jump_forward_if_not);

    // compile consequent
    compile_expression();

    if (eof())
        throw std::runtime_error("unexpected eof after if consequent");

    // if there's no alternate, backpatch the first jump, advance token, and we're done.
    if (current_token_ptr->type == token_type::right_paren) {
        program.backpatch_jump(first_backpatch_index);
        consume_token(token_type::right_paren);
        return;
    }

    // if there's an alternate, prepare a second backpatch jump (unconditional), backpatch the first
    // jump, compile alternate, backpatch second jump, advance token, bye bye.
    const size_t second_backpatch_index = program.prepare_backpatch_jump(opcode::jump_forward);
    program.backpatch_jump(first_backpatch_index);

    // compile alternate
    compile_expression();

    program.backpatch_jump(second_backpatch_index);

    consume_token(token_type::right_paren);
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
