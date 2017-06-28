#ifndef IDNA_TABLE_H
#define IDNA_TABLE_H

#include <cstdint>

const uint32_t CP_DISALLOWED = 0;
const uint32_t CP_VALID = 0x0001 << 16;
const uint32_t CP_MAPPED = 0x0002 << 16;
const uint32_t CP_DEVIATION = CP_VALID | CP_MAPPED; // 0x0003 << 16
const uint32_t CP_NO_STD3_VALID = CP_VALID | (0x0004 << 16);
const uint32_t CP_NO_STD3_MAPPED = CP_MAPPED | (0x0004 << 16);
const uint32_t MAP_TO_ONE = 0x0008 << 16;
// General_Category=Mark
const uint32_t CAT_MARK = 0x0010 << 16;
// ContextJ
const uint32_t CAT_Virama   = 0x0020 << 16;
//todo: kaip reikšmės >1
const uint32_t CAT_Joiner_D = 0x0040 << 16;
const uint32_t CAT_Joiner_L = 0x0080 << 16;
const uint32_t CAT_Joiner_R = 0x0100 << 16;
const uint32_t CAT_Joiner_T = 0x0200 << 16;
// Bidi
//todo: kaip reikšmės >1
const uint32_t CAT_Bidi_L    = 0x0400 << 16;
const uint32_t CAT_Bidi_R_AL = 0x0800 << 16;
const uint32_t CAT_Bidi_AN   = 0x1000 << 16;
const uint32_t CAT_Bidi_EN   = 0x2000 << 16;
const uint32_t CAT_Bidi_ES_CS_ET_ON_BN = 0x4000 << 16;
const uint32_t CAT_Bidi_NSM  = 0x8000 << 16;

// BEGIN-GENERATED
// ... TODO ...
// END-GENERATED


inline uint32_t getStatusMask(bool useSTD3ASCIIRules) {
    return useSTD3ASCIIRules ? (0x0007 << 16) : (0x0003 << 16);
}

inline uint32_t getValidMask(bool useSTD3ASCIIRules, bool transitional) {
    const uint32_t status_mask = getStatusMask(useSTD3ASCIIRules);
    // (CP_DEVIATION = CP_VALID | CP_MAPPED) & ~CP_MAPPED ==> CP_VALID
    return transitional ? status_mask : (status_mask & ~CP_MAPPED);
}

// TODO

#endif
