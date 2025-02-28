#pragma once

#include <variant>

/**
 * Allows us to create a new template instantiation with the template parameters of an existing
 * template instantiation plus additional template parameters.
 * Copied from https://stackoverflow.com/a/52393977
 */
template <typename T, typename... Args> struct template_appender;

/**
 * Specialization of template_appender for std::variant. Allows us to extend an existing
 * std::variant with additional types.
 */
template <typename... Args0, typename... Args1>
struct template_appender<std::variant<Args0...>, Args1...> {
    using type = std::variant<Args0..., Args1...>;
};
