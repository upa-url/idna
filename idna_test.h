#ifndef IDNA_TEST_H
#define IDNA_TEST_H

#include <string>

namespace idna_test {
    bool toASCII(std::string& output, const std::string& input, bool transitional);
    bool toUnicode(std::string& output, const std::string& input);
} // namespace idna_test

#endif // IDNA_TEST_H
