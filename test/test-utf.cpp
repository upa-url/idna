// Copyright 2024 Rimas Misevičius
// Distributed under the BSD-style license that can be
// found in the LICENSE file.
//
#include "upa/idna/iterate_utf.h"
#include "ddt/DataDrivenTest.hpp"
#include <iterator>

template <class T>
inline bool is_surrogate(T ch) {
    return (ch & 0xFFFFF800) == 0xD800;
}

// Modified version of the U8_APPEND_UNSAFE macro in utf8.h from ICU
//
// It converts code_point to UTF-8 bytes sequence and calls appendByte function for each byte.
// It assumes a valid code point (https://infra.spec.whatwg.org/#scalar-value).

template <class OutputIt>
inline void append_utf8(OutputIt outit, std::uint32_t code_point) {
    if (code_point <= 0x7f) {
        *outit++ = static_cast<uint8_t>(code_point);
    } else {
        if (code_point <= 0x7ff) {
            *outit++ = static_cast<uint8_t>((code_point >> 6) | 0xc0);
        } else {
            if (code_point <= 0xffff) {
                *outit++ = static_cast<uint8_t>((code_point >> 12) | 0xe0);
            } else {
                *outit++ = static_cast<uint8_t>((code_point >> 18) | 0xf0);
                *outit++ = static_cast<uint8_t>(((code_point >> 12) & 0x3f) | 0x80);
            }
            *outit++ = static_cast<uint8_t>(((code_point >> 6) & 0x3f) | 0x80);
        }
        *outit++ = static_cast<uint8_t>((code_point & 0x3f) | 0x80);
    }
}


int main()
{
    DataDrivenTest ddt;
    ddt.config_show_passed(false);
    ddt.config_debug_break(false);

    ddt.test_case("UTF-8 decoding", [&](DataDrivenTest::TestCase& tc) {
        for (std::uint32_t cp = 0; cp <= 0x10FFFF; ++cp) {
            if (is_surrogate(cp))
                continue;

            std::string str_utf8;
            append_utf8(std::back_inserter(str_utf8), cp);
            const auto* first = str_utf8.data();
            const auto* last = str_utf8.data() + str_utf8.length();
            const auto cp_res = upa::idna::util::getCodePoint(first, last);

            tc.assert_equal(cp, cp_res, "decoded code point");
        }
    });

    return ddt.result();
}
