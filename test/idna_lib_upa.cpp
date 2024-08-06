// Copyright 2017-2024 Rimas Misevičius
// Distributed under the BSD-style license that can be
// found in the LICENSE file.
//
#include "convert_utf.h"
#include "idna_lib.h"

// IDNA (UTS46)
#include "upa/idna.h"

// conversion
#include <algorithm>
#include <iterator>


namespace {

    inline std::string utf8_from_utf32(const std::u32string& input) {
        std::string str_utf8;
        auto iter = std::back_inserter(str_utf8);
        for (auto cp : input)
            append_utf8(iter, cp);
        return str_utf8;
    }

} // namespace


namespace idna_lib {

    bool toASCII(std::string& output, const std::string& input, bool transitional, bool is_input_ascii) {
        const bool res = upa::idna::to_ascii(output, input.data(), input.data() + input.length(),
            upa::idna::Option::VerifyDnsLength |
            upa::idna::Option::CheckHyphens |
            upa::idna::Option::CheckBidi |
            upa::idna::Option::CheckJoiners |
            upa::idna::Option::UseSTD3ASCIIRules |
            (transitional ? upa::idna::Option::Transitional : upa::idna::Option::Default) |
            (is_input_ascii ? upa::idna::Option::InputASCII : upa::idna::Option::Default)
        );

        if (!res) output.clear();

        return res;
    }

    bool toUnicode(std::string& output, const std::string& input, bool is_input_ascii) {
        std::u32string domain;

        bool res = upa::idna::to_unicode(domain, input.data(), input.data() + input.length(),
            // upa::idna::Option::VerifyDnsLength |
            upa::idna::Option::CheckHyphens |
            upa::idna::Option::CheckBidi |
            upa::idna::Option::CheckJoiners |
            upa::idna::Option::UseSTD3ASCIIRules |
            (is_input_ascii ? upa::idna::Option::InputASCII : upa::idna::Option::Default)
        );

        // to utf-8
        output = utf8_from_utf32(domain);

        return res;
    }

} // namespace idna_lib
