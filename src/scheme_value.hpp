#pragma once

#include <memory>
#include <stdint.h>
#include <variant>
#include <vector>

#include "template_appender.hpp"

using builtin_procedure = void (*)(void*, uint8_t);

using scheme_value_base = std::variant<
    int64_t,
    double,
    bool,
    builtin_procedure
>;

using lambda_constant = size_t;

using scheme_constant = template_appender<
    scheme_value_base,
    lambda_constant
>::type;

struct lambda;
using lambda_ptr = std::shared_ptr<lambda>;

using scheme_value = template_appender<
    scheme_value_base,
    lambda_ptr
>::type;

using scheme_value_ptr = std::shared_ptr<scheme_value>;

using stack_value = template_appender<
    scheme_value,
    scheme_value_ptr
>::type;

struct lambda {
    std::vector<scheme_value_ptr> captures;
    size_t bytecode_offset;
};
