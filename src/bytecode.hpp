#pragma once

#include <stdint.h>
#include <string>
#include <variant>
#include <unordered_map>
#include <vector>

using builtin_procedure = void (*)(void*, uint8_t);

using result_variant = std::variant<
    int64_t,
    double,
    builtin_procedure
>;

enum class opcode : uint8_t {
    call,
    push_constant,
    ret,
};

struct bytecode {
    std::vector<uint8_t> code;

    uint8_t add_constant(const result_variant& new_constant);
    const result_variant& get_constant(uint8_t index) const;
    std::string to_string() const;

    protected:
    std::vector<result_variant> constants;
    std::unordered_map<result_variant, uint8_t> constant_to_index_map;
};
