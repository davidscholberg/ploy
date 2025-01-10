#include <array>
#include <print>

#include "tokenizer.hpp"

int main() {
    auto statements = std::to_array({
        "(+ 1 1)",
    });

    for (const auto& s : statements)
        std::print("{}\n", tokenizer{s}.to_string());
}
