#ifndef UPA_IDNA_PUNYCODE_H
#define UPA_IDNA_PUNYCODE_H

#include <string>

namespace upa {
namespace idna {
namespace punycode {

enum class status {
    success = 0,
    bad_input = 1,  // Input is invalid.
    big_output = 2, // Output would exceed the space provided.
    overflow = 3    // Wider integers needed to process input.
};

status encode(std::u16string& output, const char16_t* first, const char16_t* last);
status decode(std::u16string& output, const char16_t* first, const char16_t* last);

} // namespace punycode
} // namespace idna
} // namespace upa

#endif // UPA_IDNA_PUNYCODE_H
