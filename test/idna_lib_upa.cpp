// Copyright 2017-2024 Rimas Misevičius
// Distributed under the BSD-style license that can be
// found in the LICENSE file.
//
#include "idna_lib.h"

// conversion
#include <algorithm>
#include <codecvt>
#include <locale>    // std::wstring_convert

// IDNA (UTS46)
#include "upa/idna.h"

namespace {
#if defined(_MSC_VER) && _MSC_VER >= 1900 && _MSC_VER < 1920
    // for VS 2015 and VS 2017 
    // https://stackoverflow.com/q/32055357
    static std::wstring_convert<std::codecvt_utf8<uint32_t>, uint32_t> conv32;

    inline std::string utf32_to_bytes(const std::u32string& input) {
        return conv32.to_bytes(
            reinterpret_cast<const uint32_t*>(input.data()),
            reinterpret_cast<const uint32_t*>(input.data() + input.length()));
    }
#else
    static std::wstring_convert<std::codecvt_utf8<char32_t>, char32_t> conv32;

    inline std::string utf32_to_bytes(const std::u32string& input) {
        return conv32.to_bytes(input);
    }
#endif
}


namespace idna_lib {

    bool toASCII(std::string& output, const std::string& input, bool transitional) {
        const bool res = upa::idna::to_ascii(output, input.data(), input.data() + input.length(),
            upa::idna::Option::VerifyDnsLength |
            upa::idna::Option::CheckHyphens |
            upa::idna::Option::CheckBidi |
            upa::idna::Option::CheckJoiners |
            upa::idna::Option::UseSTD3ASCIIRules |
            (transitional ? upa::idna::Option::Transitional : upa::idna::Option::Default)
            );

        if (!res) output.clear();

        return res;
    }

    bool toUnicode(std::string& output, const std::string& input) {
        std::u32string domain;

        bool res = upa::idna::to_unicode(domain, input.data(), input.data() + input.length(),
            // upa::idna::Option::VerifyDnsLength |
            upa::idna::Option::CheckHyphens |
            upa::idna::Option::CheckBidi |
            upa::idna::Option::CheckJoiners |
            upa::idna::Option::UseSTD3ASCIIRules
            );

#if 0
        // IdnaTest.txt (10.0.0) still have incorrect tests for toUnicode with error code [A4_2]
        // In IdnaTestV2.txt starting with Version 15.0.0 these cases are labeled [X4_2]
        // XXX: To pass them toUnicode must return error if returned domain has empty non-root label
        if (res) {
            auto domain_b = domain.begin();
            auto domain_e = domain.end();
            if (domain_b != domain_e) {
                auto start = domain_b;
                while (start != domain_e) { // while non-root
                    auto it = std::find(start, domain_e, '.');
                    if (start == it) { // is empty
                        res = false;
                        break;
                    }
                    if (it == domain_e) break;
                    start = ++it; // skip delimiter
                }
            } else {
                res = false;
            }
        }
#endif

        // to utf-8
        output = utf32_to_bytes(domain);

        return res;
    }

} // namespace idna_lib
