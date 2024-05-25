// Copyright 2017-2024 Rimas Misevičius
// Distributed under the BSD-style license that can be
// found in the LICENSE file.
//
#ifndef UPA_IDNA_IDNA_H
#define UPA_IDNA_IDNA_H

#include "bitmask_operators.hpp"
#include "idna_table.h"
#include "iterate_utf.h"
#include "nfc.h"
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
inline bool to_ascii(std::string& domain, const CharT* input, const CharT* input_end, Option options) {
    // P1 - Map and further processing
    return detail::to_ascii_mapped(domain, detail::map(input, input_end, options), options);
}

template <typename CharT>
inline bool to_unicode(std::u32string& domain, const CharT* input, const CharT* input_end, Option options) {
    // P1 - Map and further processing
    return detail::to_unicode_mapped(domain, detail::map(input, input_end, options), options);
}

/// @brief Implements the domain to ASCII algorithm
///
/// See: https://url.spec.whatwg.org/#concept-domain-to-ascii
///
/// @param[out] output buffer to store result string
/// @param[in]  domain input domain string
/// @param[in]  domain_end the end of input domain string
/// @param[in]  be_strict
/// @return `true` on success, or `false` on failure
template <typename CharT>
inline bool domain_to_ascii(std::string& output, const CharT* domain, const CharT* domain_end, bool be_strict = false) {
    const bool res = to_ascii(output, domain, domain_end, be_strict
        ? Option::CheckBidi | Option::CheckJoiners | Option::UseSTD3ASCIIRules | Option::VerifyDnsLength
        : Option::CheckBidi | Option::CheckJoiners);
    // 3. If result is the empty string, domain-to-ASCII validation error, return failure.
    //
    // Note. Result of to_ascii can be the empty string if input:
    // 1) consists entirely of IDNA ignored code points;
    // 2) is "xn--".
    return res && !output.empty();
}

/// @brief Implements the domain to Unicode algorithm
///
/// See: https://url.spec.whatwg.org/#concept-domain-to-unicode
///
/// @param[out] output buffer to store result string
/// @param[in]  domain input domain string
/// @param[in]  domain_end the end of input domain string
/// @param[in]  be_strict
/// @return `true` on success, or `false` on errors
template <typename CharT>
inline bool domain_to_unicode(std::u32string& output, const CharT* domain, const CharT* domain_end, bool be_strict = false) {
    return to_unicode(output, domain, domain_end, be_strict
        ? Option::CheckBidi | Option::CheckJoiners | Option::UseSTD3ASCIIRules
        : Option::CheckBidi | Option::CheckJoiners);
}

/// @brief Encodes Unicode version
///
/// The version is encoded as follows: <version 1st number> * 0x1000000 +
/// <version 2nd number> * 0x10000 + <version 3rd number> * 0x100 + <version 4th number>
///
/// For example for Unicode version 15.1.0 it returns 0x0F010000
///
/// @param[in] n1 version 1st number
/// @param[in] n2 version 2nd number
/// @param[in] n3 version 3rd number
/// @param[in] n4 version 4th number
/// @return encoded Unicode version
constexpr unsigned make_unicode_version(unsigned n1, unsigned n2 = 0,
    unsigned n3 = 0, unsigned n4 = 0) noexcept {
    return n1 << 24 | n2 << 16 | n3 << 8 | n4;
}

/// @brief Gets Unicode version that IDNA library conforms to
///
/// @return encoded Unicode version
/// @see make_unicode_version
inline unsigned unicode_version() {
    return make_unicode_version(15, 1);
}


} // namespace idna
} // namespace upa

#endif // UPA_IDNA_IDNA_H
