#include <format>
#include <limits>
#include <stdexcept>
#include <variant>

#include "bytecode.hpp"
#include "overload.hpp"

uint8_t bytecode::add_constant(const result_variant& new_constant) {
    if (constants.size() == std::numeric_limits<uint8_t>::max())
        throw std::runtime_error("exceeded max number of constants allowed");

    if (constant_to_index_map.contains(new_constant)) {
        return constant_to_index_map[new_constant];
    }

    constants.emplace_back(new_constant);
    const auto i = constants.size() - 1;
    constant_to_index_map[new_constant] = i;
    return i;
}

const result_variant& bytecode::get_constant(uint8_t index) const {
    if (index >= constants.size())
        throw std::runtime_error("constant index out of bounds");

    return constants[index];
}

std::string bytecode::to_string() const {
    std::string str = "constants: [";
    for (const auto& c : constants)
        str += std::visit(
            overload{
                [](const builtin_procedure& v) {
                    return std::format("{}, ", reinterpret_cast<void*>(v));
                },
                [](const auto& v) {
                    return std::format("{}, ", v);
                },
            },
            c
        );

    str += "]\nbytecode: [";
    for (const auto& b : code)
        str += std::format("{}, ", b);

    str += "]";
    return str;
}
