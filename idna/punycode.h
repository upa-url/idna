#ifndef PUNYCODE__H
#define PUNYCODE__H

#include <string>

namespace punycode {

bool decode(std::u16string& output, const char16_t* first, const char16_t* last);
bool encode(std::u16string& output, const char16_t* first, const char16_t* last);

}

#endif // PUNYCODE__H
