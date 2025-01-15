#pragma once

#include <string>

#include "compiler.hpp"

struct virtual_machine {
    void execute(const bytecode& p);
    std::string to_string() const;

    protected:
    const uint8_t* instruction_ptr;
    std::vector<result_variant> stack;

    template <template <typename> typename Op>
    void execute_binary_op();
    template <template <typename> typename Op>
    void execute_unary_op();
};
