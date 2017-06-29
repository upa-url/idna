#include "idna_test.h"

// conversion
#include <codecvt>
//#include <locale>

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

        // to utf-8
        output = res ? conv16.to_bytes(domain) : "";

        return res;
    }

} // namespace idna_test
