/*
 * Copyright 2016 WebAssembly Community Group participants
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef WABT_LITERAL_H_
#define WABT_LITERAL_H_

#include <cstdint>

#include "wabt/common.h"

namespace wabt {

// These functions all return Result::Ok on success and Result::Error on
// failure.
//
// NOTE: the functions are written for use with wast-lexer, assuming that the
// literal has already matched the patterns defined there. As a result, the
// only validation that is done is for overflow, not for otherwise bogus input.

enum class LiteralType {
  Int,
  Float,
  Hexfloat,
  Infinity,
  Nan,
};

enum class ParseIntType {
  UnsignedOnly = 0,
  SignedAndUnsigned = 1,
};

/* Size of char buffer required to hold hex representation of a float/double */
#define WABT_MAX_FLOAT_HEX 20
#define WABT_MAX_DOUBLE_HEX 40

Result ParseHexdigit(char c, uint32_t* out);
Result ParseInt8(const char* s,
                 const char* end,
                 uint8_t* out,
                 ParseIntType parse_type);
Result ParseInt16(const char* s,
                  const char* end,
                  uint16_t* out,
                  ParseIntType parse_type);
Result ParseInt32(const char* s,
                  const char* end,
                  uint32_t* out,
                  ParseIntType parse_type);
Result ParseInt64(const char* s,
                  const char* end,
                  uint64_t* out,
                  ParseIntType parse_type);
Result ParseUint64(const char* s, const char* end, uint64_t* out);
Result ParseUint128(const char* s, const char* end, v128* out);
Result ParseFloat(LiteralType literal_type,
                  const char* s,
                  const char* end,
                  uint32_t* out_bits);
Result ParseDouble(LiteralType literal_type,
                   const char* s,
                   const char* end,
                   uint64_t* out_bits);

// Same as above but taking a string_view
inline Result ParseInt8(std::string_view v,
                        uint8_t* out,
                        ParseIntType parse_type) {
  return ParseInt8(v.data(), v.data() + v.size(), out, parse_type);
}

inline Result ParseInt16(std::string_view v,
                         uint16_t* out,
                         ParseIntType parse_type) {
  return ParseInt16(v.data(), v.data() + v.size(), out, parse_type);
}

inline Result ParseInt32(std::string_view v,
                         uint32_t* out,
                         ParseIntType parse_type) {
  return ParseInt32(v.data(), v.data() + v.size(), out, parse_type);
}

inline Result ParseInt64(std::string_view v,
                         uint64_t* out,
                         ParseIntType parse_type) {
  return ParseInt64(v.data(), v.data() + v.size(), out, parse_type);
}

inline Result ParseUint64(std::string_view v, uint64_t* out) {
  return ParseUint64(v.data(), v.data() + v.size(), out);
}

inline Result ParseUint128(std::string_view v, v128* out) {
  return ParseUint128(v.data(), v.data() + v.size(), out);
}

inline Result ParseFloat(LiteralType literal_type,
                         std::string_view v,
                         uint32_t* out_bits) {
  return ParseFloat(literal_type, v.data(), v.data() + v.size(), out_bits);
}

inline Result ParseDouble(LiteralType literal_type,
                          std::string_view v,
                          uint64_t* out_bits) {
  return ParseDouble(literal_type, v.data(), v.data() + v.size(), out_bits);
}

void WriteFloatHex(char* buffer, size_t size, uint32_t bits);
void WriteDoubleHex(char* buffer, size_t size, uint64_t bits);
void WriteUint128(char* buffer, size_t size, v128 bits);

}  // namespace wabt

#endif /* WABT_LITERAL_H_ */
