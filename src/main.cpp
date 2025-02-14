#include <array>
#include <print>

#include "compiler.hpp"
#include "tokenizer.hpp"
#include "virtual_machine.hpp"

int main() {
    auto statements = std::to_array({
        "(+ (- 4 3) -2)",
        "(+ 1 2 3 4 5 (- 5 2 1) (*))",
        "(* (+ -3.2 2) (/ 6.2 2))",
        "(odd? (* 5 1))",
        "((if #f + -) 3 (* 5 2))",
        "((if (odd? (* 5 1)) + -) 3 (* 5 2))",
        "((lambda (x) (* x x)) 5)",
        "((lambda (f) (f 5)) (lambda (x) (* x x)))",
        "((((lambda (x) (lambda (y) (lambda (z) (* x y z)))) 5) 6) 2)",
        "'(6 . 3)",
        "'('6)",
        "(quote ((quote 6)))",
        "(cons 'a '(b c))",
        "(cons '(1 2 3) 4)",
    });

    for (const auto& s : statements) {
        std::print("source: {}\n", s);
        const tokenizer t{s};

        const compiler c{t.tokens};
        std::print("disassembly:\n{}", c.program.disassemble());

        virtual_machine vm;
        vm.execute(c.program);
        std::print("{}\n\n", vm.to_string());
    }
}
