#include "idna_test.h"
#include <mutex>

// ICU
#include "unicode/uidna.h"


namespace {
    const UIDNA* getUIDNA(bool trans) {
        static UIDNA* uidna[2];
        static std::once_flag once;

        std::call_once(once, [] {
            UErrorCode err = U_ZERO_ERROR;
            // VerifyDnsLength: true
            // CheckHyphens: true
            // CheckBidi: true
            // CheckJoiners: true
            // UseSTD3ASCIIRules : true
            uidna[0] = uidna_openUTS46(
                UIDNA_USE_STD3_RULES
                | UIDNA_CHECK_BIDI
                | UIDNA_CHECK_CONTEXTJ
                | UIDNA_NONTRANSITIONAL_TO_ASCII
                | UIDNA_NONTRANSITIONAL_TO_UNICODE
                , &err);
            if (U_FAILURE(err)) {
                //todo: CHECK(false) << "failed to open UTS46 data with error: " << err;
                uidna[0] = nullptr;
            }
            uidna[1] = uidna_openUTS46(
                UIDNA_USE_STD3_RULES
                | UIDNA_CHECK_BIDI
                | UIDNA_CHECK_CONTEXTJ
                , &err);
            if (U_FAILURE(err)) {
                //todo: CHECK(false) << "failed to open UTS46 data with error: " << err;
                uidna[1] = nullptr;
            }
        });
        return uidna[trans ? 1 : 0];
    }
}



namespace idna_test {

    bool toASCII(std::string& output, const std::string& input, bool transitional) {
        const UIDNA* uidna = getUIDNA(transitional);

        char buff[1024 * 2];
        while (true) {
            UErrorCode err = U_ZERO_ERROR;
            UIDNAInfo info = UIDNA_INFO_INITIALIZER;
            int output_length = uidna_nameToASCII_UTF8(uidna,
                input.data(), static_cast<int32_t>(input.length()),
                buff, sizeof(buff),
                &info, &err);
            if (U_SUCCESS(err) && info.errors == 0) {
                output.assign(buff, output_length);
                return true;
            }

            if (err != U_BUFFER_OVERFLOW_ERROR || info.errors)
                return false;  // IDNA error, give up.

            // Not enough room in our buffer, expand.
            //output.reserve(output_length);
            return false;
        }
    }

    bool toUnicode(std::string& output, const std::string& input) {
        const UIDNA* uidna = getUIDNA(false);

        char buff[1024 * 2];
        while (true) {
            UErrorCode err = U_ZERO_ERROR;
            UIDNAInfo info = UIDNA_INFO_INITIALIZER;
            int output_length = uidna_nameToUnicodeUTF8(uidna,
                input.data(), static_cast<int32_t>(input.length()),
                buff, sizeof(buff),
                &info, &err);
#if 0
            const uint32_t idna_err = info.errors & ~static_cast<uint32_t>(UIDNA_ERROR_EMPTY_LABEL);
#else
            const uint32_t idna_err = info.errors;
#endif
            if (U_SUCCESS(err) && idna_err == 0) {
                output.assign(buff, output_length);
                return true;
            }

            if (err != U_BUFFER_OVERFLOW_ERROR || info.errors)
                return false;  // IDNA error, give up.

            // Not enough room in our buffer, expand.
            //output.reserve(output_length);
            return false;
        }
    }

} // namespace idna_test
