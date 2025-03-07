#include <format>
#include <functional>
#include <memory>
#include <print>
#include <stdexcept>
#include <variant>

#include "overload.hpp"
#include "virtual_machine.hpp"

static constexpr overload scheme_constant_to_stack_value_visitor{
    [](const hand_rolled_procedure_constant& v) -> stack_value {
        return stack_value{std::make_shared<lambda>(std::vector<scheme_value_ptr>{}, v.bytecode_offset)};
    },
    [](const lambda_constant& v) -> stack_value {
        return stack_value{std::make_shared<lambda>(std::vector<scheme_value_ptr>{}, v.bytecode_offset)};
    },
    [](const auto& v) -> stack_value {
        return stack_value{v};
    },
};

static constexpr overload scheme_value_to_stack_value_visitor{
    [](const auto& v) -> stack_value {
        return stack_value{v};
    },
};

static constexpr overload stack_value_to_scheme_value_visitor{
    [](const scheme_value_ptr& v) -> scheme_value {
        return *v;
    },
    [](const auto& v) -> scheme_value {
        return v;
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
    [](const auto&) -> bool {
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
            return stack_value{Op<double>()(static_cast<double>(a), b)};
        },
        [](const double& a, const int64_t& b) -> stack_value {
            return stack_value{Op<double>()(a, static_cast<double>(b))};
        },
        [](const double& a, const double& b) -> stack_value {
            return stack_value{Op<double>()(a, b)};
        },
        [](const auto&, const auto&) -> stack_value {
            throw std::runtime_error("unexpected type for binary op");
        },
    };

    if constexpr (AllowNoArgs) {
        // if vm coarity state is any, clear call frame and return immediately since this procedure
        // produces no side effects.
        if (vm->coarity_state == coarity_type::any) {
            vm->clear_call_frame();
            return;
        }

        if (argc == 0) {
            vm->stack[vm->stack.size() - 1] = int64_t{Identity};
            return;
        }
    } else {
        if (argc == 0)
            throw std::runtime_error("need at least one arg for this procedure");
    }

    // if vm coarity state is any, clear call frame and return immediately since this procedure
    // produces no side effects.
    if (vm->coarity_state == coarity_type::any) {
        vm->clear_call_frame();
        return;
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

template <template <typename> typename Op>
static void native_monotonic_reduce(void* vm_void_ptr, uint8_t argc) {
    virtual_machine* vm = static_cast<virtual_machine*>(vm_void_ptr);

    constexpr stack_value_overload binary_visitor{
        [](const int64_t& a, const int64_t& b) -> bool {
            return Op<int64_t>()(a, b);
        },
        [](const int64_t& a, const double& b) -> bool {
            return Op<double>()(static_cast<double>(a), b);
        },
        [](const double& a, const int64_t& b) -> bool {
            return Op<double>()(a, static_cast<double>(b));
        },
        [](const double& a, const double& b) -> bool {
            return Op<double>()(a, b);
        },
        [](const auto&, const auto&) -> bool {
            throw std::runtime_error("unexpected type for binary op");
        },
    };

    if (argc < 2)
        throw std::runtime_error("need at least two args for this procedure");

    // if vm coarity state is any, clear call frame and return immediately since this procedure
    // produces no side effects.
    if (vm->coarity_state == coarity_type::any) {
        vm->clear_call_frame();
        return;
    }

    size_t last_i = vm->stack.size() - 1;
    size_t first_i = last_i + 1 - argc;

    bool reduction = true;
    for (size_t i = first_i; i < last_i; i++)
        if (!std::visit(binary_visitor, vm->stack[i], vm->stack[i + 1])) {
            reduction = false;
            break;
        }

    vm->stack[first_i - 1] = reduction;

    vm->pop_excess(1);
}

void builtin_car(void* vm_void_ptr, uint8_t argc) {
    virtual_machine* vm = static_cast<virtual_machine*>(vm_void_ptr);

    if (argc != 1)
        throw std::runtime_error("procedure needs one arg");

    // if vm coarity state is any, clear call frame and return immediately since this procedure
    // produces no side effects.
    if (vm->coarity_state == coarity_type::any) {
        vm->clear_call_frame();
        return;
    }

    size_t pair_i = vm->stack.size() - 1;
    size_t dest_i = pair_i - 1;
    vm->stack[dest_i] = std::visit(
        scheme_value_to_stack_value_visitor,
        std::visit(
            stack_value_overload{
                [](const pair_ptr& a) -> scheme_value {
                    return a->car;
                },
                [](const auto&) -> scheme_value {
                    throw std::runtime_error("unexpected type for car");
                },
            },
            vm->stack[pair_i]
        )
    );

    vm->pop_excess(1);
}

void builtin_cdr(void* vm_void_ptr, uint8_t argc) {
    virtual_machine* vm = static_cast<virtual_machine*>(vm_void_ptr);

    if (argc != 1)
        throw std::runtime_error("procedure needs one arg");

    // if vm coarity state is any, clear call frame and return immediately since this procedure
    // produces no side effects.
    if (vm->coarity_state == coarity_type::any) {
        vm->clear_call_frame();
        return;
    }

    size_t pair_i = vm->stack.size() - 1;
    size_t dest_i = pair_i - 1;
    vm->stack[dest_i] = std::visit(
        scheme_value_to_stack_value_visitor,
        std::visit(
            stack_value_overload{
                [](const pair_ptr& a) -> scheme_value {
                    return a->cdr;
                },
                [](const auto&) -> scheme_value {
                    throw std::runtime_error("unexpected type for cdr");
                },
            },
            vm->stack[pair_i]
        )
    );

    vm->pop_excess(1);
}

void builtin_cons(void* vm_void_ptr, uint8_t argc) {
    virtual_machine* vm = static_cast<virtual_machine*>(vm_void_ptr);

    if (argc != 2)
        throw std::runtime_error("procedure needs two args");

    // if vm coarity state is any, clear call frame and return immediately since this procedure
    // produces no side effects.
    if (vm->coarity_state == coarity_type::any) {
        vm->clear_call_frame();
        return;
    }

    vm->execute_cons(2);
    vm->pop_excess(1);
}

void builtin_display(void* vm_void_ptr, uint8_t argc) {
    virtual_machine* vm = static_cast<virtual_machine*>(vm_void_ptr);

    // TODO: need to update this when ports are added
    if (argc != 1)
        throw std::runtime_error("procedure only takes one arg");

    std::print("{}", std::visit(stack_value_formatter_overload<false>{}, vm->stack.back()));

    vm->clear_call_frame();
}

void builtin_divide(void* vm_void_ptr, uint8_t argc) {
    native_fold_left<std::divides, 1, false>(vm_void_ptr, argc);
}

void builtin_equal_numeric(void* vm_void_ptr, uint8_t argc) {
    native_monotonic_reduce<std::equal_to>(vm_void_ptr, argc);
}

void builtin_eqv(void* vm_void_ptr, uint8_t argc) {
    virtual_machine* vm = static_cast<virtual_machine*>(vm_void_ptr);

    if (argc != 2)
        throw std::runtime_error("procedure needs two args");

    // if vm coarity state is any, clear call frame and return immediately since this procedure
    // produces no side effects.
    if (vm->coarity_state == coarity_type::any) {
        vm->clear_call_frame();
        return;
    }

    constexpr stack_value_overload eqv_visitor{
        []<typename T>(const T& a, const T& b) -> bool {
            return a == b;
        },
        []<typename T, typename U>(const T&, const U&) -> bool {
            return false;
        },
    };

    size_t second_i = vm->stack.size() - 1;
    size_t first_i = second_i - 1;

    vm->stack[first_i - 1] = std::visit(eqv_visitor, vm->stack[first_i], vm->stack[second_i]);

    vm->pop_excess(1);
}

void builtin_greater(void* vm_void_ptr, uint8_t argc) {
    native_monotonic_reduce<std::greater>(vm_void_ptr, argc);
}

void builtin_greater_equal(void* vm_void_ptr, uint8_t argc) {
    native_monotonic_reduce<std::greater_equal>(vm_void_ptr, argc);
}

void builtin_less(void* vm_void_ptr, uint8_t argc) {
    native_monotonic_reduce<std::less>(vm_void_ptr, argc);
}

void builtin_less_equal(void* vm_void_ptr, uint8_t argc) {
    native_monotonic_reduce<std::less_equal>(vm_void_ptr, argc);
}

void builtin_minus(void* vm_void_ptr, uint8_t argc) {
    native_fold_left<std::minus, 0, false>(vm_void_ptr, argc);
}

void builtin_multiply(void* vm_void_ptr, uint8_t argc) {
    native_fold_left<std::multiplies, 1, true>(vm_void_ptr, argc);
}

void builtin_newline(void* vm_void_ptr, uint8_t argc) {
    virtual_machine* vm = static_cast<virtual_machine*>(vm_void_ptr);

    // TODO: need to update this when ports are added
    if (argc != 0)
        throw std::runtime_error("procedure takes no args");

    std::print("\n");

    vm->clear_call_frame();
}

void builtin_null(void* vm_void_ptr, uint8_t argc) {
    virtual_machine* vm = static_cast<virtual_machine*>(vm_void_ptr);

    constexpr stack_value_overload unary_visitor{
        [](const empty_list&) -> stack_value {
            return true;
        },
        [](const auto&) -> stack_value {
            return false;
        },
    };

    if (argc != 1)
        throw std::runtime_error("procedure can only take one arg");

    // if vm coarity state is any, clear call frame and return immediately since this procedure
    // produces no side effects.
    if (vm->coarity_state == coarity_type::any) {
        vm->clear_call_frame();
        return;
    }

    size_t last_i = vm->stack.size() - 1;

    vm->stack[last_i - 1] = std::visit(unary_visitor, vm->stack[last_i]);
    vm->pop_excess(1);
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

    // if vm coarity state is any, clear call frame and return immediately since this procedure
    // produces no side effects.
    if (vm->coarity_state == coarity_type::any) {
        vm->clear_call_frame();
        return;
    }

    size_t last_i = vm->stack.size() - 1;

    vm->stack[last_i - 1] = std::visit(unary_visitor, vm->stack[last_i]);
    vm->pop_excess(1);
}

void builtin_plus(void* vm_void_ptr, uint8_t argc) {
    native_fold_left<std::plus, 0, true>(vm_void_ptr, argc);
}

void virtual_machine::clear_call_frame() {
    stack.erase(stack.begin() + call_frame_stack.back().frame_index, stack.end());
}

void virtual_machine::execute(const bytecode& program) {
    begin_instruction_ptr = program.code.data();
    instruction_ptr = begin_instruction_ptr;

    while (true) {
        switch (*instruction_ptr) {
            case static_cast<uint8_t>(opcode::push_constant):
                instruction_ptr++;

                if (coarity_state == coarity_type::any)
                    break;

                stack.emplace_back(std::visit(
                    scheme_constant_to_stack_value_visitor,
                    program.get_constant(*instruction_ptr)
                ));
                break;
            case static_cast<uint8_t>(opcode::cons):
                if (coarity_state == coarity_type::any)
                    break;

                execute_cons();
                stack.pop_back();
                break;
            case static_cast<uint8_t>(opcode::push_shared_var):
                if (coarity_state == coarity_type::any) {
                    instruction_ptr++;
                    break;
                }

                execute_push_shared_var();
                break;
            case static_cast<uint8_t>(opcode::push_stack_var):
                if (coarity_state == coarity_type::any) {
                    instruction_ptr++;
                    break;
                }

                execute_push_stack_var();
                break;
            case static_cast<uint8_t>(opcode::set_shared_var):
                execute_set_shared_var();
                break;
            case static_cast<uint8_t>(opcode::set_stack_var):
                execute_set_stack_var();
                break;
            case static_cast<uint8_t>(opcode::add_stack_var):
                get_executing_call_frame().stack_var_count++;
                break;
            case static_cast<uint8_t>(opcode::set_coarity_any):
                coarity_state = coarity_type::any;
                break;
            case static_cast<uint8_t>(opcode::set_coarity_one):
                coarity_state = coarity_type::one;
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
                    instruction_ptr += bytecode::read_value<jump_size_type>(instruction_ptr);
                else
                    instruction_ptr += sizeof(jump_size_type);

                stack.pop_back();

                continue;
            case static_cast<uint8_t>(opcode::jump_forward):
                instruction_ptr++;
                instruction_ptr += bytecode::read_value<jump_size_type>(instruction_ptr);
                continue;
            case static_cast<uint8_t>(opcode::halt):
                return;
            case static_cast<uint8_t>(opcode::push_continuation):
                execute_push_continuation();
                break;
        }

        instruction_ptr++;
    }
}

void virtual_machine::execute_capture_shared_var() {
    instruction_ptr++;
    size_t shared_var_index = *instruction_ptr;

    auto executing_lambda = get_executing_lambda();

    if (shared_var_index >= executing_lambda->captures.size())
        throw std::runtime_error("parent lambda capture index out of bounds for capture");

    const auto& value = executing_lambda->captures[shared_var_index];

    // TODO: if capture is self-referential, make it a weak_ptr in the captures vector.
    static const stack_value_overload lambda_visitor{
        [&value](const lambda_ptr& l_ptr) -> void {
            l_ptr->captures.emplace_back(value);
        },
        [](const auto&) -> void {
            throw std::runtime_error("expected lambda on stack top for capture");
        },
    };

    std::visit(lambda_visitor, stack.back());
}

void virtual_machine::execute_capture_stack_var() {
    instruction_ptr++;
    size_t stack_var_index = get_executing_call_frame().frame_index + 1 + (*instruction_ptr);

    if (stack_var_index >= stack.size())
        throw std::runtime_error("stack empty for capture");

    const auto& value = std::visit(stack_value_to_scheme_value_ptr_visitor, stack[stack_var_index]);
    stack[stack_var_index] = value;

    // TODO: if capture is self-referential, make it a weak_ptr in the captures vector.
    static const stack_value_overload lambda_visitor{
        [&value](const lambda_ptr& l_ptr) -> void {
            l_ptr->captures.emplace_back(value);
        },
        [](const auto&) -> void {
            throw std::runtime_error("expected lambda on stack top for capture");
        },
    };

    std::visit(lambda_visitor, stack.back());
}

void virtual_machine::execute_expect_argc() {
    if (call_frame_stack.empty())
        throw std::runtime_error("call frame stack empty for expect_argc");

    instruction_ptr++;
    if (*instruction_ptr != call_frame_stack.back().stack_var_count)
        throw std::runtime_error("expected argc does not match actual argc");
}

void virtual_machine::execute_push_continuation() {
    stack.emplace_back(std::make_shared<continuation>(
        call_frame_stack,
        stack,
        coarity_state
    ));
}

void virtual_machine::execute_push_shared_var() {
    auto executing_lambda = get_executing_lambda();

    instruction_ptr++;
    size_t shared_var_index = *instruction_ptr;
    if (shared_var_index >= executing_lambda->captures.size())
        throw std::runtime_error("lambda capture index out of bounds for push");

    stack.emplace_back(std::visit(
        scheme_value_to_stack_value_visitor,
        *(executing_lambda->captures[shared_var_index])
    ));
}

void virtual_machine::execute_push_stack_var() {
    instruction_ptr++;
    size_t stack_var_index = get_executing_call_frame().frame_index + 1 + (*instruction_ptr);

    if (stack_var_index >= stack.size())
        throw std::runtime_error("stack empty for capture");

    if (const auto* sc_ptr_ptr = std::get_if<scheme_value_ptr>(&stack[stack_var_index]))
        stack.emplace_back(std::visit(scheme_value_to_stack_value_visitor, **sc_ptr_ptr));
    else
        stack.emplace_back(stack[stack_var_index]);
}

void virtual_machine::execute_set_shared_var() {
    auto executing_lambda = get_executing_lambda();

    instruction_ptr++;
    size_t shared_var_index = *instruction_ptr;
    if (shared_var_index >= executing_lambda->captures.size())
        throw std::runtime_error("lambda capture index out of bounds for set");

    *(executing_lambda->captures[shared_var_index]) = std::visit(stack_value_to_scheme_value_visitor, stack.back());

    stack.pop_back();
}

void virtual_machine::execute_set_stack_var() {
    instruction_ptr++;
    size_t stack_var_index = get_executing_call_frame().frame_index + 1 + (*instruction_ptr);

    if (stack_var_index >= stack.size())
        throw std::runtime_error("invalid stack index for set");

    if (const auto* dest_sc_ptr_ptr = std::get_if<scheme_value_ptr>(&stack[stack_var_index]))
        **dest_sc_ptr_ptr = std::visit(stack_value_to_scheme_value_visitor, stack.back());
    else if (const auto* source_sc_ptr_ptr = std::get_if<scheme_value_ptr>(&stack.back()))
        stack[stack_var_index] = std::visit(scheme_value_to_stack_value_visitor, **source_sc_ptr_ptr);
    else
        stack[stack_var_index] = stack.back();

    stack.pop_back();
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

    const auto& callable_variant = std::visit(stack_value_to_scheme_value_visitor, stack[current_call_frame.frame_index]);
    if (const auto* bp_ptr = std::get_if<builtin_procedure>(&callable_variant)) {
        (*bp_ptr)(this, static_cast<uint8_t>(argc));

        call_frame_stack.pop_back();
    } else if (const auto* lambda_ptr_ptr = std::get_if<lambda_ptr>(&callable_variant)) {
        current_call_frame.executing_lambda = *lambda_ptr_ptr;
        current_call_frame.stack_var_count = static_cast<uint8_t>(argc);
        current_call_frame.return_ptr = instruction_ptr;
        current_call_frame.return_coarity_state = coarity_state;
        instruction_ptr = begin_instruction_ptr + (*lambda_ptr_ptr)->bytecode_offset - 1;
    } else if (const auto* continuation_ptr_ptr = std::get_if<continuation_ptr>(&callable_variant)) {
        // save args passed to continuation
        const std::vector<stack_value> cont_args{stack.cbegin() + current_call_frame.frame_index + 1, stack.cend()};

        // restore continuation state
        call_frame_stack = (*continuation_ptr_ptr)->frozen_call_frame_stack;
        stack = (*continuation_ptr_ptr)->frozen_value_stack;
        coarity_state = (*continuation_ptr_ptr)->frozen_coarity_state;

        // append continuation args to stack and execute a lambda return
        stack.insert(stack.end(), cont_args.cbegin(), cont_args.cend());
        execute_ret();
    } else {
        throw std::runtime_error("expected callable at frame index");
    }
}

void virtual_machine::execute_cons() {
    execute_cons(1);
}

void virtual_machine::execute_cons(size_t dest_from_top) {
    if (stack.size() < 2)
        throw std::runtime_error("need two stack elements in order to cons");

    size_t cdr_i = stack.size() - 1;
    size_t car_i = cdr_i - 1;

    stack[cdr_i - dest_from_top] = std::make_shared<pair>(
        std::visit(stack_value_to_scheme_value_visitor, stack[car_i]),
        std::visit(stack_value_to_scheme_value_visitor, stack[cdr_i])
    );

    // NOTE: callers must pop the stack as needed
}

void virtual_machine::execute_ret() {
    if (call_frame_stack.empty())
        throw std::runtime_error("call frame stack empty for ret");

    const auto& current_call_frame = call_frame_stack.back();
    coarity_state = current_call_frame.return_coarity_state;

    if (coarity_state == coarity_type::one) {
        size_t frame_start = current_call_frame.frame_index;
        size_t return_value_start = frame_start + 1 + current_call_frame.stack_var_count;

        if (return_value_start != stack.size() - 1)
            throw std::runtime_error("expected one return value");

        stack.erase(stack.begin() + frame_start, stack.begin() + return_value_start);
    } else {
        // remove entire call frame from value stack including any return values
        clear_call_frame();
    }

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

std::string virtual_machine::stack_top_to_string() const {
    if (stack.empty())
        throw std::runtime_error("stack empty");

    return std::visit(stack_value_formatter_overload<true>{}, stack.back());
}

std::string virtual_machine::stack_to_string() const {
    std::string str = "[";
    for (const auto& v : stack)
        str += std::visit(stack_value_formatter_overload<true>{}, v) + ", ";

    str += "]";
    return str;
}

std::string virtual_machine::to_string() const {
    return std::format("vm stack: {}", stack_to_string());
}
