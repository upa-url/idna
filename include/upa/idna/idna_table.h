// Copyright 2017-2024 Rimas Misevičius
// Distributed under the BSD-style license that can be
// found in the LICENSE file.
//
#ifndef UPA_IDNA_IDNA_TABLE_H
#define UPA_IDNA_IDNA_TABLE_H

#include <cstddef>
#include <cstdint>

namespace upa { // NOLINT(modernize-concat-nested-namespaces)
namespace idna { // NOLINT(modernize-concat-nested-namespaces)
namespace util {

// ASCII
const std::uint8_t AC_VALID = 0x01;
const std::uint8_t AC_MAPPED = 0x02;
const std::uint8_t AC_DISALLOWED_STD3 = 0x04;

// Unicode
const std::uint32_t CP_DISALLOWED = 0;
const std::uint32_t CP_VALID = 0x0001 << 16;
const std::uint32_t CP_MAPPED = 0x0002 << 16;
const std::uint32_t CP_DEVIATION = CP_VALID | CP_MAPPED; // 0x0003 << 16
const std::uint32_t CP_DISALLOWED_STD3 = 0x0004 << 16;
const std::uint32_t CP_NO_STD3_VALID = CP_VALID | CP_DISALLOWED_STD3;
const std::uint32_t MAP_TO_ONE = 0x0008 << 16;
// General_Category=Mark
const std::uint32_t CAT_MARK = 0x0010 << 16;
// ContextJ
const std::uint32_t CAT_Virama   = 0x0020 << 16;
//todo: as values >1
const std::uint32_t CAT_Joiner_D = 0x0040 << 16;
const std::uint32_t CAT_Joiner_L = 0x0080 << 16;
const std::uint32_t CAT_Joiner_R = 0x0100 << 16;
const std::uint32_t CAT_Joiner_T = 0x0200 << 16;
// Bidi
//todo: as values >1
const std::uint32_t CAT_Bidi_L    = 0x0400 << 16;
const std::uint32_t CAT_Bidi_R_AL = 0x0800 << 16;
const std::uint32_t CAT_Bidi_AN   = 0x1000 << 16;
const std::uint32_t CAT_Bidi_EN   = 0x2000 << 16;
const std::uint32_t CAT_Bidi_ES_CS_ET_ON_BN = 0x4000 << 16;
const std::uint32_t CAT_Bidi_NSM  = 0x8000 << 16;

// BEGIN-GENERATED
const std::size_t blockShift = 4;
const std::uint32_t blockMask = 0xF;
const std::uint32_t defaultStart = 0x323B0;
const std::uint32_t defaultValue = 0;
const std::uint32_t specRange1 = 0xE0100;
const std::uint32_t specRange2 = 0xE01EF;
const std::uint32_t specValue = 0x20000;

extern const std::uint32_t blockData[];
extern const std::uint16_t blockIndex[];
extern const char32_t allCharsTo[];

extern const std::uint8_t comp_disallowed_std3[3];

extern const std::uint8_t asciiData[128];
// END-GENERATED


constexpr std::uint32_t getStatusMask(bool useSTD3ASCIIRules) noexcept {
    return useSTD3ASCIIRules ? (0x0007 << 16) : (0x0003 << 16);
}

inline std::uint32_t getValidMask(bool useSTD3ASCIIRules, bool transitional) noexcept {
    const std::uint32_t status_mask = getStatusMask(useSTD3ASCIIRules);
    // (CP_DEVIATION = CP_VALID | CP_MAPPED) & ~CP_MAPPED ==> CP_VALID
    return transitional ? status_mask : (status_mask & ~CP_MAPPED);
}

inline std::uint32_t getCharInfo(uint32_t cp) {
    if (cp >= defaultStart) {
        if (cp >= specRange1 && cp <= specRange2) {
            return specValue;
        }
        return defaultValue;
    }
    return blockData[(blockIndex[cp >> blockShift] << blockShift) | (cp & blockMask)];
}

template <class StrT>
inline std::size_t apply_mapping(uint32_t val, StrT& output) {
    if (val & MAP_TO_ONE) {
        output.push_back(val & 0xFFFF);
        return 1;
    }
    if (val & 0xFFFF) {
        std::size_t len = (val & 0xFFFF) >> 13;
        std::size_t ind = val & 0x1FFF;
        if (len == 7) {
            len += ind >> 8;
            ind &= 0xFF;
        }
        const auto* ptr = static_cast<const char32_t*>(allCharsTo) + ind;
        output.append(ptr, len);
        return len;
    }
    return 0;
}

} // namespace util
} // namespace idna
} // namespace upa

#endif // UPA_IDNA_IDNA_TABLE_H
