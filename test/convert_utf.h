#ifndef UPA_CONVERT_UTF_H
#define UPA_CONVERT_UTF_H

#include <cstdint>


// Modified version of the U8_APPEND_UNSAFE macro in utf8.h from ICU
//
// It converts code_point to UTF-8 bytes sequence and calls appendByte function for each byte.
// It assumes a valid code point (https://infra.spec.whatwg.org/#scalar-value).

template <class OutputIt>
inline void append_utf8(OutputIt outit, char32_t code_point) {
    if (code_point <= 0x7f) {
        *outit++ = static_cast<std::uint8_t>(code_point);
    } else {
        if (code_point <= 0x7ff) {
            *outit++ = static_cast<std::uint8_t>((code_point >> 6) | 0xc0);
        } else {
            if (code_point <= 0xffff) {
                *outit++ = static_cast<std::uint8_t>((code_point >> 12) | 0xe0);
            } else {
                *outit++ = static_cast<std::uint8_t>((code_point >> 18) | 0xf0);
                *outit++ = static_cast<std::uint8_t>(((code_point >> 12) & 0x3f) | 0x80);
            }
            *outit++ = static_cast<std::uint8_t>(((code_point >> 6) & 0x3f) | 0x80);
        }
        *outit++ = static_cast<std::uint8_t>((code_point & 0x3f) | 0x80);
    }
}


#endif // UPA_CONVERT_UTF_H
