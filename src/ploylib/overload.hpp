#pragma once

#include <string>
#include <variant>

#include "scheme_value.hpp"

/**
 * Allows us to easily construct a callable object with overloads for a variant type. Useful for
 * std::visit.
 *
 * The overloads can be lambdas provided in an initializer list at object instantiaion.
 *
 * This was copied from https://en.cppreference.com/w/cpp/utility/variant/visit2
 */
template<class... Ts>
struct overload : Ts... {
    using Ts::operator()...;
};

/**
 * Specialized overload object for stack_value that has built-in overloads for feeding the
 * underlying scheme_value in a scheme_value_ptr back to an overload in this object (which may be a
 * lambda provided in the initializer list). This is useful for making specialized overloads for
 * stack_values without having to worry about scheme_value_ptrs being processed properly. This
 * struct has both unary and binary scheme_value_ptr overloads.
 */
template<class... Ts>
struct stack_value_overload : Ts... {
    using Ts::operator()...;

    auto operator()(const scheme_value_ptr& a) const {
        return std::visit(
            [this](const auto& a) {
                return (*this)(a);
            },
            *a
        );
    }

    auto operator()(const scheme_value_ptr& a, const auto& b) const {
        return std::visit(
            [this, &b](const auto& a) {
                return (*this)(a, b);
            },
            *a
        );
    }

    auto operator()(const auto& a, const scheme_value_ptr& b) const {
        return std::visit(
            [this, &a](const auto& b) {
                return (*this)(a, b);
            },
            *b
        );
    }

    auto operator()(const scheme_value_ptr& a, const scheme_value_ptr& b) const {
        return std::visit(
            [this](const auto& a, const auto& b) {
                return (*this)(a, b);
            },
            *a,
            *b
        );
    }
};

/**
 * Represents a callable object with overloads for each type contained within stack_value for the
 * purpose of generating external representations (i.e. strings) of stack_values. Meant to be used
 * with std::visit. ShowSchemeValuePtr determines whether or not a scheme_value_ptr is marked as a
 * pointer or just displayed as the underlying object.
 */
template <bool ShowSchemeValuePtr>
struct stack_value_formatter_overload {
    std::string operator()(const empty_list& v) const {
        return std::format("()");
    }

    std::string operator()(const symbol& v) const {
        return std::format("{}", v);
    }

    std::string operator()(const builtin_procedure& v) const {
        return std::format("bp: {}", reinterpret_cast<void*>(v));
    }

    std::string operator()(const continuation_ptr& v) const {
        return std::format("cont: {}", reinterpret_cast<const void*>(v->frozen_call_frame_stack.back().return_ptr));
    }

    std::string operator()(const lambda_ptr& v) const {
        return std::format("lambda: {}", v->bytecode_offset);
    }

    /**
     * This is a recursive helper function for the pair_ptr overload that allows us to collapse the
     * dot notation when we have a chain of pairs (meaning a pair whose cdr is also a pair,
     * recursively).
     */
    std::string pair_contents_to_string(const pair_ptr& a) const {
        // NOTE: pairs consist of two scheme_values, not stack_values, but because all valid
        // scheme_value types are also valid stack_value types, the recursive call within the
        // visitor works fine.
        const std::string car_str = std::visit(
            [this](const auto& a) {
                return (*this)(a);
            },
            a->car
        );

        if (const auto* const cdr_empty_list = std::get_if<empty_list>(&(a->cdr)))
            return std::format("{}", car_str);

        if (const auto* const cdr_pair_ptr_ptr = std::get_if<pair_ptr>(&(a->cdr))) {
            const auto cdr_str = pair_contents_to_string(*cdr_pair_ptr_ptr);
            return std::format("{} {}", car_str, cdr_str);
        }

        return std::format(
            "{} . {}",
            car_str,
            std::visit(
                [this](const auto& a) {
                    return (*this)(a);
                },
                a->cdr
            )
        );
    }

    std::string operator()(const pair_ptr& a) const {
        const auto pair_str = pair_contents_to_string(a);
        return std::format("({})", pair_str);
    }

    std::string operator()(const scheme_value_ptr& a) const {
        if constexpr (ShowSchemeValuePtr) {
            return std::format(
                "ptr: {}",
                std::visit(
                    [this](const auto& a) {
                        return (*this)(a);
                    },
                    *a
                )
            );
        } else {
            return std::visit(
                [this](const auto& a) {
                    return (*this)(a);
                },
                *a
            );
        }
    }

    std::string operator()(const auto& v) const {
        return std::format("{}", v);
    }
};
