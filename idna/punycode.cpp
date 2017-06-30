#include "punycode.h"
#include "iterate_utf16.h"
#include <vector>
extern "C" {
#include "../punycode_rfc/punycode_rfc.h"
}

static void appendCodePoint(std::u16string& output, uint32_t cp) {
    if (cp <= 0xFFFF) {
        output.push_back(static_cast<char16_t>(cp));
    } else if (cp < 0x10FFFF) {
        // http://unicode.org/faq/utf_bom.html#utf16-4
        // https://en.wikipedia.org/wiki/UTF-16#Description
        const uint32_t cc = cp - 0x10000;
        char16_t cu1 = static_cast<char16_t>(0xD800 | (cc >> 10)); // high surrogate
        char16_t cu2 = static_cast<char16_t>(0xDC00 | (cc & 0x03FF)); // low surrogate
        output.push_back(cu1);
        output.push_back(cu2);
    }
}

namespace punycode {

bool decode(std::u16string& output, const char16_t* first, const char16_t* last) {
    std::vector<char> inp;
    inp.reserve(last - first);
    for (auto it = first; it != last; it++) {
        const char16_t ch = *it;
        if (ch >= 0x80) return false;
        inp.push_back(static_cast<char>(ch));
    }

    size_t out_length = inp.size() * 2;
    std::vector<punycode_uint> out(out_length);

    const punycode_status stat = punycode_decode(inp.size(), inp.data(), &out_length, out.data(), NULL);
    if (stat == punycode_success) {
        output.clear();
        output.reserve(out_length); // todo: if the out has cp > 0xFFFF
        for (size_t ind = 0; ind < out_length; ind++)
            appendCodePoint(output, out[ind]);
        return true;
    }
    return false;
}

bool encode(std::u16string& output, const char16_t* first, const char16_t* last) {
    std::vector<punycode_uint> inp;
    inp.reserve(last - first);
    for (auto it = first; it != last;) {
        const uint32_t cp = getCodePoint(it, last);
        inp.push_back(static_cast<punycode_uint>(cp));
    }

    size_t out_length = inp.size() * 8; // TODO
    std::vector<char> out(out_length);

    const punycode_status stat = punycode_encode(inp.size(), inp.data(), NULL, &out_length, out.data());
    if (stat == punycode_success) {
        output.clear();
        output.reserve(out_length);
        for (size_t ind = 0; ind < out_length; ind++)
            output.push_back(out[ind]);
        return true;
    }
    return false;
}

} // namespace punycode
