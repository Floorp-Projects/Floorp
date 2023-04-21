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

#include "wabt/wast-lexer.h"

#include <cassert>
#include <cstdio>

#include "wabt/config.h"

#include "wabt/lexer-source.h"

#define ERROR(...) Error(GetLocation(), __VA_ARGS__)

namespace wabt {

namespace {

#include "prebuilt/lexer-keywords.cc"

}  // namespace

WastLexer::WastLexer(std::unique_ptr<LexerSource> source,
                     std::string_view filename,
                     Errors* errors)
    : source_(std::move(source)),
      filename_(filename),
      line_(1),
      buffer_(static_cast<const char*>(source_->data())),
      buffer_end_(buffer_ + source_->size()),
      line_start_(buffer_),
      token_start_(buffer_),
      cursor_(buffer_),
      errors_(errors) {}

// static
std::unique_ptr<WastLexer> WastLexer::CreateBufferLexer(
    std::string_view filename,
    const void* data,
    size_t size,
    Errors* errors) {
  return std::make_unique<WastLexer>(std::make_unique<LexerSource>(data, size),
                                     filename, errors);
}

Token WastLexer::GetToken() {
  while (true) {
    token_start_ = cursor_;
    switch (PeekChar()) {
      case kEof:
        return BareToken(TokenType::Eof);

      case '(':
        if (MatchString("(;")) {
          if (ReadBlockComment()) {
            continue;
          }
          return BareToken(TokenType::Eof);
        } else if (MatchString("(@")) {
          GetIdChars();
          // offset=2 to skip the "(@" prefix
          return TextToken(TokenType::LparAnn, 2);
        } else {
          ReadChar();
          return BareToken(TokenType::Lpar);
        }
        break;

      case ')':
        ReadChar();
        return BareToken(TokenType::Rpar);

      case ';':
        if (MatchString(";;")) {
          if (ReadLineComment()) {
            continue;
          }
          return BareToken(TokenType::Eof);
        } else {
          ReadChar();
          ERROR("unexpected char");
          continue;
        }
        break;

      case ' ':
      case '\t':
      case '\r':
      case '\n':
        ReadWhitespace();
        continue;

      case '"':
        return GetStringToken();

      case '+':
      case '-':
        ReadChar();
        switch (PeekChar()) {
          case 'i':
            return GetInfToken();

          case 'n':
            return GetNanToken();

          case '0':
            return MatchString("0x") ? GetHexNumberToken(TokenType::Int)
                                     : GetNumberToken(TokenType::Int);
          case '1':
          case '2':
          case '3':
          case '4':
          case '5':
          case '6':
          case '7':
          case '8':
          case '9':
            return GetNumberToken(TokenType::Int);

          default:
            return GetReservedToken();
        }
        break;

      case '0':
        return MatchString("0x") ? GetHexNumberToken(TokenType::Nat)
                                 : GetNumberToken(TokenType::Nat);

      case '1':
      case '2':
      case '3':
      case '4':
      case '5':
      case '6':
      case '7':
      case '8':
      case '9':
        return GetNumberToken(TokenType::Nat);

      case '$':
        return GetIdChars();  // Initial $ is idchar, so this produces id token

      case 'a':
        return GetNameEqNumToken("align=", TokenType::AlignEqNat);

      case 'i':
        return GetInfToken();

      case 'n':
        return GetNanToken();

      case 'o':
        return GetNameEqNumToken("offset=", TokenType::OffsetEqNat);

      default:
        if (IsKeyword(PeekChar())) {
          return GetKeywordToken();
        } else if (IsIdChar(PeekChar())) {
          return GetReservedToken();
        } else {
          ReadChar();
          ERROR("unexpected char");
          continue;
        }
    }
  }
}

Location WastLexer::GetLocation() {
  auto column = [=](const char* p) {
    return std::max(1, static_cast<int>(p - line_start_ + 1));
  };
  return Location(filename_, line_, column(token_start_), column(cursor_));
}

std::string_view WastLexer::GetText(size_t offset) {
  // Bounds checks are necessary because token_start may have been moved
  // (e.g. if GetStringToken found a newline and reset token_start to
  // point at it).

  if (token_start_ + offset >= buffer_end_)
    return {};

  if (cursor_ <= token_start_ + offset)
    return {};

  return std::string_view(token_start_ + offset,
                          (cursor_ - token_start_) - offset);
}

Token WastLexer::BareToken(TokenType token_type) {
  return Token(GetLocation(), token_type);
}

Token WastLexer::LiteralToken(TokenType token_type, LiteralType literal_type) {
  return Token(GetLocation(), token_type, Literal(literal_type, GetText()));
}

Token WastLexer::TextToken(TokenType token_type, size_t offset) {
  return Token(GetLocation(), token_type, GetText(offset));
}

int WastLexer::PeekChar() {
  return cursor_ < buffer_end_ ? static_cast<uint8_t>(*cursor_) : kEof;
}

int WastLexer::ReadChar() {
  return cursor_ < buffer_end_ ? static_cast<uint8_t>(*cursor_++) : kEof;
}

bool WastLexer::MatchChar(char c) {
  if (PeekChar() == c) {
    ReadChar();
    return true;
  }
  return false;
}

bool WastLexer::MatchString(std::string_view s) {
  const char* saved_cursor = cursor_;
  for (char c : s) {
    if (ReadChar() != c) {
      cursor_ = saved_cursor;
      return false;
    }
  }
  return true;
}

void WastLexer::Newline() {
  line_++;
  line_start_ = cursor_;
}

bool WastLexer::ReadBlockComment() {
  int nesting = 1;
  while (true) {
    switch (ReadChar()) {
      case kEof:
        ERROR("EOF in block comment");
        return false;

      case ';':
        if (MatchChar(')') && --nesting == 0) {
          return true;
        }
        break;

      case '(':
        if (MatchChar(';')) {
          nesting++;
        }
        break;

      case '\n':
        Newline();
        break;
    }
  }
}

bool WastLexer::ReadLineComment() {
  while (true) {
    switch (ReadChar()) {
      case kEof:
        return false;

      case '\n':
        Newline();
        return true;
    }
  }
}

void WastLexer::ReadWhitespace() {
  while (true) {
    switch (PeekChar()) {
      case ' ':
      case '\t':
      case '\r':
        ReadChar();
        break;

      case '\n':
        ReadChar();
        Newline();
        break;

      default:
        return;
    }
  }
}

Token WastLexer::GetStringToken() {
  const char* saved_token_start = token_start_;
  bool has_error = false;
  bool in_string = true;
  ReadChar();
  while (in_string) {
    switch (ReadChar()) {
      case kEof:
        return BareToken(TokenType::Eof);

      case '\n':
        token_start_ = cursor_ - 1;
        ERROR("newline in string");
        has_error = true;
        Newline();
        continue;

      case '"':
        if (PeekChar() == '"') {
          ERROR("invalid string token");
          has_error = true;
        }
        in_string = false;
        break;

      case '\\': {
        switch (ReadChar()) {
          case 't':
          case 'n':
          case 'r':
          case '"':
          case '\'':
          case '\\':
            // Valid escape.
            break;

          case '0':
          case '1':
          case '2':
          case '3':
          case '4':
          case '5':
          case '6':
          case '7':
          case '8':
          case '9':
          case 'a':
          case 'b':
          case 'c':
          case 'd':
          case 'e':
          case 'f':
          case 'A':
          case 'B':
          case 'C':
          case 'D':
          case 'E':
          case 'F':  // Hex byte escape.
            if (IsHexDigit(PeekChar())) {
              ReadChar();
            } else {
              token_start_ = cursor_ - 2;
              goto error;
            }
            break;

          case 'u': {
            token_start_ = cursor_ - 2;
            if (ReadChar() != '{') {
              goto error;
            }

            // Value must be a valid unicode scalar value.
            uint32_t digit;
            uint32_t scalar_value = 0;

            while (IsHexDigit(PeekChar())) {
              ParseHexdigit(*cursor_++, &digit);

              scalar_value = (scalar_value << 4) | digit;
              // Maximum value of a unicode code point.
              if (scalar_value >= 0x110000) {
                goto error;
              }
            }

            if (PeekChar() != '}') {
              goto error;
            }

            // Scalars between 0xd800 and 0xdfff are not allowed.
            if ((scalar_value >= 0xd800 && scalar_value < 0xe000) ||
                token_start_ == cursor_ - 3) {
              ReadChar();
              goto error;
            }
            break;
          }

          default:
            token_start_ = cursor_ - 2;
            goto error;

          error:
            ERROR("bad escape \"%.*s\"",
                  static_cast<int>(cursor_ - token_start_), token_start_);
            has_error = true;
            break;
        }
        break;
      }
    }
  }
  token_start_ = saved_token_start;
  if (has_error) {
    return Token(GetLocation(), TokenType::Invalid);
  }

  return TextToken(TokenType::Text);
}

// static
bool WastLexer::IsCharClass(int c, CharClass bit) {
  // Generated by the following python script:
  //
  //   def Range(c, lo, hi): return lo <= c <= hi
  //   def IsDigit(c): return Range(c, '0', '9')
  //   def IsHexDigit(c): return IsDigit(c) or Range(c.lower(), 'a', 'f')
  //   def IsKeyword(c): return Range(c, 'a', 'z')
  //   def IsIdChar(c): return Range(c, '!', '~') and c not in '"(),;[]{}'
  //
  //   print ([0] + [
  //       (8 if IsDigit(c) else 0) |
  //       (4 if IsHexDigit(c) else 0) |
  //       (2 if IsKeyword(c) else 0) |
  //       (1 if IsIdChar(c) else 0)
  //       for c in map(chr, range(0, 127))
  //   ])
  static const char kCharClasses[257] = {
      0,  0,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  0,  0,  0,  0,  0,  0,  0,
      0,  0,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  0,  0,  0,  1,  0,  1,  1,
      1,  1,  1, 0, 0, 1, 1, 0, 1, 1, 1, 13, 13, 13, 13, 13, 13, 13, 13,
      13, 13, 1, 0, 1, 1, 1, 1, 1, 5, 5, 5,  5,  5,  5,  1,  1,  1,  1,
      1,  1,  1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  1,  1,  1,  1,  0,  1,  0,
      1,  1,  1, 7, 7, 7, 7, 7, 7, 3, 3, 3,  3,  3,  3,  3,  3,  3,  3,
      3,  3,  3, 3, 3, 3, 3, 3, 3, 3, 0, 1,  0,  1,
  };

  assert(c >= -1 && c < 256);
  return (kCharClasses[c + 1] & static_cast<int>(bit)) != 0;
}

bool WastLexer::ReadNum() {
  if (IsDigit(PeekChar())) {
    ReadChar();
    return MatchChar('_') || IsDigit(PeekChar()) ? ReadNum() : true;
  }
  return false;
}

bool WastLexer::ReadHexNum() {
  if (IsHexDigit(PeekChar())) {
    ReadChar();
    return MatchChar('_') || IsHexDigit(PeekChar()) ? ReadHexNum() : true;
  }
  return false;
}

WastLexer::ReservedChars WastLexer::ReadReservedChars() {
  ReservedChars ret{ReservedChars::None};
  while (true) {
    auto peek = PeekChar();
    if (IsIdChar(peek)) {
      ReadChar();
      if (ret == ReservedChars::None) {
        ret = ReservedChars::Id;
      }
    } else if (peek == '"') {
      GetStringToken();
      ret = ReservedChars::Some;
    } else {
      break;
    }
  }
  return ret;
}

void WastLexer::ReadSign() {
  if (PeekChar() == '+' || PeekChar() == '-') {
    ReadChar();
  }
}

Token WastLexer::GetNumberToken(TokenType token_type) {
  if (ReadNum()) {
    if (MatchChar('.')) {
      token_type = TokenType::Float;
      if (IsDigit(PeekChar()) && !ReadNum()) {
        return GetReservedToken();
      }
    }
    if (MatchChar('e') || MatchChar('E')) {
      token_type = TokenType::Float;
      ReadSign();
      if (!ReadNum()) {
        return GetReservedToken();
      }
    }
    if (NoTrailingReservedChars()) {
      if (token_type == TokenType::Float) {
        return LiteralToken(token_type, LiteralType::Float);
      } else {
        return LiteralToken(token_type, LiteralType::Int);
      }
    }
  }
  return GetReservedToken();
}

Token WastLexer::GetHexNumberToken(TokenType token_type) {
  if (ReadHexNum()) {
    if (MatchChar('.')) {
      token_type = TokenType::Float;
      if (IsHexDigit(PeekChar()) && !ReadHexNum()) {
        return GetReservedToken();
      }
    }
    if (MatchChar('p') || MatchChar('P')) {
      token_type = TokenType::Float;
      ReadSign();
      if (!ReadNum()) {
        return GetReservedToken();
      }
    }
    if (NoTrailingReservedChars()) {
      if (token_type == TokenType::Float) {
        return LiteralToken(token_type, LiteralType::Hexfloat);
      } else {
        return LiteralToken(token_type, LiteralType::Int);
      }
    }
  }
  return GetReservedToken();
}

Token WastLexer::GetInfToken() {
  if (MatchString("inf")) {
    if (NoTrailingReservedChars()) {
      return LiteralToken(TokenType::Float, LiteralType::Infinity);
    }
    return GetReservedToken();
  }
  return GetKeywordToken();
}

Token WastLexer::GetNanToken() {
  if (MatchString("nan")) {
    if (MatchChar(':')) {
      if (MatchString("0x") && ReadHexNum() && NoTrailingReservedChars()) {
        return LiteralToken(TokenType::Float, LiteralType::Nan);
      }
    } else if (NoTrailingReservedChars()) {
      return LiteralToken(TokenType::Float, LiteralType::Nan);
    }
  }
  return GetKeywordToken();
}

Token WastLexer::GetNameEqNumToken(std::string_view name,
                                   TokenType token_type) {
  if (MatchString(name)) {
    if (MatchString("0x")) {
      if (ReadHexNum() && NoTrailingReservedChars()) {
        return TextToken(token_type, name.size());
      }
    } else if (ReadNum() && NoTrailingReservedChars()) {
      return TextToken(token_type, name.size());
    }
  }
  return GetKeywordToken();
}

Token WastLexer::GetIdChars() {
  if (ReadReservedChars() == ReservedChars::Id) {
    return TextToken(TokenType::Var);
  }

  return TextToken(TokenType::Reserved);
}

Token WastLexer::GetKeywordToken() {
  ReadReservedChars();
  TokenInfo* info =
      Perfect_Hash::InWordSet(token_start_, cursor_ - token_start_);
  if (!info) {
    return TextToken(TokenType::Reserved);
  }
  if (IsTokenTypeBare(info->token_type)) {
    return BareToken(info->token_type);
  } else if (IsTokenTypeType(info->token_type) ||
             IsTokenTypeRefKind(info->token_type)) {
    return Token(GetLocation(), info->token_type, info->value_type);
  } else {
    assert(IsTokenTypeOpcode(info->token_type));
    return Token(GetLocation(), info->token_type, info->opcode);
  }
}

Token WastLexer::GetReservedToken() {
  ReadReservedChars();
  return TextToken(TokenType::Reserved);
}

void WastLexer::Error(Location loc, const char* format, ...) {
  WABT_SNPRINTF_ALLOCA(buffer, length, format);
  errors_->emplace_back(ErrorLevel::Error, loc, buffer);
}

}  // namespace wabt
