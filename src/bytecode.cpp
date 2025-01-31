#include <format>
#include <limits>
#include <stdexcept>
#include <unordered_map>
#include <variant>

#include "bytecode.hpp"
#include "overload.hpp"
#include "virtual_machine.hpp"

uint8_t bytecode::add_constant(const scheme_value& new_constant) {
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

std::string bytecode::disassemble() const {
    std::unordered_map<builtin_procedure, std::string_view> bp_ptr_to_name;
    for (const auto& [k, v] : bp_name_to_ptr)
        bp_ptr_to_name[v] = k;

    const overload scheme_value_formatter{
        [&bp_ptr_to_name](const builtin_procedure& v) {
            if (!bp_ptr_to_name.contains(v))
                throw std::runtime_error("builtin procedure not found by address");
            return std::format("bp: {}", bp_ptr_to_name[v]);
        },
        [](const auto& v) {
            return std::format("{}", v);
        },
    };

    constexpr auto disassembly_line_formatter = [](
        const auto offset,
        const auto opcode_name,
        const auto additional_info
    ) {
        return std::format(
            "{:>4}: {:<20} {}\n",
            offset,
            opcode_name,
            additional_info
        );
    };

    const uint8_t* instruction_ptr = code.data();
    size_t frame_index_count = 0;

    std::string str;

    while (instruction_ptr - code.data() < code.size()) {
        switch (*instruction_ptr) {
            case static_cast<uint8_t>(opcode::call):
                str += disassembly_line_formatter(
                    instruction_ptr - code.data(),
                    "call",
                    std::format("frame index count: {}", frame_index_count)
                );

                frame_index_count--;
                instruction_ptr++;
                break;
            case static_cast<uint8_t>(opcode::push_constant):
                str += disassembly_line_formatter(
                    instruction_ptr - code.data(),
                    "push_constant",
                    std::visit(scheme_value_formatter, constants[*(instruction_ptr + 1)])
                );

                instruction_ptr += 2;
                break;
            case static_cast<uint8_t>(opcode::push_frame_index):
                str += disassembly_line_formatter(
                    instruction_ptr - code.data(),
                    "push_frame_index",
                    ""
                );

                frame_index_count++;
                instruction_ptr++;
                break;
            case static_cast<uint8_t>(opcode::jump_forward_if_not):
                str += disassembly_line_formatter(
                    instruction_ptr - code.data(),
                    "jump_forward_if_not",
                    std::format("{} bytes", aligned_read<bytecode::jump_size_type>(instruction_ptr + 1) + 1)
                );

                instruction_ptr++;
                instruction_ptr += get_aligned_access_size<bytecode::jump_size_type>(instruction_ptr);
                break;
            case static_cast<uint8_t>(opcode::jump_forward):
                str += disassembly_line_formatter(
                    instruction_ptr - code.data(),
                    "jump_forward",
                    std::format("{} bytes", aligned_read<bytecode::jump_size_type>(instruction_ptr + 1) + 1)
                );

                instruction_ptr++;
                instruction_ptr += get_aligned_access_size<bytecode::jump_size_type>(instruction_ptr);
                break;
            case static_cast<uint8_t>(opcode::halt):
                str += disassembly_line_formatter(
                    instruction_ptr - code.data(),
                    "halt",
                    ""
                );

                instruction_ptr++;
                break;
        }
    }

    return str;
}

const scheme_value& bytecode::get_constant(uint8_t index) const {
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
