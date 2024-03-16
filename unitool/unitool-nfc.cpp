//
#include "unicode_data_tools.h"

using namespace upa::tools;


static void output_newline(std::ostream& fout_h, std::ostream& fout_cpp);
static void make_ccc_table(const std::string& data_path, std::ostream& fout_h, std::ostream& fout_cpp);
static void make_decomposition_table(const std::string& data_path, std::ostream& fout_h, std::ostream& fout_cpp);

int main(int argc, char* argv[])
{
    if (argc != 2) {
        std::cerr <<
            "unitool-nfc <data directory path>\n"
            "\n"
            "Specify the directory path where the DerivedCombiningClass.txt, UnicodeData.txt\n"
            "files are located.\n";
        return 1;
    }

    // Append '/' to data_path
    std::string data_path{ argv[1] };
    if (data_path.length() > 0) {
        char c = data_path[data_path.length() - 1];
        if (c != '\\' && c != '/') data_path += '/';
    }

    // Output files
    auto file_name = data_path + "GEN-nfc-tables.h.txt";
    std::ofstream fout_h(file_name, std::ios_base::out);
    if (!fout_h.is_open()) {
        std::cerr << "Can't open destination file: " << file_name << std::endl;
        return 2;
    }

    file_name = data_path + "GEN-nfc-tables.cpp.txt";
    std::ofstream fout_cpp(file_name, std::ios_base::out);
    if (!fout_cpp.is_open()) {
        std::cerr << "Can't open destination file: " << file_name << std::endl;
        return 2;
    }

    make_ccc_table(data_path, fout_h, fout_cpp);
    output_newline(fout_h, fout_cpp);
    make_decomposition_table(data_path, fout_h, fout_cpp);

    return 0;
}

static void output_newline(std::ostream& fout_h, std::ostream& fout_cpp)
{
    fout_h << std::endl;
    fout_cpp << std::endl;
}

// ==================================================================
// Canonical_Combining_Class (ccc)

static void make_ccc_table(const std::string& data_path, std::ostream& fout_h, std::ostream& fout_cpp)
{
    using item_type = std::uint8_t;
    using item_num_type = item_type;

    const int MAX_CODE_POINT = 0x10FFFF;

    std::vector<item_type> arr_ccc(MAX_CODE_POINT + 1);

    std::string file_name = data_path + "DerivedCombiningClass.txt";
    parse_UnicodeData<1>(file_name,
        [&](int cp0, int cp1, const auto& col) {
            const std::uint8_t value = std::stoi(col[0]);
            for (int cp = cp0; cp <= cp1; cp++)
                arr_ccc[cp] = value;
        });

    // Special ranges to reduce table size
    special_ranges<item_num_type> spec_ccc(arr_ccc, 0);
    const size_t count_chars = spec_ccc.m_range[0].from; // MAX_CODE_POINT + 1;
    const int index_levels = 1; // 1 arba 2

    // Find block size
    const auto binf = find_block_size(arr_ccc, count_chars, sizeof(item_num_type), index_levels);
    const size_t block_size = binf.block_size;

    // memory used
    std::cout << "block_size=" << block_size << "; mem=" << binf.total_mem() << "\n";

    //=======================================================================
    // Generate code

    const char* sz_item_num_type = getUIntType<item_num_type>();

    // Constants
    output_unsigned_constant(fout_h, "size_t", "ccc_block_shift", binf.size_shift, 10);
    output_unsigned_constant(fout_h, "std::uint32_t", "ccc_block_mask", binf.code_point_mask(), 16);
    output_unsigned_constant(fout_h, "std::uint32_t", "ccc_default_start", count_chars, 16);
    output_unsigned_constant(fout_h, "std::uint32_t", "ccc_default_value", arr_ccc[count_chars], 16);

    // CCC blocks
    std::vector<int> block_index;

    fout_h << "extern const " << sz_item_num_type << " ccc_block[];\n";
    fout_cpp << "const " << sz_item_num_type << " ccc_block[] = {";
    {
        OutputFmt outfmt(fout_cpp, 100);

        typedef std::map<array_view<item_type>, int> BlokcsMap;
        BlokcsMap blocks;
        int index = 0;
        for (size_t ind = 0; ind < count_chars; ind += block_size) {
            size_t chunk_size = std::min(block_size, arr_ccc.size() - ind);
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
// Canonical Decomposition

static void make_decomposition_table(const std::string& data_path, std::ostream& fout_h, std::ostream& fout_cpp)
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

    struct decomp_item_type : public item_type {
        std::u32string charsTo;
    };

    constexpr int MAX_CODE_POINT = 0x10FFFF;

    std::vector<decomp_item_type> arr_decomp(MAX_CODE_POINT + 1);

    std::string file_name = data_path + "UnicodeData.txt";
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

    // Set values
    int stats[5] = {};
    std::u32string allCharsTo;
    for (int cp = 0; cp <= MAX_CODE_POINT; ++cp) {
        ++stats[arr_decomp[cp].charsTo.length()];

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

    //=======================================================================
    // Output Data

    special_ranges<item_num_type> spec(arr_decomp);
    const size_t count_chars = spec.m_range[0].from;

    std::cout << "=== 16 bit BLOCK ===\n";
    block_info binf = find_block_size(arr_decomp, count_chars, sizeof(item_num_type));
    size_t block_size = binf.block_size;

    // total memory used
    std::cout << "block_size=" << block_size << "; mem: " << binf.total_mem() << "\n";
    std::cout << "allCharsTo size: " << allCharsTo.size() << "; mem: " << allCharsTo.size() * sizeof(allCharsTo[0]) << "\n";
    std::cout << "TOTAL MEM: " << binf.total_mem() + allCharsTo.size() * sizeof(allCharsTo[0]) << "\n";

    //=======================================================================
    // Generate code

    const char* sz_item_num_type = getUIntType<item_num_type>();

    // Constants
    output_unsigned_constant(fout_h, "size_t", "decomp_block_shift", binf.size_shift, 10);
    output_unsigned_constant(fout_h, "std::uint32_t", "decomp_block_mask", binf.code_point_mask(), 16);
    output_unsigned_constant(fout_h, "std::uint32_t", "decomp_default_start", count_chars, 16);
    output_unsigned_constant(fout_h, "std::uint32_t", "decomp_default_value", arr_decomp[count_chars].value, 16);

    // Decomposition blocks
    std::vector<int> block_index;

    fout_h << "extern const " << sz_item_num_type << " decomp_block[];\n";
    fout_cpp << "const " << sz_item_num_type << " decomp_block[] = {";
    {
        OutputFmt outfmt(fout_cpp, 100);

        typedef std::map<array_view<decomp_item_type>, int> BlokcsMap;
        BlokcsMap blocks;
        int index = 0;
        for (size_t ind = 0; ind < count_chars; ind += block_size) {
            size_t chunk_size = std::min(block_size, arr_decomp.size() - ind);
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
