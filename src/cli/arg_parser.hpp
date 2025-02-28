#pragma once

#include <format>
#include <print>
#include <stdexcept>
#include <string.h>

/**
 * Contains basic instructions for how to use this program.
 */
inline constexpr const char* const usage_str = R"(
usage: ploy [-h|--help] [-d|--disassemble] <file>

-h|--help           Display this message and quit.
-d|--disassemble    Print disassembly in addition to program output.
<file>              The file path of the scheme program to execute.)";

/**
 * Exception type for anything cli arg related. Will combine the given what message with the usage
 * string.
 */
struct arg_error : std::runtime_error {
    arg_error(const auto what) :
        std::runtime_error{
            std::format("{}\n{}", what, usage_str)
        }
    {}
};

/**
 * Parses cli args.
 */
struct arg_parser {

    /**
     * If true indicates to show the usage string and exit.
     */
    bool show_help = false;

    /**
     * If true indicates to show the disassembly for the given program.
     */
    bool disassemble = false;

    /**
     * File path to run.
     */
    const char* file_path = nullptr;

    /**
     * Checks if given arg equals either the given short option or long option.
     */
    bool is_flag(const char* const arg, const char* const short_opt, const char* const long_opt) {
        return (
            !strcmp(arg, short_opt)
            or !strcmp(arg, long_opt)
        );
    }

    /**
     * Parses args passed to the program on the cli.
     */
    arg_parser(size_t argc, char** argv) {
        for (size_t i = 1; i < argc; i++) {
            const char* const arg = argv[i];

            if (is_flag(arg, "-h", "--help")) {
                show_help = true;
                return;
            }

            if (is_flag(arg, "-d", "--disassemble"))
                disassemble = true;
            else if (file_path)
                throw arg_error(std::format("unexpected arg: {}", arg));
            else
                file_path = arg;
        }

        if (!file_path)
            throw arg_error("file path required");
    }
};

