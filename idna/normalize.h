#ifndef NORMALIZE_H
#define NORMALIZE_H

#include <string>

bool normalize_nfc(std::u16string& str);
bool is_normalized_nfc(const char16_t* first, const char16_t* last);

#endif // NORMALIZE_H
