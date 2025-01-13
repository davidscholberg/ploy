#pragma once

#include <stdint.h>

enum class opcode : uint8_t {
    add,
    constant_int,
    divide,
    multiply,
    negate,
    subtract,
    ret,
};
