#if LIBENVPP_PLATFORM_WINDOWS

#include <libenvpp_env.hpp>

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <array>
#include <cwchar>
#include <type_traits>

#include <libenvpp_check.hpp>

namespace env::detail {

static inline std::string convert_string(const std::wstring& str)
{
	const auto* src = str.c_str();
	auto mbstate = std::mbstate_t{};
	const auto len = std::wcsrtombs(nullptr, &src, 0, &mbstate);
	auto dst = std::string(len, '\0'); // Implicitly contains space for the null terminator
	std::wcsrtombs(dst.data(), &src, dst.size(), &mbstate);
	return dst;
}

static inline std::wstring convert_string(const std::string& str)
{
	const auto* src = str.c_str();
	auto mbstate = std::mbstate_t{};
	const auto len = std::mbsrtowcs(nullptr, &src, 0, &mbstate);
	auto dst = std::wstring(len, L'\0'); // Implicitly contains space for the null terminator
	std::mbsrtowcs(dst.data(), &src, dst.size(), &mbstate);
	return dst;
}

std::unordered_map<std::string, std::string> get_environment()
{
	auto env_map = std::unordered_map<std::string, std::string>{};

	const auto environment = GetEnvironmentStringsW();
	if (!environment) {
		return env_map;
	}

	for (const auto* var = environment; *var; ++var) {
		auto var_name_value = std::array<std::wstring, 2>{};
		auto idx = std::size_t{0};
		for (; *var; ++var) {
			if (idx == 0 && *var == L'=') {
				++idx;
			} else {
				var_name_value[idx] += *var;
			}
		}
		if (!var_name_value[0].empty()) {
			env_map[convert_string(var_name_value[0])] = convert_string(var_name_value[1]);
		}
	}

	[[maybe_unused]] const auto env_strings_were_freed = FreeEnvironmentStringsW(environment);
	LIBENVPP_CHECK(env_strings_were_freed);

	return env_map;
}

std::optional<std::string> get_environment_variable(const std::string_view name)
{
	const auto var_name = convert_string(std::string(name));
	const auto buffer_size = GetEnvironmentVariableW(var_name.c_str(), nullptr, 0);
	if (buffer_size == 0) {
		return {};
	}
	// -1 because std::string already contains implicit null terminator
	auto value = std::wstring(buffer_size - 1, L'\0');
	const auto env_var_got = GetEnvironmentVariableW(var_name.c_str(), value.data(), buffer_size);
	LIBENVPP_CHECK(env_var_got != 0);
	return convert_string(value);
}

void set_environment_variable(const std::string_view name, const std::string_view value)
{
	[[maybe_unused]] const auto env_var_was_set =
	    SetEnvironmentVariableW(convert_string(std::string(name)).c_str(), convert_string(std::string(value)).c_str());
	LIBENVPP_CHECK(env_var_was_set);
}

void delete_environment_variable(const std::string_view name)
{
	[[maybe_unused]] const auto env_var_was_deleted =
	    SetEnvironmentVariableW(convert_string(std::string(name)).c_str(), nullptr);
	LIBENVPP_CHECK(env_var_was_deleted);
}

} // namespace env::detail

#endif