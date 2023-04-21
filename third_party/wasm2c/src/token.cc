/*
 * Copyright 2017 WebAssembly Community Group participants
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

#include "wabt/token.h"

namespace wabt {

const char* GetTokenTypeName(TokenType token_type) {
  static const char* s_names[] = {
#define WABT_TOKEN(name, string) string,
#define WABT_TOKEN_FIRST(name, string)
#define WABT_TOKEN_LAST(name, string)
#include "wabt/token.def"
#undef WABT_TOKEN
#undef WABT_TOKEN_FIRST
#undef WABT_TOKEN_LAST
  };

  static_assert(
      WABT_ARRAY_SIZE(s_names) == WABT_ENUM_COUNT(TokenType),
      "Expected TokenType names list length to match number of TokenTypes.");

  int x = static_cast<int>(token_type);
  if (x < WABT_ENUM_COUNT(TokenType)) {
    return s_names[x];
  }

  return "Invalid";
}

Token::Token(Location loc, TokenType token_type)
    : loc(loc), token_type_(token_type) {
  assert(IsTokenTypeBare(token_type_));
}

Token::Token(Location loc, TokenType token_type, Type type)
    : loc(loc), token_type_(token_type) {
  assert(HasType());
  Construct(type_, type);
}

Token::Token(Location loc, TokenType token_type, std::string_view text)
    : loc(loc), token_type_(token_type) {
  assert(HasText());
  Construct(text_, text);
}

Token::Token(Location loc, TokenType token_type, Opcode opcode)
    : loc(loc), token_type_(token_type) {
  assert(HasOpcode());
  Construct(opcode_, opcode);
}

Token::Token(Location loc, TokenType token_type, const Literal& literal)
    : loc(loc), token_type_(token_type) {
  assert(HasLiteral());
  Construct(literal_, literal);
}

std::string Token::to_string() const {
  if (IsTokenTypeBare(token_type_)) {
    return GetTokenTypeName(token_type_);
  } else if (HasLiteral()) {
    return std::string(literal_.text);
  } else if (HasOpcode()) {
    return opcode_.GetName();
  } else if (HasText()) {
    return std::string(text_);
  } else if (IsTokenTypeRefKind(token_type_)) {
    return type_.GetRefKindName();
  } else {
    assert(HasType());
    return type_.GetName();
  }
}

std::string Token::to_string_clamp(size_t max_length) const {
  std::string s = to_string();
  if (s.length() > max_length) {
    return s.substr(0, max_length - 3) + "...";
  } else {
    return s;
  }
}

}  // namespace wabt
