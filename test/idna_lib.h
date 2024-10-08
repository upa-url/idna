// Copyright 2017-2024 Rimas Misevičius
// Distributed under the BSD-style license that can be
// found in the LICENSE file.
//
#ifndef IDNA_LIB_H
#define IDNA_LIB_H

#include <string>

namespace idna_lib {
    bool toASCII(std::string& output, const std::string& input, bool transitional, bool is_input_ascii = false);
    bool toUnicode(std::string& output, const std::string& input, bool is_input_ascii = false);
} // namespace idna_lib

#endif // IDNA_LIB_H
