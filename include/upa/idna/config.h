// Copyright 2025 Rimas Misevičius
// Distributed under the BSD-style license that can be
// found in the LICENSE file.
//
#ifndef UPA_IDNA_CONFIG_H
#define UPA_IDNA_CONFIG_H

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
