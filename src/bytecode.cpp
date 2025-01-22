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

void bytecode::backpatch_jump(const size_t backpatch_index) {
    const size_t jump_size = code.size() - backpatch_index;

    if (jump_size > std::numeric_limits<jump_size_type>::max())
        throw std::runtime_error("jump size is too large for its type");

    aligned_write<jump_size_type>(jump_size, code.data() + backpatch_index);
}

const result_variant& bytecode::get_constant(uint8_t index) const {
    if (index >= constants.size())
        throw std::runtime_error("constant index out of bounds");

    return constants[index];
}

size_t bytecode::prepare_backpatch_jump(const opcode jump_type) {
    code.emplace_back(static_cast<uint8_t>(jump_type));

    const size_t backpatch_index = code.size();

    code.resize(code.size() + get_aligned_access_size<jump_size_type>(&(code.back()) + 1));

    return backpatch_index;
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
