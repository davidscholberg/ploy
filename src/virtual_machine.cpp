#include <format>
#include <functional>
#include <stdexcept>
#include <variant>

#include "overload.hpp"
#include "virtual_machine.hpp"

template <template <typename> typename Op, uint8_t Identity, bool AllowNoArgs>
void builtin_fold_left(void* vm_void_ptr, uint8_t argc) {
    virtual_machine* vm = static_cast<virtual_machine*>(vm_void_ptr);

    constexpr overload unary_visitor{
        [](const int64_t& a) {
            return result_variant{Op<int64_t>()(Identity, a)};
        },
        [](const double& a) {
            return result_variant{Op<double>()(Identity, a)};
        },
        []([[maybe_unused]] const auto& a) -> result_variant {
            throw std::runtime_error("unexpected type for unary op");
        },
    };

    constexpr overload binary_visitor{
        [](const int64_t& a, const int64_t& b) {
            return result_variant{Op<int64_t>()(a, b)};
        },
        [](const int64_t& a, const double& b) {
            return result_variant{Op<double>()(a, b)};
        },
        [](const double& a, const int64_t& b) {
            return result_variant{Op<double>()(a, b)};
        },
        [](const double& a, const double& b) {
            return result_variant{Op<double>()(a, b)};
        },
        []([[maybe_unused]] const auto& a, [[maybe_unused]] const auto& b) -> result_variant {
            throw std::runtime_error("unexpected type for binary op");
        },
    };

    if constexpr (AllowNoArgs) {
        if (argc == 0) {
            vm->stack.emplace_back(Identity);
            return;
        }
    } else {
        if (argc == 0)
            throw std::runtime_error("need at least one arg for this procedure");
    }

    size_t last_i = vm->stack.size() - 1;

    if (argc == 1) {
        vm->stack[last_i] = std::visit(unary_visitor, vm->stack[last_i]);
    } else {
        size_t first_i = last_i + 1 - argc;

        result_variant result = vm->stack[first_i];
        for (size_t i = first_i + 1; i <= last_i; i++)
            result = std::visit(binary_visitor, result, vm->stack[i]);

        vm->stack[first_i] = result;
        vm->stack.erase(vm->stack.end() - argc + 1, vm->stack.end());
    }
}

void builtin_divide(void* vm_void_ptr, uint8_t argc) {
    builtin_fold_left<std::divides, 1, false>(vm_void_ptr, argc);
}

// TODO: this should not be a procedure call but rather a special form that conditionally evaluates
// the consequent and alternate (i.e. second and third expression args). Need to add conditional
// bytecode jumps for that (which also means we'll probably need to start caring about the bytecode
// array alignment assuming we'll need to jump more than 255 bytes at a time).
void builtin_if(void* vm_void_ptr, uint8_t argc) {
    virtual_machine* vm = static_cast<virtual_machine*>(vm_void_ptr);

    constexpr overload unary_visitor{
        [](const bool& a) -> bool {
            return a;
        },
        // Any value that's not explicitly a boolean false is considered true.
        [](const auto& a) -> bool {
            return true;
        },
    };

    if (argc != 3)
        throw std::runtime_error("procedure can only take three args");

    size_t alternate_i = vm->stack.size() - 1;
    size_t consequent_i = alternate_i - 1;
    size_t cond_i = alternate_i - 2;

    if (std::visit(unary_visitor, vm->stack[cond_i]))
        vm->stack[cond_i] = vm->stack[consequent_i];
    else
        vm->stack[cond_i] = vm->stack[alternate_i];

    vm->stack.erase(vm->stack.end() - 2, vm->stack.end());
}

void builtin_minus(void* vm_void_ptr, uint8_t argc) {
    builtin_fold_left<std::minus, 0, false>(vm_void_ptr, argc);
}

void builtin_multiply(void* vm_void_ptr, uint8_t argc) {
    builtin_fold_left<std::multiplies, 1, true>(vm_void_ptr, argc);
}

void builtin_odd(void* vm_void_ptr, uint8_t argc) {
    virtual_machine* vm = static_cast<virtual_machine*>(vm_void_ptr);

    constexpr overload unary_visitor{
        [](const int64_t& a) {
            return result_variant{a % 2 != 0};
        },
        []([[maybe_unused]] const auto& a) -> result_variant {
            throw std::runtime_error("unexpected type for unary op");
        },
    };

    if (argc != 1)
        throw std::runtime_error("procedure can only take one arg");

    size_t last_i = vm->stack.size() - 1;

    vm->stack[last_i] = std::visit(unary_visitor, vm->stack[last_i]);
}

void builtin_plus(void* vm_void_ptr, uint8_t argc) {
    builtin_fold_left<std::plus, 0, true>(vm_void_ptr, argc);
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
            case static_cast<uint8_t>(opcode::ret):
                return;
        }

        instruction_ptr++;
    }
}

void virtual_machine::execute_call() {
    if (stack.empty())
        throw std::runtime_error("stack empty for procedure call");

    instruction_ptr++;
    uint8_t argc = *instruction_ptr;

    if (argc > stack.size() - 1)
        throw std::runtime_error("not enough procedure args on stack");

    const auto bp_variant = stack.back();
    if (const auto* bp_ptr = std::get_if<builtin_procedure>(&bp_variant)) {
        stack.pop_back();
        (*bp_ptr)(this, argc);
    } else {
        throw std::runtime_error("expected builtin procedure on stack top");
    }
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
