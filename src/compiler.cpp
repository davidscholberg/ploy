#include <charconv>
#include <format>
#include <limits>
#include <stdexcept>
#include <system_error>
#include <variant>

#include "compiler.hpp"
#include "opcodes.hpp"

std::string bytecode::to_string() const {
    std::string str = "constants: [";
    for (const auto& c : constants)
        str += std::visit(
            [](const auto& v) {
                return std::format("{}, ", v);
            },
            c
        );

    str += "]\nbytecode: [";
    for (const auto& b : code)
        str += std::format("{}, ", b);

    str += "]";
    return str;
}

compiler::compiler(const std::vector<token>& tokens)
    : current_token_ptr{tokens.data()} {
        while (!eof()) {
            if (current_token_ptr->type != token_type::left_paren)
                throw std::runtime_error("expected left paren");

            compile_combination();
        }

    program.code.emplace_back(static_cast<uint8_t>(opcode::ret));
}

void compiler::compile_combination() {
    current_token_ptr++;
    compile_expression();
    consume_token(token_type::right_paren);
}

void compiler::compile_error() {
    throw std::runtime_error(std::format("unexpected token: {}", static_cast<uint8_t>(current_token_ptr->type)));
}

void compiler::compile_expression() {
    const auto compile_func = token_compile_map[static_cast<size_t>(current_token_ptr->type)];
    (this->*compile_func)();
}

void compiler::compile_int() {
    if (program.constants.size() == std::numeric_limits<uint8_t>::max())
        throw std::runtime_error("exceeded max number of constants allowed");

    const std::string_view& sv = current_token_ptr->value;
    int64_t int_value;
    auto [ptr, ec] = std::from_chars(sv.data(), sv.data() + sv.size(), int_value);
    if (ec != std::errc())
        throw std::runtime_error("couldn't parse int");

    program.constants.emplace_back(int_value);
    program.code.emplace_back(static_cast<uint8_t>(opcode::constant));
    program.code.emplace_back(program.constants.size() - 1);

    current_token_ptr++;
}

void compiler::compile_minus() {
    if ((current_token_ptr - 1)->type == token_type::left_paren)
        compile_binary_op(opcode::subtract);
    else
        compile_unary_op(opcode::negate);
}

void compiler::compile_plus() {
    compile_binary_op(opcode::add);
}

void compiler::compile_star() {
    compile_binary_op(opcode::multiply);
}

void compiler::compile_forward_slash() {
    compile_binary_op(opcode::divide);
}

void compiler::compile_binary_op(const opcode op) {
	current_token_ptr++;
	compile_expression();
	compile_expression();
	program.code.emplace_back(static_cast<uint8_t>(op));
}

void compiler::compile_unary_op(const opcode op) {
	current_token_ptr++;
	compile_expression();
	program.code.emplace_back(static_cast<uint8_t>(op));
}

void compiler::consume_token(const token_type type) {
    if (current_token_ptr->type != type)
        throw std::runtime_error(std::format("unexpected token: {}", static_cast<uint8_t>(current_token_ptr->type)));

    current_token_ptr++;
}

bool compiler::eof() {
    return current_token_ptr->type == token_type::eof;
}
