/* SPDX-FileCopyrightText: 2026 N.Leiten
 * SPDX-License-Identifier: LicenseRef-KW-Modloader
 */
#ifndef KWMOD_BUNDLE_FORMAT_HPP
#define KWMOD_BUNDLE_FORMAT_HPP
#include <array>
#include <cstdint>
#include <fstream>
#include <limits>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

namespace kwmod_bundle
{
inline void require(bool value, const std::string &message)
{
    if (!value)
        throw std::runtime_error(message);
}
inline void append_utf8(std::string &out, uint32_t cp)
{
    require(cp <= 0x10ffff && !(cp >= 0xd800 && cp <= 0xdfff), "invalid Unicode scalar");
    if (cp < 0x80)
        out += char(cp);
    else if (cp < 0x800) {
        out += char(0xc0 | (cp >> 6));
        out += char(0x80 | (cp & 63));
    } else if (cp < 0x10000) {
        out += char(0xe0 | (cp >> 12));
        out += char(0x80 | ((cp >> 6) & 63));
        out += char(0x80 | (cp & 63));
    } else {
        out += char(0xf0 | (cp >> 18));
        out += char(0x80 | ((cp >> 12) & 63));
        out += char(0x80 | ((cp >> 6) & 63));
        out += char(0x80 | (cp & 63));
    }
}
inline std::vector<uint32_t> decode_utf8(const std::string &value)
{
    std::vector<uint32_t> out;
    for (size_t i = 0; i < value.size();) {
        uint32_t c = static_cast<unsigned char>(value[i++]);
        unsigned remaining = 0;
        uint32_t minimum = 0;
        if (c >= 0xc2 && c <= 0xdf) {
            remaining = 1;
            minimum = 0x80;
            c &= 31;
        } else if (c >= 0xe0 && c <= 0xef) {
            remaining = 2;
            minimum = 0x800;
            c &= 15;
        } else if (c >= 0xf0 && c <= 0xf4) {
            remaining = 3;
            minimum = 0x10000;
            c &= 7;
        } else
            require(c < 0x80, "invalid UTF-8 input");
        require(remaining <= value.size() - i, "truncated UTF-8 input");
        for (unsigned j = 0; j < remaining; ++j) {
            const unsigned byte = static_cast<unsigned char>(value[i++]);
            require((byte & 0xc0) == 0x80, "invalid UTF-8 continuation");
            c = (c << 6) | (byte & 63);
        }
        require(c >= minimum && c <= 0x10ffff && !(c >= 0xd800 && c <= 0xdfff), "invalid UTF-8 scalar");
        out.push_back(c);
    }
    return out;
}
struct Json {
    enum class Kind { Object, Array, String, Number, Literal } kind = Kind::Literal;
    std::map<std::string, Json> object;
    std::vector<Json> array;
    std::string text;
    uint64_t number = 0;
    const Json &at(const std::string &key) const
    {
        require(kind == Kind::Object && object.count(key), "missing inventory field: " + key);
        return object.at(key);
    }
    const std::string &string() const
    {
        require(kind == Kind::String, "inventory string expected");
        return text;
    }
    uint64_t integer() const
    {
        require(kind == Kind::Number, "inventory integer expected");
        return number;
    }
};
class JsonReader
{
    const std::string &data_;
    size_t position_ = 0;
    void whitespace()
    {
        while (position_ < data_.size() &&
               (data_[position_] == ' ' || data_[position_] == '\n' || data_[position_] == '\r' || data_[position_] == '\t'))
            ++position_;
    }
    char take()
    {
        require(position_ < data_.size(), "truncated inventory JSON");
        return data_[position_++];
    }
    uint32_t hex4()
    {
        uint32_t value = 0;
        for (unsigned i = 0; i < 4; ++i) {
            const char c = take();
            const unsigned digit = c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : c >= 'A' && c <= 'F' ? c - 'A' + 10 : 16;
            require(digit < 16, "invalid JSON Unicode escape");
            value = value * 16 + digit;
        }
        return value;
    }
    std::string string()
    {
        require(take() == '"', "JSON string expected");
        std::string out;
        for (;;) {
            const char c = take();
            if (c == '"')
                break;
            require(static_cast<unsigned char>(c) >= 32, "unescaped JSON control byte");
            if (c != '\\') {
                out += c;
                continue;
            }
            const char escaped = take();
            if (escaped == '"' || escaped == '\\' || escaped == '/')
                out += escaped;
            else if (escaped == 'b')
                out += '\b';
            else if (escaped == 'f')
                out += '\f';
            else if (escaped == 'n')
                out += '\n';
            else if (escaped == 'r')
                out += '\r';
            else if (escaped == 't')
                out += '\t';
            else {
                require(escaped == 'u', "unknown JSON escape");
                auto cp = hex4();
                if (cp >= 0xd800 && cp <= 0xdbff) {
                    require(take() == '\\' && take() == 'u', "missing JSON low surrogate");
                    const auto low = hex4();
                    require(low >= 0xdc00 && low <= 0xdfff, "invalid JSON low surrogate");
                    cp = 0x10000 + ((cp - 0xd800) << 10) + low - 0xdc00;
                }
                append_utf8(out, cp);
            }
        }
        (void)decode_utf8(out);
        return out;
    }
    Json value(unsigned depth)
    {
        require(depth < 64, "inventory JSON nesting limit");
        whitespace();
        require(position_ < data_.size(), "missing JSON value");
        Json out;
        const char c = data_[position_];
        if (c == '{' || c == '[') {
            ++position_;
            out.kind = c == '{' ? Json::Kind::Object : Json::Kind::Array;
            const char closing = c == '{' ? '}' : ']';
            whitespace();
            if (position_ < data_.size() && data_[position_] == closing) {
                ++position_;
                return out;
            }
            for (;;) {
                whitespace();
                if (c == '{') {
                    const auto key = string();
                    whitespace();
                    require(take() == ':', "JSON colon expected");
                    require(out.object.emplace(key, value(depth + 1)).second, "duplicate inventory JSON key");
                } else
                    out.array.push_back(value(depth + 1));
                whitespace();
                const char separator = take();
                if (separator == closing)
                    break;
                require(separator == ',', "JSON comma expected");
            }
        } else if (c == '"') {
            out.kind = Json::Kind::String;
            out.text = string();
        } else if (c >= '0' && c <= '9') {
            out.kind = Json::Kind::Number;
            const size_t begin = position_;
            while (position_ < data_.size() && data_[position_] >= '0' && data_[position_] <= '9') {
                const unsigned digit = data_[position_++] - '0';
                require(out.number <= (std::numeric_limits<uint64_t>::max() - digit) / 10, "inventory integer overflow");
                out.number = out.number * 10 + digit;
            }
            require(position_ - begin == 1 || c != '0', "JSON leading zero");
        } else {
            bool matched = false;
            for (const auto *literal : {"true", "false", "null"}) {
                const std::string word(literal);
                if (data_.compare(position_, word.size(), word) == 0) {
                    position_ += word.size();
                    matched = true;
                    break;
                }
            }
            require(matched, "unsupported inventory JSON value");
        }
        return out;
    }

  public:
    explicit JsonReader(const std::string &data) : data_(data) {}
    Json parse()
    {
        auto out = value(0);
        whitespace();
        require(position_ == data_.size(), "trailing inventory JSON data");
        return out;
    }
};
inline std::string quote(const std::string &value)
{
    static constexpr char hex[] = "0123456789abcdef";
    (void)decode_utf8(value);
    std::string out = "\"";
    for (const unsigned char c : value) {
        if (c == '"' || c == '\\') {
            out += '\\';
            out += char(c);
        } else if (c < 32) {
            out += "\\u00";
            out += hex[c >> 4];
            out += hex[c & 15];
        } else
            out += char(c);
    }
    return out + '"';
}
class Sha256
{
    std::array<uint32_t, 8> state_{{0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19}};
    std::array<unsigned char, 64> block_{};
    size_t used_ = 0;
    uint64_t bytes_ = 0;
    static uint32_t rotate(uint32_t x, unsigned n)
    {
        return (x >> n) | (x << (32 - n));
    }
    void transform()
    {
        static constexpr uint32_t k[64] = {
            0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5, 0xd807aa98, 0x12835b01, 0x243185be,
            0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174, 0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa,
            0x5cb0a9dc, 0x76f988da, 0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967, 0x27b70a85,
            0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85, 0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3,
            0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070, 0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f,
            0x682e6ff3, 0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};
        uint32_t w[64]{};
        for (unsigned i = 0; i < 16; ++i)
            for (unsigned j = 0; j < 4; ++j)
                w[i] = (w[i] << 8) | block_[4 * i + j];
        for (unsigned i = 16; i < 64; ++i) {
            const auto a = w[i - 15], b = w[i - 2];
            w[i] = w[i - 16] + (rotate(a, 7) ^ rotate(a, 18) ^ (a >> 3)) + w[i - 7] + (rotate(b, 17) ^ rotate(b, 19) ^ (b >> 10));
        }
        auto a = state_[0], b = state_[1], c = state_[2], d = state_[3], e = state_[4], f = state_[5], g = state_[6], h = state_[7];
        for (unsigned i = 0; i < 64; ++i) {
            const auto t1 = h + (rotate(e, 6) ^ rotate(e, 11) ^ rotate(e, 25)) + ((e & f) ^ (~e & g)) + k[i] + w[i];
            const auto t2 = (rotate(a, 2) ^ rotate(a, 13) ^ rotate(a, 22)) + ((a & b) ^ (a & c) ^ (b & c));
            h = g;
            g = f;
            f = e;
            e = d + t1;
            d = c;
            c = b;
            b = a;
            a = t1 + t2;
        }
        state_[0] += a;
        state_[1] += b;
        state_[2] += c;
        state_[3] += d;
        state_[4] += e;
        state_[5] += f;
        state_[6] += g;
        state_[7] += h;
    }

  public:
    void update(const char *data, size_t size)
    {
        require(size <= (std::numeric_limits<uint64_t>::max() / 8) - bytes_, "SHA256 input too large");
        bytes_ += size;
        for (size_t i = 0; i < size; ++i) {
            block_[used_++] = static_cast<unsigned char>(data[i]);
            if (used_ == block_.size()) {
                transform();
                used_ = 0;
            }
        }
    }
    std::string finish()
    {
        const auto bits = bytes_ * 8;
        block_[used_++] = 0x80;
        if (used_ > 56) {
            while (used_ < 64)
                block_[used_++] = 0;
            transform();
            used_ = 0;
        }
        while (used_ < 56)
            block_[used_++] = 0;
        for (unsigned i = 0; i < 8; ++i)
            block_[56 + i] = static_cast<unsigned char>(bits >> (56 - 8 * i));
        transform();
        static constexpr char hex[] = "0123456789abcdef";
        std::string out;
        for (auto word : state_)
            for (int shift = 28; shift >= 0; shift -= 4)
                out += hex[(word >> shift) & 15];
        return out;
    }
};
} // namespace kwmod_bundle
#endif
