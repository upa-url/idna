// Copyright 2024 Rimas Misevičius
// Distributed under the BSD-style license that can be
// found in the LICENSE file.
//
#include "upa/idna/nfc.h"
#include "nfc_table.h"
#include <algorithm>
#include <utility> // std::move


namespace upa::idna {

namespace hangul {
    constexpr char32_t SBase = 0xAC00;
    constexpr char32_t LBase = 0x1100;
    constexpr char32_t VBase = 0x1161;
    constexpr char32_t TBase = 0x11A7;
    constexpr char32_t LCount = 19;
    constexpr char32_t VCount = 21;
    constexpr char32_t TCount = 28;
    constexpr char32_t NCount = VCount * TCount; // 588
    constexpr char32_t SCount = LCount * NCount; // 11172
} // namespace hangul

void compose(std::u32string& str)
{
    if (str.length() == 0)
        return;
    std::size_t dest = 0;

    std::size_t i = 1;
    for (; i < str.length(); ++i) {
        auto last = str[i - 1];
        const auto ch = str[i];

        // 1. check to see if two current characters are L and V
        if (last >= hangul::LBase && last < hangul::LBase + hangul::LCount) {
            if (ch >= hangul::VBase && ch < hangul::VBase + hangul::VCount) {
                const auto LIndex = last - hangul::LBase;
                const auto VIndex = ch - hangul::VBase;
                // make syllable of form LV
                last = hangul::SBase + (LIndex * hangul::VCount + VIndex) * hangul::TCount;
                ++i; // ch consumed
                // check to see if the next character is T
                if (i < str.length()) {
                    const auto next_ch = str[i];
                    if (next_ch > hangul::TBase && next_ch < hangul::TBase + hangul::TCount) {
                        // make syllable of form LVT
                        last += next_ch - hangul::TBase; // TIndex
                        ++i; // next_ch consumed
                    }
                }
            }
            str[dest++] = last;
        }
        // 2. check to see if two current characters are LV and T
        else if (last >= hangul::SBase && last < hangul::SBase + hangul::SCount) {
            // SIndex = last - hangul::SBase
            if ((last - hangul::SBase) % hangul::TCount == 0 &&
                ch > hangul::TBase && ch < hangul::TBase + hangul::TCount) {
                // make syllable of form LVT
                last += ch - hangul::TBase; // TIndex
                ++i; // ch consumed
            }
            str[dest++] = last;
        }
        else {
            const auto L_dest = dest++;
            auto L_info = normalize::get_composition_info(last);
            int prev_ccc = -1;
            for (; i < str.length(); ++i) {
                const auto C = str[i];
                const int C_ccc = normalize::get_ccc(C);
                if (L_info != 0 && prev_ccc < C_ccc) {
                    // Check <last, C> has canonically equivalent Primary Composite
                    const auto* comp_arr = normalize::get_composition_data(L_info);
                    const auto* comp_arr_end = comp_arr + normalize::get_composition_len(L_info);
                    const auto* comp = std::lower_bound(comp_arr, comp_arr_end, C,
                        [](const normalize::codepoint_key_val& a, char32_t b) { return a.key < b; });
                    if (comp != comp_arr_end && comp->key == C) {
                        last = comp->val;
                        L_info = normalize::get_composition_info(last);
                        continue;
                    }
                }

                if (C_ccc == 0)
                    break;
                prev_ccc = C_ccc;
                str[dest++] = C;
            }
            str[L_dest] = last;
        }
    }
    if (i == str.length())
        str[dest++] = str[i - 1];
    str.resize(dest);
}

void canonical_decompose(std::u32string& str)
{
    std::u32string out;

    for (auto cp : str) {
        if (cp >= hangul::SBase && cp < hangul::SBase + hangul::SCount) {
            // Hangul Decomposition Algorithm
            const auto SIndex = cp - hangul::SBase;
            out += static_cast<char32_t>(hangul::LBase + SIndex / hangul::NCount); // L
            out += static_cast<char32_t>(hangul::VBase + (SIndex % hangul::NCount) / hangul::TCount); // V
            if (SIndex % hangul::TCount != 0)
                out += static_cast<char32_t>(hangul::TBase + SIndex % hangul::TCount); // T
        } else {
            const auto cp_info = normalize::get_decomposition_info(cp);
            if (cp_info) {
                // decompose
                out.append(
                    normalize::get_decomposition_chars(cp_info),
                    normalize::get_decomposition_len(cp_info)
                );
            } else {
                // no decomposition
                out += cp;
            }
        }
    }

    // Canonical Ordering Algorithm
    // Use Insertion sort:
    // https://en.wikipedia.org/wiki/Insertion_sort
    for (std::size_t i = 1; i < out.length(); ++i) {
        const auto ccc = normalize::get_ccc(out[i]);

        // is there a need to sort?
        if (ccc != 0 && normalize::get_ccc(out[i - 1]) > ccc) {
            // sort
            const auto cp = out[i];
            std::size_t j = i;
            do {
                out[j] = out[j - 1]; --j;
            } while (j != 0 && normalize::get_ccc(out[j - 1]) > ccc);
            out[j] = cp;
        }
    }

    str = std::move(out);
}

void normalize_nfc(std::u32string& str) {
    canonical_decompose(str);
    compose(str);
}

bool is_normalized_nfc(const char32_t* first, const char32_t* last) {
    std::u32string str{ first, last };
    normalize_nfc(str);
    return std::equal(first, last, str.data(), str.data() + str.length());
}


} // namespace upa::idna
