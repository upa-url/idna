// idna_test.cpp : Defines the entry point for the console application.
//

#include "idna_test.h"
#include "ddt/DataDrivenTest.hpp"
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <string>

void run_idna_tests(const char* file_name);
static std::string get_column(const std::string& line, std::size_t& pos);
static bool is_error(const std::string& col);

int main()
{
    run_idna_tests("test-data/IdnaTest.txt");
//  run_idna_tests("test-data/IdnaTest-9.0.0.txt");
//  run_idna_tests("test-data/IdnaTest-7.0.0.txt");

    return 0;
}

void run_idna_tests(const char* file_name)
{
    DataDrivenTest ddt;
    ddt.config_show_passed(false);
    ddt.config_debug_break(false);

    std::cout << "========== " << file_name << " ==========\n";
    std::ifstream file(file_name, std::ios_base::in);
    if (!file.is_open()) {
        std::cerr << "Can't open tests file: " << file_name << std::endl;
        return;
    }

    int line_num = 0;
    std::string line;
    std::string output;
    std::string case_name;
    while (std::getline(file, line)) {
        line_num++;
        // Comments are indicated with hash marks
        auto i_comment = line.find('#');
        if (i_comment != line.npos)
            line.resize(i_comment);
        // got line without comment
        if (line.length() > 0) {
            try {
                std::size_t pos = 0;
                const std::string c1 = get_column(line, pos);
                const std::string c2 = get_column(line, pos);
                const std::string c3 = get_column(line, pos);
                const std::string c4 = get_column(line, pos);
                const std::string c5 = get_column(line, pos);

                // source
                const std::string& source(c2);

                // ToUnicode
                const bool exp_unicode_ok = !is_error(c3);
                const std::string& exp_unicode(c3.empty() ? source : c3);

                // ToASCII
                const bool exp_ascii_ok = !is_error(c4);
                const std::string& exp_ascii(c4.empty() ? exp_unicode : c4);

                // test
                case_name.assign("(").append(std::to_string(line_num)).append(") ").append(line);
                ddt.test_case(case_name.c_str(), [&](DataDrivenTest::TestCase& tc) {
                    bool ok;

                    // ToUnicode
                    ok = idna_test::toUnicode(output, source);
                    tc.assert_equal(exp_unicode_ok, ok, "ToUnicode success");
                    if (exp_unicode_ok && ok) {
                        tc.assert_equal(exp_unicode, output, "ToUnicode output");
                    }

                    // ToASCII
                    if (c1 == "T" || c1 == "B") {
                        ok = idna_test::toASCII(output, source, true);
                        tc.assert_equal(exp_ascii_ok, ok, "ToASCII(trans.) success");
                        if (exp_ascii_ok && ok) {
                            tc.assert_equal(exp_ascii, output, "ToASCII(trans.) output");
                        }
                    }
                    if (c1 == "N" || c1 == "B") {
                        ok = idna_test::toASCII(output, source, false);
                        tc.assert_equal(exp_ascii_ok, ok, "ToASCII(non-trans.) success");
                        if (exp_ascii_ok && ok) {
                            tc.assert_equal(exp_ascii, output, "ToASCII(non-trans.) output");
                        }
                    }
                });
            }
            catch (std::exception& ex) {
                std::cerr << "ERROR: " << ex.what() << std::endl;
                std::cerr << " LINE(" << line_num << "): " << line << std::endl;
            }
        }
    }

}

inline static void AsciiTrimSpaceTabs(const char*& first, const char*& last) {
    auto ascii_ws = [](char c) { return c == ' ' || c == '\t'; };
    // trim space
    while (first < last && ascii_ws(*first)) first++;
    while (first < last && ascii_ws(*(last - 1))) last--;
}

inline static std::uint16_t hexstr_to_int(const char* first, const char* last) {
    std::uint16_t num = 0;
    for (auto it = first; it != last; it++) {
        char c = *it;
        std::uint16_t digit;

        if (c >= '0' && c <= '9')
            digit = c - '0';
        else if (c >= 'A' && c <= 'F')
            digit = c - 'A' + 10;
        else if (c >= 'a' && c <= 'f')
            digit = c - 'a' + 10;
        else
            throw std::runtime_error("invalid hex number");
        num = num * 0x10 + digit;
    }
    return num;
}

inline static int unescape_code_point(const char*& first, const char* last) {
    const char* p = first;
    if (last - p >= 6 && p[0] == '\\' && p[1] == 'u') {
        int code_point = hexstr_to_int(p + 2, p + 6);
        p += 6; first = p;

        // surrogate pair?
        if (code_point >= 0xd800 && code_point <= 0xdfff) {
            if (code_point >= 0xdc00) {
                return code_point; // ERR: non leading surrogate
            }
            if (!(last - p >= 6 && p[0] == '\\' && p[1] == 'u')) {
                return code_point; // ERR: trail surrogate missing
            }
            int second = hexstr_to_int(p + 2, p + 6);
            if (!(second >= 0xdc00 && second <= 0xdfff)) {
                return code_point; // ERR: non trail surrogate
            }
            p += 6; first = p;

            code_point = ((code_point - 0xd800) << 10) | ((second - 0xdc00) & 0x3ff);
            code_point += 0x10000;
        }
        return code_point;
    }
    throw std::runtime_error("invalid escape");
}

inline static void code_point_to_utf8(int code_point, std::string& output) {
    if (code_point < 0x80) {
        output.push_back(static_cast<char>(code_point));
    } else {
        if (code_point < 0x800) {
            output.push_back(static_cast<char>(0xc0 | (code_point >> 6)));
        } else {
            if (code_point < 0x10000) {
                output.push_back(static_cast<char>(0xe0 | (code_point >> 12)));
            } else {
                output.push_back(static_cast<char>(0xf0 | (code_point >> 18)));
                output.push_back(static_cast<char>(0x80 | ((code_point >> 12) & 0x3f)));
            }
            output.push_back(static_cast<char>(0x80 | ((code_point >> 6) & 0x3f)));
        }
        output.push_back(static_cast<char>(0x80 | (code_point & 0x3f)));
    }
}

std::string get_column(const std::string& line, std::size_t& pos) {
    // Columns (c1, c2,...) are separated by semicolons
    std::size_t pos_end = line.find(';', pos);
    if (pos_end == line.npos) pos_end = line.length();

    // Leading and trailing spaces and tabs in each column are ignored
    const char* first = line.data() + pos;
    const char* last = line.data() + pos_end;
    AsciiTrimSpaceTabs(first, last);

    // unescape \uXXXX or \x{XXXX}
    const char* p = std::find(first, last, '\\');
    std::string output(first, p);
    while (p != last) {
        const char ch = *p;
        if (ch == '\\') {
            int code_point = unescape_code_point(p, last);
            code_point_to_utf8(code_point, output);
        } else {
            output.push_back(ch);
            p++;
        }
    }

    // skip ';'
    pos = pos_end < line.length() ? pos_end + 1 : pos_end;
    return output;
}

inline bool is_error(const std::string& col) {
    return col.length() >= 2 && col[0] == '[' && col[col.length() - 1] == ']';
}
