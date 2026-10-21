// io_util.hpp — small filesystem helpers shared across the harness.
//
// Author: Aditya Kopalli, 2026. MIT licensed.

#ifndef PEV_IO_UTIL_HPP
#define PEV_IO_UTIL_HPP

#include <stdexcept>
#include <string>

namespace pev {

// Thrown for I/O and dataset-structure problems (missing files, etc.).
class IoError : public std::runtime_error {
public:
    explicit IoError(const std::string& msg) : std::runtime_error(msg) {}
};

// Reads an entire file into a string. Throws IoError if it cannot be opened.
std::string read_text_file(const std::string& path);

// Writes text to a file, creating parent directories as needed.
void write_text_file(const std::string& path, const std::string& content);

// Creates a directory (and parents) if it does not already exist.
void ensure_directory(const std::string& path);

}  // namespace pev

#endif  // PEV_IO_UTIL_HPP
