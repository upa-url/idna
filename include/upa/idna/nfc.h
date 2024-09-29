// Copyright 2024 Rimas Misevičius
// Distributed under the BSD-style license that can be
// found in the LICENSE file.
//
#ifndef UPA_IDNA_NFC_H
#define UPA_IDNA_NFC_H

#include <string>

namespace upa::idna {


void compose(std::u32string& str);
void canonical_decompose(std::u32string& str);

void normalize_nfc(std::u32string& str);
bool is_normalized_nfc(const char32_t* first, const char32_t* last);


} // namespace upa::idna

#endif // UPA_IDNA_NFC_H
