#pragma once

#include <memory>
#include <stdint.h>
#include <string_view>
#include <variant>
#include <vector>

#include "template_appender.hpp"

using builtin_procedure = void (*)(void*, uint8_t);
using empty_list = uint8_t;
using symbol = std::string_view;

using scheme_value_base = std::variant<
    int64_t,
    double,
    bool,
    builtin_procedure,
    empty_list,
    symbol
>;

/**
 * Scheme constant type used by the compiler to represent a hand-rolled lambda object. Hand-rolled
 * lambdas are lambdas that are pre-defined with hard-coded bytecode. This is typically reserved for
 * standard procedures that are uncompilable from scheme code or must call other lambdas.
 *
 * Objects of this struct are hashed based on the name field only.
 */
struct hand_rolled_lambda_constant {

    /**
     * Name of this lambda.
     */
    std::string_view name;

    /**
     * Offset into the bytecode where the hand_rolled_lambda jumps to when called.
     */
    size_t bytecode_offset;

    /**
     * Equality overload for unordered_map key support. Only the name matters for comparison.
     */
    auto operator==(const hand_rolled_lambda_constant& other) const {
        return name == other.name;
    }
};

/**
 * std::hash specialization for unordered_map key support. Only the name matters for comparison.
 */
template<>
struct std::hash<hand_rolled_lambda_constant> {
    auto operator()(const hand_rolled_lambda_constant& v) const {
        return std::hash<std::string_view>{}(v.name);
    }
};

/**
 * Scheme constant type used by the compiler to represent a lambda object.
 */
struct lambda_constant {

    /**
     * Offset into the bytecode where the lambda implementation is defined.
     */
    size_t bytecode_offset;

    /**
     * Equality overload for unordered_map key support.
     */
    auto operator==(const lambda_constant& other) const {
        return bytecode_offset == other.bytecode_offset;
    }
};

/**
 * std::hash specialization for unordered_map key support.
 */
template<>
struct std::hash<lambda_constant> {
    auto operator()(const lambda_constant& v) const {
        return std::hash<size_t>{}(v.bytecode_offset);
    }
};

using scheme_constant = template_appender<
    scheme_value_base,
    hand_rolled_lambda_constant,
    lambda_constant
>::type;

struct continuation;
using continuation_ptr = std::shared_ptr<continuation>;

struct lambda;
using lambda_ptr = std::shared_ptr<lambda>;

struct pair;
using pair_ptr = std::shared_ptr<pair>;

using scheme_value = template_appender<
    scheme_value_base,
    continuation_ptr,
    lambda_ptr,
    pair_ptr
>::type;

using scheme_value_ptr = std::shared_ptr<scheme_value>;

using stack_value = template_appender<
    scheme_value,
    scheme_value_ptr
>::type;

/**
 * Structure for keeping track of calls in the vm.
 */
struct call_frame {

    /**
     * If non-empty, represents the currently executing lambda. If empty, either the call hasn't
     * happened yet or the call frame is for a builtin procedure or continuation.
     */
    lambda_ptr executing_lambda;

    /**
     * Location of the beginning of the call frame index on the stack. This will always point to a
     * callable object on the value stack. Useful for both calling the intended procedure and
     * tracking the procedure's args.
     */
    size_t frame_index;

    /**
     * Represents the location in the bytecode to return to after the call is finished.
     */
    const uint8_t* return_ptr;

    /**
     * Holds the argument count for the procedure once it is called.
     */
    uint8_t argc;
};

/**
 * Enum that represents how the vm should handle the continuation arity (coarity) of a given
 * expression (i.e. the number of values expected as results of the expression).
 */
enum class coarity_type {

    /**
     * Any number of values can result from the expression, and they will be discarded. Used for all
     * non-final expressions of an expression sequence (e.g. a begin expression, a lambda body,
     * etc.).
     */
    any,

    /**
     * Exactly one value is expected as a result of the expression. Used when evaluating procedure
     * call arguments, if conditions, etc.
     */
    one,
};

/**
 * Structure that represents a continuation object. Conceptually, a continuation represents the
 * entire future computation beyond a specific point in the program's execution. In scheme, a
 * continuation can be a callable first class object where, when called, the current execution
 * context is abandoned and the program continues from the saved context inside the continuation.
 * Continuations may even be called several times, continuing on with the exact same context each
 * time.
 */
struct continuation {

    /**
     * Copy of the vm's call frame stack at the moment the continuation is instantiated.
     */
    std::vector<call_frame> frozen_call_frame_stack;

    /**
     * Copy of the vm's value stack at the moment the continuation is instantiated.
     */
    std::vector<stack_value> frozen_value_stack;

    /**
     * Copy of the vm's coarity_state at the moment the continuation is instantiated.
     */
    coarity_type frozen_coarity_state;
};

/**
 * Structure representing a lambda during runtime. A lambda is a callable object whose
 * implementation can be written by the user in scheme. Lambdas can capture variables from the
 * surrounding scopes.
 */
struct lambda {

    /**
     * Array of variables that this lambda has captured.
     */
    std::vector<scheme_value_ptr> captures;

    /**
     * Location in the bytecode where the lambda implementation begins.
     */
    size_t bytecode_offset;
};

struct pair {
    scheme_value car;
    scheme_value cdr;
};
