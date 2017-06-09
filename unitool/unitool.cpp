// unitool.cpp : Defines the entry point for the console application.
//
#include <algorithm>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>


enum class DataType {
    NONE,
    // IdnaMappingTable.txt
    IdnaMappingTable,
    // DerivedGeneralCategory.txt
    Category_M,
    // DerivedBidiClass.txt
    Bidi_Class,
    // DerivedCombiningClass.txt
    Combining_Class_Virama,
    // DerivedJoiningType.txt
    Joining_Type,
};

static void parse_UnicodeData(const char* file_name, const char* fout_name, DataType dtype);
static std::string get_column(const std::string& line, std::size_t& pos);

int main(int argc, char* argv[])
{
    DataType dtype = DataType::NONE;
    if (argc > 3 && argv[3][0] == '-') {
        switch (argv[3][1]) {
        case 'I':
            dtype = DataType::IdnaMappingTable;
            break;
        case 'M':
            dtype = DataType::Category_M;
            break;
        case 'B':
            dtype = DataType::Bidi_Class;
            break;
        case 'V':
            dtype = DataType::Combining_Class_Virama;
            break;
        case 'J':
            dtype = DataType::Joining_Type;
            break;
        }
    }
    if (dtype != DataType::NONE) {
        parse_UnicodeData(argv[1], argv[2], dtype);
    } else {
        std::cerr << "unitool Data.txt Output.json -I|-M|-B|-V|-J" << std::endl;
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
        // constructors
        RangeItem() : cp0(0), cp1(0) {}
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
        // assignment
        RangeItem& operator=(RangeItem&& src) {
            cp0 = src.cp0;
            cp1 = src.cp1;
            value = std::move(src.value);
            return *this;
        }
    };

    // CodePointRanges();
    void add(int cp0, int cp1, std::string value);
    void add(std::string cpstr, std::string value);
    void add(std::string cpstr) { add(cpstr, std::string{}); }

    void sort();
    void check();
    void merge();
    void output_js(OutputFmt& outfmt, bool in_ranges = true);
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

template <typename UIntT>
inline void unsigned_to_str(UIntT num, std::string& output, UIntT base) {
    static const char digit[] = "0123456789ABCDEF";

    // divider
    UIntT divider = 1;
    const UIntT num0 = num / base;
    while (divider <= num0) divider *= base;

    // convert
    do {
        output.push_back(digit[num / divider % base]);
        divider /= base;
    } while (divider);
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

void CodePointRanges::sort() {
    std::sort(m_ranges.begin(), m_ranges.end(), [](const RangeItem &a, const RangeItem &b){
        return a.cp0 < b.cp0;
    });
}

void CodePointRanges::check() {
    const RangeItem* rPrev = nullptr;
    for (const RangeItem& r : m_ranges) {
        if (r.cp0 > r.cp1) {
            std::cerr << "Invalid: " << r.cp0 << " > " << r.cp1 << ": " << r.value << std::endl;
        }
        if (rPrev) {
            const bool same_start = rPrev->cp0 == r.cp0;
            const bool unordered = rPrev->cp0 > r.cp0;
            const bool overlaps = rPrev->cp1 >= r.cp0;
            if (same_start || unordered || overlaps) {
                if (same_start) std::cerr << "[Same start]";
                if (unordered) std::cerr << "[Unordered]";
                if (overlaps) std::cerr << "[Overlaps]";
                std::cerr << ":\n";
                std::cerr << " 1) " << rPrev->cp0 << ".." << rPrev->cp1 << ": " << rPrev->value << std::endl;
                std::cerr << " 2) " << r.cp0 << ".." << r.cp1 << ": " << r.value << std::endl;
            }
        }
        rPrev = &r;
    }
}

void CodePointRanges::merge() {
    if (m_ranges.size() < 2) return;

    size_t ilast = 0;
    for (size_t ind = 1; ind < m_ranges.size(); ind++) {
        RangeItem& rlast = m_ranges[ilast];
        RangeItem& r = m_ranges[ind];
        if (rlast.cp1 + 1 == r.cp0 && rlast.value == r.value) {
            rlast.cp1 = r.cp1;
        } else if (++ilast < ind) {
            m_ranges[ilast] = std::move(r);
        }
    }
    m_ranges.resize(ilast + 1);
}

void CodePointRanges::output_js(OutputFmt& outfmt, bool in_ranges) {
    std::string item;
    if (in_ranges) {
        for (const RangeItem& r : m_ranges) {
            item.assign("[");
            unsigned_to_str(r.cp0, item, 10);
            item.append(", ");
            unsigned_to_str(r.cp1, item, 10);
            if (!r.value.empty())
                item.append(", ").append(r.value);
            item.append("]");
            // output
            outfmt.output(item);
        }
    } else {
        for (const RangeItem& r : m_ranges) {
            for (auto cp = r.cp0; cp <= r.cp1; cp++) {
                item.resize(0);
                unsigned_to_str(cp, item, 10);
                // output
                outfmt.output(item);
            }
        }
    }
}

// Split

template<class InputIt, class T, class FunT>
inline void split(InputIt first, InputIt last, const T& delim, FunT output) {
    auto start = first;
    for (auto it = first; it != last;) {
        if (*it == delim) {
            output(start, it);
            start = ++it;
        } else {
            it++;
        }
    }
    output(start, last);
}

// Utility

inline static void AsciiTrimSpaceTabs(const char*& first, const char*& last) {
    auto ascii_ws = [](char c) { return c == ' ' || c == '\t'; };
    // trim space
    while (first < last && ascii_ws(*first)) first++;
    while (first < last && ascii_ws(*(last - 1))) last--;
}

std::string get_column(const std::string& line, std::size_t& pos) {
    // Columns (c1, c2,...) are separated by semicolons
    std::size_t pos_end = line.find(';', pos);
    if (pos_end == line.npos) pos_end = line.length();

    // Leading and trailing spaces and tabs in each column are ignored
    const char* first = line.data() + pos;
    const char* last = line.data() + pos_end;
    AsciiTrimSpaceTabs(first, last);

    // skip ';'
    pos = pos_end < line.length() ? pos_end + 1 : pos_end;
    return std::string(first, last);
}


// Parse input file

void parse_UnicodeData(const char* file_name, const char* fout_name, DataType dtype)
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
    CodePointRanges ranges;
    bool in_ranges = (dtype != DataType::Combining_Class_Virama);

    int line_num = 0;
    std::string line;
    std::string value;
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
                const std::string c0 = get_column(line, pos);
                const std::string c1 = get_column(line, pos);
                const std::string c2 = get_column(line, pos);

                switch (dtype) {
                case DataType::IdnaMappingTable:
                    value.assign("\"").append(c1).append("\"");
                    if (!c2.empty()) {
                        value.append(",[");
                        // parse c2
                        bool first = true;
                        split(c2.data(), c2.data() + c2.length(), ' ', [&first, &value](const char* it0, const char* it1) {
                            if (first) first = false; else value += ',';
                            unsigned_to_str<int>(hexstr_to_int(it0, it1), value, 10);
                        });
                        value.append("]");
                    }
                    ranges.add(c0, value);
                    break;
                case DataType::Category_M:
                    if (c1.length() > 0 && c1[0] == 'M')
                        ranges.add(c0);
                    break;
                case DataType::Bidi_Class:
                    if (c1 != "L")
                        ranges.add(c0, value.assign("\"").append(c1).append("\""));
                    break;
                case DataType::Combining_Class_Virama:
                    if (c1 == "9")
                        ranges.add(c0);
                    break;
                case DataType::Joining_Type:
                    if (c1 == "D" || c1 == "L" || c1 == "R" || c1 == "T")
                        ranges.add(c0, value.assign("\"").append(c1).append("\""));
                    break;
                }
            }
            catch (std::exception& ex) {
                std::cerr << "ERROR: " << ex.what() << std::endl;
                std::cerr << " LINE(" << line_num << "): " << line << std::endl;
            }
        }
    }

    // output
    ranges.sort();
    ranges.check();
    ranges.merge();
    ranges.output_js(outfmt, in_ranges);
}
