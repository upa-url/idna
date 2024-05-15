// Copyright 2017-2024 Rimas Misevičius
// Distributed under the BSD-style license that can be
// found in the LICENSE file.
//
#ifndef UPA_IDNA_H
#define UPA_IDNA_H

#include <string>

namespace upa {
namespace idna {

enum class Option {
    Default           = 0,
    UseSTD3ASCIIRules = 0x0001,
    Transitional      = 0x0002,
    VerifyDnsLength   = 0x0004,
    CheckHyphens      = 0x0008,
    CheckBidi         = 0x0010,
    CheckJoiners      = 0x0020,
};

bool ToASCII(std::string& domain, const char16_t* input, const char16_t* input_end, Option options);
bool ToUnicode(std::u32string& domain, const char16_t* input, const char16_t* input_end, Option options);

} // namespace idna
} // namespace upa

// --------------------------------------------
// enable bit mask on idna::Option
#include "bitmask_operators.hpp"
template<>
struct enable_bitmask_operators<upa::idna::Option> {
    static const bool enable = true;
};

#endif // UPA_IDNA_H
