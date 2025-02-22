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
        if (auto* const l_ptr = std::get_if<lambda_constant>(&(constants[c.lambda_constant_id])))
            l_ptr->bytecode_offset = code.size();
        else if (auto* const hrl_ptr = std::get_if<hand_rolled_lambda_constant>(&(constants[c.lambda_constant_id])))
            hrl_ptr->bytecode_offset = code.size();
        else
            throw std::runtime_error("expected lambda constant");
        code.insert(code.end(), c.code.cbegin(), c.code.cend());
    }

    compiled_blocks.clear();
    compiled_blocks.shrink_to_fit();
}

std::string bytecode::disassemble() const {
    std::unordered_map<builtin_procedure, std::string_view> bp_ptr_to_name;
    for (const auto& [k, v] : bp_name_to_ptr)
        bp_ptr_to_name[v] = k;

    const auto get_label = [](const auto format_str, const size_t dest_offset) {
        return std::vformat(format_str, std::make_format_args(dest_offset));
    };

    const auto get_lambda_label = [&get_label](const size_t dest_offset) {
        return get_label("lambda{}", dest_offset);
    };

    const overload scheme_constant_formatter{
        [&bp_ptr_to_name](const builtin_procedure& v) {
            if (!bp_ptr_to_name.contains(v))
                throw std::runtime_error("builtin procedure not found by address");
            return std::format("bp: {}", bp_ptr_to_name[v]);
        },
        [](const hand_rolled_lambda_constant& v) {
            return std::format("lambda: {}", v.name);
        },
        [&get_lambda_label](const lambda_constant& v) {
            return get_lambda_label(v.bytecode_offset);
        },
        [](const empty_list&) {
            return std::format("()");
        },
        [](const symbol& v) {
            return std::format("symbol: {}", v);
        },
        [](const auto& v) {
            return std::format("{}", v);
        },
    };

    const auto get_jump_dest_offset = [](const auto& code, const uint8_t* const instruction_ptr) {
        const auto jump_size = read_value<jump_size_type>(instruction_ptr + 1) + 1;
        const size_t dest_offset = (instruction_ptr + jump_size) - code.data();
        return dest_offset;
    };

    const auto get_jump_label = [](const size_t dest_offset) {
        return std::format("j{}", dest_offset);
    };

    std::unordered_map<size_t, std::string> offset_to_label_map;
    for (size_t offset = 0; offset < code.size(); offset += opcode_infos.at(code[offset]).size)
        if (
            code[offset] == static_cast<uint8_t>(opcode::jump_forward)
            or code[offset] == static_cast<uint8_t>(opcode::jump_forward_if_not)
        ) {
            const size_t dest_offset = get_jump_dest_offset(code, code.data() + offset);
            offset_to_label_map[dest_offset] = std::format("{}:", get_jump_label(dest_offset));
        }
    for (const auto& c : constants)
        if (const auto* const l_ptr = std::get_if<lambda_constant>(&c))
            offset_to_label_map[l_ptr->bytecode_offset] = std::format("{}:", get_lambda_label(l_ptr->bytecode_offset));
        else if (const auto* const hrl_ptr = std::get_if<hand_rolled_lambda_constant>(&c))
            offset_to_label_map[hrl_ptr->bytecode_offset] = std::format("lambda: {}:", hrl_ptr->name);

    const auto* const start_ptr = code.data();
    const auto disassembly_line_formatter = [&offset_to_label_map, start_ptr](
        const auto* const current_ptr,
        const auto additional_info
    ) {
        const size_t offset = current_ptr - start_ptr;

        std::string newline = "";
        std::string label = "";

        if (offset_to_label_map.contains(offset)) {
            label = offset_to_label_map[offset];
            if (label.starts_with("lambda"))
                newline = "\n";
        }

        return newline + std::format(
            "{:<20} {:>4}: {:<21} {}\n",
            label,
            offset,
            opcode_infos.at(*current_ptr).name,
            additional_info
        );
    };

    const uint8_t* instruction_ptr = code.data();
    std::string str;

    while (instruction_ptr - code.data() < code.size()) {
        switch (*instruction_ptr) {
            case static_cast<uint8_t>(opcode::call):
                str += disassembly_line_formatter(instruction_ptr, "");
                break;
            case static_cast<uint8_t>(opcode::capture_shared_var):
                str += disassembly_line_formatter(instruction_ptr, *(instruction_ptr + 1));
                break;
            case static_cast<uint8_t>(opcode::capture_stack_var):
                str += disassembly_line_formatter(instruction_ptr, *(instruction_ptr + 1));
                break;
            case static_cast<uint8_t>(opcode::cons):
                str += disassembly_line_formatter(instruction_ptr, "");
                break;
            case static_cast<uint8_t>(opcode::expect_argc):
                str += disassembly_line_formatter(instruction_ptr, *(instruction_ptr + 1));
                break;
            case static_cast<uint8_t>(opcode::push_continuation):
                str += disassembly_line_formatter(instruction_ptr, "");
                break;
            case static_cast<uint8_t>(opcode::push_constant):
                str += disassembly_line_formatter(
                    instruction_ptr,
                    std::visit(scheme_constant_formatter, constants[*(instruction_ptr + 1)])
                );
                break;
            case static_cast<uint8_t>(opcode::push_frame_index):
                str += disassembly_line_formatter(instruction_ptr, "");
                break;
            case static_cast<uint8_t>(opcode::push_shared_var):
                str += disassembly_line_formatter(instruction_ptr, *(instruction_ptr + 1));
                break;
            case static_cast<uint8_t>(opcode::push_stack_var):
                str += disassembly_line_formatter(instruction_ptr, *(instruction_ptr + 1));
                break;
            case static_cast<uint8_t>(opcode::jump_forward_if_not):
                str += disassembly_line_formatter(
                    instruction_ptr,
                    get_jump_label(get_jump_dest_offset(code, instruction_ptr))
                );
                break;
            case static_cast<uint8_t>(opcode::jump_forward):
                str += disassembly_line_formatter(
                    instruction_ptr,
                    get_jump_label(get_jump_dest_offset(code, instruction_ptr))
                );
                break;
            case static_cast<uint8_t>(opcode::set_coarity_any):
                str += disassembly_line_formatter(instruction_ptr, "");
                break;
            case static_cast<uint8_t>(opcode::set_coarity_one):
                str += disassembly_line_formatter(instruction_ptr, "");
                break;
            case static_cast<uint8_t>(opcode::ret):
                str += disassembly_line_formatter(instruction_ptr, "");
                break;
            case static_cast<uint8_t>(opcode::halt):
                str += disassembly_line_formatter(instruction_ptr, "");
                break;
            default:
                throw std::runtime_error("invalid opcode");
        }

        instruction_ptr += opcode_infos.at(*instruction_ptr).size;
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

uint8_t bytecode::push_hand_rolled_lambda(const std::string_view& name) {
    const hand_rolled_lambda_constant hrlc{name};

    if (constant_to_index_map.contains(hrlc)) {
        return constant_to_index_map[hrlc];
    }

    uint8_t constant_index = add_constant(hrlc);
    compiled_blocks.emplace_back(hr_lambda_name_to_code.at(name), constant_index);

    return constant_index;
}

void bytecode::push_lambda(uint8_t lambda_constant_index) {
    compiling_blocks.emplace_back(lambda_code{{}, lambda_constant_index});
}
