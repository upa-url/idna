// Copyright 2024-2025 Rimas Misevičius
// Distributed under the BSD-style license that can be
// found in the LICENSE file.
//
#ifndef UPA_IDNA_NFC_H
#define UPA_IDNA_NFC_H

#include "config.h" // IWYU pragma: export
#include <string>

namespace upa::idna {


UPA_IDNA_API void compose(std::u32string& str);
UPA_IDNA_API void canonical_decompose(std::u32string& str);

UPA_IDNA_API void normalize_nfc(std::u32string& str);
[[nodiscard]] UPA_IDNA_API bool is_normalized_nfc(const char32_t* first, const char32_t* last);


} // namespace upa::idna

#endif // UPA_IDNA_NFC_H
