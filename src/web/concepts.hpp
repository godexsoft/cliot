#pragma once

#include <string>
#include <type_traits>

// clang-format off
template <typename T>
concept ConnectionChannel = requires(T a, std::string s) {
    { a->write(std::move(s)) };
    { a->read_one() } -> std::convertible_to<std::string>;
};

template <typename T>
concept ConnectionHandler = requires(T a) {
    { a.borrow() } -> ConnectionChannel;
    { typename T::shared_link_t() } -> ConnectionChannel;
};

template <typename T>
concept SimpleRequestProvider = requires(T a, std::string s) {
    { a.get(s) } -> std::convertible_to<std::string>;
    { a.post(s) } -> std::convertible_to<std::string>;
};
// clang-format on
