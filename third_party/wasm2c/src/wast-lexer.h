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

#ifndef WABT_WAST_LEXER_H_
#define WABT_WAST_LEXER_H_

#include <cstddef>
#include <cstdio>
#include <memory>

#include "src/common.h"
#include "src/lexer-source-line-finder.h"
#include "src/literal.h"
#include "src/make-unique.h"
#include "src/opcode.h"
#include "src/token.h"

namespace wabt {

class ErrorHandler;
class LexerSource;
class WastParser;

class WastLexer {
 public:
  WABT_DISALLOW_COPY_AND_ASSIGN(WastLexer);

  WastLexer(std::unique_ptr<LexerSource> source, string_view filename);

  // Convenience functions.
  static std::unique_ptr<WastLexer> CreateBufferLexer(string_view filename,
                                                      const void* data,
                                                      size_t size);

  Token GetToken(WastParser* parser);

  // TODO(binji): Move this out of the lexer.
  std::unique_ptr<LexerSourceLineFinder> MakeLineFinder() {
    return MakeUnique<LexerSourceLineFinder>(source_->Clone());
  }

 private:
  static const int kEof = -1;
  enum class CharClass { Reserved = 1, Keyword = 2, HexDigit = 4, Digit = 8 };

  Location GetLocation();
  string_view GetText(size_t offset = 0);

  Token BareToken(TokenType);
  Token LiteralToken(TokenType, LiteralType);
  Token TextToken(TokenType, size_t offset = 0);

  int PeekChar();
  int ReadChar();
  bool MatchChar(char);
  bool MatchString(string_view);
  void Newline();
  bool ReadBlockComment(WastParser*);  // Returns false if EOF.
  bool ReadLineComment();              // Returns false if EOF.
  void ReadWhitespace();

  static bool IsCharClass(int c, CharClass);
  static bool IsDigit(int c) { return IsCharClass(c, CharClass::Digit); }
  static bool IsHexDigit(int c) { return IsCharClass(c, CharClass::HexDigit); }
  static bool IsKeyword(int c) { return IsCharClass(c, CharClass::Keyword); }
  static bool IsReserved(int c) { return IsCharClass(c, CharClass::Reserved); }

  bool ReadNum();
  bool ReadHexNum();
  int ReadReservedChars();
  bool NoTrailingReservedChars() { return ReadReservedChars() == 0; }
  void ReadSign();
  Token GetStringToken(WastParser*);
  Token GetNumberToken(TokenType);
  Token GetHexNumberToken(TokenType);
  Token GetInfToken();
  Token GetNanToken();
  Token GetNameEqNumToken(string_view name, TokenType);
  Token GetIdToken();
  Token GetKeywordToken();
  Token GetReservedToken();

  std::unique_ptr<LexerSource> source_;
  std::string filename_;
  int line_;
  const char* buffer_;
  const char* buffer_end_;
  const char* line_start_;
  const char* token_start_;
  const char* cursor_;
};

}  // namespace wabt

#endif /* WABT_WAST_LEXER_H_ */
