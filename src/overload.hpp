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

    std::pair<std::string, bool> pair_contents_to_string(const pair_ptr& a) const {
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
            return {std::format("{}", car_str), true};

        if (const auto* const cdr_pair_ptr_ptr = std::get_if<pair_ptr>(&(a->cdr))) {
            const auto [cdr_str, is_list] = pair_contents_to_string(*cdr_pair_ptr_ptr);
            if (is_list)
                return {std::format("{} {}", car_str, cdr_str), true};
            return {std::format("({} . {})", car_str, cdr_str), false};
        }

        return {
            std::format(
                "({} . {})",
                car_str,
                std::visit(
                    [this](const auto& a) {
                        return (*this)(a);
                    },
                    a->cdr
                )
            ),
            false
        };
    }

    std::string operator()(const pair_ptr& a) const {
        const auto [pair_str, is_list] = pair_contents_to_string(a);

        if (is_list)
            return std::format("({})", pair_str);

        return std::format("{}", pair_str);
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
