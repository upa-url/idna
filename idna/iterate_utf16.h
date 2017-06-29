#ifndef ITERATE_UTF16_H
#define ITERATE_UTF16_H

#include <cstdint>

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

template <class InputIt>
inline uint32_t getCodePoint(InputIt& it, InputIt last) {
    // assume it != last
    const uint32_t c1 = *it++;
    if (is_surrogate_lead(c1) && it != last) {
        const uint32_t c2 = *it;
        if (is_surrogate_trail(c2)) {
            it++;
            return get_suplementary(c1, c2);
        }
    }
    return c1;
}

template <class InputIt>
inline uint32_t prevCodePoint(InputIt first, InputIt& it) {
    // assume it != first
    const uint32_t c2 = *(--it);
    if (is_surrogate_trail(c2) && it != first) {
        const uint32_t c1 = *(--it);
        if (is_surrogate_lead(c1)) {
            return get_suplementary(c1, c2);
        }
        it++;
    }
    return c2;
}

template <class InputIt>
inline uint32_t peekCodePoint(InputIt it, InputIt last) {
    return getCodePoint(it, last);
}

// safe

template <class InputIt>
inline InputIt skipCodePoint(InputIt it, InputIt last) {
    if (it != last) {
        const uint32_t c1 = *it++;
        if (is_surrogate_lead(c1) && it != last) {
            if (is_surrogate_trail(*it))
                it++;
        }
    }
    return it;
}

template <class InputIt>
inline InputIt skipCodePoints(size_t count, InputIt it, InputIt last) {
    while (count && it != last) {
        const uint32_t c1 = *it++;
        if (is_surrogate_lead(c1) && it != last) {
            if (is_surrogate_trail(*it))
                it++;
        }
        count--;
    }
    return it;
}


template <class InputIt>
inline size_t countCharUnits(InputIt it, InputIt last) {
    if (it != last) {
        const uint32_t c1 = *it++;
        if (is_surrogate_lead(c1) && it != last) {
            if (is_surrogate_trail(*it))
                return 2;
        }
        return 1;
    }
    return 0;
}

template <class InputIt>
inline size_t pos_of_code_point_at(size_t count, InputIt it, InputIt last) {
    InputIt start = it;
    while (count && it != last) {
        const uint32_t c1 = *it++;
        if (is_surrogate_lead(c1) && it != last) {
            if (is_surrogate_trail(*it))
                it++;
        }
        count--;
    }
    return std::distance(start, it);
}

#endif // ITERATE_UTF16_H
