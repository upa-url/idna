// Copyright 2017-2025 Rimas Misevičius
// Distributed under the BSD-style license that can be
// found in the LICENSE file.
//
#include "unicode_data_tools.h"
#include <filesystem>
#include <unordered_set>

using namespace upa::tools;


static void make_mapping_table(const std::filesystem::path& data_path);

int main(int argc, char* argv[])
{
    if (argc != 2) {
        std::cerr <<
            "unitool <data directory path>\n"
            "\n"
            "Specify the directory path where the following files are located:\n"
            " DerivedBidiClass.txt\n"
            " DerivedCombiningClass.txt\n"
            " DerivedGeneralCategory.txt\n"
            " DerivedJoiningType.txt\n"
            " DerivedNormalizationProps.txt\n"
            " IdnaMappingTable.txt\n"
            " UnicodeData.txt\n";
        return 1;
    }
    make_mapping_table(argv[1]);
    return 0;
}


// ==================================================================
// Make all in one mapping table

// Definitions of the CP_... constants
#include "../include/upa/idna/idna_table.h"
using namespace upa::idna::util;

// char mapping to char type
using char_to_t = char32_t;
using string_to_t = std::basic_string<char_to_t>;

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
    string_to_t charsTo;
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

static void make_comp_disallowed_tables(const std::filesystem::path& data_path,
    const std::vector<char_item>& arrChars,
    std::ostream& fout_head, std::ostream& fout);

void make_mapping_table(const std::filesystem::path& data_path) {
    // XXX: intentional memory leak to speed up program exit
    std::vector<char_item>& arrChars(*new std::vector<char_item>(MAX_CODE_POINT + 1));

    // mapped chars string
    string_to_t allCharsTo;

    // http://www.unicode.org/reports/tr46/#IDNA_Mapping_Table
    // IdnaMappingTable.txt
    auto file_name = data_path / "IdnaMappingTable.txt";
    parse_UnicodeData<2>(file_name, [&](int cp0, int cp1, const auto& col) {
        bool has_mapped = false;

        // state
        uint32_t state = 0;
        if (col[0] == "disallowed") {
            state = CP_DISALLOWED;
        } else if (col[0] == "ignored") {
            state = CP_MAPPED;
        } else if (col[0] == "mapped") {
            state = CP_MAPPED;
            has_mapped = true;
        } else if (col[0] == "deviation") {
            state = CP_DEVIATION;
            has_mapped = true;
        } else if (col[0] == "valid") {
            state = CP_VALID;
        } else {
            // TODO: throw
            std::cerr << "Unknown state: " << col[0] << std::endl;
        }

        // mapped to
        string_to_t charsTo;
        if (has_mapped && col[1].length() > 0) {
            // parse col[1]
            split(col[1].data(), col[1].data() + col[1].length(), ' ', [&charsTo](const char* it0, const char* it1) {
                const int cp = hexstr_to_int(it0, it1);
                // TODO C++17: if constexpr
                if (sizeof(char_to_t) == sizeof(char16_t)) {
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
                } else if (sizeof(char_to_t) == sizeof(char32_t)) {
                    if (cp < 0x10FFFF) {
                        charsTo.push_back(static_cast<char32_t>(cp));
                    } else {
                        // TODO: throw
                        std::cerr << "Invalid code point: " << cp << std::endl;
                    }
                }
            });
        }

        // put value
        for (int cp = cp0; cp <= cp1; cp++) {
            uint32_t value = state;
            // Allowed STD3 characters, see "Validity Criteria" 7.3. in
            // https://www.unicode.org/reports/tr46/tr46-33.html#Validity_Criteria
            // If the code point is an ASCII code point (U+0000..U+007F), then it must
            // be a lowercase letter (a-z), a digit (0-9), or a hyphen-minus (U+002D)
            if (cp <= 0x7F && value == CP_VALID &&
                !((cp >= 'a' && cp <= 'z') || (cp >= '0' && cp <= '9') || cp == 0x2D || cp == 0x2E)) {
                value = CP_NO_STD3_VALID;
            }
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
            if (chitem->charsTo.length() == 1 && chitem->charsTo[0] <= 0xFFFF) {
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
    file_name = data_path / "DerivedGeneralCategory.txt";
    parse_UnicodeData<1>(file_name, [&](int cp0, int cp1, const auto& col) {
        if (col[0].length() > 0 && col[0][0] == 'M') {
            for (int cp = cp0; cp <= cp1; cp++) {
                if (allowed_char(arrChars[cp].value)) {
                    arrChars[cp].value |= CAT_MARK;
                } else {
                    ///std::cout << "-- Mark: " << cp << std::endl;
                }
            }
        }
    });

    // DerivedCombiningClass.txt
    file_name = data_path / "DerivedCombiningClass.txt";
    parse_UnicodeData<1>(file_name, [&](int cp0, int cp1, const auto& col) {
        // 9 - Virama
        if (col[0] == "9") {
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
    file_name = data_path / "DerivedJoiningType.txt";
    parse_UnicodeData<1>(file_name, [&](int cp0, int cp1, const auto& col) {
        if (col[0].length() == 1) {
            uint32_t flag = 0;
            switch (col[0][0]) {
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
    file_name = data_path / "DerivedBidiClass.txt";
    parse_UnicodeData<1>(file_name, [&](int cp0, int cp1, const auto& col) {
        uint32_t flag = 0;
        if (col[0] == "L") {
            flag = CAT_Bidi_L;
        } else if (col[0] == "R" || col[0] == "AL") {
            flag = CAT_Bidi_R_AL;
        } else if (col[0] == "AN") {
            flag = CAT_Bidi_AN;
        } else if (col[0] == "EN") {
            flag = CAT_Bidi_EN;
        } else if (col[0] == "ES" || col[0] == "CS" || col[0] == "ET" || col[0] == "ON" || col[0] == "BN") {
            flag = CAT_Bidi_ES_CS_ET_ON_BN;
        } else if (col[0] == "NSM") {
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
    std::cout << "uni_chars_to size: " << allCharsTo.size() << "; mem: " << allCharsTo.size() * sizeof(allCharsTo[0]) << "\n";
    std::cout << "TOTAL MEM: " << binf.total_mem() + allCharsTo.size() * sizeof(allCharsTo[0]) << "\n";

#if 0
    std::cout << "=== 16 bit BLOCK ===\n";
    std::cout << "--- Flags\n";
    find_block_size<char_item, char_item_less<0xFFFF0000>>(arrChars, count_chars, sizeof(uint16_t), 2);
    std::cout << "--- Mapping\n";
    find_block_size<char_item, char_item_less<0xFFFF>>(arrChars, count_chars, sizeof(uint16_t), 2);
    return;
#endif

    //=======================================================================
    // Generate code

    file_name = data_path / "GEN-idna-tables.txt";
    std::ofstream fout(file_name, std::ios_base::out);
    if (!fout.is_open()) {
        std::cerr << "Can't open destination file: " << file_name << std::endl;
        return;
    }

    file_name = data_path / "GEN-idna-tables.H.txt";
    std::ofstream fout_head(file_name, std::ios_base::out);
    if (!fout_head.is_open()) {
        std::cerr << "Can't open destination file: " << file_name << std::endl;
        return;
    }

    // Constants
    output_unsigned_constant(fout_head, "std::size_t", "uni_block_shift", binf.size_shift, 10);
    output_unsigned_constant(fout_head, "std::uint32_t", "uni_block_mask", binf.code_point_mask(), 16);
    output_unsigned_constant(fout_head, "std::uint32_t", "uni_default_start", count_chars, 16);
    output_unsigned_constant(fout_head, "std::uint32_t", "uni_default_value", arrChars[count_chars].value, 16);
    output_unsigned_constant(fout_head, "std::uint32_t", "uni_spec_range1", spec.m_range[1].from /*0xE0100*/, 16);
    output_unsigned_constant(fout_head, "std::uint32_t", "uni_spec_range2", spec.m_range[1].to /*0xE01EF*/, 16);
    output_unsigned_constant(fout_head, "std::uint32_t", "uni_spec_value", arrChars[spec.m_range[1].from].value, 16);
    fout_head << "\n";
    // ---

    std::vector<int> blockIndex;

    fout_head << "extern UPA_IDNA_API const std::uint32_t uni_data[];\n";
    fout << "const std::uint32_t uni_data[] = {";
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
        fout_head << "extern UPA_IDNA_API const " << sztype << " uni_data_index[];\n";
        fout << "const " << sztype << " uni_data_index[] = {";
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
        fout_head << "extern const " << sztype << " indexToBlock[];\n";
        fout << "const " << sztype << " indexToBlock[] = {";
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
        fout_head << "extern const " << sztype << " indexToIndex[];\n";
        fout << "const " << sztype << " indexToIndex[] = {";
        {
            OutputFmt outfmt(fout, 100);
            for (int ind : indexToIndex) {
                outfmt.output(ind, 10);
            }
        }
        fout << "};\n\n";
    }

    const char* sztype = getCharType<char_to_t>();
    fout_head << "extern UPA_IDNA_API const " << sztype << " uni_chars_to[];\n";
    fout << "const " << sztype << " uni_chars_to[] = {";
    {
        OutputFmt outfmt(fout, 100);
        for (auto ch : allCharsTo) {
            outfmt.output(ch, 16);
        }
    }
    fout << "};\n\n";

    // Make table of IDNA disallowed code points that can be changed by NFC
    fout_head << '\n';
    make_comp_disallowed_tables(data_path, arrChars, fout_head, fout);

    // ASCII data
    fout_head << '\n';
    fout_head << "extern UPA_IDNA_API const std::uint8_t ascii_data[128];\n";
    fout << "const std::uint8_t ascii_data[128] = {";
    {
        OutputFmt outfmt(fout, 100);
        for (std::uint8_t ch = 0; ch < 128; ++ch) {
            outfmt.output((arrChars[ch].value >> 16) & 0x07, 16);
        }
    }
    fout << "};\n\n";
}

// Make table of IDNA disallowed code points that can be changed by NFC

namespace hangul {
    constexpr char32_t SBase = 0xAC00;
    constexpr char32_t LBase = 0x1100;
    constexpr char32_t VBase = 0x1161;
    constexpr char32_t TBase = 0x11A7;
    constexpr char32_t LCount = 19;
    constexpr char32_t VCount = 21;
    constexpr char32_t TCount = 28;
    constexpr char32_t NCount = VCount * TCount; // 588
    constexpr char32_t SCount = LCount * NCount; // 11172
}

bool is_hangul_composable(char32_t cp) {
    if (cp >= hangul::SBase && cp < hangul::SBase + hangul::SCount)
        return true;
    if (cp >= hangul::LBase && cp < hangul::LBase + hangul::LCount)
        return true;
    if (cp >= hangul::VBase && cp < hangul::VBase + hangul::VCount)
        return true;
    if (cp >= hangul::TBase && cp < hangul::TBase + hangul::TCount)
        return true;
    return false;
}

void make_comp_disallowed_tables(const std::filesystem::path& data_path,
    const std::vector<char_item>& arrChars,
    std::ostream& fout_head, std::ostream& fout)
{
    // Full composition exclusion
    std::unordered_set<char32_t> composition_exclusion;
    auto file_name = data_path / "DerivedNormalizationProps.txt";
    parse_UnicodeData<1>(file_name,
        [&](int cp0, int cp1, const auto& col) {
            if (col[0] == "Full_Composition_Exclusion") {
                for (auto cp = cp0; cp <= cp1; ++cp)
                    composition_exclusion.insert(cp);
            }
        });

    std::unordered_set<char32_t> composables;

    file_name = data_path / "UnicodeData.txt";
    parse_UnicodeData<5>(file_name,
        [&](int cp0, int cp1, const auto& col) {
            // https://www.unicode.org/reports/tr44/#Character_Decomposition_Mappings
            const auto& decomposition_mapping = col[4];
            if (decomposition_mapping.length() > 0 && decomposition_mapping[0] != '<') {
                // Canonical decomposition mapping
                composables.insert(cp0);

                // Composition mapping
                if (composition_exclusion.count(cp0) == 0) {
                    std::u32string charsTo;
                    split(decomposition_mapping.data(), decomposition_mapping.data() + decomposition_mapping.length(), ' ',
                        [&charsTo](const char* it0, const char* it1) {
                            const int cp = hexstr_to_int(it0, it1);
                            charsTo.push_back(static_cast<char32_t>(cp));
                        });
                    if (charsTo.size() == 2) {
                        composables.insert(charsTo[0]);
                        composables.insert(charsTo[1]);
                    }
                }
            }
        });

    std::vector<std::uint32_t> comp_disallowed;
    std::vector<std::uint32_t> comp_disallowed_std3;

    for (char32_t cp = 0; cp < arrChars.size(); ++cp) {
        const auto chinf = arrChars[cp].value;
        if ((chinf == CP_DISALLOWED || (chinf & CP_DISALLOWED_STD3) != 0) &&
            (composables.count(cp) != 0 || is_hangul_composable(cp))
            ) {
            if (chinf == CP_DISALLOWED)
                comp_disallowed.push_back(cp);
            else
                comp_disallowed_std3.push_back(cp);
        }
    }

    if (!comp_disallowed.empty()) {
        fout_head << "extern const std::uint32_t comp_disallowed[" << comp_disallowed.size() << "];\n";
        fout << "const std::uint32_t comp_disallowed[" << comp_disallowed.size() << "] = {";
        {
            OutputFmt outfmt(fout, 100);
            for (auto ch : comp_disallowed) {
                outfmt.output(ch, 16);
            }
        }
        fout << "};\n\n";
    }

    if (!comp_disallowed_std3.empty()) {
        // Starting with Unicode 16.0.0, disallowed STD3 characters are in the ASCII range.
        // See "Validity Criteria" 7.3. in https://www.unicode.org/reports/tr46/tr46-33.html#Validity_Criteria
        fout_head << "extern const std::uint8_t comp_disallowed_std3[" << comp_disallowed_std3.size() << "];\n";
        fout << "const std::uint8_t comp_disallowed_std3[" << comp_disallowed_std3.size() << "] = {";
        {
            OutputFmt outfmt(fout, 100);
            for (auto ch : comp_disallowed_std3) {
                outfmt.output(ch, 16);
            }
        }
        fout << "};\n\n";
    }
}
