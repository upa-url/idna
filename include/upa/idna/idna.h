// Copyright 2017-2024 Rimas Misevičius
// Distributed under the BSD-style license that can be
// found in the LICENSE file.
//
#ifndef UPA_IDNA_H
#define UPA_IDNA_H

#include "bitmask_operators.hpp"
#include "upa/idna/idna_table.h"
#include "upa/idna/iterate_utf.h"
#include "upa/idna/nfc.h"
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

// enable bit mask on idna::Option
template<>
struct enable_bitmask_operators<Option> {
    static const bool enable = true;
};

namespace detail {

// Bit flags
inline bool has(Option option, const Option value) {
    return (option & value) == value;
}

// IDNA map and normalize NFC
template <typename CharT>
inline std::u32string map(const CharT* input, const CharT* input_end, Option options) {
    using namespace upa::idna::util;

    // P1 - Map
    std::u32string mapped;
    const uint32_t status_mask = getStatusMask(has(options, Option::UseSTD3ASCIIRules));
    for (auto it = input; it != input_end; ) {
        const uint32_t cp = getCodePoint(it, input_end);
        const uint32_t value = getCharInfo(cp);

        switch (value & status_mask) {
        case CP_VALID:
            mapped.push_back(cp);
            break;
        case CP_MAPPED:
            if (has(options, Option::Transitional) && cp == 0x1E9E) {
                // replace U+1E9E capital sharp s by “ss”
                mapped.append(U"ss", 2);
            } else {
                apply_mapping(value, mapped);
            }
            break;
        case CP_DEVIATION:
            if (has(options, Option::Transitional)) {
                apply_mapping(value, mapped);
            } else {
                mapped.push_back(cp);
            }
            break;
        default:
            // CP_DISALLOWED
            // CP_NO_STD3_MAPPED, CP_NO_STD3_VALID if Option::UseSTD3ASCIIRules
            // Starting with Unicode 15.1.0 - don't record an error (error = true)
            mapped.push_back(cp);
            break;
        }
    }

    // P2 - Normalize
    normalize_nfc(mapped);

    return mapped;
}

bool to_ascii_mapped(std::string& domain, std::u32string&& mapped, Option options);
bool to_unicode_mapped(std::u32string& domain, std::u32string&& mapped, Option options);

} // namespace detail

template <typename CharT>
bool to_ascii(std::string& domain, const CharT* input, const CharT* input_end, Option options) {
    // P1 - Map and further processing
    return detail::to_ascii_mapped(domain, detail::map(input, input_end, options), options);
}

template <typename CharT>
bool to_unicode(std::u32string& domain, const CharT* input, const CharT* input_end, Option options) {
    // P1 - Map and further processing
    return detail::to_unicode_mapped(domain, detail::map(input, input_end, options), options);
}

} // namespace idna
} // namespace upa

#endif // UPA_IDNA_H
