#include "io_util.hpp"

#include <filesystem>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;

namespace pev {

std::string read_text_file(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        throw IoError("cannot open file for reading: " + path);
    }
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

void ensure_directory(const std::string& path) {
    if (path.empty()) {
        return;
    }
    std::error_code ec;
    fs::create_directories(path, ec);
    if (ec) {
        throw IoError("cannot create directory '" + path + "': " + ec.message());
    }
}

void write_text_file(const std::string& path, const std::string& content) {
    const fs::path p(path);
    if (p.has_parent_path()) {
        ensure_directory(p.parent_path().string());
    }
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) {
        throw IoError("cannot open file for writing: " + path);
    }
    out << content;
    if (!out) {
        throw IoError("error while writing file: " + path);
    }
}

}  // namespace pev
