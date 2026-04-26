// Copyright 2025-2026 Rimas Misevičius
// Distributed under the BSD-style license that can be
// found in the LICENSE file.
//
#ifndef UPA_IDNA_CONFIG_H
#define UPA_IDNA_CONFIG_H

// Macros for compilers that support the C++20 or later standard
// https://devblogs.microsoft.com/cppblog/msvc-now-correctly-reports-__cplusplus/
#if defined(_MSVC_LANG) ? (_MSVC_LANG >= 202002) : (__cplusplus >= 202002)
# define UPA_IDNA_CPP_20
# define UPA_IDNA_CONSTEXPR_20 constexpr
#else
# define UPA_IDNA_CONSTEXPR_20 inline
#endif

// Define UPA_IDNA_API macro to mark symbols for export/import
// when compiling as shared library
#if defined (UPA_LIB_EXPORT) || defined (UPA_LIB_IMPORT)
# ifdef _MSC_VER
#  ifdef UPA_LIB_EXPORT
#   define UPA_IDNA_API __declspec(dllexport)
#  else
#   define UPA_IDNA_API __declspec(dllimport)
#  endif
# elif defined(__clang__) || defined(__GNUC__)
#  define UPA_IDNA_API __attribute__((visibility ("default")))
# endif
#endif
#ifndef UPA_IDNA_API
# define UPA_IDNA_API
#endif

#endif // UPA_IDNA_CONFIG_H
