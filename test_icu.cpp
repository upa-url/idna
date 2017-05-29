#include "idna_test.h"

namespace idna_test {

    bool toASCII(std::string& output, const std::string& input, bool transitional) {
        output = input;
        return true;
    }

    bool toUnicode(std::string& output, const std::string& input) {
        output = input;
        return true;
    }

} // namespace idna_test
