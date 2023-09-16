#ifndef UPA_IDNA_NORMALIZE_H
#define UPA_IDNA_NORMALIZE_H

#include <string>

namespace upa {
namespace idna {

bool normalize_nfc(std::u16string& str);
bool is_normalized_nfc(const char16_t* first, const char16_t* last);

} // namespace idna
} // namespace upa

#endif // UPA_IDNA_NORMALIZE_H
