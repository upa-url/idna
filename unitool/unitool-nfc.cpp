// Copyright 2017-2025 Rimas Misevičius
// Distributed under the BSD-style license that can be
// found in the LICENSE file.
//
#include "unicode_data_tools.h"
#include <filesystem>
#include <unordered_set>

using namespace upa::tools;


static void output_newline(std::ostream& fout_h, std::ostream& fout_cpp);
static void make_ccc_table(const std::filesystem::path& data_path, std::ostream& fout_h, std::ostream& fout_cpp);
static void make_composition_tables(const std::filesystem::path& data_path, std::ostream& fout_h, std::ostream& fout_cpp);

int main(int argc, char* argv[])
{
    if (argc != 2) {
        std::cerr <<
            "unitool-nfc <data directory path>\n"
            "\n"
            "Specify the directory path where the following files are located:\n"
            " DerivedCombiningClass.txt\n"
            " DerivedNormalizationProps.txt\n"
            " UnicodeData.txt\n";
        return 1;
    }

    // Data files path
    const std::filesystem::path data_path{ argv[1] };

    // Output files
    auto file_name = data_path / "GEN-nfc-tables.h.txt";
    std::ofstream fout_h(file_name, std::ios_base::out);
    if (!fout_h.is_open()) {
        std::cerr << "Can't open destination file: " << file_name << std::endl;
        return 2;
    }

    file_name = data_path / "GEN-nfc-tables.cpp.txt";
    std::ofstream fout_cpp(file_name, std::ios_base::out);
    if (!fout_cpp.is_open()) {
        std::cerr << "Can't open destination file: " << file_name << std::endl;
        return 2;
    }

    make_ccc_table(data_path, fout_h, fout_cpp);
    output_newline(fout_h, fout_cpp);
    make_composition_tables(data_path, fout_h, fout_cpp);

    return 0;
}

static void output_newline(std::ostream& fout_h, std::ostream& fout_cpp)
{
    fout_h << std::endl;
    fout_cpp << std::endl;
}

// ==================================================================
// Canonical_Combining_Class (ccc)

static void make_ccc_table(const std::filesystem::path& data_path, std::ostream& fout_h, std::ostream& fout_cpp)
{
    using item_type = std::uint8_t;
    using item_num_type = item_type;

    std::vector<item_type> arr_ccc(MAX_CODE_POINT + 1);

    auto file_name = data_path / "DerivedCombiningClass.txt";
    parse_UnicodeData<1>(file_name,
        [&](int cp0, int cp1, const auto& col) {
            const std::uint8_t value = std::stoi(col[0]);
            for (int cp = cp0; cp <= cp1; cp++)
                arr_ccc[cp] = value;
        });

    // Special ranges to reduce table size
    special_ranges<item_num_type> spec_ccc(arr_ccc, 0);
    const std::size_t count_chars = spec_ccc.m_range[0].from; // MAX_CODE_POINT + 1;
    const int index_levels = 1; // 1 arba 2

    // Find block size
    const auto binf = find_block_size(arr_ccc, count_chars, sizeof(item_num_type), index_levels);
    const std::size_t block_size = binf.block_size;

    // memory used
    std::cout << "block_size=" << block_size << "; mem=" << binf.total_mem() << "\n";

    //=======================================================================
    // Generate code

    const char* sz_item_num_type = getUIntType<item_num_type>();

    // Constants
    output_unsigned_constant(fout_h, "std::size_t", "ccc_block_shift", binf.size_shift, 10);
    output_unsigned_constant(fout_h, "std::uint32_t", "ccc_block_mask", binf.code_point_mask(), 16);
    output_unsigned_constant(fout_h, "std::uint32_t", "ccc_default_start", count_chars, 16);
    output_unsigned_constant(fout_h, sz_item_num_type, "ccc_default_value", arr_ccc[count_chars], 16);

    // CCC blocks
    std::vector<int> block_index;

    fout_h << "extern const " << sz_item_num_type << " ccc_block[];\n";
    fout_cpp << "const " << sz_item_num_type << " ccc_block[] = {";
    {
        OutputFmt outfmt(fout_cpp, 100);

        typedef std::map<array_view<item_type>, int> BlokcsMap;
        BlokcsMap blocks;
        int index = 0;
        for (std::size_t ind = 0; ind < count_chars; ind += block_size) {
            std::size_t chunk_size = std::min(block_size, arr_ccc.size() - ind);
            array_view<item_type> block(arr_ccc.data() + ind, chunk_size);

            auto res = blocks.insert(BlokcsMap::value_type(block, index));
            if (res.second) {
                for (const auto& item : block) {
                    outfmt.output(static_cast<item_num_type>(item), 16);
                }
                block_index.push_back(index);
                index++;
            } else {
                // index of previously inserted block
                block_index.push_back(res.first->second);
            }
        }
    }
    fout_cpp << "};\n\n";

    {   // Single level index
        const char* sztype = getUIntType(block_index);
        fout_h << "extern const " << sztype << " ccc_block_index[];\n";
        fout_cpp << "const " << sztype << " ccc_block_index[] = {";
        {
            OutputFmt outfmt(fout_cpp, 100);
            for (int index : block_index) {
                outfmt.output(index, 10);
            }
        }
        fout_cpp << "};\n\n";
    }

}

// ==================================================================
// Canonical Decomposition and Composition

struct codepoint_key_val {
    char32_t key;
    char32_t val;
};

inline bool operator==(const codepoint_key_val& lhs, const codepoint_key_val& rhs) {
    return lhs.key == rhs.key && lhs.val == rhs.val;
}


static void make_composition_tables(const std::filesystem::path& data_path, std::ostream& fout_h, std::ostream& fout_cpp)
{
    using item_num_type = uint16_t;
    struct item_type {
        item_type() = default;
        item_type(item_type&& src) noexcept = default;

        operator item_num_type() const noexcept {
            return value;
        }

        item_num_type value = 0;
    };

    struct comp_item_type : public item_type {
        std::vector<codepoint_key_val> comp_key_val;
    };

    struct decomp_item_type : public item_type {
        std::u32string charsTo;
    };

    std::vector<decomp_item_type> arr_decomp(MAX_CODE_POINT + 1);

    auto file_name = data_path / "UnicodeData.txt";
    parse_UnicodeData<5>(file_name,
        [&](int cp0, int cp1, const auto& col) {
            // https://www.unicode.org/reports/tr44/#Character_Decomposition_Mappings
            const auto& decomposition_mapping = col[4];
            if (decomposition_mapping.length() > 0 && decomposition_mapping[0] != '<') {
                // Canonical decomposition mapping
                std::u32string charsTo;
                split(decomposition_mapping.data(), decomposition_mapping.data() + decomposition_mapping.length(), ' ',
                    [&charsTo](const char* it0, const char* it1) {
                        const int cp = hexstr_to_int(it0, it1);
                        charsTo.push_back(static_cast<char32_t>(cp));
                    });
                arr_decomp[cp0].charsTo = std::move(charsTo);
            }
        });

    //=======================================================================
    // Composition Data
    {
        // Full composition exclusion
        std::unordered_set<char32_t> composition_exclusion;
        file_name = data_path / "DerivedNormalizationProps.txt";
        parse_UnicodeData<1>(file_name,
            [&](int cp0, int cp1, const auto& col) {
                if (col[0] == "Full_Composition_Exclusion") {
                    for (auto cp = cp0; cp <= cp1; ++cp)
                        composition_exclusion.insert(cp);
                }
            });

        // Create Composition table
        std::vector<comp_item_type> arr_comp(MAX_CODE_POINT + 1);
        for (char32_t cp = 0; cp <= MAX_CODE_POINT; ++cp) {
            if (arr_decomp[cp].charsTo.length() == 2 &&
                composition_exclusion.count(cp) == 0) {
                const auto cp1 = arr_decomp[cp].charsTo[0];
                const auto cp2 = arr_decomp[cp].charsTo[1];
                arr_comp[cp1].comp_key_val.push_back({ cp2, cp });
            }
        }
        // sort items data
        for (auto& item : arr_comp) {
            std::sort(item.comp_key_val.begin(), item.comp_key_val.end(),
                [](const auto& a, const auto& b) {
                    return a.key < b.key;
                });
        }

        // Generate Composition data

        std::vector<codepoint_key_val> all_comp_data;
        for (char32_t cp = 0; cp <= MAX_CODE_POINT; ++cp) {
            if (arr_comp[cp].comp_key_val.size() > 0) {
                const std::size_t len = arr_comp[cp].comp_key_val.size();
                std::size_t pos = 0;

                auto it = std::search(all_comp_data.begin(), all_comp_data.end(),
                    arr_comp[cp].comp_key_val.begin(), arr_comp[cp].comp_key_val.end());
                if (it == all_comp_data.end()) {
                    pos = all_comp_data.size();
                    all_comp_data.insert(all_comp_data.end(), arr_comp[cp].comp_key_val.begin(), arr_comp[cp].comp_key_val.end());
                } else
                    pos = std::distance(all_comp_data.begin(), it);

                if (pos <= 0x7FF && len <= 0x1F)
                    arr_comp[cp].value = (static_cast<item_num_type>(len) << 11) | static_cast<item_num_type>(pos);
                else
                    std::cerr << "FATAL: Too big pos or len:" << pos << ' ' << len << std::endl;
            }
        }

        // Output Data

        special_ranges<item_num_type> spec(arr_comp);
        const std::size_t count_chars = spec.m_range[0].from;

        std::cout << "=== 16 bit BLOCK ===\n";
        block_info binf = find_block_size(arr_comp, count_chars, sizeof(item_num_type));
        std::size_t block_size = binf.block_size;

        // total memory used
        std::cout << "block_size=" << block_size << "; mem: " << binf.total_mem() << "\n";
        std::cout << "comp_block_data size: " << all_comp_data.size() << "; mem: " << all_comp_data.size() * sizeof(all_comp_data[0]) << "\n";
        std::cout << "TOTAL MEM: " << binf.total_mem() + all_comp_data.size() * sizeof(all_comp_data[0]) << "\n";

        // Generate code

        const char* sz_item_num_type = getUIntType<item_num_type>();

        // Constants
        output_unsigned_constant(fout_h, "std::size_t", "comp_block_shift", binf.size_shift, 10);
        output_unsigned_constant(fout_h, "std::uint32_t", "comp_block_mask", binf.code_point_mask(), 16);
        output_unsigned_constant(fout_h, "std::uint32_t", "comp_default_start", count_chars, 16);
        output_unsigned_constant(fout_h, sz_item_num_type, "comp_default_value", arr_comp[count_chars].value, 16);

        // Composition blocks
        std::vector<int> block_index;

        fout_h << "extern const " << sz_item_num_type << " comp_block[];\n";
        fout_cpp << "const " << sz_item_num_type << " comp_block[] = {";
        {
            OutputFmt outfmt(fout_cpp, 100);

            typedef std::map<array_view<comp_item_type>, int> BlokcsMap;
            BlokcsMap blocks;
            int index = 0;
            for (std::size_t ind = 0; ind < count_chars; ind += block_size) {
                std::size_t chunk_size = std::min(block_size, arr_comp.size() - ind);
                array_view<comp_item_type> block(arr_comp.data() + ind, chunk_size);

                auto res = blocks.insert(BlokcsMap::value_type(block, index));
                if (res.second) {
                    for (const auto& item : block) {
                        outfmt.output(static_cast<item_num_type>(item), 16);
                    }
                    block_index.push_back(index);
                    index++;
                } else {
                    // index of previously inserted block
                    block_index.push_back(res.first->second);
                }
            }
        }
        fout_cpp << "};\n\n";

        {   // Single level index
            const char* sztype = getUIntType(block_index);
            fout_h << "extern const " << sztype << " comp_block_index[];\n";
            fout_cpp << "const " << sztype << " comp_block_index[] = {";
            {
                OutputFmt outfmt(fout_cpp, 100);
                for (int index : block_index) {
                    outfmt.output(index, 10);
                }
            }
            fout_cpp << "};\n\n";
        }

        fout_h << "extern const codepoint_key_val comp_block_data[];\n";
        fout_cpp << "const codepoint_key_val comp_block_data[] = {";
        {
            OutputFmt outfmt(fout_cpp, 100);
            for (auto& item : all_comp_data) {
                std::string str{ '{' };
                unsigned_to_numstr(item.key, str, 16);
                str.push_back(',');
                unsigned_to_numstr(item.val, str, 16);
                str.push_back('}');
                outfmt.output(str);
            }
        }
        fout_cpp << "};\n\n";
    }

    output_newline(fout_h, fout_cpp);

    //=======================================================================
    // Decomposition data
    {
        // https://www.unicode.org/reports/tr44/#Character_Decomposition_Mappings
        // Starting from Unicode 2.1.9, the decomposition mappings in UnicodeData.txt can
        // be used to derive the full decomposition of any single character in canonical
        // order, without the need to separately apply the Canonical Ordering Algorithm.
        bool has_decomposable = true;
        while (has_decomposable) {
            has_decomposable = false;
            for (int cp = 0; cp <= MAX_CODE_POINT; ++cp) {
                std::u32string charsTo;
                for (auto ch : arr_decomp[cp].charsTo) {
                    if (arr_decomp[ch].charsTo.length() > 0) {
                        has_decomposable = true;
                        charsTo += arr_decomp[ch].charsTo;
                    } else
                        charsTo += ch;
                }
                arr_decomp[cp].charsTo = std::move(charsTo);
            }
        }

        // Generate Docomposition data

        std::u32string allCharsTo;
        for (int cp = 0; cp <= MAX_CODE_POINT; ++cp) {
            if (arr_decomp[cp].charsTo.length() > 0) {
                const std::size_t len = arr_decomp[cp].charsTo.length();
                std::size_t pos = allCharsTo.find(arr_decomp[cp].charsTo);
                if (pos == allCharsTo.npos) {
                    pos = allCharsTo.length();
                    allCharsTo.append(arr_decomp[cp].charsTo);
                }
                if (pos <= 0xFFF && len <= 0xF)
                    arr_decomp[cp].value = (static_cast<item_num_type>(len) << 12) | static_cast<item_num_type>(pos);
                else
                    std::cerr << "FATAL: Too long mapping string" << std::endl;
            }
        }

        // Output Data

        special_ranges<item_num_type> spec(arr_decomp);
        const std::size_t count_chars = spec.m_range[0].from;

        std::cout << "=== 16 bit BLOCK ===\n";
        block_info binf = find_block_size(arr_decomp, count_chars, sizeof(item_num_type));
        std::size_t block_size = binf.block_size;

        // total memory used
        std::cout << "block_size=" << block_size << "; mem: " << binf.total_mem() << "\n";
        std::cout << "allCharsTo size: " << allCharsTo.size() << "; mem: " << allCharsTo.size() * sizeof(allCharsTo[0]) << "\n";
        std::cout << "TOTAL MEM: " << binf.total_mem() + allCharsTo.size() * sizeof(allCharsTo[0]) << "\n";

        // Generate code

        const char* sz_item_num_type = getUIntType<item_num_type>();

        // Constants
        output_unsigned_constant(fout_h, "std::size_t", "decomp_block_shift", binf.size_shift, 10);
        output_unsigned_constant(fout_h, "std::uint32_t", "decomp_block_mask", binf.code_point_mask(), 16);
        output_unsigned_constant(fout_h, "std::uint32_t", "decomp_default_start", count_chars, 16);
        output_unsigned_constant(fout_h, sz_item_num_type, "decomp_default_value", arr_decomp[count_chars].value, 16);

        // Decomposition blocks
        std::vector<int> block_index;

        fout_h << "extern const " << sz_item_num_type << " decomp_block[];\n";
        fout_cpp << "const " << sz_item_num_type << " decomp_block[] = {";
        {
            OutputFmt outfmt(fout_cpp, 100);

            typedef std::map<array_view<decomp_item_type>, int> BlokcsMap;
            BlokcsMap blocks;
            int index = 0;
            for (std::size_t ind = 0; ind < count_chars; ind += block_size) {
                std::size_t chunk_size = std::min(block_size, arr_decomp.size() - ind);
                array_view<decomp_item_type> block(arr_decomp.data() + ind, chunk_size);

                auto res = blocks.insert(BlokcsMap::value_type(block, index));
                if (res.second) {
                    for (const auto& item : block) {
                        outfmt.output(static_cast<item_num_type>(item), 16);
                    }
                    block_index.push_back(index);
                    index++;
                } else {
                    // index of previously inserted block
                    block_index.push_back(res.first->second);
                }
            }
        }
        fout_cpp << "};\n\n";

        {   // Single level index
            const char* sztype = getUIntType(block_index);
            fout_h << "extern const " << sztype << " decomp_block_index[];\n";
            fout_cpp << "const " << sztype << " decomp_block_index[] = {";
            {
                OutputFmt outfmt(fout_cpp, 100);
                for (int index : block_index) {
                    outfmt.output(index, 10);
                }
            }
            fout_cpp << "};\n\n";
        }

        fout_h << "extern const char32_t decomp_block_data[];\n";
        fout_cpp << "const char32_t decomp_block_data[] = {";
        {
            OutputFmt outfmt(fout_cpp, 100);
            for (auto ch : allCharsTo) {
                outfmt.output(ch, 16);
            }
        }
        fout_cpp << "};\n\n";
    }
}
