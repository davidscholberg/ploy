#include <format>
#include <functional>
#include <stdexcept>
#include <variant>

#include "overload.hpp"
#include "virtual_machine.hpp"

static constexpr overload boolean_eval_visitor{
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

    constexpr overload unary_visitor{
        [](const int64_t& a) {
            return scheme_value{Op<int64_t>()(Identity, a)};
        },
        [](const double& a) {
            return scheme_value{Op<double>()(Identity, a)};
        },
        []([[maybe_unused]] const auto& a) -> scheme_value {
            throw std::runtime_error("unexpected type for unary op");
        },
    };

    constexpr overload binary_visitor{
        [](const int64_t& a, const int64_t& b) {
            return scheme_value{Op<int64_t>()(a, b)};
        },
        [](const int64_t& a, const double& b) {
            return scheme_value{Op<double>()(a, b)};
        },
        [](const double& a, const int64_t& b) {
            return scheme_value{Op<double>()(a, b)};
        },
        [](const double& a, const double& b) {
            return scheme_value{Op<double>()(a, b)};
        },
        []([[maybe_unused]] const auto& a, [[maybe_unused]] const auto& b) -> scheme_value {
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

        scheme_value result = vm->stack[first_i];
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

    constexpr overload unary_visitor{
        [](const int64_t& a) {
            return scheme_value{a % 2 != 0};
        },
        []([[maybe_unused]] const auto& a) -> scheme_value {
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
    instruction_ptr = program.code.data();

    while (true) {
        switch (*instruction_ptr) {
            case static_cast<uint8_t>(opcode::call):
                execute_call();
                break;
            case static_cast<uint8_t>(opcode::push_constant):
                instruction_ptr++;
                stack.emplace_back(program.get_constant(*instruction_ptr));
                break;
            case static_cast<uint8_t>(opcode::push_frame_index):
                call_frame_stack.emplace_back(stack.size(), nullptr);
                break;
            case static_cast<uint8_t>(opcode::jump_forward_if_not):
                if (stack.empty())
                    throw std::runtime_error("stack empty for conditional jump");

                instruction_ptr++;

                if (!std::visit(boolean_eval_visitor, stack.back()))
                    instruction_ptr += program.aligned_read<bytecode::jump_size_type>(instruction_ptr);
                else
                    instruction_ptr += get_aligned_access_size<bytecode::jump_size_type>(instruction_ptr);

                stack.pop_back();

                continue;
            case static_cast<uint8_t>(opcode::jump_forward):
                instruction_ptr++;
                instruction_ptr += program.aligned_read<bytecode::jump_size_type>(instruction_ptr);
                continue;
            case static_cast<uint8_t>(opcode::halt):
                return;
        }

        instruction_ptr++;
    }
}

void virtual_machine::execute_call() {
    // TODO: when we add lambdas, we'll need to handle the return addr of the call frame here

    if (stack.empty())
        throw std::runtime_error("stack empty for procedure call");

    if (call_frame_stack.empty())
        throw std::runtime_error("call frame stack empty for procedure call");

    const call_frame& current_call_frame = call_frame_stack.back();
    size_t argc = stack.size() - 1 - current_call_frame.frame_index;
    if (argc > std::numeric_limits<uint8_t>::max())
        throw std::runtime_error("exceeded max number of args allowed");

    const auto bp_variant = stack[current_call_frame.frame_index];
    if (const auto* bp_ptr = std::get_if<builtin_procedure>(&bp_variant)) {
        (*bp_ptr)(this, argc);
    } else {
        throw std::runtime_error("expected builtin procedure on stack top");
    }

    call_frame_stack.pop_back();
}

void virtual_machine::pop_excess(const size_t return_value_count) {
    // TODO: no clue if this is correct for cases when the return value count > argc + 1
    stack.erase(stack.begin() + call_frame_stack.back().frame_index + return_value_count, stack.end());
}

std::string virtual_machine::to_string() const {
    std::string str = "stack: [";
    for (const auto& v : stack)
        str += std::visit(
            overload{
                [](const builtin_procedure& v) {
                    return std::format("{}, ", reinterpret_cast<void*>(v));
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
