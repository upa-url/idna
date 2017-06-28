// unitool.cpp : Defines the entry point for the console application.
//
#include <algorithm>
#include <array>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <set>
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

static void make_mapping_table(std::string data_path);
static void parse_UnicodeData(const char* file_name, const char* fout_name, DataType dtype);
static std::string get_column(const std::string& line, std::size_t& pos);

int main(int argc, char* argv[])
{
    if (argc == 3 && argv[2][0] == '-' && argv[2][1] == 'a') {
        make_mapping_table(argv[1]);
        return 0;
    }

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

// string <--> number

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

template <typename UIntT>
inline void unsigned_to_numstr(UIntT num, std::string& output, UIntT base) {
    if (num > 0) {
        switch (base) {
        case 8: output += '0'; break;
        case 16: output += "0x"; break;
        }
    }
    unsigned_to_str(num, output, base);
}

inline static int hexstr_to_int(const char* first, const char* last) {
    if (first == last)
        throw std::runtime_error("invalid hex number");

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

// class OutputFmt

class OutputFmt {
public:
    enum class Style { NONE = 0, CPP, JS };
    OutputFmt(std::ostream& fout, size_t max_line_len, Style style = Style::NONE);
    void output(const std::string& item);
    template <typename UIntT>
    void output(UIntT num, int base);
    ~OutputFmt();
protected:
    std::ostream& m_fout;
    bool m_first;
    size_t m_line_len;
    // options
    Style m_style;
    const size_t m_max_line_len;
    static const size_t m_indent = 2;
};

template <typename UIntT>
inline void OutputFmt::output(UIntT num, int base) {
    std::string strNum;
    unsigned_to_numstr<UIntT>(num, strNum, static_cast<UIntT>(base));
    output(strNum);
}

OutputFmt::OutputFmt(std::ostream& fout, size_t max_line_len, Style style)
    : m_fout(fout)
    , m_first(true)
    , m_line_len(0)
    , m_style(style)
    , m_max_line_len(max_line_len)
{
    static char* strOpen[3] = { "", "{", "[" };
    m_fout << strOpen[static_cast<int>(m_style)] << std::endl;
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
    static char* strClose[3] = { "", "}", "]" };
    m_fout << strClose[static_cast<int>(m_style)] << std::endl;
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

    OutputFmt outfmt(fout, 100, OutputFmt::Style::JS);
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

template <class OutputFun>
void parse_UnicodeData(const std::string& file_name, OutputFun outputFun)
{
    std::cout << "FILE: " << file_name << std::endl;
    std::ifstream file(file_name, std::ios_base::in);
    if (!file.is_open()) {
        std::cerr << "Can't open data file: " << file_name << std::endl;
        return;
    }

    // parse lines
    int line_num = 0;
    std::string line;
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
                const std::string cpstr = get_column(line, pos);
                const std::string col1 = get_column(line, pos);
                const std::string col2 = get_column(line, pos);

                // code points range
                int cp0, cp1;
                const size_t ind = cpstr.find("..");
                if (ind == cpstr.npos) {
                    cp0 = hexstr_to_int(cpstr.data(), cpstr.data() + cpstr.length());
                    cp1 = cp0;
                } else {
                    cp0 = hexstr_to_int(cpstr.data(), cpstr.data() + ind);
                    cp1 = hexstr_to_int(cpstr.data() + ind + 2, cpstr.data() + cpstr.length());
                }

                outputFun(cp0, cp1, col1, col2);
            }
            catch (std::exception& ex) {
                std::cerr << "ERROR: " << ex.what() << std::endl;
                std::cerr << " LINE(" << line_num << "): " << line << std::endl;
            }
        }
    }
}

// ==================================================================
// Make all in one mapping table

#include "../idna/idna_table.h"

inline uint32_t allowed_char(uint32_t v) {
    // CP_DEVIATION, CP_VALID, CP_NO_STD3_VALID
    return v & CP_VALID;
}


template <typename UIntT>
inline std::string unsigned_to_numstr(UIntT num, int base) {
    std::string strNum;
    unsigned_to_numstr<UIntT>(num, strNum, static_cast<UIntT>(base));
    return strNum;
}

struct char_item {
    char_item() : value(0) {}
    char_item(char_item&& src)
        : value(src.value)
        , charsTo(std::move(src.charsTo))
    {}
    uint32_t value;
    std::u16string charsTo;
};

// default < operator
inline bool operator<(const char_item& lhs, const char_item& rhs) {
    return lhs.value < rhs.value;
}

template <uint32_t Mask>
struct char_item_less {
    inline bool operator()(const char_item& lhs, const char_item& rhs) {
        return (lhs.value & Mask) < (rhs.value & Mask);
    }
};


template <class T, class Compare = std::less<T>>
class array_view {
public:
    array_view(const array_view& src)
        : begin_(src.begin_)
        , end_(src.end_)
    {}
    array_view(const T* begin, size_t count)
        : begin_(begin)
        , end_(begin_ + count)
    {}

    const T* begin() const { return begin_; }
    const T* end() const { return end_; }
    size_t size() const { return end_ - begin_; }

    friend inline bool operator<(const array_view& lhs, const array_view& rhs) {
        return std::lexicographical_compare(lhs.begin(), lhs.end(), rhs.begin(), rhs.end(), Compare());
    }
protected:
    const T* begin_;
    const T* end_;
};


struct block_info {
    template <class T, class Compare = std::less<T>>
    size_t calc_mem_size(const std::vector<T>& arrValues, size_t count, size_t value_size, int shift);

    template <class T, class Compare = std::less<T>>
    size_t calc_mem_size2(const std::vector<T>& arrValues, size_t count, size_t value_size, int shift);

    inline size_t total_mem() const {
        return blocks_mem + index_mem;
    }

    inline uint32_t code_point_mask() const {
        return 0xffffffff >> (32 - size_shift);
    }

    // input
    int size_shift;
    size_t block_size;
    // result
    size_t blocks_count;
    size_t blocks_mem;
    size_t index_count;
    size_t index_mem;
    size_t values_count;
};

template <class T, class Compare>
inline size_t block_info::calc_mem_size(const std::vector<T>& arrValues, size_t count, size_t value_size, int shift) {
    typedef array_view<T, Compare> array_view_T;

    size_shift = shift;
    block_size = static_cast<size_t>(1) << size_shift;

    // count blocks
    std::set<array_view_T> blocks;

    for (size_t ind = 0; ind < count; ind += block_size) {
        size_t chunk_size = std::min(block_size, arrValues.size() - ind);
        array_view_T block(arrValues.data() + ind, chunk_size);
        blocks.insert(block);
    }

    // blocks count & mem size
    blocks_count = blocks.size();
    blocks_mem = blocks_count * block_size * value_size;

    // index count & mem size
    index_count = (count / block_size) + (count % block_size != 0);
    if (blocks_count <= 0xFF) {
        index_mem = index_count;
    } else if (blocks_count <= 0xFFFF) {
        index_mem = index_count * 2;
    } else {
        index_mem = index_count * 4;
    }

    // stored values count
    values_count = std::min(index_count * block_size, arrValues.size());

    return total_mem();
}

template <class T, class Compare>
inline size_t block_info::calc_mem_size2(const std::vector<T>& arrValues, size_t count, size_t value_size, int shift) {
    typedef array_view<T, Compare> array_view_T;
    typedef std::map<array_view_T, int> BlokcsMap;

    size_shift = shift;
    block_size = static_cast<size_t>(1) << size_shift;

    // count blocks
    BlokcsMap blocks;
    std::vector<int> blockIndex;

    int index = 0;
    for (size_t ind = 0; ind < count; ind += block_size) {
        size_t chunk_size = std::min(block_size, arrValues.size() - ind);
        array_view_T block(arrValues.data() + ind, chunk_size);

        auto res = blocks.insert(BlokcsMap::value_type(block, index));
        if (res.second) {
            // for (const char_item& chitem : block) out(chitem.value);
            blockIndex.push_back(index);
            index++;
        } else {
            // index of previously inserted block
            blockIndex.push_back(res.first->second);
        }
    }

    // blocks count & mem size
    blocks_count = blocks.size();
    blocks_mem = blocks_count * block_size * value_size;

    // index count & mem size
    index_count = blockIndex.size();
    size_t index_item_size;
    if (blocks_count <= 0xFF) {
        index_item_size = 1;
    } else if (blocks_count <= 0xFFFF) {
        index_item_size = 2;
    } else {
        index_item_size = 4;
    }
    // second tier >>
    block_info ii = find_block_size(blockIndex, index_count, index_item_size, 0);
    index_mem = ii.total_mem();
    // << end tier

    // stored values count
    values_count = std::min(index_count * block_size, arrValues.size());

    return total_mem();
}

template <class T, class Compare = std::less<T>>
inline block_info find_block_size(const std::vector<T>& arrValues, size_t count, size_t value_size = sizeof(T), int levels = 1) {
    size_t min_mem_size = static_cast<size_t>(-1);
    block_info min_bi;

    for (int size_shift = 1; size_shift < 16; size_shift++) {
        block_info bi;
        size_t mem_size =
            levels <= 1
            ? bi.calc_mem_size<T, Compare>(arrValues, count, value_size, size_shift)
            : bi.calc_mem_size2<T, Compare>(arrValues, count, value_size, size_shift);

        if (levels == 0) std::cout << "  ";
        std::cout << bi.block_size << "(" << bi.size_shift << ")" << ": "
            << mem_size << " = " << bi.blocks_mem << " + " << bi.index_mem
            << std::endl;

        if (min_mem_size > mem_size) {
            min_mem_size = mem_size;
            min_bi = std::move(bi);
        }
    }
    return min_bi;
}

#if 0
inline bool is_16bit_block(const array_view<char_item>& view) {
    return std::all_of(view.begin(), view.end(), [](const char_item& chitem) {
        return (chitem.value & 0xFFFF) == 0;
    });
}
void fff() {
    // block data mem size
#if 10
    // uniform bit blocks
    const size_t item_size = (mask == 0 ? 4 : 2);
    mem += num * block_size * item_size;
#else
    // 32/16 bit blocks
    for (const auto& bl : blocks) {
        mem += (is_16bit_block(bl) ? 2 : 4) * block_size;
    }
#endif
}
#endif


template <class T>
const char* getUIntType(std::vector<T> arr, size_t item_size = sizeof(T)) {
    size_t max_size = 1;
    for (const T& v : arr) {
        const size_t size = (v <= 0xFF ? 1 : (v <= 0xFFFF ? 2 : (v <= 0xFFFFFFFF ? 4 : 8)));
        if (max_size < size) max_size = size;
        if (size == item_size) break;
    }
    switch (max_size)
    {
    case 1: return "uint8_t";
    case 2: return "uint16_t";
    case 4: return "uint32_t";
    case 8: return "uint64_t";
    default: return "???";
    }
}


void make_mapping_table(std::string data_path) {
    const int MAX_CODE_POINT = 0x10FFFF;

    // XXX: intentional memory leak to speed up program exit
    std::vector<char_item>& arrChars(*new std::vector<char_item>(MAX_CODE_POINT + 1));

    // mapped chars string
    std::u16string allCharsTo;

    // Append '/' to data_path
    if (data_path.length() > 0) {
        char c = data_path[data_path.length() - 1];
        if (c != '\\' && c != '/') data_path += '/';
    }

    // http://www.unicode.org/reports/tr46/#IDNA_Mapping_Table
    // IdnaMappingTable.txt
    std::string file_name = data_path + "IdnaMappingTable.txt";
    parse_UnicodeData(file_name, [&](int cp0, int cp1, const std::string& col1, const std::string& col2) {
        bool has_mapped = false;

        // state
        uint32_t state = 0;
        if (col1 == "disallowed") {
            state = CP_DISALLOWED;
        } else if (col1 == "ignored") {
            state = CP_MAPPED;
        } else if (col1 == "mapped") {
            state = CP_MAPPED;
            has_mapped = true;
        } else if (col1 == "deviation") {
            state = CP_DEVIATION;
            has_mapped = true;
        } else if (col1 == "valid") {
            state = CP_VALID;
        } else if (col1 == "disallowed_STD3_mapped") {
            state = CP_NO_STD3_MAPPED;
            has_mapped = true;
        } else if (col1 == "disallowed_STD3_valid") {
            state = CP_NO_STD3_VALID;
        } else {
            // TODO: throw
            std::cerr << "Unknown state: " << col1 << std::endl;
        }

        // mapped to
        uint32_t value = 0;
        std::u16string charsTo;
        if (has_mapped && col2.length() > 0) {
            // parse col2
            split(col2.data(), col2.data() + col2.length(), ' ', [&charsTo](const char* it0, const char* it1) {
                const int cp = hexstr_to_int(it0, it1);
                if (cp <= 0xFFFF) {
                    charsTo.push_back(static_cast<char16_t>(cp));
                } else if (cp < 0x10FFFF) {
                    // http://unicode.org/faq/utf_bom.html#utf16-4
                    // https://en.wikipedia.org/wiki/UTF-16#Description
                    const int cc = cp - 0x10000;
                    char16_t cu1 = 0xD800 | (cc >> 10); // high surrogate
                    char16_t cu2 = 0xDC00 | (cc & 0x03FF); // low surrogate
                    charsTo.push_back(cu1);
                    charsTo.push_back(cu2);
                } else {
                    // TODO: throw
                    std::cerr << "Invalid code point: " << cp << std::endl;
                }
            });
        }

        // put value
        value |= state;
        for (int cp = cp0; cp <= cp1; cp++) {
            arrChars[cp].value = value;
            arrChars[cp].charsTo = charsTo;
        }
    });

    // build mapping chars string
    {
        std::vector<char_item*> arrCharRef;

        // collect items with mapped chars
        for (char_item& chitem : arrChars) {
            if (chitem.charsTo.length()) {
                arrCharRef.push_back(&chitem);
            }
        }

        // sort by charsTo length (longest first)
        std::sort(arrCharRef.begin(), arrCharRef.end(), [](const char_item* lhs, const char_item* rhs) {
            return lhs->charsTo.length() > rhs->charsTo.length(); // reverse
        });

        // build
        for (char_item* chitem : arrCharRef) {
            uint32_t value = 0;
            uint32_t flags = 0;
            if (chitem->charsTo.length() == 1) {
                value = chitem->charsTo[0];
                flags |= MAP_TO_ONE;
            } else {
                size_t pos = allCharsTo.find(chitem->charsTo);
                if (pos == allCharsTo.npos) {
                    pos = allCharsTo.length();
                    allCharsTo.append(chitem->charsTo);
                }

                size_t maxPos = 0;
                if (chitem->charsTo.length() < 7) {
                    value |= static_cast<uint32_t>(chitem->charsTo.length()) << 13;
                    maxPos = 0x1FFF;
                } else if (chitem->charsTo.length() < 7 + 0x1F) {
                    value |= static_cast<uint32_t>(7) << 13;
                    value |= static_cast<uint32_t>(chitem->charsTo.length() - 7) << 8;
                    maxPos = 0x00FF;
                }

                if (pos <= maxPos && maxPos) {
                    value |= pos;
                } else {
                    // TODO: throw
                    std::cerr << "FATAL: Too long mapping string" << std::endl;
                }
            }
            // save pos, length
            chitem->value |= (value | flags);
        }
    }


    // DerivedGeneralCategory.txt
    file_name = data_path + "DerivedGeneralCategory.txt";
    parse_UnicodeData(file_name, [&](int cp0, int cp1, const std::string& col1, const std::string& col2) {
        if (col1.length() > 0 && col1[0] == 'M') {
            for (int cp = cp0; cp <= cp1; cp++) {
                if (allowed_char(arrChars[cp].value)) {
                    arrChars[cp].value |= CAT_MARK;
                } else {
                    ///std::cout << "-- Mark: " << cp << std::endl;
                }
            }
        }
    });

#if 1
    // DerivedCombiningClass.txt
    file_name = data_path + "DerivedCombiningClass.txt";
    parse_UnicodeData(file_name, [&](int cp0, int cp1, const std::string& col1, const std::string& col2) {
        // 9 - Virama
        if (col1 == "9") {
            for (int cp = cp0; cp <= cp1; cp++) {
                if (allowed_char(arrChars[cp].value)) {
                    arrChars[cp].value |= CAT_Virama;
                } else {
                    ///std::cout << "-- Virama: " << cp << std::endl;
                }
            }
        }
    });

    // DerivedJoiningType.txt
    file_name = data_path + "DerivedJoiningType.txt";
    parse_UnicodeData(file_name, [&](int cp0, int cp1, const std::string& col1, const std::string& col2) {
        if (col1.length() == 1) {
            uint32_t flag = 0;
            switch (col1[0]) {
            case 'D': flag = CAT_Joiner_D; break;
            case 'L': flag = CAT_Joiner_L; break;
            case 'R': flag = CAT_Joiner_R; break;
            case 'T': flag = CAT_Joiner_T; break;
            }
            if (flag) {
                for (int cp = cp0; cp <= cp1; cp++) {
                    if (allowed_char(arrChars[cp].value)) {
                        arrChars[cp].value |= flag;
                    } else {
                        ///std::cout << "-- Joiner: " << cp << std::endl;
                    }
                }
            }
        }
    });

    // DerivedBidiClass.txt
    file_name = data_path + "DerivedBidiClass.txt";
    parse_UnicodeData(file_name, [&](int cp0, int cp1, const std::string& col1, const std::string& col2) {
        uint32_t flag = 0;
        if (col1 == "L") {
            flag = CAT_Bidi_L;
        } else if (col1 == "R" || col1 == "AL") {
            flag = CAT_Bidi_R_AL;
        } else if (col1 == "AN") {
            flag = CAT_Bidi_AN;
        } else if (col1 == "EN") {
            flag = CAT_Bidi_EN;
        } else if (col1 == "ES" || col1 == "CS" || col1 == "ET" || col1 == "ON" || col1 == "BN") {
            flag = CAT_Bidi_ES_CS_ET_ON_BN;
        } else if (col1 == "NSM") {
            flag = CAT_Bidi_NSM;
        } else {
            // Nenaudojama kategorija
        }
        if (flag) {
            for (int cp = cp0; cp <= cp1; cp++) {
                if (allowed_char(arrChars[cp].value)) {
                    arrChars[cp].value |= flag;
                } else {
                    ////std::cout << "-- Bidi: " << cp << std::endl;
                }
            }
        }
    });
#endif

    //=======================================================================
    // Output Data

    const size_t count_chars = 0x2FA1E; /*0x30000*/
    const int index_levels = 1; // 1 arba 2

    // calculate 32bit block size
    std::cout << "=== 32 bit BLOCK ===\n";
    block_info binf = find_block_size(arrChars, count_chars, sizeof(uint32_t), index_levels);
    size_t block_size = binf.block_size;

    // total memory used
    std::cout << "block_size=" << block_size << "; mem=" << binf.total_mem() << "\n";
    std::cout << "allCharsTo size: " << allCharsTo.size() << "; mem: " << allCharsTo.size() * 2 << "\n";
    std::cout << "TOTAL MEM: " << binf.total_mem() + allCharsTo.size() * 2 << "\n";

#if 0
    std::cout << "=== 16 bit BLOCK ===\n";
    std::cout << "--- Flags\n";
    find_block_size<char_item, char_item_less<0xFFFF0000>>(arrChars, count_chars, sizeof(uint16_t), 2);
    std::cout << "--- Mapping\n";
    find_block_size<char_item, char_item_less<0xFFFF>>(arrChars, count_chars, sizeof(uint16_t), 2);
    return;
#endif


    //=======================================================================
    // generate code

    file_name = data_path + "GEN-idna-tables.txt";
    std::ofstream fout(file_name, std::ios_base::out);
    if (!fout.is_open()) {
        std::cerr << "Can't open destination file: " << file_name << std::endl;
        return;
    }

    file_name = data_path + "GEN-idna-tables.H.txt";
    std::ofstream fout_head(file_name, std::ios_base::out);
    if (!fout_head.is_open()) {
        std::cerr << "Can't open destination file: " << file_name << std::endl;
        return;
    }
    // constants
    fout_head << "const size_t blockShift = " << binf.size_shift << ";\n";
    fout_head << "const uint32_t blockMask = " << unsigned_to_numstr(binf.code_point_mask(), 16) << ";\n";
    fout_head << "const uint32_t defaultStart = " << unsigned_to_numstr(count_chars, 16) << ";\n";
    fout_head << "const uint32_t defaultValue = " << unsigned_to_numstr(arrChars[count_chars].value, 16) << ";\n";
    fout_head << "const uint32_t specRange1 = " << unsigned_to_numstr(0xE0100, 16) << ";\n"; //TODO
    fout_head << "const uint32_t specRange2 = " << unsigned_to_numstr(0xE01EF, 16) << ";\n"; //TODO
    fout_head << "const uint32_t specValue = " << unsigned_to_numstr(arrChars[0xE0100].value, 16) << ";\n"; //TODO
    fout_head << "\n";
    // ---

    std::vector<int> blockIndex;

    fout_head << "extern uint32_t blockData[];\n";
    fout << "uint32_t blockData[] = {";
    {
        OutputFmt outfmt(fout, 100);

        typedef std::map<array_view<char_item>, int> BlokcsMap;
        BlokcsMap blocks;
        int index = 0;
        for (size_t ind = 0; ind < count_chars; ind += block_size) {
            size_t chunk_size = std::min(block_size, arrChars.size() - ind);
            array_view<char_item> block(arrChars.data() + ind, chunk_size);

            auto res = blocks.insert(BlokcsMap::value_type(block, index));
            if (res.second) {
                for (const char_item& chitem : block) {
                    outfmt.output(chitem.value, 16);
                }
                blockIndex.push_back(index);
                index++;
            } else {
                // index of previously inserted block
                blockIndex.push_back(res.first->second);
            }
        }
    }
    fout << "};\n\n";

    if (index_levels == 1) {
        // Vieno lygio indeksas
        const char* sztype = getUIntType(blockIndex);
        fout_head << "extern " << sztype << " blockIndex[];\n";
        fout << sztype << " blockIndex[] = {";
        {
            OutputFmt outfmt(fout, 100);
            for (int index : blockIndex) {
                outfmt.output(index, 10);
            }
        }
        fout << "};\n\n";
    }

    if (index_levels == 2) {
        // Dviejų lygių indeksas
        std::vector<int> indexToIndex;
        const char* sztype = getUIntType(blockIndex);
        fout_head << "extern " << sztype << " indexToBlock[];\n";
        fout << sztype << " indexToBlock[] = {";
        {
            size_t count = blockIndex.size();
            std::cout << "=== Index BLOCK ===\n";
            block_info bi = find_block_size(blockIndex, count, sizeof(uint16_t));

            OutputFmt outfmt(fout, 100);

            typedef std::map<array_view<int>, int> BlokcsMap;
            BlokcsMap blocks;
            int index = 0;
            for (size_t ind = 0; ind < count; ind += bi.block_size) {
                size_t chunk_size = std::min(bi.block_size, count - ind);
                array_view<int> block(blockIndex.data() + ind, chunk_size);

                auto res = blocks.insert(BlokcsMap::value_type(block, index));
                if (res.second) {
                    for (const int value : block) {
                        outfmt.output(value, 10);
                    }
                    indexToIndex.push_back(index);
                    index++;
                } else {
                    // index of previously inserted block
                    indexToIndex.push_back(res.first->second);
                }
            }
        }
        fout << "};\n\n";
        sztype = getUIntType(indexToIndex);
        fout_head << "extern " << sztype << " indexToIndex[];\n";
        fout << sztype << " indexToIndex[] = {";
        {
            OutputFmt outfmt(fout, 100);
            for (int ind : indexToIndex) {
                outfmt.output(ind, 10);
            }
        }
        fout << "};\n\n";
    }

    fout_head << "extern char16_t allCharsTo[];\n";
    fout << "char16_t allCharsTo[] = {";
    {
        OutputFmt outfmt(fout, 100);
        for (char16_t ch : allCharsTo) {
            outfmt.output(ch, 16);
        }
    }
    fout << "};\n\n";


    return;
}
