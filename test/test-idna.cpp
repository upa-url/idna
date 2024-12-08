// Copyright 2017-2024 Rimas Misevičius
// Distributed under the BSD-style license that can be
// found in the LICENSE file.
//
#include "idna_lib.h"
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <string>

// clang requires to decalare 'operator <<' prior to the call site (DataDrivenTest::assert_equal)
template <class CharT, class Traits>
inline std::basic_ostream<CharT, Traits>& operator<<(std::basic_ostream<CharT, Traits>& os, const std::u16string& str);

template <class CharT, class Traits>
inline std::basic_ostream<CharT, Traits>& operator<<(std::basic_ostream<CharT, Traits>& os, const std::u32string& str);

#include "ddt/DataDrivenTest.hpp"


int run_idna_tests_v2(const std::filesystem::path& file_name);
int run_punycode_tests(const std::filesystem::path& file_name);
static std::string get_column8(const std::string& line, std::size_t& pos);
//static std::u16string get_column16(const std::string& line, std::size_t& pos);
static std::u32string get_column32(const std::string& line, std::size_t& pos);
static bool is_error(const std::string& col);
static bool is_error_of_to_unicode(const std::string& col);

int main()
{
    int err = 0;

    err |= run_idna_tests_v2("data/IdnaTestV2.txt");
    err |= run_idna_tests_v2("data/IdnaTestV2-mano.txt");

    err |= run_punycode_tests("data/punycode-test.txt");
    err |= run_punycode_tests("data/punycode-test-mano.txt");

    return err;
}

int run_idna_tests_v2(const std::filesystem::path& file_name)
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
                const std::string c1 = get_column8(line, pos);
                const std::string c2 = get_column8(line, pos);
                const std::string c3 = get_column8(line, pos);
                const std::string c4 = get_column8(line, pos);
                const std::string c5 = get_column8(line, pos);
                const std::string c6 = get_column8(line, pos);
                const std::string c7 = get_column8(line, pos);

                // source
                const std::string& source(c1);
                const bool is_input_ascii = std::all_of(source.begin(), source.end(),
                    [](char c) { return static_cast<unsigned char>(c) < 0x80; });

                // to_unicode
                const std::string& exp_unicode(c2.empty() ? source : c2);
                const bool exp_unicode_ok = !is_error_of_to_unicode(c3);

                // to_ascii
                const std::string& exp_ascii(c4.empty() ? exp_unicode : c4);
                const bool exp_ascii_ok = c5.empty() ? exp_unicode_ok : !is_error(c5);

                // to_ascii transitional
                const std::string& exp_ascii_trans(c6.empty() ? exp_ascii : c6);
                const bool exp_ascii_trans_ok = c7.empty() ? exp_ascii_ok : !is_error(c7);


                // test
                case_name.assign("(").append(std::to_string(line_num)).append(") ").append(line);
                ddt.test_case(case_name.c_str(), [&](DataDrivenTest::TestCase& tc) {
                    bool ok;

                    // to_unicode
                    ok = idna_lib::toUnicode(output, source);
                    tc.assert_equal(exp_unicode_ok, ok, "to_unicode success");
                    tc.assert_equal(exp_unicode, output, "to_unicode output");
                    if (is_input_ascii) {
                        ok = idna_lib::toUnicode(output, source, is_input_ascii);
                        tc.assert_equal(exp_unicode_ok, ok, "ASCII to_unicode success");
                        tc.assert_equal(exp_unicode, output, "ASCII to_unicode output");
                    }

                    // to_ascii
                    ok = idna_lib::toASCII(output, source, false);
                    tc.assert_equal(exp_ascii_ok, ok, "to_ascii success");
                    if (exp_ascii_ok && ok)
                        tc.assert_equal(exp_ascii, output, "to_ascii output");
                    if (is_input_ascii) {
                        ok = idna_lib::toASCII(output, source, false, is_input_ascii);
                        tc.assert_equal(exp_ascii_ok, ok, "ASCII to_ascii success");
                        if (exp_ascii_ok && ok)
                            tc.assert_equal(exp_ascii, output, "ASCII to_ascii output");
                    }

                    // to_ascii transitional
                    ok = idna_lib::toASCII(output, source, true);
                    tc.assert_equal(exp_ascii_trans_ok, ok, "to_ascii transitional success");
                    if (exp_ascii_trans_ok && ok)
                        tc.assert_equal(exp_ascii_trans, output, "to_ascii transitional output");
                    if (is_input_ascii) {
                        ok = idna_lib::toASCII(output, source, true, is_input_ascii);
                        tc.assert_equal(exp_ascii_trans_ok, ok, "ASCII to_ascii transitional success");
                        if (exp_ascii_trans_ok && ok)
                            tc.assert_equal(exp_ascii_trans, output, "ASCII to_ascii transitional output");
                    }
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

// utf-8
inline void code_point_to_utf(int code_point, std::string& output) {
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

// utf-16
inline void code_point_to_utf(int cp, std::u16string& output) {
    if (cp <= 0xFFFF) {
        output.push_back(static_cast<char16_t>(cp));
    } else if (cp <= 0x10FFFF) {
        // http://unicode.org/faq/utf_bom.html#utf16-4
        // https://en.wikipedia.org/wiki/UTF-16#Description
        const uint32_t cc = cp - 0x10000;
        char16_t cu1 = static_cast<char16_t>(0xD800 | (cc >> 10)); // high surrogate
        char16_t cu2 = static_cast<char16_t>(0xDC00 | (cc & 0x03FF)); // low surrogate
        output.push_back(cu1);
        output.push_back(cu2);
    }
}

// utf-32
inline void code_point_to_utf(int cp, std::u32string& output) {
    output.push_back(static_cast<char32_t>(cp));
}

template <class CharT>
inline std::basic_string<CharT> get_column(const std::string& line, std::size_t& pos) {
    // Columns (c1, c2,...) are separated by semicolons
    std::size_t pos_end = line.find(';', pos);
    if (pos_end == line.npos) pos_end = line.length();

    // Leading and trailing spaces and tabs in each column are ignored
    const char* first = line.data() + pos;
    const char* last = line.data() + pos_end;
    AsciiTrimSpaceTabs(first, last);

    // unescape \uXXXX or \x{XXXX}
    const char* p = std::find(first, last, '\\');
    std::basic_string<CharT> output(first, p);
    while (p != last) {
        const char ch = *p;
        if (ch == '\\') {
            int code_point = unescape_code_point(p, last);
            code_point_to_utf(code_point, output);
        } else {
            output.push_back(ch);
            p++;
        }
    }

    // skip ';'
    pos = pos_end < line.length() ? pos_end + 1 : pos_end;
    return output;
}

static std::string get_column8(const std::string& line, std::size_t& pos) {
    return get_column<char>(line, pos);
}

//static std::u16string get_column16(const std::string& line, std::size_t& pos) {
//    return get_column<char16_t>(line, pos);
//}

static std::u32string get_column32(const std::string& line, std::size_t& pos) {
    return get_column<char32_t>(line, pos);
}

inline bool is_error(const std::string& col) {
    // error, if "[<non-empty>]"
    return col.length() >= 3 && col[0] == '[' && col[col.length() - 1] == ']';
}

inline bool is_error_of_to_unicode(const std::string& col) {
    // Ignore compatibility errors
    // For more information, see IdnaTestV2.txt
    if (is_error(col))
        return col != "[X4_2]";
    return false;
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
            os << std::hex << std::uppercase << std::setw(width) << static_cast<std::uint32_t>(ch);
            os.fill(fill_save);
            os.flags(flags_save);
        }
    }
    return os;
}

template <class CharT, class Traits>
inline std::basic_ostream<CharT, Traits>& operator<<(std::basic_ostream<CharT, Traits>& os, const std::u16string& str) {
    return output_str(os, str, 4);
}

template <class CharT, class Traits>
inline std::basic_ostream<CharT, Traits>& operator<<(std::basic_ostream<CharT, Traits>& os, const std::u32string& str) {
    return output_str(os, str, 8);
}

//
// Punycode tests
//

#include "upa/idna/punycode.h"

int run_punycode_tests(const std::filesystem::path& file_name)
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
        if (i_comment != line.npos) {
            if (i_comment == 0) {
                case_name = line;
                continue;
            }
            line.resize(i_comment);
        }
        // got line without comment
        if (line.length() > 0) {
            try {
                std::size_t pos = 0;
                const auto inp_source = get_column32(line, pos);
                const auto inp_encoded8 = get_column8(line, pos);
                const auto inp_encoded = std::u32string{ inp_encoded8.begin(), inp_encoded8.end() };

                // test
                if (case_name.empty()) case_name = line;
                ddt.test_case(case_name.c_str(), [&](DataDrivenTest::TestCase& tc) {
                    bool ok{};

                    // encode to punycode
                    std::string out_encoded; // punycode::encode(..) appends
                    ok = upa::idna::punycode::encode(out_encoded, inp_source.data(), inp_source.data() + inp_source.length()) == upa::idna::punycode::status::success;
                    tc.assert_equal(true, ok, "punycode::encode success");
                    tc.assert_equal(inp_encoded8, out_encoded, "punycode::encode");

                    // decode from punycode
                    std::u32string out_decoded; // punycode::decode(..) appends
                    ok = upa::idna::punycode::decode(out_decoded, inp_encoded.data(), inp_encoded.data() + inp_encoded.length()) == upa::idna::punycode::status::success;
                    tc.assert_equal(true, ok, "punycode::decode success");
                    tc.assert_equal(inp_source, out_decoded, "punycode::decode");
                });
            }
            catch (std::exception& ex) {
                std::cerr << "ERROR: " << ex.what() << std::endl;
                std::cerr << " LINE(" << line_num << "): " << line << std::endl;
            }
        }
        case_name.clear();
    }
    return ddt.result();
}
