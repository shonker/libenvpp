#pragma once

#include <algorithm>
#include <any>
#include <cassert>
#include <cstddef>
#include <cstdlib>
#include <functional>
#include <limits>
#include <memory>
#include <optional>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include <fmt/core.h>

#include <libenvpp_errors.hpp>
#include <libenvpp_parser.hpp>
#include <libenvpp_util.hpp>

namespace env {

class prefix;

namespace detail {

enum class variable_type {
	variable,
	range,
	option,
};

template <typename T, variable_type VariableType, bool IsRequired>
class variable_id {
  public:
	using value_type = T;
	static constexpr auto var_type = VariableType;
	static constexpr auto is_required = IsRequired;

	variable_id() = delete;
	variable_id(const variable_id&) = delete;
	variable_id(variable_id&&) = default;

	variable_id& operator=(const variable_id&) = delete;
	variable_id& operator=(variable_id&&) = default;

  private:
	variable_id(const std::size_t idx) : m_idx(idx) {}

	const std::size_t m_idx;

	friend prefix;
	template <typename Prefix>
	friend class validated_prefix;
};

class variable_data {
  public:
	using parse_and_validate_fn = std::function<std::any(const std::string_view)>;

	variable_data() = delete;

	variable_data(const variable_data&) = delete;
	variable_data(variable_data&&) = default;

	variable_data& operator=(const variable_data&) = delete;
	variable_data& operator=(variable_data&&) = default;

  private:
	variable_data(const std::string_view name, parse_and_validate_fn parser_and_validator)
	    : m_name(name), m_parser_and_validator(parser_and_validator)
	{}

	std::string m_name;
	parse_and_validate_fn m_parser_and_validator;
	std::any m_value;

	friend prefix;
	template <typename Prefix>
	friend class validated_prefix;
};

// Templated to resolve mutual dependency.
template <typename Prefix>
class validated_prefix {
  public:
	validated_prefix() = delete;

	validated_prefix(const validated_prefix&) = delete;
	validated_prefix(validated_prefix&&) = default;

	validated_prefix& operator=(const validated_prefix&) = delete;
	validated_prefix& operator=(validated_prefix&&) = default;

	template <typename T, detail::variable_type VariableType, bool IsRequired>
	[[nodiscard]] auto get(const detail::variable_id<T, VariableType, IsRequired>& var_id)
	{
		if constexpr (IsRequired) {
			return std::any_cast<T>(m_prefix.m_registered_vars[var_id.m_idx].m_value);
		} else {
			return std::optional<T>{std::any_cast<T>(m_prefix.m_registered_vars[var_id.m_idx].m_value)};
		}
	}

	[[nodiscard]] bool ok() const noexcept { return m_errors.empty() && m_warnings.empty(); }
	[[nodiscard]] std::string error_message() const { return {}; }
	[[nodiscard]] std::string warning_message() const { return {}; }

	[[nodiscard]] const std::vector<parser_error>& errors() const { return m_errors; }
	[[nodiscard]] const std::vector<unrecognized_option>& warnings() const { return m_warnings; }

  private:
	validated_prefix(Prefix&& pre) : m_prefix(std::move(pre))
	{
		for (auto& var : m_prefix.m_registered_vars) {
			const auto env_var_name = m_prefix.m_prefix_name + "_" + var.m_name;
			const auto env_var_value = "7TODO";
			try {
				var.m_value = var.m_parser_and_validator(env_var_value);
			} catch (std::exception& e) {
				m_errors.emplace_back(fmt::format("Variable '{}' with error '{}'", env_var_name, e.what()));
			}
		}
	}

	Prefix m_prefix;
	std::vector<parser_error> m_errors;
	std::vector<unrecognized_option> m_warnings;

	friend prefix;
};

} // namespace detail

template <typename T>
struct default_validator {
	void operator()(const T&) const noexcept {}
};

template <typename T>
class default_parser {
  public:
	template <typename Validator>
	default_parser(Validator validator) : m_validator(std::move(validator))
	{}

	[[nodiscard]] T operator()(const std::string_view str) const
	{
		T value = detail::construct_from_string<T>(str);
		m_validator(value);
		return value;
	}

  private:
	std::function<void(const T&)> m_validator;
};

template <typename T>
struct default_parser_and_validator {
	[[nodiscard]] T operator()(const std::string_view str) const
	{
		return default_parser<T>{default_validator<T>{}}(str);
	}
};

template <typename Validator>
default_parser(Validator) -> default_parser<
    std::remove_cv_t<std::remove_reference_t<typename detail::util::function_traits<Validator>::arg0_type>>>;

class prefix {
  public:
	prefix() = delete;
	prefix(const std::string_view prefix_name, const int edit_distance_cutoff = 2)
	    : m_prefix_name(prefix_name), m_edit_distance_cutoff(edit_distance_cutoff)
	{}
	prefix(const prefix&) = delete;
	prefix(prefix&&) = default;

	prefix& operator=(const prefix&) = delete;
	prefix& operator=(prefix&&) = default;

	~prefix() = default;

	template <typename T, typename ParserAndValidatorFn = decltype(default_parser_and_validator<T>{})>
	[[nodiscard]] auto register_variable(const std::string_view name,
	                                     ParserAndValidatorFn parser_and_validator = default_parser_and_validator<T>{})
	{
		m_registered_vars.push_back(
		    detail::variable_data{name, [parser_and_validator](const std::string_view env_value) -> std::any {
			                          return parser_and_validator(env_value);
		                          }});
		return detail::variable_id<T, detail::variable_type::variable, false>{m_registered_vars.size() - 1};
	}

	[[nodiscard]] detail::validated_prefix<prefix> validate() { return {std::move(*this)}; }

	[[nodiscard]] std::string help_message() const { return {}; }

  private:
	std::string m_prefix_name;
	int m_edit_distance_cutoff;
	std::vector<detail::variable_data> m_registered_vars;

	template <typename Prefix>
	friend class detail::validated_prefix;
};

} // namespace env
