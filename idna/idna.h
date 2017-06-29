#ifndef IDNA__H
#define IDNA__H

#include <string>

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

bool ToASCII(std::u16string& domain, Option options);
bool ToUnicode(std::u16string& domain, Option options);

} // idna

// --------------------------------------------
// enable bit mask on idna::Option
#include "bitmask_operators.hpp"
template<>
struct enable_bitmask_operators<idna::Option> {
    static const bool enable = true;
};

#endif // IDNA__H
