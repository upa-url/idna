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

status encode(std::string& output, const char32_t* first, const char32_t* last);
status decode(std::u32string& output, const char32_t* first, const char32_t* last);

} // namespace punycode
} // namespace idna
} // namespace upa

#endif // UPA_IDNA_PUNYCODE_H
