#ifndef IDNA_LIB_H
#define IDNA_LIB_H

#include <string>

namespace idna_lib {
    bool toASCII(std::string& output, const std::string& input, bool transitional);
    bool toUnicode(std::string& output, const std::string& input);
} // namespace idna_lib

#endif // IDNA_LIB_H
