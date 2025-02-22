#include <array>
#include <print>
#include <string.h>
#include <utility>

#include "compiler.hpp"
#include "tokenizer.hpp"
#include "virtual_machine.hpp"

/**
 * Array of pairs that matches a scheme program string with its expected ending stack state.
 */
static constexpr auto source_output_pairs = std::to_array<std::pair<const char*, const char*>>({
    {
        "(+ 1 2 3 4 5 (- 5 2 1) (*))",
        "[18, ]",
    },
    {
        "(* (+ -3.2 2) (/ 6.2 2))",
        "[-3.7200000000000006, ]",
    },
    {
        "((if #f + -) 3 (* 5 2))",
        "[-7, ]",
    },
    {
        "((if (odd? (* 5 1)) + -) 3 (* 5 2))",
        "[13, ]",
    },
    {
        "((lambda (x) (* x x)) 5)",
        "[25, ]",
    },
    {
        "((lambda (f) (f 5)) (lambda (x) (* x x)))",
        "[25, ]",
    },
    {
        "((((lambda (x) (lambda (y) (lambda (z) (* x y z)))) 5) 6) 2)",
        "[60, ]",
    },
    {
        "'(6 . 3)",
        "[(6 . 3), ]",
    },
    {
        "'('6)",
        "[((quote 6)), ]",
    },
    {
        "(quote ((quote 6)))",
        "[((quote 6)), ]",
    },
    {
        "(cons 'a '(b c))",
        "[(a b c), ]",
    },
    {
        "(cons '(1 2 3) 4)",
        "[((1 2 3) . 4), ]",
    },
    {
        "(cons 1 (cons 2 (cons 3 4)))",
        "[(1 2 3 . 4), ]",
    },
    {
        "(car (cdr '(1 . (2 . 3))))",
        "[2, ]",
    },
    {
        "((lambda (x) 3 (* x x)) 5)",
        "[25, ]",
    },
    {
        "((lambda (x) 3 (* 3 3) (* x x)) 5)",
        "[25, ]",
    },
    {
        "(if (call/cc (lambda (c) (c #f) #t)) 1 2)",
        "[2, ]",
    },
});

int main(int argc, char** argv) {
    if (argc > 2)
        throw std::runtime_error("only -v or --verbose are allowed as args");

    bool verbose = false;

    if (argc == 2)
        if (!strcmp(argv[1], "-v") or !strcmp(argv[1], "--verbose"))
            verbose = true;

    bool all_succeeded = true;

    for (const auto& p : source_output_pairs) {
        if (verbose)
            std::print("source: {}\n", p.first);

        const tokenizer t{p.first};
        const compiler c{t.tokens};

        if (verbose)
            std::print("disassembly:\n{}", c.program.disassemble());

        virtual_machine vm;
        vm.execute(c.program);

        const auto stack_str = vm.stack_to_string();

        if (verbose)
            std::print("vm stack: {}\n", stack_str);

        if (stack_str != p.second) {
            all_succeeded = false;
            std::print(
                "error: unexpected stack state\n"
                "source:   {}\n"
                "expected: {}\n"
                "got:      {}\n",
                p.first,
                p.second,
                stack_str
            );
        }

        if (verbose)
            std::print("\n");
    }

    if (!all_succeeded)
        std::print("error: 1 or more tests failed\n");

    return !all_succeeded;
}
