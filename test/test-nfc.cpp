// Copyright 2024 Rimas Misevičius
// Distributed under the BSD-style license that can be
// found in the LICENSE file.
//
#include "upa/idna/nfc.h"

#include "../unitool/unicode_data_tools.h"
#include <limits>

template <class CharT, class Traits>
inline std::basic_ostream<CharT, Traits>& operator<<(std::basic_ostream<CharT, Traits>& os, const std::u32string& str);

#include "ddt/DataDrivenTest.hpp"

using namespace upa::tools;

static int run_nfc_tests(const char* file_name);

int main()
{
    return run_nfc_tests("data/NormalizationTest.txt");
}

static std::u32string get_col_str32(const std::string& line, std::size_t& pos)
{
    const auto col = get_column(line, pos);
    std::u32string str32;
    split(col.data(), col.data() + col.length(), ' ',
        [&str32](const char* it0, const char* it1) {
            const int cp = hexstr_to_int(it0, it1);
            str32.push_back(static_cast<char32_t>(cp));
        });
    return str32;
}

inline std::u32string toNFC(std::u32string str)
{
    upa::idna::normalize_nfc(str);
    return str;
}

inline std::u32string toNFD(std::u32string str)
{
    upa::idna::canonical_decompose(str);
    return str;
}

static int run_nfc_tests(const char* file_name)
{
    DataDrivenTest ddt;
    ddt.config_show_passed(false);
    ddt.config_debug_break(false);

    std::cout << "========== " << file_name << " ==========\n";
    std::ifstream file(file_name, std::ios_base::in);
    if (!file.is_open()) {
        std::cerr << "Can't open tests file: " << file_name << std::endl;
        return 1;
    }

    int line_num = 0;
    std::string line;
    std::string case_name;
    while (std::getline(file, line)) {
        line_num++;
        // Comments are indicated with hash marks
        auto i_comment = line.find('#');
        if (i_comment != line.npos)
            line.resize(i_comment);
        // got line without comment
        if (line.length() > 0 && line[0] != '@') {
            try {
                std::size_t pos = 0;
                const auto c1 = get_col_str32(line, pos);
                const auto c2 = get_col_str32(line, pos);
                const auto c3 = get_col_str32(line, pos);
                const auto c4 = get_col_str32(line, pos);
                const auto c5 = get_col_str32(line, pos);

                // test
                case_name.assign("(").append(std::to_string(line_num)).append(") ").append(line);
                ddt.test_case(case_name.c_str(), [&](DataDrivenTest::TestCase& tc) {
                    // NFC
                    tc.assert_equal(c2, toNFC(c1), "c2 == toNFC(c1)");
                    tc.assert_equal(c2, toNFC(c2), "c2 == toNFC(c2)");
                    tc.assert_equal(c2, toNFC(c3), "c2 == toNFC(c3)");
                    tc.assert_equal(c4, toNFC(c4), "c4 == toNFC(c4)");
                    tc.assert_equal(c4, toNFC(c5), "c4 == toNFC(c5)");
                    // NFD
                    tc.assert_equal(c3, toNFD(c1), "c3 == toNFD(c1)");
                    tc.assert_equal(c3, toNFD(c2), "c3 == toNFD(c2)");
                    tc.assert_equal(c3, toNFD(c3), "c3 == toNFD(c3)");
                    tc.assert_equal(c5, toNFD(c4), "c5 == toNFD(c4)");
                    tc.assert_equal(c5, toNFD(c5), "c5 == toNFD(c5)");
                });
            }
            catch (std::exception& ex) {
                std::cerr << "ERROR: " << ex.what() << std::endl;
                std::cerr << " LINE(" << line_num << "): " << line << std::endl;
            }
        }
    }
    return ddt.result();
}

// stream operator
template <class CharT, class Traits, class StrT>
inline std::basic_ostream<CharT, Traits>& output_str(std::basic_ostream<CharT, Traits>& os, const StrT& str, const int width) {
    constexpr uint32_t maxCh = std::numeric_limits<CharT>::max();
    for (auto ch : str) {
        if (ch <= maxCh) {
            os << static_cast<CharT>(ch);
        } else {
            os << "\\x";
            auto flags_save = os.flags();
            auto fill_save = os.fill('0');
            os << std::hex << std::uppercase << std::setw(width) << ch;
            os.fill(fill_save);
            os.flags(flags_save);
        }
    }
    return os;
}

template <class CharT, class Traits>
inline std::basic_ostream<CharT, Traits>& operator<<(std::basic_ostream<CharT, Traits>& os, const std::u32string& str) {
    return output_str(os, str, 8);
}
