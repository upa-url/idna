#include "idna_test.h"

// conversion
#include <algorithm>
#include <codecvt>
#include <locale>    // std::wstring_convert

// IDNA (UTS46)
#include "idna/idna.h"

namespace {
    static std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t> conv16;
}


namespace idna_test {

    bool toASCII(std::string& output, const std::string& input, bool transitional) {
        // to utf-16
        std::u16string domain(conv16.from_bytes(input));

        bool res = idna::ToASCII(domain,
            idna::Option::VerifyDnsLength |
            idna::Option::CheckHyphens |
            idna::Option::CheckBidi |
            idna::Option::CheckJoiners |
            idna::Option::UseSTD3ASCIIRules |
            (transitional ? idna::Option::Transitional : idna::Option::Default)
            );

        // to utf-8
        output = res ? conv16.to_bytes(domain) : "";

        return res;
    }

    bool toUnicode(std::string& output, const std::string& input) {
        // to utf-16
        std::u16string domain(conv16.from_bytes(input));

        bool res = idna::ToUnicode(domain,
            // idna::Option::VerifyDnsLength |
            idna::Option::CheckHyphens |
            idna::Option::CheckBidi |
            idna::Option::CheckJoiners |
            idna::Option::UseSTD3ASCIIRules
            );

#if 1
        // IdnaTest.txt (10.0.0) still have incorrect tests for toUnicode with error code [A4_2]
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
        output = res ? conv16.to_bytes(domain) : "";

        return res;
    }

} // namespace idna_test
