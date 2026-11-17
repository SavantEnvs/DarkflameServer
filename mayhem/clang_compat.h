// Force-included (via CMAKE_CXX_FLAGS in mayhem/build.sh) into every C++ TU, so it also lands at the
// top of upstream's precompiled header. Under clang, __GNUC__ is 4, so dCommon/GeneralUtils.h/.cpp
// take their pre-GCC-11 ("MacOS") branch, where GeneralUtils.cpp defines the explicit
// specializations details::_parse<float/double/long double> only after the PCH (which includes
// GeneralUtils.h and code using TryParse<float>) has already implicitly instantiated them; clang 19
// rejects that ("explicit specialization after instantiation"). Declaring the primary template and
// the specializations up front is the conforming order. Only declarations: no behavior changes, and
// GeneralUtils.h redeclares the same primary template.
#pragma once
#if defined(__cplusplus) && __cplusplus >= 202002L && defined(__clang__) && !(__GNUC__ >= 11)
#include <concepts>
#include <cstddef>
#include <string_view>
namespace GeneralUtils::details {
	template <std::floating_point T>
	[[nodiscard]] T _parse(const std::string_view str, size_t& parseNum);
	template <> float _parse<float>(const std::string_view str, size_t& parseNum);
	template <> double _parse<double>(const std::string_view str, size_t& parseNum);
	template <> long double _parse<long double>(const std::string_view str, size_t& parseNum);
}
#endif
