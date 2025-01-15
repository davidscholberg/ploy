#pragma once

// Allows us to easily construct a callable object with overloads for a variant type. Useful for
// std::visit.
template<class... Ts>
struct overload : Ts... {
    using Ts::operator()...;
};
