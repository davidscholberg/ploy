#include <array>
#include <format>
#include <fstream>
#include <print>
#include <exception>

#include "arg_parser.hpp"
#include "compiler.hpp"
#include "tokenizer.hpp"
#include "virtual_machine.hpp"

/**
 * Reads the file at the given file path into a string.
 */
std::string file_to_string(const char* const file_path) {
    std::string str;

    std::ifstream f(file_path);
    f.seekg(0, std::ios::end);
    size_t file_size = f.tellg();
    f.seekg(0);

    str.reserve(file_size);
    f.read(str.data(), file_size);

    return str;
}

int main(int argc, char** argv) {
    try {
        arg_parser args(argc, argv);

        if (args.show_help) {
            std::print("{}\n", usage_str);
            return 0;
        }

        const std::string source = file_to_string(args.file_path);

        const tokenizer t{source.c_str()};
        const compiler c{t.tokens};

        if (args.disassemble)
            std::print("disassembly:\n{}program output:\n", c.program.disassemble());

        virtual_machine vm;
        vm.execute(c.program);
    } catch (std::exception& e) {
        std::print("error: {}\n", e.what());
        return 1;
    }

    return 0;
}
