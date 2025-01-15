#include <format>
#include <functional>
#include <stdexcept>
#include <variant>

#include "overload.hpp"
#include "virtual_machine.hpp"

void virtual_machine::execute(const bytecode& program) {
    instruction_ptr = program.code.data();

    while (true) {
        switch (*instruction_ptr) {
            case static_cast<uint8_t>(opcode::add):
                execute_binary_op<std::plus>();
                break;
            case static_cast<uint8_t>(opcode::constant):
                instruction_ptr++;
                stack.emplace_back(program.constants[*instruction_ptr]);
                break;
            case static_cast<uint8_t>(opcode::divide):
                execute_binary_op<std::divides>();
                break;
            case static_cast<uint8_t>(opcode::multiply):
                execute_binary_op<std::multiplies>();
                break;
            case static_cast<uint8_t>(opcode::negate):
                execute_unary_op<std::negate>();
                break;
            case static_cast<uint8_t>(opcode::subtract):
                execute_binary_op<std::minus>();
                break;
            case static_cast<uint8_t>(opcode::ret):
                return;
        }

        instruction_ptr++;
    }
}

template <template <typename> typename Op>
void virtual_machine::execute_binary_op() {
    if (stack.size() < 2)
        throw std::runtime_error("not enough operands on stack for binary op");

    // This will eventually be expanded to handle floats and maybe some other types.
    constexpr overload visitor{
        [](const int64_t& a, const int64_t& b) {
            return result_variant{Op<int64_t>()(a, b)};
        },
        []([[maybe_unused]] const auto& a, [[maybe_unused]] const auto& b) {
            throw std::runtime_error("unexpected type for binary op");
        },
    };

    size_t last_i = stack.size() - 1;
    const auto& a = stack[last_i - 1];
    const auto& b = stack[last_i];

    stack[last_i - 1] = std::visit(visitor, a, b);
    stack.pop_back();
}

template <template <typename> typename Op>
void virtual_machine::execute_unary_op() {
    if (stack.empty())
        throw std::runtime_error("stack empty for unary op");

    constexpr overload visitor{
        [](const int64_t& a) {
            return result_variant{Op<int64_t>()(a)};
        },
        []([[maybe_unused]] const auto& a) {
            throw std::runtime_error("unexpected type for unary op");
        },
    };

    size_t last_i = stack.size() - 1;
    stack[last_i] = std::visit(visitor, stack[last_i]);
}

std::string virtual_machine::to_string() const {
    std::string str = "stack: [";
    for (const auto& v : stack)
        str += std::visit(
            [](const auto& v) {
                return std::format("{}, ", v);
            },
            v
        );

    str += "]";
    return str;
}
