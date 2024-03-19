#ifndef UPA_IDNA_NFC_H
#define UPA_IDNA_NFC_H

#include <string>

namespace upa {
namespace idna {


void compose(std::u32string& str);
void canonical_decompose(std::u32string& str);

void normalize_nfc(std::u32string& str);


} // namespace idna
} // namespace upa

#endif // UPA_IDNA_NFC_H
