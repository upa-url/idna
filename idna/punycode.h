#ifndef PUNYCODE__H
#define PUNYCODE__H

#include <string>

namespace punycode {

enum class status {
    success = 0,
    bad_input = 1,  // Input is invalid.
    big_output = 2, // Output would exceed the space provided.
    overflow = 3    // Wider integers needed to process input.
};

status encode(std::u16string& output, const char16_t* first, const char16_t* last);
bool encode_OLD(std::u16string& output, const char16_t* first, const char16_t* last);
bool decode(std::u16string& output, const char16_t* first, const char16_t* last);

}

#endif // PUNYCODE__H
