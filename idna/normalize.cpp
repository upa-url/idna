// 1 - ICU; 2 - Win32
#define THE_NFC_LIB 1

#include "normalize.h"

#if THE_NFC_LIB == 1
///////////////////////////////////////////////////////////////
// ICU NFC
#include "unicode/normalizer2.h"


bool normalize_nfc(std::u16string& str) {
    if (str.empty()) return true;

    // Get instance for Unicode NFC normalization
    UErrorCode icu_error = U_ZERO_ERROR;
    const icu::Normalizer2 *normalizer = icu::Normalizer2::getNFCInstance(icu_error);

    // Normalize
    icu::UnicodeString source((const UChar*)str.data(), str.length());
    icu::UnicodeString dest(normalizer->normalize(source, icu_error));
    str.assign((const char16_t*)dest.getBuffer(), dest.length());

    return U_SUCCESS(icu_error);
}

bool is_normalized_nfc(const char16_t* first, const char16_t* last) {
    // Get instance for Unicode NFC normalization
    UErrorCode icu_error = U_ZERO_ERROR;
    const icu::Normalizer2 *normalizer = icu::Normalizer2::getNFCInstance(icu_error);

    icu::UnicodeString source((const UChar*)first, last - first);
    return !!normalizer->isNormalized(source, icu_error);
}

#elif THE_NFC_LIB == 2
///////////////////////////////////////////////////////////////
// Windows NFC
#include <windows.h>

bool normalize_nfc(std::u16string& str) {
    if (str.empty()) return true;

    // https://msdn.microsoft.com/en-us/library/windows/desktop/dd319093(v=vs.85).aspx
    int dest_size = str.length();

    for (int i = 0; i < 10; i++) {
        char16_t* dest = new char16_t[dest_size];
        int resSize = NormalizeString(NormalizationC,
            reinterpret_cast<const wchar_t*>(str.data()), str.length(),
            reinterpret_cast<wchar_t*>(dest), dest_size);

        if (resSize > 0) {
            // success
            str.assign(dest, static_cast<size_t>(resSize));
            delete[] dest;
            return true;
        }

        delete[] dest;

        if (GetLastError() != ERROR_INSUFFICIENT_BUFFER)
            return false; // Real error, not buffer error

        // New guess is negative of the return value.
        dest_size = -resSize;
    }
    return false;
}

bool is_normalized_nfc(const char16_t* first, const char16_t* last) {
    return !! IsNormalizedString(NormalizationC, reinterpret_cast<const wchar_t*>(first), last - first);
}

#endif
