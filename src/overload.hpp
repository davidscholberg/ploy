#pragma once

#include <string>
#include <string_view>
#include <unordered_map>
#include <variant>

#include "scheme_value.hpp"
#include "virtual_machine.hpp"

// Allows us to easily construct a callable object with overloads for a variant type. Useful for
// std::visit.
template<class... Ts>
struct overload : Ts... {
    using Ts::operator()...;
};

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

    std::string operator()(const lambda_ptr& v) const {
        return std::format("lambda: {}", v->bytecode_offset);
    }

    std::string operator()(const pair_ptr& a) const {
        // NOTE: pairs consist of two scheme_values, not stack_values, but because all valid
        // scheme_value types are also valid stack_value types, the recursive call within the
        // visitor works fine.
        return std::format(
            "({} . {})",
            std::visit(
                [this](const auto& a) {
                    return (*this)(a);
                },
                a->car
            ),
            std::visit(
                [this](const auto& a) {
                    return (*this)(a);
                },
                a->cdr
            )
        );
    }

    std::string operator()(const scheme_value_ptr& a) const {
        return std::format(
            "ptr: {}",
            std::visit(
                [this](const auto& a) {
                    return (*this)(a);
                },
                *a
            )
        );
    }

    std::string operator()(const auto& v) const {
        return std::format("{}", v);
    }
};
