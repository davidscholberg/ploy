#pragma once

#include <array>
#include <stdint.h>
#include <string>
#include <string.h>
#include <unordered_map>
#include <vector>

#include "scheme_value.hpp"

/**
 * Identifies an opcode, which is a byte value that tells the virtual machine to perform a
 * particular action. Each opcode has a fixed number of arguments that come directly after the
 * opcode in the bytecode. Some opcodes have no arguments.
 */
enum class opcode : uint8_t {

    /**
     * Call the callable located at the current call frame on the stack. Enforces a continuation
     * arity of one (meaning only one return value is allowed).
     */
    call,

    /**
     * Capture a shared variable from the currently executing lambda to the lambda on the stack
     * top. Has one argument that is an index into the lambdas shared variables indicating the
     * variable to capture.
     */
    capture_shared_var,

    /**
     * Capture a stack variable. Has one argument that is an index into the currently executing
     * lambda's stack variables indicating the variable to capture.
     */
    capture_stack_var,

    /**
     * Replace the top two stack values with a pair containing those stack values, where the cdr is
     * the stack top.
     */
    cons,

    /**
     * Delete a stack var identified by the opcode's one byte argument from the stack.
     */
    delete_stack_var,

    /**
     * Check to make sure that argc for the currently executing lambda is as expected. Only needed
     * for lambdas that have a fixed number of args. The one argument for this opcode is the argc to
     * expect.
     */
    expect_argc,

    /**
     * Halt the vm.
     */
    halt,

    /**
     * Unconditional jump forward. The jump offset is store in 4 bytes directly after this opcode.
     * Currently the endianness is platform dependent, but if we ever want platform-independent
     * bytecode then we'll have to pick an endian type here.
     */
    jump_forward,

    /**
     * Conditional jump forward if the value on the stack top is false. See jump_forward for jump
     * offset format.
     */
    jump_forward_if_not,

    /**
     * Push a constant from the bytecode's constants vector to the top of the stack. The one byte
     * arg is an index into the constants vector.
     */
    push_constant,

    /**
     * Push a continuation object to the stack that represents the continuation of the current
     * lambda call.
     */
    push_continuation,

    /**
     * Create a new call frame at the current stack top and push it to the call frame stack. The
     * call frame keeps track of a procedure's or lambda's arguments.
     */
    push_frame_index,

    /**
     * Push a shared var identified by the opcode's one byte argument from the currently executing
     * lambda's shared var list to the stack top.
     */
    push_shared_var,

    /**
     * Push a stack var identified by the opcode's one byte argument from the currently executing
     * lambda's stack vars to the stack top.
     */
    push_stack_var,

    /**
     * Pop the current call frame, roll the return value(s) down over the call frame's position on
     * the stack, and return to the called position.
     */
    ret,

    /**
     * Set coarity state in vm to coarity_type::any.
     */
    set_coarity_any,

    /**
     * Set coarity state in vm to coarity_type::one.
     */
    set_coarity_one,
};

/**
 * Represents the size and layout of an opcode with no argument.
 */
struct opcode_no_arg {
    uint8_t opcode_value;
};

/**
 * Represents the size and layout of an opcode with one one-byte argument.
 */
struct opcode_one_arg {
    uint8_t opcode_value;
    uint8_t arg;
};

/**
 * The type used for jump offsets embedded into the byte as jump opcode arguments.
 */
using jump_size_type = uint32_t;

/**
 * Represents the size and layout of a jump opcode with its jump offset argument.
 */
struct opcode_jump {
    uint8_t opcode_value;
    uint8_t jump_size[sizeof(jump_size_type)];
};

/**
 * Contains information about an opcode.
 */
struct opcode_info {

    /**
     * Name of the opcode.
     */
    const char* const name;

    /**
     * Size of the opcode in bytes (including arguments).
     */
    uint8_t size;
};

/**
 * Map of opcode numeric values to their opcode_info.
 */
inline constexpr auto opcode_infos = std::to_array<opcode_info>({
    {"call", sizeof(opcode_no_arg)},
    {"capture_shared_var", sizeof(opcode_one_arg)},
    {"capture_stack_var", sizeof(opcode_one_arg)},
    {"cons", sizeof(opcode_no_arg)},
    {"delete_stack_var", sizeof(opcode_one_arg)},
    {"expect_argc", sizeof(opcode_one_arg)},
    {"halt", sizeof(opcode_no_arg)},
    {"jump_forward", sizeof(opcode_jump)},
    {"jump_forward_if_not", sizeof(opcode_jump)},
    {"push_constant", sizeof(opcode_one_arg)},
    {"push_continuation", sizeof(opcode_no_arg)},
    {"push_frame_index", sizeof(opcode_no_arg)},
    {"push_shared_var", sizeof(opcode_one_arg)},
    {"push_stack_var", sizeof(opcode_one_arg)},
    {"ret", sizeof(opcode_no_arg)},
    {"set_coarity_any", sizeof(opcode_no_arg)},
    {"set_coarity_one", sizeof(opcode_no_arg)},
});

/**
 * Temporary structure for holding the bytecode of a lambda before it is concatenated to the final
 * bytecode array. Also contains the lambda_constant id it is associated with.
 *
 * NOTE: The use of the word lambda here is not quite accurate since this structure can also hold
 * hand-rolled procedures.
 */
struct lambda_code {
    std::vector<uint8_t> code;
    uint8_t lambda_constant_id;
};

/**
 * Structure that holds the bytecode and scheme_constants resulting from compiling a scheme program.
 */
struct bytecode {

    /**
     * Final bytecode array generated by the compiler.
     */
    std::vector<uint8_t> code;

    /**
     * Adds a new scheme constant and returns its id to be used in the bytecode, or returns id of
     * existing constant.
     */
    uint8_t add_constant(const scheme_constant& new_constant);

    /**
     * Append given byte to the current compiling block.
     */
    void append_byte(uint8_t value);

    /**
     * Append given byte to the compiling block specified by scope depth.
     */
    void append_byte(uint8_t value, size_t scope_depth);

    /**
     * Append given opcode to the current compiling block.
     */
    void append_opcode(opcode value);

    /**
     * Append given opcode to the compiling block specified by scope depth.
     */
    void append_opcode(opcode value, size_t scope_depth);

    /**
     * Backpatch a previously prepared jump offset at the given bytecode index of the current
     * compiling block.
     */
    void backpatch_jump(const size_t jump_size_index);

    /**
     * Concatenate all of the compiled blocks to the final bytecode array.
     */
    void concat_blocks();

    /**
     * Return the disassembly of this bytecode object as a string.
     */
    std::string disassemble() const;

    /**
     * Get the scheme constant specified by its id.
     */
    const scheme_constant& get_constant(uint8_t index) const;

    /**
     * Reserve space for a jump opcode and its offset arg and return the bytecode offset where the
     * jump offset will need to be backpatched once the conditional expression is finished
     * compiling.
     */
    size_t prepare_backpatch_jump(const opcode jump_type);

    /**
     * Pop the finished compiling block off the compiling block stack and onto the compiled block
     * stack.
     */
    void pop_lambda();

    /**
     * Pushes a hand-rolled procedure to the compiled blocks stack. Returns the associated constant id.
     */
    uint8_t push_hand_rolled_procedure(const std::string_view& name);

    /**
     * Push a new compiling block onto the stack. lambda_constant_index is the constant associated
     * with this block.
     */
    void push_lambda(uint8_t lambda_constant_index);

    /**
     * Read a value of type T located at the given byte array pointer.
     */
    template <typename T>
    static constexpr T read_value(const uint8_t* const ptr) {
        if constexpr (sizeof(T) == 1)
            return *ptr;

        T value;
        memcpy(&value, ptr, sizeof(T));
        return value;
    }

    std::string to_string() const;

    /**
     * Write a value of type T to the given byte array pointer.
     */
    template <typename T>
    static constexpr void write_value(T value, uint8_t* const ptr) {
        if constexpr (sizeof(T) == 1) {
            *ptr = value;
            return;
        }

        memcpy(ptr, &value, sizeof(T));
    }

    protected:

    /**
     * Array of scheme constants referred to by the bytecode.
     */
    std::vector<scheme_constant> constants;

    /**
     * Maps constants to their index in the constants array.
     */
    std::unordered_map<scheme_constant, uint8_t> constant_to_index_map;

    /**
     * Stack of compiling code blocks.
     */
    std::vector<lambda_code> compiling_blocks;

    /**
     * Stack of compiled code blocks.
     */
    std::vector<lambda_code> compiled_blocks;
};
