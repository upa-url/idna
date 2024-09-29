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
#include <algorithm>
#include <string>
#include <type_traits> // std::make_unsigned

namespace upa::idna {

enum class Option {
    Default           = 0,
    UseSTD3ASCIIRules = 0x0001,
    Transitional      = 0x0002,
    VerifyDnsLength   = 0x0004,
    CheckHyphens      = 0x0008,
    CheckBidi         = 0x0010,
    CheckJoiners      = 0x0020,
    // ASCII optimization
    InputASCII        = 0x1000,
};

} // namespace upa::idna

// enable bit mask operators on upa::idna::Option
template<>
struct enable_bitmask_operators<upa::idna::Option> {
    static const bool enable = true;
};

namespace upa::idna {
namespace detail {

// Bit flags
inline bool has(Option option, const Option value) {
    return (option & value) == value;
}

inline Option domain_options(bool be_strict, bool is_input_ascii) {
    // https://url.spec.whatwg.org/#concept-domain-to-ascii
    // https://url.spec.whatwg.org/#concept-domain-to-unicode
    // Note. The to_unicode ignores Option::VerifyDnsLength
    auto options = be_strict
        ? Option::CheckBidi | Option::CheckJoiners | Option::UseSTD3ASCIIRules | Option::VerifyDnsLength
        : Option::CheckBidi | Option::CheckJoiners;
    if (is_input_ascii)
        options |= Option::InputASCII;
    return options;
}

template <typename CharT>
constexpr char ascii_to_lower_char(CharT c) noexcept {
    return static_cast<char>((c <= 'Z' && c >= 'A') ? (c | 0x20) : c);
}

// IDNA map and normalize NFC
template <typename CharT>
inline bool map(std::u32string& mapped, const CharT* input, const CharT* input_end, Option options, bool is_to_ascii) {
    using UCharT = std::make_unsigned_t<CharT>;

    // P1 - Map
    if (has(options, Option::InputASCII)) {
        // The input is in ASCII and can contain `xn--` labels
        mapped.reserve(input_end - input);
        if (has(options, Option::UseSTD3ASCIIRules)) {
            for (const auto* it = input; it != input_end; ++it) {
                const auto cp = static_cast<UCharT>(*it);
                switch (util::asciiData[cp]) {
                case util::AC_VALID:
                    mapped.push_back(cp);
                    break;
                case util::AC_MAPPED:
                    mapped.push_back(cp | 0x20);
                    break;
                default:
                    // util::AC_DISALLOWED_STD3
                    if (is_to_ascii)
                        return false;
                    mapped.push_back(cp);
                }
            }
        } else {
            for (const auto* it = input; it != input_end; ++it)
                mapped.push_back(ascii_to_lower_char(*it));
        }
    } else {
        const uint32_t status_mask = util::getStatusMask(has(options, Option::UseSTD3ASCIIRules));
        for (auto it = input; it != input_end; ) {
            const uint32_t cp = util::getCodePoint(it, input_end);
            const uint32_t value = util::getCharInfo(cp);

            switch (value & status_mask) {
            case util::CP_VALID:
                mapped.push_back(cp);
                break;
            case util::CP_MAPPED:
                if (has(options, Option::Transitional) && cp == 0x1E9E) {
                    // replace U+1E9E capital sharp s by “ss”
                    mapped.append(U"ss", 2);
                } else {
                    util::apply_mapping(value, mapped);
                }
                break;
            case util::CP_DEVIATION:
                if (has(options, Option::Transitional)) {
                    util::apply_mapping(value, mapped);
                } else {
                    mapped.push_back(cp);
                }
                break;
            default:
                // CP_DISALLOWED
                // CP_NO_STD3_MAPPED, CP_NO_STD3_VALID if Option::UseSTD3ASCIIRules
                // Starting with Unicode 15.1.0 - don't record an error
                if (is_to_ascii && // to_ascii optimization
                    ((value & util::CP_DISALLOWED_STD3) == 0
                    ? !std::binary_search(std::begin(util::comp_disallowed), std::end(util::comp_disallowed), cp)
                    : !std::binary_search(std::begin(util::comp_disallowed_std3), std::end(util::comp_disallowed_std3), cp)))
                    return false;
                mapped.push_back(cp);
                break;
            }
        }

        // P2 - Normalize
        normalize_nfc(mapped);
    }

    return true;
}

bool to_ascii_mapped(std::string& domain, const std::u32string& mapped, Option options);
bool to_unicode_mapped(std::u32string& domain, const std::u32string& mapped, Option options);

} // namespace detail


/// @brief Implements the Unicode IDNA ToASCII
///
/// See: https://www.unicode.org/reports/tr46/#ToASCII
///
/// @param[out] domain buffer to store result string
/// @param[in]  input source domain string
/// @param[in]  input_end the end of source domain string
/// @param[in]  options
/// @return `true` on success, or `false` on failure
template <typename CharT>
inline bool to_ascii(std::string& domain, const CharT* input, const CharT* input_end, Option options) {
    // P1 - Map and further processing
    std::u32string mapped;
    domain.clear();
    return
        detail::map(mapped, input, input_end, options, true) &&
        detail::to_ascii_mapped(domain, mapped, options);
}

/// @brief Implements the Unicode IDNA ToUnicode
///
/// See: https://www.unicode.org/reports/tr46/#ToUnicode
///
/// @param[out] domain buffer to store result string
/// @param[in]  input source domain string
/// @param[in]  input_end the end of source domain string
/// @param[in]  options
/// @return `true` on success, or `false` on errors
template <typename CharT>
inline bool to_unicode(std::u32string& domain, const CharT* input, const CharT* input_end, Option options) {
    // P1 - Map and further processing
    std::u32string mapped;
    detail::map(mapped, input, input_end, options, false);
    return detail::to_unicode_mapped(domain, mapped, options);
}

/// @brief Implements the domain to ASCII algorithm
///
/// See: https://url.spec.whatwg.org/#concept-domain-to-ascii
///
/// @param[out] domain buffer to store result string
/// @param[in]  input source domain string
/// @param[in]  input_end the end of source domain string
/// @param[in]  be_strict
/// @param[in]  is_input_ascii
/// @return `true` on success, or `false` on failure
template <typename CharT>
inline bool domain_to_ascii(std::string& domain, const CharT* input, const CharT* input_end,
    bool be_strict = false, bool is_input_ascii = false)
{
    const bool res = to_ascii(domain, input, input_end, detail::domain_options(be_strict, is_input_ascii));

    // 3. If result is the empty string, domain-to-ASCII validation error, return failure.
    //
    // Note. Result of to_ascii can be the empty string if input consists entirely of
    // IDNA ignored code points.
    return res && !domain.empty();
}

/// @brief Implements the domain to Unicode algorithm
///
/// See: https://url.spec.whatwg.org/#concept-domain-to-unicode
///
/// @param[out] domain buffer to store result string
/// @param[in]  input source domain string
/// @param[in]  input_end the end of source domain string
/// @param[in]  be_strict
/// @param[in]  is_input_ascii
/// @return `true` on success, or `false` on errors
template <typename CharT>
inline bool domain_to_unicode(std::u32string& domain, const CharT* input, const CharT* input_end,
    bool be_strict = false, bool is_input_ascii = false)
{
    return to_unicode(domain, input, input_end, detail::domain_options(be_strict, is_input_ascii));
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


} // namespace upa::idna

#endif // UPA_IDNA_IDNA_H
