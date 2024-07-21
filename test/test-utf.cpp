// Copyright 2024 Rimas Misevičius
// Distributed under the BSD-style license that can be
// found in the LICENSE file.
//
#include "upa/idna/iterate_utf.h"
#include "ddt/DataDrivenTest.hpp"
#include "convert_utf.h"
#include <iterator>

template <class T>
inline bool is_surrogate(T ch) {
    return (ch & 0xFFFFF800) == 0xD800;
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
