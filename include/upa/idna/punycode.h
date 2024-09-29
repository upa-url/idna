// Copyright 2017-2024 Rimas Misevičius
// Distributed under the BSD-style license that can be
// found in the LICENSE file.
//
#ifndef UPA_IDNA_PUNYCODE_H
#define UPA_IDNA_PUNYCODE_H

#include <string>

namespace upa::idna::punycode {

enum class status {
    success = 0,
    bad_input = 1,  // Input is invalid.
    big_output = 2, // Output would exceed the space provided.
    overflow = 3    // Wider integers needed to process input.
};

status encode(std::string& output, const char32_t* first, const char32_t* last);
status decode(std::u32string& output, const char32_t* first, const char32_t* last);

} // namespace upa::idna::punycode

#endif // UPA_IDNA_PUNYCODE_H
