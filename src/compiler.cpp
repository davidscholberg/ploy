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
    push_lambda();

    while (!eof()) {
        compile_expression();
    }

    program.append_opcode(opcode::ret);
    pop_lambda();
    program.concat_blocks();
}

uint8_t compiler::add_shared_var(const std::string_view& var_name, size_t scope_depth) {
    if (scope_depth >= lambda_stack.size())
        throw std::runtime_error("adding shared var to non-existent scope");

    auto& ctx = lambda_stack[scope_depth];

    if (ctx.shared_vars.contains(var_name))
        throw std::runtime_error("shared var already exists");

    uint8_t var_id = ctx.shared_vars.size();

    if (var_id == std::numeric_limits<uint8_t>::max())
        throw std::runtime_error("shared var limit exceeded");

    ctx.shared_vars[var_name] = var_id;
    return var_id;
}

void compiler::add_stack_var(const std::string_view& var_name) {
    if (lambda_stack.empty())
        throw std::runtime_error("no lambda to add stack var to");

    auto& ctx = lambda_stack.back();

    if (ctx.stack_vars.contains(var_name))
        throw std::runtime_error("stack var already exists");

    uint8_t var_id = ctx.stack_vars.size();

    if (var_id == std::numeric_limits<uint8_t>::max())
        throw std::runtime_error("stack var limit exceeded");

    ctx.stack_vars[var_name] = var_id;
}

void compiler::compile_boolean() {
    uint8_t constant_index = program.add_constant(current_token_ptr->type == token_type::boolean_true);

    program.append_opcode(opcode::push_constant);
    program.append_byte(constant_index);

    current_token_ptr++;
}

void compiler::compile_procedure_call() {
    program.append_opcode(opcode::push_frame_index);

    // compile procedure expression
    compile_expression();

    // compile procedure args
    while (!eof() and current_token_ptr->type != token_type::right_paren)
        compile_expression();

    if (eof())
        throw std::runtime_error("unexpected eof in procedure call expression");

    current_token_ptr++;

    program.append_opcode(opcode::call);
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
            else if (current_token_ptr->value == "lambda")
                compile_lambda();
            else
                compile_procedure_call();

            break;
        default:
            throw std::runtime_error(std::format("unexpected token: {}", static_cast<uint8_t>(current_token_ptr->type)));
    }
}

void compiler::compile_identifier() {
    if (bp_name_to_ptr.contains(current_token_ptr->value)) {
        uint8_t constant_index = program.add_constant(bp_name_to_ptr.at(current_token_ptr->value));

        program.append_opcode(opcode::push_constant);
        program.append_byte(constant_index);
    } else {
        const auto [var_type, var_id] = get_var_type_and_id(current_token_ptr->value);

        if (var_type == variable_type::stack)
            program.append_opcode(opcode::push_stack_var);
        else
            program.append_opcode(opcode::push_shared_var);

        program.append_byte(var_id);
    }

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

void compiler::compile_lambda() {
    push_lambda();

    // add lambda args to current lambda_context
    current_token_ptr++;
    consume_token(token_type::left_paren);
    uint8_t argc = 0;
    while (!eof() and current_token_ptr->type != token_type::right_paren) {
        if (current_token_ptr->type != token_type::identifier)
            throw std::runtime_error("non-identifier in lambda arg list");

        if (argc == std::numeric_limits<uint8_t>::max())
            throw std::runtime_error("exceeded lambda arg limit");

        add_stack_var(current_token_ptr->value);
        argc++;
        current_token_ptr++;
    }
    consume_token(token_type::right_paren);

    // compile expect_argc opcode which checks argc on stack
    program.append_opcode(opcode::expect_argc);
    program.append_byte(argc);

    // compile lambda body
    // TODO: might be a good idea to have an opcode that discards anything that the non-final
    // expresssions of the lambda return.
    while (!eof() and current_token_ptr->type != token_type::right_paren)
        compile_expression();
    consume_token(token_type::right_paren);

    program.append_opcode(opcode::ret);

    pop_lambda();
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

    program.append_opcode(opcode::push_constant);
    program.append_byte(constant_index);

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

std::pair<variable_type, uint8_t> compiler::get_var_type_and_id(const std::string_view& name) {
    if (lambda_stack.empty())
        throw std::runtime_error("no lambda context to get variable from");

    return get_var_type_and_id(name, lambda_stack.size() - 1);
}

std::pair<variable_type, uint8_t> compiler::get_var_type_and_id(const std::string_view& name, size_t scope_depth) {
    bool is_current_scope = (scope_depth == lambda_stack.size() - 1);
    auto& scope_ctx = lambda_stack[scope_depth];

    if (scope_ctx.shared_vars.contains(name))
        return {variable_type::shared, scope_ctx.shared_vars[name]};

    if (scope_ctx.stack_vars.contains(name)) {
        if (is_current_scope)
            return {variable_type::stack, scope_ctx.stack_vars[name]};

        program.append_opcode(opcode::capture_stack_var, scope_depth);
        program.append_byte(scope_ctx.stack_vars[name], scope_depth);

        uint8_t var_id = add_shared_var(name, scope_depth);
        return {variable_type::shared, var_id};
    }

    if (scope_depth == 0)
        throw std::runtime_error("var name not found");

    const auto [var_type, var_id] = get_var_type_and_id(name, scope_depth - 1);

    if (!is_current_scope) {
        program.append_opcode(opcode::capture_shared_var, scope_depth);
        program.append_byte(var_id, scope_depth);
    }

    // add shared_var to scope_ctx
    // return type and new id
    uint8_t new_var_id = add_shared_var(name, scope_depth);
    return {variable_type::shared, new_var_id};
}

void compiler::pop_lambda() {
    // TODO: add this lambda context to the bytecode (only in debug mode, for the purpose of resolving var names in the disassembly)
    lambda_stack.pop_back();
    program.pop_lambda();
}

void compiler::push_lambda() {
    uint8_t lambda_constant_index = program.add_constant(lambda_constant{lambda_offset_placeholder++});

    if (!lambda_stack.empty()) {
        program.append_opcode(opcode::push_constant);
        program.append_byte(lambda_constant_index);
    }

    program.push_lambda(lambda_constant_index);
    lambda_stack.emplace_back(lambda_context{});
}
