#pragma once

#include <variant>

#include "scheme_value.hpp"

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
