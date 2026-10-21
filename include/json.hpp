// json.hpp — a tiny, self-contained JSON value type with a recursive-descent
// parser and a pretty-printing serializer.
//
// This is written from scratch for the AV Perception Evaluation Platform so the
// project has ZERO third-party dependencies. It is intentionally small: it
// supports the subset of JSON this project needs (objects, arrays, strings,
// numbers, booleans, null) with ordered object keys for deterministic output.
//
// Design notes:
//  * Object keys are stored in a std::map<std::string, Json> so serialization
//    is deterministic (sorted by key) regardless of insertion order.
//  * Numbers are stored as double; integers print without a trailing ".0".
//  * Parsing throws pev::json::ParseError with a byte offset on malformed input.
//
// Author: Aditya Kopalli, 2026. MIT licensed (see LICENSE).

#ifndef PEV_JSON_HPP
#define PEV_JSON_HPP

#include <cstdint>
#include <cstdio>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace pev {
namespace json {

// Thrown on malformed JSON during parsing.
class ParseError : public std::runtime_error {
public:
    explicit ParseError(const std::string& msg) : std::runtime_error(msg) {}
};

class Json {
public:
    enum class Type { Null, Bool, Number, String, Array, Object };

    using Array = std::vector<Json>;
    using Object = std::map<std::string, Json>;

    // --- Construction -------------------------------------------------------
    Json() = default;
    Json(std::nullptr_t) {}
    Json(bool b) : type_(Type::Bool), bool_(b) {}
    Json(int n) : type_(Type::Number), num_(static_cast<double>(n)) {}
    Json(long n) : type_(Type::Number), num_(static_cast<double>(n)) {}
    Json(long long n) : type_(Type::Number), num_(static_cast<double>(n)) {}
    Json(std::size_t n) : type_(Type::Number), num_(static_cast<double>(n)) {}
    Json(double d) : type_(Type::Number), num_(d) {}
    Json(const char* s) : type_(Type::String), str_(s) {}
    Json(std::string s) : type_(Type::String), str_(std::move(s)) {}

    static Json array() {
        Json j;
        j.type_ = Type::Array;
        return j;
    }
    static Json object() {
        Json j;
        j.type_ = Type::Object;
        return j;
    }

    // --- Type queries -------------------------------------------------------
    Type type() const { return type_; }
    bool is_null() const { return type_ == Type::Null; }
    bool is_bool() const { return type_ == Type::Bool; }
    bool is_number() const { return type_ == Type::Number; }
    bool is_string() const { return type_ == Type::String; }
    bool is_array() const { return type_ == Type::Array; }
    bool is_object() const { return type_ == Type::Object; }

    // --- Typed accessors (throw on type mismatch) ---------------------------
    bool as_bool() const {
        require(Type::Bool, "bool");
        return bool_;
    }
    double as_number() const {
        require(Type::Number, "number");
        return num_;
    }
    long long as_int() const {
        require(Type::Number, "number");
        return static_cast<long long>(num_);
    }
    const std::string& as_string() const {
        require(Type::String, "string");
        return str_;
    }
    const Array& as_array() const {
        require(Type::Array, "array");
        return arr_;
    }
    Array& as_array() {
        require(Type::Array, "array");
        return arr_;
    }
    const Object& as_object() const {
        require(Type::Object, "object");
        return obj_;
    }
    Object& as_object() {
        require(Type::Object, "object");
        return obj_;
    }

    // --- Object helpers -----------------------------------------------------
    bool contains(const std::string& key) const {
        return type_ == Type::Object && obj_.find(key) != obj_.end();
    }
    // Throws ParseError if the key is absent — handy for required fields.
    const Json& at(const std::string& key) const {
        require(Type::Object, "object");
        auto it = obj_.find(key);
        if (it == obj_.end()) {
            throw ParseError("missing required key: '" + key + "'");
        }
        return it->second;
    }
    // Mutating subscript: promotes a null value to an object on first use.
    Json& operator[](const std::string& key) {
        if (type_ == Type::Null) {
            type_ = Type::Object;
        }
        require(Type::Object, "object");
        return obj_[key];
    }

    // --- Array helpers ------------------------------------------------------
    void push_back(Json value) {
        if (type_ == Type::Null) {
            type_ = Type::Array;
        }
        require(Type::Array, "array");
        arr_.push_back(std::move(value));
    }
    std::size_t size() const {
        if (type_ == Type::Array) return arr_.size();
        if (type_ == Type::Object) return obj_.size();
        return 0;
    }

    // --- Serialization ------------------------------------------------------
    // Pretty-prints with a 2-space indent. Pass indent < 0 for compact output.
    std::string dump(int indent = 2) const {
        std::ostringstream os;
        write(os, indent, 0);
        return os.str();
    }

    // --- Parsing ------------------------------------------------------------
    static Json parse(const std::string& text) {
        Parser p(text);
        p.skip_ws();
        Json result = p.parse_value();
        p.skip_ws();
        if (!p.at_end()) {
            throw ParseError("trailing characters after JSON value at offset " +
                             std::to_string(p.pos()));
        }
        return result;
    }

private:
    Type type_ = Type::Null;
    bool bool_ = false;
    double num_ = 0.0;
    std::string str_;
    Array arr_;
    Object obj_;

    void require(Type t, const char* what) const {
        if (type_ != t) {
            throw ParseError(std::string("JSON type error: expected ") + what);
        }
    }

    static void write_indent(std::ostringstream& os, int indent, int depth) {
        if (indent < 0) return;
        os << '\n';
        for (int i = 0; i < indent * depth; ++i) os << ' ';
    }

    static void write_string(std::ostringstream& os, const std::string& s) {
        os << '"';
        for (char c : s) {
            switch (c) {
                case '"': os << "\\\""; break;
                case '\\': os << "\\\\"; break;
                case '\n': os << "\\n"; break;
                case '\t': os << "\\t"; break;
                case '\r': os << "\\r"; break;
                case '\b': os << "\\b"; break;
                case '\f': os << "\\f"; break;
                default:
                    if (static_cast<unsigned char>(c) < 0x20) {
                        char buf[8];
                        std::snprintf(buf, sizeof(buf), "\\u%04x",
                                      static_cast<unsigned>(
                                          static_cast<unsigned char>(c)));
                        os << buf;
                    } else {
                        os << c;
                    }
            }
        }
        os << '"';
    }

    static void write_number(std::ostringstream& os, double d) {
        // %.10g gives clean, shortest-ish output for the rounded values this
        // project generates (e.g. 0.87, 12.3, 5) and round-trips them.
        char buf[64];
        std::snprintf(buf, sizeof(buf), "%.10g", d);
        os << buf;
    }

    void write(std::ostringstream& os, int indent, int depth) const {
        switch (type_) {
            case Type::Null:
                os << "null";
                break;
            case Type::Bool:
                os << (bool_ ? "true" : "false");
                break;
            case Type::Number:
                write_number(os, num_);
                break;
            case Type::String:
                write_string(os, str_);
                break;
            case Type::Array: {
                if (arr_.empty()) {
                    os << "[]";
                    break;
                }
                os << '[';
                bool first = true;
                for (const Json& item : arr_) {
                    if (!first) os << ',';
                    first = false;
                    write_indent(os, indent, depth + 1);
                    item.write(os, indent, depth + 1);
                }
                write_indent(os, indent, depth);
                os << ']';
                break;
            }
            case Type::Object: {
                if (obj_.empty()) {
                    os << "{}";
                    break;
                }
                os << '{';
                bool first = true;
                for (const auto& kv : obj_) {
                    if (!first) os << ',';
                    first = false;
                    write_indent(os, indent, depth + 1);
                    write_string(os, kv.first);
                    os << (indent < 0 ? ":" : ": ");
                    kv.second.write(os, indent, depth + 1);
                }
                write_indent(os, indent, depth);
                os << '}';
                break;
            }
        }
    }

    // Recursive-descent parser over a borrowed string.
    class Parser {
    public:
        explicit Parser(const std::string& text) : text_(text) {}

        bool at_end() const { return pos_ >= text_.size(); }
        std::size_t pos() const { return pos_; }

        void skip_ws() {
            while (pos_ < text_.size()) {
                char c = text_[pos_];
                if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
                    ++pos_;
                } else {
                    break;
                }
            }
        }

        Json parse_value() {
            skip_ws();
            if (at_end()) throw ParseError("unexpected end of input");
            char c = text_[pos_];
            switch (c) {
                case '{': return parse_object();
                case '[': return parse_array();
                case '"': return Json(parse_string());
                case 't': case 'f': return parse_bool();
                case 'n': return parse_null();
                default: return parse_number();
            }
        }

    private:
        const std::string& text_;
        std::size_t pos_ = 0;

        [[noreturn]] void fail(const std::string& msg) {
            throw ParseError(msg + " at offset " + std::to_string(pos_));
        }

        char expect(char c) {
            if (at_end() || text_[pos_] != c) {
                fail(std::string("expected '") + c + "'");
            }
            return text_[pos_++];
        }

        void consume_literal(const char* lit) {
            for (const char* p = lit; *p; ++p) {
                if (at_end() || text_[pos_] != *p) {
                    fail(std::string("invalid literal, expected '") + lit + "'");
                }
                ++pos_;
            }
        }

        Json parse_null() {
            consume_literal("null");
            return Json(nullptr);
        }

        Json parse_bool() {
            if (text_[pos_] == 't') {
                consume_literal("true");
                return Json(true);
            }
            consume_literal("false");
            return Json(false);
        }

        Json parse_number() {
            std::size_t start = pos_;
            if (!at_end() && (text_[pos_] == '-' || text_[pos_] == '+')) ++pos_;
            bool any = false;
            while (!at_end()) {
                char c = text_[pos_];
                if ((c >= '0' && c <= '9') || c == '.' || c == 'e' ||
                    c == 'E' || c == '+' || c == '-') {
                    ++pos_;
                    any = true;
                } else {
                    break;
                }
            }
            if (!any) fail("invalid number");
            std::string token = text_.substr(start, pos_ - start);
            try {
                return Json(std::stod(token));
            } catch (const std::exception&) {
                fail("invalid number '" + token + "'");
            }
        }

        std::string parse_string() {
            expect('"');
            std::string out;
            while (!at_end()) {
                char c = text_[pos_++];
                if (c == '"') return out;
                if (c == '\\') {
                    if (at_end()) fail("unterminated escape");
                    char e = text_[pos_++];
                    switch (e) {
                        case '"': out.push_back('"'); break;
                        case '\\': out.push_back('\\'); break;
                        case '/': out.push_back('/'); break;
                        case 'n': out.push_back('\n'); break;
                        case 't': out.push_back('\t'); break;
                        case 'r': out.push_back('\r'); break;
                        case 'b': out.push_back('\b'); break;
                        case 'f': out.push_back('\f'); break;
                        case 'u': out += parse_unicode_escape(); break;
                        default: fail("invalid escape sequence");
                    }
                } else {
                    out.push_back(c);
                }
            }
            fail("unterminated string");
        }

        // Decodes a \uXXXX escape (basic multilingual plane) into UTF-8.
        std::string parse_unicode_escape() {
            if (pos_ + 4 > text_.size()) fail("truncated \\u escape");
            unsigned code = 0;
            for (int i = 0; i < 4; ++i) {
                char c = text_[pos_++];
                code <<= 4;
                if (c >= '0' && c <= '9') code |= static_cast<unsigned>(c - '0');
                else if (c >= 'a' && c <= 'f') code |= static_cast<unsigned>(c - 'a' + 10);
                else if (c >= 'A' && c <= 'F') code |= static_cast<unsigned>(c - 'A' + 10);
                else fail("invalid hex digit in \\u escape");
            }
            std::string out;
            if (code < 0x80) {
                out.push_back(static_cast<char>(code));
            } else if (code < 0x800) {
                out.push_back(static_cast<char>(0xC0 | (code >> 6)));
                out.push_back(static_cast<char>(0x80 | (code & 0x3F)));
            } else {
                out.push_back(static_cast<char>(0xE0 | (code >> 12)));
                out.push_back(static_cast<char>(0x80 | ((code >> 6) & 0x3F)));
                out.push_back(static_cast<char>(0x80 | (code & 0x3F)));
            }
            return out;
        }

        Json parse_array() {
            expect('[');
            Json arr = Json::array();
            skip_ws();
            if (!at_end() && text_[pos_] == ']') {
                ++pos_;
                return arr;
            }
            while (true) {
                arr.push_back(parse_value());
                skip_ws();
                if (at_end()) fail("unterminated array");
                char c = text_[pos_++];
                if (c == ']') break;
                if (c != ',') fail("expected ',' or ']' in array");
            }
            return arr;
        }

        Json parse_object() {
            expect('{');
            Json obj = Json::object();
            skip_ws();
            if (!at_end() && text_[pos_] == '}') {
                ++pos_;
                return obj;
            }
            while (true) {
                skip_ws();
                std::string key = parse_string();
                skip_ws();
                expect(':');
                obj[key] = parse_value();
                skip_ws();
                if (at_end()) fail("unterminated object");
                char c = text_[pos_++];
                if (c == '}') break;
                if (c != ',') fail("expected ',' or '}' in object");
            }
            return obj;
        }
    };
};

}  // namespace json
}  // namespace pev

#endif  // PEV_JSON_HPP
