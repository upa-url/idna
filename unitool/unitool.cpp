// unitool.cpp : Defines the entry point for the console application.
//
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>


static void parse_UnicodeData(const char* file_name, const char* fout_name);
static std::string get_column(const std::string& line, std::size_t& pos);

int main(int argc, char* argv[])
{
    if (argc > 2) {
        parse_UnicodeData(argv[1], argv[2]);
    } else {
        std::cerr << "unitool UnicodeData.txt" << std::endl;
    }

    return 0;
}

// class OutputFmt

class OutputFmt {
public:
    OutputFmt(std::ostream& fout, size_t max_line_len);
    void output(const std::string& item);
    ~OutputFmt();
protected:
    std::ostream& m_fout;
    bool m_first;
    size_t m_line_len;
    // options
    const size_t m_max_line_len;
    static const size_t m_indent = 2;
};

OutputFmt::OutputFmt(std::ostream& fout, size_t max_line_len)
    : m_fout(fout)
    , m_first(true)
    , m_line_len(0)
    , m_max_line_len(max_line_len)
{
    m_fout << "[" << std::endl;
}

void OutputFmt::output(const std::string& item) {
    if (m_first) {
        m_first = false;
        m_fout << std::setw(m_indent) << "";
        m_line_len = m_indent;
    } else if (m_line_len + item.length() > (m_max_line_len - 3)) {
        // 3 == ", ,".length
        m_fout << ",\n";
        m_fout << std::setw(m_indent) << "";
        m_line_len = m_indent;
    } else {
        m_fout << ", ";
        m_line_len += 2;
    }
    m_fout << item;
    m_line_len += item.length();
}

OutputFmt::~OutputFmt() {
    m_fout << "]" << std::endl;
}

// class CodePointRanges

class CodePointRanges {
public:
    struct RangeItem {
        int cp0, cp1;
        std::string value;
        RangeItem(int acp0, int acp1, std::string avalue)
            : cp0(acp0)
            , cp1(acp1)
            , value(avalue)
        {}
        RangeItem(RangeItem&& src)
            : cp0(src.cp0)
            , cp1(src.cp1)
            , value(std::move(src.value))
        {}
    };

    // CodePointRanges();
    void add(int cp0, int cp1, std::string value);
    void add(std::string cpstr, std::string value);

protected:
    std::vector<RangeItem> m_ranges;
};

void CodePointRanges::add(int cp0, int cp1, std::string value) {
    if (m_ranges.empty()) {
        m_ranges.push_back(RangeItem{ cp0, cp1, value });
    } else {
        RangeItem& last = m_ranges[m_ranges.size() - 1];
        if (last.cp1 + 1 == cp0 && last.value == value) {
            last.cp1 = cp1;
        } else {
            m_ranges.push_back(RangeItem{ cp0, cp1, value });
        }
    }
}

inline static int hexstr_to_int(const char* first, const char* last) {
    int num = 0;
    for (auto it = first; it != last; it++) {
        char c = *it;
        int digit;

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

void CodePointRanges::add(std::string cpstr, std::string value) {
    size_t pos = cpstr.find("..");
    if (pos == cpstr.npos) {
        const int cp0 = hexstr_to_int(cpstr.data(), cpstr.data() + cpstr.length());
        add(cp0, cp0, value);
    } else {
        const int cp0 = hexstr_to_int(cpstr.data(), cpstr.data() + pos);
        const int cp1 = hexstr_to_int(cpstr.data() + pos + 2, cpstr.data() + cpstr.length());
        add(cp0, cp1, value);
    }
}

// Parse input file

void parse_UnicodeData(const char* file_name, const char* fout_name)
{
    std::cout << "FILE: " << file_name << std::endl;
    std::ifstream file(file_name, std::ios_base::in);
    if (!file.is_open()) {
        std::cerr << "Can't open data file: " << file_name << std::endl;
        return;
    }
    std::ofstream fout(fout_name, std::ios_base::out);
    if (!fout.is_open()) {
        std::cerr << "Can't open destination file: " << file_name << std::endl;
        return;
    }

    OutputFmt outfmt(fout, 100);

    int line_num = 0;
    std::string line;
    std::string item;
    while (std::getline(file, line)) {
        line_num++;
        //// Comments are indicated with hash marks
        //auto i_comment = line.find('#');
        //if (i_comment != line.npos)
        //  line.resize(i_comment);
        //// got line without comment
        if (line.length() > 0) {
            try {
                std::size_t pos = 0;
                const std::string c0 = get_column(line, pos);
                const std::string c1 = get_column(line, pos);
                const std::string c2 = get_column(line, pos);
                const std::string c3 = get_column(line, pos);
                const std::string c4 = get_column(line, pos);

                //// General_Category=M
                //if (c2.find('M') != c2.npos) {
                //  outfmt.output(item.assign("0x").append(c0));
                //}

                // Bidi_Class
                if (!c4.empty()) {
                    item = "0x";
                    item += c0;
                    item += ": \"";
                    item += c4;
                    item += "\"";
                    outfmt.output(item);
                }
            }
            catch (std::exception& ex) {
                std::cerr << "ERROR: " << ex.what() << std::endl;
                std::cerr << " LINE(" << line_num << "): " << line << std::endl;
            }
        }
    }
}

std::string get_column(const std::string& line, std::size_t& pos) {
    // Columns (c1, c2,...) are separated by semicolons
    std::size_t pos_end = line.find(';', pos);
    if (pos_end == line.npos) pos_end = line.length();

    std::string output(line.substr(pos, pos_end - pos));

    // skip ';'
    pos = pos_end < line.length() ? pos_end + 1 : pos_end;
    return output;
}

