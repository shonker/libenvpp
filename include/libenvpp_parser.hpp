#pragma once

#include <cstddef>
#include <cstdint>
#include <exception>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

#include <fmt/core.h>

#include <libenvpp_errors.hpp>
#include <libenvpp_util.hpp>

namespace env::detail {

template <typename T>
struct is_string_constructible : std::conditional_t<                                 //
                                     std::is_constructible_v<T, std::string_view>    //
                                         || std::is_constructible_v<T, std::string>, //
                                     std::true_type,                                 //
                                     std::false_type> {};

template <typename T>
inline constexpr auto is_string_constructible_v = is_string_constructible<T>::value;

//////////////////////////////////////////////////////////////////////////

template <typename T, typename = void>
struct is_stringstream_constructible : std::false_type {};

template <typename T>
struct is_stringstream_constructible<T,
                                     std::void_t<decltype(std::declval<std::istringstream&>() >> std::declval<T&>())>>
    : std::true_type {};

template <typename T>
inline constexpr auto is_stringstream_constructible_v = is_stringstream_constructible<T>::value;

//////////////////////////////////////////////////////////////////////////

class parser_error : public std::runtime_error {
  public:
	parser_error(const std::string_view message) : std::runtime_error(std::string(message)) {}
};

template <typename T>
T construct_from_string(const std::string_view str)
{
	if constexpr (is_string_constructible_v<T>) {
		try {
			return T(std::string(str));
		} catch (const parser_error&) {
			throw;
		} catch (const std::exception& e) {
			throw parser_error{fmt::format("String constructor failed for input '{}' with '{}'", str, e.what())};
		} catch (...) {
			throw parser_error{fmt::format("String constructor failed for input '{}' with unknown error", str)};
		}
	} else if constexpr (is_stringstream_constructible_v<T>) {
		auto stream = std::istringstream(std::string(str));
		T parsed;
		try {
			stream >> parsed;
		} catch (const parser_error&) {
			throw;
		} catch (const std::exception& e) {
			throw parser_error{fmt::format("Stream operator>> failed for input '{}' with '{}'", str, e.what())};
		} catch (...) {
			throw parser_error{fmt::format("Stream operator>> failed for input '{}' with unknown error", str)};
		}
		if (stream.fail()) {
			throw parser_error{fmt::format("Stream operator>> failed for input '{}'", str)};
		}
		if (static_cast<std::size_t>(stream.tellg()) < str.size()) {
			throw parser_error{fmt::format("Input '{}' was only parsed partially with remaining data '{}'", str,
			                               stream.str().substr(stream.tellg()))};
		}
		if constexpr (std::is_unsigned_v<T>) {
			auto signed_parsed = std::int64_t{};
			auto signed_stream = std::istringstream(std::string(str));
			signed_stream >> signed_parsed;
			if (signed_stream.fail() || static_cast<std::size_t>(signed_stream.tellg()) < str.size()) {
				throw parser_error{
				    fmt::format("Failed to validate whether '{}' was correctly parsed as '{}'", str, parsed)};
			}
			if (signed_parsed < 0) {
				throw parser_error{fmt::format("Cannot parse negative number '{}' as unsigned type", str)};
			}
		}
		return parsed;
	} else {
		static_assert(util::always_false_v<T>,
		              "Type is not constructible from string. Implement one of the supported construction mechanisms.");
	}
}

} // namespace env::detail