// Copyright 2024 Rimas Misevičius
// Distributed under the BSD-style license that can be
// found in the LICENSE file.
//
#ifndef UPA_IDNA_NFC_TABLE_H
#define UPA_IDNA_NFC_TABLE_H

#include <cstddef>
#include <cstdint>

namespace upa { // NOLINT(modernize-concat-nested-namespaces)
namespace idna { // NOLINT(modernize-concat-nested-namespaces)
namespace normalize {

struct codepoint_key_val {
    char32_t key;
    char32_t val;
};


// BEGIN-GENERATED
const std::size_t ccc_block_shift = 5;
const std::uint32_t ccc_block_mask = 0x1F;
const std::uint32_t ccc_default_start = 0x1E94B;
const std::uint8_t ccc_default_value = 0;
extern const std::uint8_t ccc_block[];
extern const std::uint8_t ccc_block_index[];

const std::size_t comp_block_shift = 5;
const std::uint32_t comp_block_mask = 0x1F;
const std::uint32_t comp_default_start = 0x11936;
const std::uint16_t comp_default_value = 0;
extern const std::uint16_t comp_block[];
extern const std::uint8_t comp_block_index[];
extern const codepoint_key_val comp_block_data[];

const std::size_t decomp_block_shift = 6;
const std::uint32_t decomp_block_mask = 0x3F;
const std::uint32_t decomp_default_start = 0x2FA1E;
const std::uint16_t decomp_default_value = 0;
extern const std::uint16_t decomp_block[];
extern const std::uint8_t decomp_block_index[];
extern const char32_t decomp_block_data[];
// END-GENERATED


// Canonical_Combining_Class (ccc)
inline std::uint8_t get_ccc(std::uint32_t cp) {
    if (cp >= ccc_default_start)
        return ccc_default_value;
    return ccc_block[
        (ccc_block_index[cp >> ccc_block_shift] << ccc_block_shift) |
        (cp & ccc_block_mask)
    ];
}

// Composition data
inline std::uint16_t get_composition_info(std::uint32_t cp) {
    if (cp >= comp_default_start)
        return comp_default_value;
    return comp_block[
        (comp_block_index[cp >> comp_block_shift] << comp_block_shift) |
        (cp & comp_block_mask)
    ];
}

inline std::size_t get_composition_len(std::uint16_t ci) {
    return ci >> 11;
}

inline const codepoint_key_val* get_composition_data(std::uint16_t ci) {
    return static_cast<const codepoint_key_val*>(comp_block_data) + (ci & 0x7FF);
}

// Decomposition data
inline std::uint16_t get_decomposition_info(std::uint32_t cp) {
    if (cp >= decomp_default_start)
        return decomp_default_value;
    return decomp_block[
        (decomp_block_index[cp >> decomp_block_shift] << decomp_block_shift) |
        (cp & decomp_block_mask)
    ];
}

inline std::size_t get_decomposition_len(std::uint16_t di) {
    return di >> 12;
}

inline const char32_t* get_decomposition_chars(std::uint16_t di) {
    return static_cast<const char32_t*>(decomp_block_data) + (di & 0xFFF);
}

} // namespace normalize
} // namespace idna
} // namespace upa

#endif // #ifndef UPA_IDNA_NFC_TABLE_H
