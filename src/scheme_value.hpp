#pragma once

#include <stdint.h>
#include <variant>

using builtin_procedure = void (*)(void*, uint8_t);

using scheme_value = std::variant<
    int64_t,
    double,
    bool,
    builtin_procedure
>;
