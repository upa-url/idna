// Copyright 2017-2024 Rimas Misevičius
// Distributed under the BSD-style license that can be
// found in the LICENSE file.
//
#include "upa/idna/idna.h"
#include "upa/idna/idna_table.h"
#include "upa/idna/nfc.h"
#include "upa/idna/punycode.h"

#include <algorithm>
#include <string>
#include <vector>

namespace upa {
namespace idna {

// Split

template<class InputIt, class T, class FunT>
inline void split(InputIt first, InputIt last, const T& delim, FunT output) {
    auto start = first;
    while (true) {
        auto it = std::find(start, last, delim);
        output(start, it);
        if (it == last) break;
        start = ++it; // skip delimiter
    }
}

// Processing

enum BidiRes : int {
    IsBidiDomain = 0x01,
    IsBidiError = 0x02
};

static bool validate_label(const char32_t* label, const char32_t* label_end, Option options, bool full_check, int& bidiRes);
static bool validate_bidi(const char32_t* label, const char32_t* label_end, int& bidiRes);

static bool processing_mapped(std::u32string* pdecoded, const std::u32string& mapped, Option options) {
    using namespace util;

    bool error = false;

    // P3 - Break
    int bidiRes = 0;
    bool first_label = true;
    split(mapped.data(), mapped.data() + mapped.length(), 0x002E, [&](const char32_t* label, const char32_t* label_end) {
        if (first_label) {
            first_label = false;
        } else {
            if (pdecoded) pdecoded->push_back('.');
        }
        // P4 - Convert/Validate
        if (label_end - label >= 4 && label[0] == 'x' && label[1] == 'n' && label[2] == '-' && label[3] == '-') {
            if (*(label_end - 1) == '-' && label_end - label != 5) {
                // For compatibility with ICU, report errors on "xn--" or "xn--ascii-" labels.
                // Ignore "xn---", it will fail punycode::decode.
                // More info: https://github.com/whatwg/url/issues/760#issuecomment-1462706617
                error = true;
                // Decode "xn--ascii-" to "ascii" for to_unicode:
                if (pdecoded && label_end - label > 5)
                    pdecoded->append(label + 4, label_end - 1);
            } else {
                std::u32string ulabel;
                if (punycode::decode(ulabel, label + 4, label_end) == punycode::status::success) {
                    error = error || !validate_label(ulabel.data(), ulabel.data() + ulabel.length(), options & ~Option::Transitional, true, bidiRes);
                    if (pdecoded) pdecoded->append(ulabel);
                } else {
                    error = true; // punycode decode error
                    if (pdecoded) pdecoded->append(label, label_end);
                }
            }
        } else {
            error = error || !validate_label(label, label_end, options, false, bidiRes);
            if (pdecoded) pdecoded->append(label, label_end);
        }
    });

    return !error;
}

static bool validate_label(const char32_t* label, const char32_t* label_end, Option options, bool full_check, int& bidiRes) {
    using namespace util;

    if (label != label_end) {
        // V1 - The label must be in Unicode Normalization Form NFC
        if (full_check && !is_normalized_nfc(label, label_end))
            return false;

        if (detail::has(options, Option::CheckHyphens)) {
            // V2
            const size_t label_length = label_end - label;
            if (label_length >= 4 && label[2] == '-' && label[3] == '-')
                return false;
            // V3
            if (label[0] == '-' || *(label_end - 1) == '-')
                return false;
        } else if (full_check) {
            // V4: If not CheckHyphens, the label must not begin with “xn--”
            // https://github.com/whatwg/url/issues/603#issuecomment-842625331
            const size_t label_length = label_end - label;
            if (label_length >= 4 && label[0] == 'x' && label[1] == 'n' && label[2] == '-' && label[3] == '-')
                return false;
        }

        // V5 - can be ignored (todo)

        // V6
        const uint32_t cpflags = getCharInfo(label[0]); // label != label_end
        if (cpflags & CAT_MARK)
            return false;

        // V7
        // TODO: if (full_check)
        const uint32_t valid_mask = getValidMask(
            detail::has(options, Option::UseSTD3ASCIIRules),
            detail::has(options, Option::Transitional));
        for (auto it = label; it != label_end;) {
            const uint32_t cpflags = getCharInfo(*it++); // it != label_end
            if ((cpflags & valid_mask) != CP_VALID) {
                return false;
            }
        }

        // V8
        if (detail::has(options, Option::CheckJoiners)) {
            // https://tools.ietf.org/html/rfc5892#appendix-A
            for (auto it = label; it != label_end;) {
                auto start = it;
                const uint32_t cp = *it++; // it != label_end
                if (cp == 0x200C) {
                    // ZERO WIDTH NON-JOINER
                    if (start == label)
                        return false;
                    uint32_t cpflags = getCharInfo(*(--start)); // label != start
                    if (!(cpflags & CAT_Virama)) {
                        // {R,D} is required on the right
                        if (it == label_end)
                            return false;
                        // (Joining_Type:{L,D})(Joining_Type:T)* \u200C
                        while (!(cpflags & (CAT_Joiner_L | CAT_Joiner_D))) {
                            if (!(cpflags & CAT_Joiner_T) || start == label)
                                return false;
                            cpflags = getCharInfo(*(--start)); // label != start
                        }
                        // \u200C (Joining_Type:T)*(Joining_Type:{R,D})
                        cpflags = getCharInfo(*it++); // it != label_end
                        while (!(cpflags & (CAT_Joiner_R | CAT_Joiner_D))) {
                            if (!(cpflags & CAT_Joiner_T) || it == label_end)
                                return false;
                            cpflags = getCharInfo(*it++); // it != label_end
                        }
                        // HACK: because 0x200C is Non_Joining (U); 0x200D is Join_Causing (C), i.e.
                        // not L, D, R, T; then the cycle can be continued with `it` increased here
                    }
                } else if (cp == 0x200D) {
                    //  ZERO WIDTH JOINER
                    if (start == label ||
                        !(getCharInfo(*(--start)) & CAT_Virama)  // label != start
                        ) {
                        return false;
                    }
                }
            }
        }

        // V9
        if (detail::has(options, Option::CheckBidi)) {
            if (!validate_bidi(label, label_end, bidiRes))
                return false;
        }
    }
    return true;
}

inline bool is_bidi(const char32_t* first, const char32_t* last) {
    using namespace util;

    // https://tools.ietf.org/html/rfc5893#section-2
    for (auto it = first; it != last;) {
        const uint32_t cpflags = getCharInfo(*it++); // it != last
        // A "Bidi domain name" is a domain name that contains at least one RTL
        // label. An RTL label is a label that contains at least one character
        // of type R, AL, or AN.
        if (cpflags & (CAT_Bidi_R_AL | CAT_Bidi_AN))
            return true;
    }
    return false;
}

static bool validate_bidi(const char32_t* label, const char32_t* label_end, int& bidiRes) {
    using namespace util;

    // To check rules the label must have at least one character
    if (label == label_end)
        return true;

    // if there is a bidi error, then only check domain is bidi
    if (bidiRes & IsBidiError) {
        // error if bidi domain
        return !is_bidi(label, label_end);
    }

    // 1. The first character must be a character with Bidi property L, R, or AL
    uint32_t cpflags = getCharInfo(*label++); // label != label_end
    if (cpflags & CAT_Bidi_R_AL) {
        // RTL
        uint32_t end_cpflags = cpflags;
        uint32_t all_cpflags = 0;
        for (auto it = label; it != label_end;) {
            cpflags = getCharInfo(*it++); // it != label_end
            // 2. R, AL, AN, EN, ES, CS, ET, ON, BN, NSM
            if (!(cpflags & (CAT_Bidi_R_AL | CAT_Bidi_AN | CAT_Bidi_EN | CAT_Bidi_ES_CS_ET_ON_BN | CAT_Bidi_NSM)))
                return false;
            // 3. NSM
            if (!(cpflags & CAT_Bidi_NSM))
                end_cpflags = cpflags;
            // 4. EN, AN
            all_cpflags |= cpflags;
        }
        // 3. R, AL, AN, EN
        if (!(end_cpflags & (CAT_Bidi_R_AL | CAT_Bidi_AN | CAT_Bidi_EN)))
            return false;
        // 4. EN, AN
        if ((all_cpflags & (CAT_Bidi_AN | CAT_Bidi_EN)) == (CAT_Bidi_AN | CAT_Bidi_EN))
            return false;
        // is bidi domain
        bidiRes |= IsBidiDomain;
    } else if (cpflags & CAT_Bidi_L) {
        // LTR
        uint32_t end_cpflags = cpflags;
        for (auto it = label; it != label_end;) {
            cpflags = getCharInfo(*it++); // it != label_end
#if 0
            // 5. L, EN, ES, CS, ET, ON, BN, NSM
            if (!(cpflags & (CAT_Bidi_L | CAT_Bidi_EN | CAT_Bidi_ES_CS_ET_ON_BN | CAT_Bidi_NSM))) {
                // error if bidi domain
                if ((bidiRes & IsBidiDomain) || (cpflags & (CAT_Bidi_R_AL | CAT_Bidi_AN)) || is_bidi(it, label_end)) {
                    return false;
                } else {
                    bidiRes |= IsBidiError;
                }
            }
            // 6. NSM
            if (!(cpflags & CAT_Bidi_NSM))
                end_cpflags = cpflags;
#else
            // 5. L, EN, ES, CS, ET, ON, BN, NSM; 6. NSM
            if (cpflags & (CAT_Bidi_L | CAT_Bidi_EN | CAT_Bidi_ES_CS_ET_ON_BN)) {
                end_cpflags = cpflags;
            } else if (!(cpflags & CAT_Bidi_NSM)) {
                // error if bidi domain
                if ((bidiRes & IsBidiDomain) || (cpflags & (CAT_Bidi_R_AL | CAT_Bidi_AN)) || is_bidi(it, label_end)) {
                    return false;
                } else {
                    bidiRes |= IsBidiError;
                }
            }
#endif
        }
        // 6. L, EN
        if (!(end_cpflags & (CAT_Bidi_L | CAT_Bidi_EN))) {
            // error if bidi domain
            if (bidiRes & IsBidiDomain) {
                return false;
            } else {
                bidiRes |= IsBidiError;
            }
        }
    } else {
        // error if bidi domain
        if ((bidiRes & IsBidiDomain) || (cpflags & (CAT_Bidi_R_AL | CAT_Bidi_AN)) || is_bidi(label, label_end)) {
            return false;
        } else {
            bidiRes |= IsBidiError;
        }
    }
    return true;
}

namespace detail {

bool to_ascii_mapped(std::string& domain, const std::u32string& mapped, Option options) {
    // A1
    bool ok = processing_mapped(nullptr, mapped, options);

    // A2 - Break the result into labels at U+002E FULL STOP
    if (mapped.length() == 0) {
        // to simplify root label detection
        if (detail::has(options, Option::VerifyDnsLength))
            ok = false;
    } else {
        const char32_t* first = mapped.data();
        const char32_t* last = mapped.data() + mapped.length();
        size_t domain_len = static_cast<size_t>(-1);
        bool first_label = true;
        std::string encoded;
        split(first, last, 0x002E, [&](const char32_t* label, const char32_t* label_end) {
            // root is ending empty label
            const bool is_root = (label == last);

            // join
            if (first_label) {
                first_label = false;
            } else {
                encoded.push_back('.');
            }

            // A3 - to Punycode
            const size_t label_start_ind = encoded.length();
            if (std::any_of(label, label_end, [](char32_t ch) { return ch >= 0x80; })) {
                // has non-ASCII
                std::string alabel;
                if (punycode::encode(alabel, label, label_end) == punycode::status::success) {
                    encoded.push_back('x');
                    encoded.push_back('n');
                    encoded.push_back('-');
                    encoded.push_back('-');
                    encoded.append(alabel);
                } else {
                    encoded.append(label, label_end);
                    ok = false; // punycode error
                }
            } else {
                encoded.append(label, label_end);
            }

            // A4 - DNS length restrictions
            if (detail::has(options, Option::VerifyDnsLength) && !is_root) {
                const size_t label_length = encoded.length() - label_start_ind;
                // A4_2
                if (label_length < 1 || label_length > 63)
                    ok = false;
                // A4_1
                domain_len += (1 + label_length); // dot & label
                if (domain_len > 253) // early detect
                    ok = false;
            }
        });

        // A4_1
        if (detail::has(options, Option::VerifyDnsLength) && domain_len == 0)
            ok = false;

        domain = std::move(encoded);
    }

    return ok;
}

bool to_unicode_mapped(std::u32string& domain, const std::u32string& mapped, Option options) {
    return processing_mapped(&domain, mapped, options);
}


} // namespace detail
} // namespace idna
} // namespace upa
