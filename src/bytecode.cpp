#include <format>
#include <limits>
#include <stdexcept>
#include <unordered_map>
#include <variant>
#include <ranges>

#include "bytecode.hpp"
#include "overload.hpp"
#include "virtual_machine.hpp"

uint8_t bytecode::add_constant(const scheme_constant& new_constant) {
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

void bytecode::append_byte(uint8_t value) {
    if (compiling_blocks.empty())
        throw std::runtime_error("no blocks to write to");

    append_byte(value, compiling_blocks.size() - 1);
}

void bytecode::append_byte(uint8_t value, size_t scope_depth) {
    compiling_blocks[scope_depth].code.emplace_back(value);
}

void bytecode::append_opcode(opcode value) {
    append_byte(static_cast<uint8_t>(value));
}

void bytecode::append_opcode(opcode value, size_t scope_depth) {
    append_byte(static_cast<uint8_t>(value), scope_depth);
}

void bytecode::backpatch_jump(const size_t backpatch_index) {
    auto& current_code_block = compiling_blocks.back().code;

    const size_t jump_size = current_code_block.size() - backpatch_index;

    if (jump_size > std::numeric_limits<jump_size_type>::max())
        throw std::runtime_error("jump size is too large for its type");

    write_value<jump_size_type>(jump_size, current_code_block.data() + backpatch_index);
}

void bytecode::concat_blocks() {
    code.emplace_back(static_cast<uint8_t>(opcode::push_frame_index));
    code.emplace_back(static_cast<uint8_t>(opcode::push_constant));
    code.emplace_back(compiled_blocks.back().lambda_constant_id);
    code.emplace_back(static_cast<uint8_t>(opcode::call));
    code.emplace_back(static_cast<uint8_t>(opcode::halt));

    size_t total_size = code.size();
    for (const auto& c : compiled_blocks)
        total_size += c.code.size();
    code.reserve(total_size);

    for (const auto& c : std::views::reverse(compiled_blocks)) {
        constants[c.lambda_constant_id] = static_cast<lambda_constant>(code.size());
        code.insert(code.end(), c.code.cbegin(), c.code.cend());
    }

    compiled_blocks.clear();
    compiled_blocks.shrink_to_fit();
}

std::string bytecode::disassemble() const {
    std::unordered_map<builtin_procedure, std::string_view> bp_ptr_to_name;
    for (const auto& [k, v] : bp_name_to_ptr)
        bp_ptr_to_name[v] = k;

    const overload scheme_constant_formatter{
        [&bp_ptr_to_name](const builtin_procedure& v) {
            if (!bp_ptr_to_name.contains(v))
                throw std::runtime_error("builtin procedure not found by address");
            return std::format("bp: {}", bp_ptr_to_name[v]);
        },
        [](const lambda_constant& v) {
            return std::format("lambda: {}", v);
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
            case static_cast<uint8_t>(opcode::capture_shared_var):
                str += disassembly_line_formatter(
                    instruction_ptr - code.data(),
                    "capture_shared_var",
                    *(instruction_ptr + 1)
                );

                instruction_ptr += 2;
                break;
            case static_cast<uint8_t>(opcode::capture_stack_var):
                str += disassembly_line_formatter(
                    instruction_ptr - code.data(),
                    "capture_stack_var",
                    *(instruction_ptr + 1)
                );

                instruction_ptr += 2;
                break;
            case static_cast<uint8_t>(opcode::expect_argc):
                str += disassembly_line_formatter(
                    instruction_ptr - code.data(),
                    "expect_argc",
                    *(instruction_ptr + 1)
                );

                instruction_ptr += 2;
                break;
            case static_cast<uint8_t>(opcode::push_constant):
                str += disassembly_line_formatter(
                    instruction_ptr - code.data(),
                    "push_constant",
                    std::visit(scheme_constant_formatter, constants[*(instruction_ptr + 1)])
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
            case static_cast<uint8_t>(opcode::push_shared_var):
                str += disassembly_line_formatter(
                    instruction_ptr - code.data(),
                    "push_shared_var",
                    *(instruction_ptr + 1)
                );

                instruction_ptr += 2;
                break;
            case static_cast<uint8_t>(opcode::push_stack_var):
                str += disassembly_line_formatter(
                    instruction_ptr - code.data(),
                    "push_stack_var",
                    *(instruction_ptr + 1)
                );

                instruction_ptr += 2;
                break;
            case static_cast<uint8_t>(opcode::jump_forward_if_not):
                str += disassembly_line_formatter(
                    instruction_ptr - code.data(),
                    "jump_forward_if_not",
                    std::format("{} bytes", read_value<bytecode::jump_size_type>(instruction_ptr + 1) + 1)
                );

                instruction_ptr++;
                instruction_ptr += sizeof(bytecode::jump_size_type);
                break;
            case static_cast<uint8_t>(opcode::jump_forward):
                str += disassembly_line_formatter(
                    instruction_ptr - code.data(),
                    "jump_forward",
                    std::format("{} bytes", read_value<bytecode::jump_size_type>(instruction_ptr + 1) + 1)
                );

                instruction_ptr++;
                instruction_ptr += sizeof(bytecode::jump_size_type);
                break;
            case static_cast<uint8_t>(opcode::ret):
                str += disassembly_line_formatter(
                    instruction_ptr - code.data(),
                    "ret",
                    ""
                );

                instruction_ptr++;
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

const scheme_constant& bytecode::get_constant(uint8_t index) const {
    if (index >= constants.size())
        throw std::runtime_error("constant index out of bounds");

    return constants[index];
}

size_t bytecode::prepare_backpatch_jump(const opcode jump_type) {
    append_opcode(jump_type);

    auto& current_code_block = compiling_blocks.back().code;
    const size_t backpatch_index = current_code_block.size();

    current_code_block.resize(current_code_block.size() + sizeof(jump_size_type));

    return backpatch_index;
}

void bytecode::pop_lambda() {
    compiled_blocks.emplace_back(std::move(compiling_blocks.back()));
    compiling_blocks.pop_back();
}

void bytecode::push_lambda(uint8_t lambda_constant_index) {
    compiling_blocks.emplace_back(lambda_code{{}, lambda_constant_index});
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
