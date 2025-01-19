#include <array>
#include <print>

#include "compiler.hpp"
#include "tokenizer.hpp"
#include "virtual_machine.hpp"

int main() {
    auto statements = std::to_array({
        "(+ (- 4 3) -2)",
        "(* (+ -3.2 2) (/ 6.2 2))",
        "(odd? (* 5 1))",
        "((if #f + -) 3 (* 5 2))",
        "((if (odd? (* 5 2)) + -) 3 (* 5 2))",
    });

    for (const auto& s : statements) {
        const tokenizer t{s};
        std::print("{}\n", t.to_string());

        const compiler c{t.tokens};
        std::print("{}\n", c.program.to_string());

        virtual_machine vm;
        vm.execute(c.program);
        std::print("{}\n\n", vm.to_string());
    }
}
