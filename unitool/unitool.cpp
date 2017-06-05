// unitool.cpp : Defines the entry point for the console application.
//
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>


class OutputFmt {
public:
    OutputFmt(std::ostream& fout, size_t max_line_len);
    void output(const std::string& code);
    ~OutputFmt();
protected:
    std::ostream& m_fout;
    bool m_first;
    size_t m_line_len;
    // options
    const size_t m_max_line_len;
    static const size_t m_indent = 2;
};

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

OutputFmt::OutputFmt(std::ostream& fout, size_t max_line_len)
    : m_fout(fout)
    , m_first(true)
    , m_line_len(0)
    , m_max_line_len(max_line_len)
{
    m_fout << "[" << std::endl;
}

void OutputFmt::output(const std::string& code) {
    if (m_first) {
        m_first = false;
        m_fout << std::setw(m_indent) << "";
        m_line_len = m_indent;
    } else if (m_line_len + code.length() > (m_max_line_len - 5)) {
        // 5 is ", 0x,".length
        m_fout << ",\n";
        m_fout << std::setw(m_indent) << "";
        m_line_len = m_indent;
    } else {
        m_fout << ", ";
        m_line_len += 2;
    }
    m_fout << "0x" << code;
    m_line_len += 2 + code.length();
}

OutputFmt::~OutputFmt() {
    m_fout << "]" << std::endl;
}

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
    std::string output;
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

                if (c2.find('M') != c2.npos)
                    outfmt.output(c0);

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

