// unitool.cpp : Defines the entry point for the console application.
//
#include "unicode_data_tools.h"

using namespace upa::tools;


static void make_mapping_table(std::string data_path);

int main(int argc, char* argv[])
{
    if (argc != 2) {
        std::cerr <<
            "unitool <data directory path>\n"
            "\n"
            "Specify the directory path where the IdnaMappingTable.txt, DerivedBidiClass.txt, DerivedCombiningClass.txt,\n"
            "DerivedGeneralCategory.txt, DerivedJoiningType.txt files are located.\n";
        return 1;
    }
    make_mapping_table(argv[1]);
    return 0;
}


// ==================================================================
// Make all in one mapping table

#include "../src/idna_table.h"

// We save Mark, Virama, Joinner, Bidi catogories only for chars having CP_VALID flag
// set (it includes: CP_DEVIATION, CP_VALID, or CP_NO_STD3_VALID). This will dramatically
// reduce the size of lookup tables.
inline uint32_t allowed_char(uint32_t v) {
    // CP_DEVIATION, CP_VALID, CP_NO_STD3_VALID
    return v & CP_VALID;
}

// char_item struct

struct char_item {
    char_item() = default;
    char_item(char_item&& src) noexcept = default;

    operator uint32_t() const noexcept {
        return value;
    }

    uint32_t value = 0;
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

// Make mapping table

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

    special_ranges<uint32_t> spec(arrChars, 2);
    //TODO spec.range.size() >= 2

    const size_t count_chars = spec.m_range[0].from;
    const int index_levels = 1; // 1 arba 2

    // calculate 32bit block size
    std::cout << "=== 32 bit BLOCK ===\n";
    block_info binf = find_block_size(arrChars, count_chars, sizeof(uint32_t), index_levels);
    size_t block_size = binf.block_size;

    // total memory used
    std::cout << "block_size=" << block_size << "; mem: " << binf.total_mem() << "\n";
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
    fout_head << "const uint32_t specRange1 = " << unsigned_to_numstr(spec.m_range[1].from /*0xE0100*/, 16) << ";\n";
    fout_head << "const uint32_t specRange2 = " << unsigned_to_numstr(spec.m_range[1].to /*0xE01EF*/, 16) << ";\n";
    fout_head << "const uint32_t specValue = " << unsigned_to_numstr(arrChars[spec.m_range[1].from].value, 16) << ";\n";
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
