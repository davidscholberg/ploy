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

    compile_expression_sequence<coarity_type::any, &compiler::eof>();

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

    uint8_t var_id = static_cast<uint8_t>(ctx.shared_vars.size());

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

    uint8_t var_id = static_cast<uint8_t>(ctx.stack_vars.size());

    if (var_id == std::numeric_limits<uint8_t>::max())
        throw std::runtime_error("stack var limit exceeded");

    ctx.stack_vars[var_name] = var_id;
}

void compiler::compile_boolean() {
    uint8_t constant_index = program.add_constant(generate_boolean_constant());

    program.append_opcode(opcode::push_constant);
    program.append_byte(constant_index);

    current_token_ptr++;
}

void compiler::compile_define() {
    current_token_ptr++;

    if (eof())
        throw std::runtime_error("unexpected eof after define");

    if (current_token_ptr->type != token_type::identifier)
        throw std::runtime_error("expected identifier in define");

    add_stack_var(current_token_ptr->value);

    push_coarity(coarity_type::one);

    current_token_ptr++;
    compile_expression();

    program.append_opcode(opcode::add_stack_var);

    pop_coarity();

    consume_token(token_type::right_paren);
}

void compiler::compile_procedure_call() {
    push_coarity(coarity_type::one);

    program.append_opcode(opcode::push_frame_index);

    // compile procedure expression
    compile_expression();

    // compile procedure args
    while (!eof() and current_token_ptr->type != token_type::right_paren)
        compile_expression();

    if (eof())
        throw std::runtime_error("unexpected eof in procedure call expression");

    pop_coarity();

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
        case token_type::single_quote:
            current_token_ptr++;
            compile_external_representation_abbr();
            break;
        case token_type::left_paren:
            current_token_ptr++;

            // TODO: maybe use a map for special forms if there's enough of them.
            if (current_token_ptr->value == "if")
                compile_if();
            else if (current_token_ptr->value == "lambda")
                compile_lambda();
            else if (current_token_ptr->value == "set!")
                compile_set();
            else if (current_token_ptr->value == "define")
                compile_define();
            else if (current_token_ptr->value == "quote")
                compile_external_representation();
            else
                compile_procedure_call();

            break;
        default:
            throw std::runtime_error(std::format("unexpected token: {}", static_cast<uint8_t>(current_token_ptr->type)));
    }
}

void compiler::compile_external_representation() {
    current_token_ptr++;
    compile_external_representation_abbr();
    consume_token(token_type::right_paren);
}

void compiler::compile_external_representation_abbr() {
    switch (current_token_ptr->type) {
        case token_type::number:
            compile_number();
            break;
        case token_type::boolean_true:
        case token_type::boolean_false:
            compile_boolean();
            break;
        case token_type::identifier:
            program.append_opcode(opcode::push_constant);
            program.append_byte(program.add_constant(symbol{current_token_ptr->value}));
            current_token_ptr++;
            break;
        case token_type::single_quote:
            // NOTE: currently we don't expand single quotes to (quote x) in tokenizer, and as a
            // result we have to essentially compile a custom pair here with a static quote symbol.
            // This solution is questionable at best since we now have two locations in the code
            // that compile pairs.
            program.append_opcode(opcode::push_constant);
            program.append_byte(program.add_constant(symbol{quote_symbol}));

            current_token_ptr++;
            compile_external_representation_abbr();

            program.append_opcode(opcode::push_constant);
            program.append_byte(program.add_constant(empty_list{}));

            program.append_opcode(opcode::cons);
            program.append_opcode(opcode::cons);
            break;
        case token_type::left_paren:
            current_token_ptr++;
            compile_pair();
            break;
        default:
            throw std::runtime_error(std::format("unexpected token for external representation: {}", static_cast<uint8_t>(current_token_ptr->type)));
    }
}

void compiler::compile_identifier() {
    if (bp_name_to_ptr.contains(current_token_ptr->value)) {
        uint8_t constant_index = program.add_constant(bp_name_to_ptr.at(current_token_ptr->value));

        program.append_opcode(opcode::push_constant);
        program.append_byte(constant_index);
    } else if (hrp_name_to_code.contains(current_token_ptr->value)) {
        uint8_t constant_index = program.push_hand_rolled_procedure(current_token_ptr->value);

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

    push_coarity(coarity_type::one);

    // compile test
    compile_expression();

    pop_coarity();

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
    compile_expression_sequence<coarity_type::one, &compiler::eof, &compiler::at_sentinel<token_type::right_paren>>();
    consume_token(token_type::right_paren);

    program.append_opcode(opcode::ret);

    pop_lambda();
}

void compiler::compile_number() {
    uint8_t constant_index = program.add_constant(generate_number_constant());

    program.append_opcode(opcode::push_constant);
    program.append_byte(constant_index);

    current_token_ptr++;
}

void compiler::compile_pair() {
    compile_external_representation_abbr();

    if (eof())
        throw std::runtime_error("unexpected eof in pair");

    if (current_token_ptr->type == token_type::dot) {
        current_token_ptr++;
        compile_external_representation_abbr();
        consume_token(token_type::right_paren);
    } else if (current_token_ptr->type == token_type::right_paren) {
        uint8_t constant_index = program.add_constant(empty_list{});

        program.append_opcode(opcode::push_constant);
        program.append_byte(constant_index);

        current_token_ptr++;
    } else {
        compile_pair();
    }

    program.append_opcode(opcode::cons);
}

void compiler::compile_set() {
    current_token_ptr++;

    if (eof())
        throw std::runtime_error("unexpected eof after set!");

    if (current_token_ptr->type != token_type::identifier)
        throw std::runtime_error("expected identifier in set!");

    const auto [var_type, var_id] = get_var_type_and_id(current_token_ptr->value);

    push_coarity(coarity_type::one);

    current_token_ptr++;
    compile_expression();

    if (var_type == variable_type::stack)
        program.append_opcode(opcode::set_stack_var);
    else
        program.append_opcode(opcode::set_shared_var);

    program.append_byte(var_id);

    pop_coarity();

    consume_token(token_type::right_paren);
}

void compiler::consume_token(const token_type type) {
    if (current_token_ptr->type != type)
        throw std::runtime_error(std::format("unexpected token: {}", static_cast<uint8_t>(current_token_ptr->type)));

    current_token_ptr++;
}

bool compiler::eof() {
    return at_sentinel<token_type::eof>();
}

scheme_constant compiler::generate_boolean_constant() {
    return current_token_ptr->type == token_type::boolean_true;
}

scheme_constant compiler::generate_number_constant() {
    const std::string_view& sv = current_token_ptr->value;

    if (sv.find(".") == std::string::npos) {
        int64_t int_value;
        auto [ptr, ec] = std::from_chars(sv.data(), sv.data() + sv.size(), int_value);

        if (ec != std::errc())
            throw std::runtime_error("couldn't parse int");

        return int_value;
    }

    double double_value;
    auto [ptr, ec] = std::from_chars(sv.data(), sv.data() + sv.size(), double_value);

    if (ec != std::errc())
        throw std::runtime_error("couldn't parse double");

    return double_value;
}

lambda_context& compiler::get_current_lambda() {
    if (lambda_stack.empty())
        throw std::runtime_error("no lambda context to get");

    return lambda_stack.back();
}

std::pair<variable_type, uint8_t> compiler::get_var_type_and_id(const std::string_view& name) {
    if (lambda_stack.empty())
        throw std::runtime_error("no lambda context to get variable from");

    return get_var_type_and_id(name, lambda_stack.size() - 1);
}

std::pair<variable_type, uint8_t> compiler::get_var_type_and_id(const std::string_view& name, size_t scope_depth) {
    bool is_current_scope = (scope_depth == lambda_stack.size() - 1);
    auto& scope_ctx = lambda_stack[scope_depth];

    if (scope_ctx.stack_vars.contains(name)) {
        uint8_t var_id = scope_ctx.stack_vars[name];

        if (!is_current_scope) {
            program.append_opcode(opcode::capture_stack_var, scope_depth);
            program.append_byte(scope_ctx.stack_vars[name], scope_depth);
        }

        return {variable_type::stack, var_id};
    }

    if (scope_ctx.shared_vars.contains(name)) {
        uint8_t var_id = scope_ctx.shared_vars[name];

        if (!is_current_scope) {
            program.append_opcode(opcode::capture_shared_var, scope_depth);
            program.append_byte(scope_ctx.shared_vars[name], scope_depth);
        }

        return {variable_type::shared, var_id};
    }

    if (scope_depth == 0)
        throw std::runtime_error("var name not found");

    const auto [var_type, var_id] = get_var_type_and_id(name, scope_depth - 1);

    uint8_t new_var_id = add_shared_var(name, scope_depth);

    if (!is_current_scope) {
        program.append_opcode(opcode::capture_shared_var, scope_depth);
        program.append_byte(new_var_id, scope_depth);
    }

    return {variable_type::shared, new_var_id};
}

void compiler::pop_coarity() {
    auto& current_lambda_ctx = get_current_lambda();

    if (current_lambda_ctx.coarity_stack.empty())
        throw std::runtime_error("can't pop from empty coarity stack");

    const coarity_type old_type = current_lambda_ctx.coarity_stack.back();
    current_lambda_ctx.coarity_stack.pop_back();

    if (!current_lambda_ctx.coarity_stack.empty()) {
        const coarity_type current_type = current_lambda_ctx.coarity_stack.back();

        if (current_type != old_type)
            program.append_opcode(current_type == coarity_type::any ? opcode::set_coarity_any : opcode::set_coarity_one);
    }
}

void compiler::push_coarity(coarity_type type) {
    auto& current_lambda_ctx = get_current_lambda();

    if (
        current_lambda_ctx.coarity_stack.empty()
        or current_lambda_ctx.coarity_stack.back() != type
    )
        program.append_opcode(type == coarity_type::any ? opcode::set_coarity_any : opcode::set_coarity_one);

    current_lambda_ctx.coarity_stack.emplace_back(type);
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

void compiler::set_coarity(coarity_type type) {
    auto& current_coarity_stack = get_current_lambda().coarity_stack;

    if (current_coarity_stack.empty())
        throw std::runtime_error("can't set coarity on empty coarity stack");

    if (current_coarity_stack.back() != type) {
        program.append_opcode(type == coarity_type::any ? opcode::set_coarity_any : opcode::set_coarity_one);
        current_coarity_stack[current_coarity_stack.size() - 1] = type;
    }
}
