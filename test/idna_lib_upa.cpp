#include "idna_lib.h"

// conversion
#include <algorithm>
#include <codecvt>
#include <locale>    // std::wstring_convert

// IDNA (UTS46)
#include "upa/idna/idna.h"

namespace {
#if defined(_MSC_VER) && _MSC_VER >= 1900 && _MSC_VER < 1920
    // for VS 2015 and VS 2017 
    // https://stackoverflow.com/q/32055357
    static std::wstring_convert<std::codecvt_utf8_utf16<uint16_t>, uint16_t> conv16;

    inline std::u16string utf16_from_bytes(const std::string& input) {
        auto out(conv16.from_bytes(input));
        return std::u16string(reinterpret_cast<const char16_t*>(out.data()), out.length());
    }
    inline std::string utf16_to_bytes(const std::u16string& input) {
        return conv16.to_bytes(
            reinterpret_cast<const uint16_t*>(input.data()),
            reinterpret_cast<const uint16_t*>(input.data() + input.length()));
    }
#else
    static std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t> conv16;

    inline std::u16string utf16_from_bytes(const std::string& input) {
        return conv16.from_bytes(input);
    }
    inline std::string utf16_to_bytes(const std::u16string& input) {
        return conv16.to_bytes(input);
    }
#endif
}


namespace idna_lib {

    bool toASCII(std::string& output, const std::string& input, bool transitional) {
        // to utf-16
        std::u16string domain(utf16_from_bytes(input));

        bool res = upa::idna::ToASCII(domain, domain.data(), domain.data() + domain.length(),
            upa::idna::Option::VerifyDnsLength |
            upa::idna::Option::CheckHyphens |
            upa::idna::Option::CheckBidi |
            upa::idna::Option::CheckJoiners |
            upa::idna::Option::UseSTD3ASCIIRules |
            (transitional ? upa::idna::Option::Transitional : upa::idna::Option::Default)
            );

        // to utf-8
        output = res ? utf16_to_bytes(domain) : "";

        return res;
    }

    bool toUnicode(std::string& output, const std::string& input) {
        // to utf-16
        std::u16string domain(utf16_from_bytes(input));

        bool res = upa::idna::ToUnicode(domain, domain.data(), domain.data() + domain.length(),
            // upa::idna::Option::VerifyDnsLength |
            upa::idna::Option::CheckHyphens |
            upa::idna::Option::CheckBidi |
            upa::idna::Option::CheckJoiners |
            upa::idna::Option::UseSTD3ASCIIRules
            );

#if 1
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
        output = utf16_to_bytes(domain);

        return res;
    }

} // namespace idna_lib