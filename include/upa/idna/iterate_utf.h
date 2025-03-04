// Copyright 2017-2024 Rimas Misevičius
// Distributed under the BSD-style license that can be
// found in the LICENSE file.
//
#ifndef UPA_IDNA_ITERATE_UTF_H
#define UPA_IDNA_ITERATE_UTF_H

#include <cstdint>

namespace upa { // NOLINT(modernize-concat-nested-namespaces)
namespace idna { // NOLINT(modernize-concat-nested-namespaces)
namespace util {

// Get code point from UTF-8

constexpr char32_t kReplacementCharacter = 0xFFFD;

// https://encoding.spec.whatwg.org/#utf-8-decoder
inline uint32_t getCodePoint(const char*& it, const char* last) noexcept {
    const auto uchar = [](char c) { return static_cast<unsigned char>(c); };
    // assume it != last
    uint32_t c1 = uchar(*it++);
    if (c1 >= 0x80) {
        if (c1 < 0xC2 || c1 > 0xF4)
            return kReplacementCharacter;
        if (c1 <= 0xDF) {
            // 2-bytes
            if (it == last || uchar(*it) < 0x80 || uchar(*it) > 0xBF)
                return kReplacementCharacter;
            c1 = ((c1 & 0x1F) << 6) | (uchar(*it++) & 0x3F);
        } else if (c1 <= 0xEF) {
            // 3-bytes
            const unsigned char clb = c1 == 0xE0 ? 0xA0 : 0x80;
            const unsigned char cub = c1 == 0xED ? 0x9F : 0xBF;
            if (it == last || uchar(*it) < clb || uchar(*it) > cub)
                return kReplacementCharacter;
            c1 = ((c1 & 0x0F) << 6) | (uchar(*it++) & 0x3F);
            if (it == last || uchar(*it) < 0x80 || uchar(*it) > 0xBF)
                return kReplacementCharacter;
            c1 = (c1 << 6) | (uchar(*it++) & 0x3F);
        } else {
            // 4-bytes
            const unsigned char clb = c1 == 0xF0 ? 0x90 : 0x80;
            const unsigned char cub = c1 == 0xF4 ? 0x8F : 0xBF;
            if (it == last || uchar(*it) < clb || uchar(*it) > cub)
                return kReplacementCharacter;
            c1 = ((c1 & 0x07) << 6) | (uchar(*it++) & 0x3F);
            if (it == last || uchar(*it) < 0x80 || uchar(*it) > 0xBF)
                return kReplacementCharacter;
            c1 = (c1 << 6) | (uchar(*it++) & 0x3F);
            if (it == last || uchar(*it) < 0x80 || uchar(*it) > 0xBF)
                return kReplacementCharacter;
            c1 = (c1 << 6) | (uchar(*it++) & 0x3F);
        }
    }
    return c1;
}

// Get code point from UTF-16

template <class T>
constexpr bool is_surrogate_lead(T ch) noexcept {
    return (ch & 0xFFFFFC00) == 0xD800;
}

template <class T>
constexpr bool is_surrogate_trail(T ch) noexcept {
    return (ch & 0xFFFFFC00) == 0xDC00;
}

// Get a supplementary code point value(U + 10000..U + 10ffff)
// from its lead and trail surrogates.
inline uint32_t get_suplementary(uint32_t lead, uint32_t trail) noexcept {
    constexpr uint32_t surrogate_offset = (static_cast<uint32_t>(0xD800) << 10) + 0xDC00 - 0x10000;
    return (lead << 10) + trail - surrogate_offset;
}

// assumes it != last

inline uint32_t getCodePoint(const char16_t*& it, const char16_t* last) noexcept {
    // assume it != last
    const uint32_t c1 = *it++;
    if (is_surrogate_lead(c1) && it != last) {
        const uint32_t c2 = *it;
        if (is_surrogate_trail(c2)) {
            ++it;
            return get_suplementary(c1, c2);
        }
    }
    return c1;
}

// Get code point from UTF-32

inline uint32_t getCodePoint(const char32_t*& it, const char32_t*) noexcept {
    // assume it != last
    return *it++;
}

} // namespace util
} // namespace idna
} // namespace upa

#endif // UPA_IDNA_ITERATE_UTF_H
