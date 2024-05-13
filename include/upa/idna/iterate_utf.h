#ifndef UPA_IDNA_ITERATE_UTF_H
#define UPA_IDNA_ITERATE_UTF_H

#include <cstdint>

namespace upa {
namespace idna {
namespace util {

// UTF-16 iterate

template <class T>
inline bool is_surrogate_lead(T ch) {
    return (ch & 0xFFFFFC00) == 0xD800;
}

template <class T>
inline bool is_surrogate_trail(T ch) {
    return (ch & 0xFFFFFC00) == 0xDC00;
}

// Get a supplementary code point value(U + 10000..U + 10ffff)
// from its lead and trail surrogates.
inline uint32_t get_suplementary(uint32_t lead, uint32_t trail) {
    const uint32_t surrogate_offset = (static_cast<uint32_t>(0xD800) << 10) + 0xDC00 - 0x10000;
    return (lead << 10) + trail - surrogate_offset;
}

// assumes it != last

inline uint32_t getCodePoint(const char16_t*& it, const char16_t* last) {
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

} // namespace util
} // namespace idna
} // namespace upa

#endif // UPA_IDNA_ITERATE_UTF_H
