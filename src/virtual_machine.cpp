#include <format>
#include <functional>
#include <memory>
#include <stdexcept>
#include <variant>

#include "overload.hpp"
#include "virtual_machine.hpp"

static constexpr overload scheme_constant_to_stack_value_visitor{
    [](const lambda_constant& v) -> stack_value {
        return stack_value{std::make_shared<lambda>(lambda{{}, v})};
    },
    [](const auto& v) -> stack_value {
        return stack_value{v};
    },
};

static constexpr overload stack_value_to_scheme_value_ptr_visitor{
    [](const scheme_value_ptr& v) -> scheme_value_ptr {
        return v;
    },
    [](const auto& v) -> scheme_value_ptr {
        return std::make_shared<scheme_value>(v);
    },
};

static constexpr stack_value_overload boolean_eval_visitor{
    [](const bool& a) -> bool {
        return a;
    },
    // Any value that's not explicitly a boolean false is considered true.
    [](const auto& a) -> bool {
        return true;
    },
};

template <template <typename> typename Op, uint8_t Identity, bool AllowNoArgs>
static void native_fold_left(void* vm_void_ptr, uint8_t argc) {
    virtual_machine* vm = static_cast<virtual_machine*>(vm_void_ptr);

    constexpr stack_value_overload unary_visitor{
        [](const int64_t& a) -> stack_value {
            return stack_value{Op<int64_t>()(Identity, a)};
        },
        [](const double& a) -> stack_value {
            return stack_value{Op<double>()(Identity, a)};
        },
        [](const auto&) -> stack_value {
            throw std::runtime_error("unexpected type for unary op");
        },
    };

    constexpr stack_value_overload binary_visitor{
        [](const int64_t& a, const int64_t& b) -> stack_value {
            return stack_value{Op<int64_t>()(a, b)};
        },
        [](const int64_t& a, const double& b) -> stack_value {
            return stack_value{Op<double>()(a, b)};
        },
        [](const double& a, const int64_t& b) -> stack_value {
            return stack_value{Op<double>()(a, b)};
        },
        [](const double& a, const double& b) -> stack_value {
            return stack_value{Op<double>()(a, b)};
        },
        [](const auto&, const auto&) -> stack_value {
            throw std::runtime_error("unexpected type for binary op");
        },
    };

    if constexpr (AllowNoArgs) {
        if (argc == 0) {
            vm->stack[vm->stack.size() - 1] = Identity;
            return;
        }
    } else {
        if (argc == 0)
            throw std::runtime_error("need at least one arg for this procedure");
    }

    size_t last_i = vm->stack.size() - 1;

    if (argc == 1) {
        vm->stack[last_i - 1] = std::visit(unary_visitor, vm->stack[last_i]);
    } else {
        size_t first_i = last_i + 1 - argc;

        stack_value result = vm->stack[first_i];
        for (size_t i = first_i + 1; i <= last_i; i++)
            result = std::visit(binary_visitor, result, vm->stack[i]);

        vm->stack[first_i - 1] = result;
    }

    vm->pop_excess(1);
}

void builtin_divide(void* vm_void_ptr, uint8_t argc) {
    native_fold_left<std::divides, 1, false>(vm_void_ptr, argc);
}

void builtin_minus(void* vm_void_ptr, uint8_t argc) {
    native_fold_left<std::minus, 0, false>(vm_void_ptr, argc);
}

void builtin_multiply(void* vm_void_ptr, uint8_t argc) {
    native_fold_left<std::multiplies, 1, true>(vm_void_ptr, argc);
}

void builtin_odd(void* vm_void_ptr, uint8_t argc) {
    virtual_machine* vm = static_cast<virtual_machine*>(vm_void_ptr);

    constexpr stack_value_overload unary_visitor{
        [](const int64_t& a) -> stack_value {
            return stack_value{a % 2 != 0};
        },
        [](const auto&) -> stack_value {
            throw std::runtime_error("unexpected type for unary op");
        },
    };

    if (argc != 1)
        throw std::runtime_error("procedure can only take one arg");

    size_t last_i = vm->stack.size() - 1;

    vm->stack[last_i - 1] = std::visit(unary_visitor, vm->stack[last_i]);
    vm->pop_excess(1);
}

void builtin_plus(void* vm_void_ptr, uint8_t argc) {
    native_fold_left<std::plus, 0, true>(vm_void_ptr, argc);
}

void virtual_machine::execute(const bytecode& program) {
    begin_instruction_ptr = program.code.data();
    instruction_ptr = begin_instruction_ptr;

    while (true) {
        switch (*instruction_ptr) {
            case static_cast<uint8_t>(opcode::push_constant):
                instruction_ptr++;
                stack.emplace_back(std::visit(
                    scheme_constant_to_stack_value_visitor,
                    program.get_constant(*instruction_ptr)
                ));
                break;
            case static_cast<uint8_t>(opcode::push_shared_var):
                execute_push_shared_var();
                break;
            case static_cast<uint8_t>(opcode::push_stack_var):
                execute_push_stack_var();
                break;
            case static_cast<uint8_t>(opcode::capture_shared_var):
                execute_capture_shared_var();
                break;
            case static_cast<uint8_t>(opcode::capture_stack_var):
                execute_capture_stack_var();
                break;
            case static_cast<uint8_t>(opcode::push_frame_index):
                call_frame_stack.emplace_back(lambda_ptr{}, stack.size(), nullptr);
                break;
            case static_cast<uint8_t>(opcode::call):
                execute_call();
                break;
            case static_cast<uint8_t>(opcode::expect_argc):
                execute_expect_argc();
                break;
            case static_cast<uint8_t>(opcode::ret):
                execute_ret();
                break;
            case static_cast<uint8_t>(opcode::jump_forward_if_not):
                if (stack.empty())
                    throw std::runtime_error("stack empty for conditional jump");

                instruction_ptr++;

                if (!std::visit(boolean_eval_visitor, stack.back()))
                    instruction_ptr += bytecode::read_value<bytecode::jump_size_type>(instruction_ptr);
                else
                    instruction_ptr += sizeof(bytecode::jump_size_type);

                stack.pop_back();

                continue;
            case static_cast<uint8_t>(opcode::jump_forward):
                instruction_ptr++;
                instruction_ptr += bytecode::read_value<bytecode::jump_size_type>(instruction_ptr);
                continue;
            case static_cast<uint8_t>(opcode::halt):
                return;
        }

        instruction_ptr++;
    }
}

void virtual_machine::execute_capture_shared_var() {
    instruction_ptr++;
    size_t shared_var_index = *instruction_ptr;

    auto executing_lambda = get_executing_lambda();

    if (shared_var_index >= executing_lambda->captures.size())
        throw std::runtime_error("parent lambda capture index out of bounds");

    const auto current_lambda_variant = stack.back();
    if (auto* current_lambda_ptr = std::get_if<lambda_ptr>(&current_lambda_variant)) {
        (*current_lambda_ptr)->captures.emplace_back(executing_lambda->captures[shared_var_index]);
    } else {
        throw std::runtime_error("expected lambda on stack top for capture");
    }
}

void virtual_machine::execute_capture_stack_var() {
    instruction_ptr++;
    size_t stack_var_index = get_executing_call_frame().frame_index + 1 + (*instruction_ptr);

    if (stack_var_index >= stack.size())
        throw std::runtime_error("stack empty for capture");

    scheme_value_ptr value = std::visit(stack_value_to_scheme_value_ptr_visitor, stack[stack_var_index]);
    stack[stack_var_index] = value;

    const auto current_lambda_variant = stack.back();
    if (auto* current_lambda_ptr = std::get_if<lambda_ptr>(&current_lambda_variant)) {
        (*current_lambda_ptr)->captures.emplace_back(value);
    } else {
        throw std::runtime_error("expected lambda on stack top for capture");
    }
}

void virtual_machine::execute_expect_argc() {
    if (call_frame_stack.empty())
        throw std::runtime_error("call frame stack empty for expect_argc");

    instruction_ptr++;
    if (*instruction_ptr != call_frame_stack.back().argc)
        throw std::runtime_error("expected argc does not match actual argc");
}

void virtual_machine::execute_push_shared_var() {
    auto executing_lambda = get_executing_lambda();

    instruction_ptr++;
    size_t shared_var_index = *instruction_ptr;
    if (shared_var_index >= executing_lambda->captures.size())
        throw std::runtime_error("parent lambda capture index out of bounds");

    stack.emplace_back(executing_lambda->captures[shared_var_index]);
}

void virtual_machine::execute_push_stack_var() {
    instruction_ptr++;
    size_t stack_var_index = get_executing_call_frame().frame_index + 1 + (*instruction_ptr);

    if (stack_var_index >= stack.size())
        throw std::runtime_error("stack empty for capture");

    stack.emplace_back(stack[stack_var_index]);
}

void virtual_machine::execute_call() {
    if (stack.empty())
        throw std::runtime_error("stack empty for procedure call");

    if (call_frame_stack.empty())
        throw std::runtime_error("call frame stack empty for procedure call");

    call_frame& current_call_frame = call_frame_stack.back();

    size_t argc = stack.size() - 1 - current_call_frame.frame_index;
    if (argc > std::numeric_limits<uint8_t>::max())
        throw std::runtime_error("exceeded max number of args allowed");

    const auto callable_variant = stack[current_call_frame.frame_index];
    if (const auto* bp_ptr = std::get_if<builtin_procedure>(&callable_variant)) {
        (*bp_ptr)(this, argc);

        call_frame_stack.pop_back();
    } else if (const auto* lambda_ptr_ptr = std::get_if<lambda_ptr>(&callable_variant)) {
        current_call_frame.executing_lambda = *lambda_ptr_ptr;
        current_call_frame.argc = argc;
        current_call_frame.return_ptr = instruction_ptr;
        instruction_ptr = begin_instruction_ptr + (*lambda_ptr_ptr)->bytecode_offset - 1;
    } else {
        throw std::runtime_error("expected callable at frame index");
    }
}

void virtual_machine::execute_ret() {
    if (call_frame_stack.empty())
        throw std::runtime_error("call frame stack empty for ret");

    const auto& current_call_frame = call_frame_stack.back();
    size_t shift_offset = current_call_frame.argc + 1;
    for (size_t i = current_call_frame.frame_index + 1 + current_call_frame.argc; i < stack.size(); i++)
        stack[i - shift_offset] = stack[i];

    // TODO: i think we can remove the above for loop and just use this call to erase (but modify it
    // to remove the callable and args from the stack)
    stack.erase(stack.end() - shift_offset, stack.end());

    instruction_ptr = call_frame_stack.back().return_ptr;
    call_frame_stack.pop_back();
}

call_frame& virtual_machine::get_executing_call_frame() {
    for (auto it = call_frame_stack.rbegin(); it != call_frame_stack.rend(); it++)
        if (it->executing_lambda)
            return *it;

    throw std::runtime_error("no executing call frame");
}

lambda_ptr& virtual_machine::get_executing_lambda() {
    return get_executing_call_frame().executing_lambda;
}

void virtual_machine::pop_excess(const size_t return_value_count) {
    const auto& current_call_frame = call_frame_stack.back();
    // TODO: fix for result count being greater than argc + 1
    stack.erase(stack.begin() + current_call_frame.frame_index + return_value_count, stack.end());
}

std::string virtual_machine::to_string() const {
    std::string str = "stack: [";
    for (const auto& v : stack)
        str += std::visit(
            stack_value_overload{
                [](const builtin_procedure& v) {
                    return std::format("bp: {}, ", reinterpret_cast<void*>(v));
                },
                [](const lambda_ptr& v) {
                    return std::format("lambda: {}, ", v->bytecode_offset);
                },
                [](const auto& v) {
                    return std::format("{}, ", v);
                },
            },
            v
        );

    str += "]";
    return str;
}
