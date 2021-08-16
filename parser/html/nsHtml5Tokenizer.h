/*
 * Copyright (c) 2005-2007 Henri Sivonen
 * Copyright (c) 2007-2017 Mozilla Foundation
 * Portions of comments Copyright 2004-2010 Apple Computer, Inc., Mozilla
 * Foundation, and Opera Software ASA.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

/*
 * THIS IS A GENERATED FILE. PLEASE DO NOT EDIT.
 * Please edit Tokenizer.java instead and regenerate.
 */

#ifndef nsHtml5Tokenizer_h
#define nsHtml5Tokenizer_h

#include "jArray.h"
#include "nsAHtml5TreeBuilderState.h"
#include "nsAtom.h"
#include "nsGkAtoms.h"
#include "nsHtml5ArrayCopy.h"
#include "nsHtml5AtomTable.h"
#include "nsHtml5DocumentMode.h"
#include "nsHtml5Highlighter.h"
#include "nsHtml5Macros.h"
#include "nsHtml5NamedCharacters.h"
#include "nsHtml5NamedCharactersAccel.h"
#include "nsHtml5String.h"
#include "nsHtml5TokenizerLoopPolicies.h"
#include "nsIContent.h"
#include "nsTraceRefcnt.h"

class nsHtml5StreamParser;

class nsHtml5AttributeName;
class nsHtml5ElementName;
class nsHtml5TreeBuilder;
class nsHtml5MetaScanner;
class nsHtml5UTF16Buffer;
class nsHtml5StateSnapshot;
class nsHtml5Portability;

class nsHtml5Tokenizer {
 private:
  static const int32_t DATA_AND_RCDATA_MASK = ~1;

 public:
  static const int32_t DATA = 0;

  static const int32_t RCDATA = 1;

  static const int32_t SCRIPT_DATA = 2;

  static const int32_t RAWTEXT = 3;

  static const int32_t SCRIPT_DATA_ESCAPED = 4;

  static const int32_t ATTRIBUTE_VALUE_DOUBLE_QUOTED = 5;

  static const int32_t ATTRIBUTE_VALUE_SINGLE_QUOTED = 6;

  static const int32_t ATTRIBUTE_VALUE_UNQUOTED = 7;

  static const int32_t PLAINTEXT = 8;

  static const int32_t TAG_OPEN = 9;

  static const int32_t CLOSE_TAG_OPEN = 10;

  static const int32_t TAG_NAME = 11;

  static const int32_t BEFORE_ATTRIBUTE_NAME = 12;

  static const int32_t ATTRIBUTE_NAME = 13;

  static const int32_t AFTER_ATTRIBUTE_NAME = 14;

  static const int32_t BEFORE_ATTRIBUTE_VALUE = 15;

  static const int32_t AFTER_ATTRIBUTE_VALUE_QUOTED = 16;

  static const int32_t BOGUS_COMMENT = 17;

  static const int32_t MARKUP_DECLARATION_OPEN = 18;

  static const int32_t DOCTYPE = 19;

  static const int32_t BEFORE_DOCTYPE_NAME = 20;

  static const int32_t DOCTYPE_NAME = 21;

  static const int32_t AFTER_DOCTYPE_NAME = 22;

  static const int32_t BEFORE_DOCTYPE_PUBLIC_IDENTIFIER = 23;

  static const int32_t DOCTYPE_PUBLIC_IDENTIFIER_DOUBLE_QUOTED = 24;

  static const int32_t DOCTYPE_PUBLIC_IDENTIFIER_SINGLE_QUOTED = 25;

  static const int32_t AFTER_DOCTYPE_PUBLIC_IDENTIFIER = 26;

  static const int32_t BEFORE_DOCTYPE_SYSTEM_IDENTIFIER = 27;

  static const int32_t DOCTYPE_SYSTEM_IDENTIFIER_DOUBLE_QUOTED = 28;

  static const int32_t DOCTYPE_SYSTEM_IDENTIFIER_SINGLE_QUOTED = 29;

  static const int32_t AFTER_DOCTYPE_SYSTEM_IDENTIFIER = 30;

  static const int32_t BOGUS_DOCTYPE = 31;

  static const int32_t COMMENT_START = 32;

  static const int32_t COMMENT_START_DASH = 33;

  static const int32_t COMMENT = 34;

  static const int32_t COMMENT_END_DASH = 35;

  static const int32_t COMMENT_END = 36;

  static const int32_t COMMENT_END_BANG = 37;

  static const int32_t NON_DATA_END_TAG_NAME = 38;

  static const int32_t MARKUP_DECLARATION_HYPHEN = 39;

  static const int32_t MARKUP_DECLARATION_OCTYPE = 40;

  static const int32_t DOCTYPE_UBLIC = 41;

  static const int32_t DOCTYPE_YSTEM = 42;

  static const int32_t AFTER_DOCTYPE_PUBLIC_KEYWORD = 43;

  static const int32_t BETWEEN_DOCTYPE_PUBLIC_AND_SYSTEM_IDENTIFIERS = 44;

  static const int32_t AFTER_DOCTYPE_SYSTEM_KEYWORD = 45;

  static const int32_t CONSUME_CHARACTER_REFERENCE = 46;

  static const int32_t CONSUME_NCR = 47;

  static const int32_t CHARACTER_REFERENCE_TAIL = 48;

  static const int32_t HEX_NCR_LOOP = 49;

  static const int32_t DECIMAL_NRC_LOOP = 50;

  static const int32_t HANDLE_NCR_VALUE = 51;

  static const int32_t HANDLE_NCR_VALUE_RECONSUME = 52;

  static const int32_t CHARACTER_REFERENCE_HILO_LOOKUP = 53;

  static const int32_t SELF_CLOSING_START_TAG = 54;

  static const int32_t CDATA_START = 55;

  static const int32_t CDATA_SECTION = 56;

  static const int32_t CDATA_RSQB = 57;

  static const int32_t CDATA_RSQB_RSQB = 58;

  static const int32_t SCRIPT_DATA_LESS_THAN_SIGN = 59;

  static const int32_t SCRIPT_DATA_ESCAPE_START = 60;

  static const int32_t SCRIPT_DATA_ESCAPE_START_DASH = 61;

  static const int32_t SCRIPT_DATA_ESCAPED_DASH = 62;

  static const int32_t SCRIPT_DATA_ESCAPED_DASH_DASH = 63;

  static const int32_t BOGUS_COMMENT_HYPHEN = 64;

  static const int32_t RAWTEXT_RCDATA_LESS_THAN_SIGN = 65;

  static const int32_t SCRIPT_DATA_ESCAPED_LESS_THAN_SIGN = 66;

  static const int32_t SCRIPT_DATA_DOUBLE_ESCAPE_START = 67;

  static const int32_t SCRIPT_DATA_DOUBLE_ESCAPED = 68;

  static const int32_t SCRIPT_DATA_DOUBLE_ESCAPED_LESS_THAN_SIGN = 69;

  static const int32_t SCRIPT_DATA_DOUBLE_ESCAPED_DASH = 70;

  static const int32_t SCRIPT_DATA_DOUBLE_ESCAPED_DASH_DASH = 71;

  static const int32_t SCRIPT_DATA_DOUBLE_ESCAPE_END = 72;

  static const int32_t PROCESSING_INSTRUCTION = 73;

  static const int32_t PROCESSING_INSTRUCTION_QUESTION_MARK = 74;

  static const int32_t COMMENT_LESSTHAN = 76;

  static const int32_t COMMENT_LESSTHAN_BANG = 77;

  static const int32_t COMMENT_LESSTHAN_BANG_DASH = 78;

  static const int32_t COMMENT_LESSTHAN_BANG_DASH_DASH = 79;

 private:
  static const int32_t LEAD_OFFSET = (0xD800 - (0x10000 >> 10));

  static char16_t LT_GT[];
  static char16_t LT_SOLIDUS[];
  static char16_t RSQB_RSQB[];
  static char16_t REPLACEMENT_CHARACTER[];
  static char16_t LF[];
  static char16_t CDATA_LSQB[];
  static char16_t OCTYPE[];
  static char16_t UBLIC[];
  static char16_t YSTEM[];
  static staticJArray<char16_t, int32_t> TITLE_ARR;
  static staticJArray<char16_t, int32_t> SCRIPT_ARR;
  static staticJArray<char16_t, int32_t> STYLE_ARR;
  static staticJArray<char16_t, int32_t> PLAINTEXT_ARR;
  static staticJArray<char16_t, int32_t> XMP_ARR;
  static staticJArray<char16_t, int32_t> TEXTAREA_ARR;
  static staticJArray<char16_t, int32_t> IFRAME_ARR;
  static staticJArray<char16_t, int32_t> NOEMBED_ARR;
  static staticJArray<char16_t, int32_t> NOSCRIPT_ARR;
  static staticJArray<char16_t, int32_t> NOFRAMES_ARR;

 protected:
  nsHtml5TreeBuilder* tokenHandler;
  nsHtml5StreamParser* encodingDeclarationHandler;
  bool lastCR;
  int32_t stateSave;

 private:
  int32_t returnStateSave;

 protected:
  int32_t index;

 private:
  bool forceQuirks;
  char16_t additional;
  int32_t entCol;
  int32_t firstCharKey;
  int32_t lo;
  int32_t hi;
  int32_t candidate;
  int32_t charRefBufMark;

 protected:
  int32_t value;

 private:
  bool seenDigits;

 protected:
  int32_t cstart;

 private:
  nsHtml5String publicId;
  nsHtml5String systemId;
  autoJArray<char16_t, int32_t> strBuf;
  int32_t strBufLen;
  autoJArray<char16_t, int32_t> charRefBuf;
  int32_t charRefBufLen;
  autoJArray<char16_t, int32_t> bmpChar;
  autoJArray<char16_t, int32_t> astralChar;

 protected:
  nsHtml5ElementName* endTagExpectation;

 private:
  jArray<char16_t, int32_t> endTagExpectationAsArray;

 protected:
  bool endTag;

 private:
  bool containsHyphen;
  nsHtml5ElementName* tagName;
  nsHtml5ElementName* nonInternedTagName;

 protected:
  nsHtml5AttributeName* attributeName;

 private:
  nsHtml5AttributeName* nonInternedAttributeName;
  RefPtr<nsAtom> doctypeName;
  nsHtml5String publicIdentifier;
  nsHtml5String systemIdentifier;
  nsHtml5HtmlAttributes* attributes;
  bool newAttributesEachTime;
  bool shouldSuspend;

 protected:
  bool confident;

 private:
  int32_t line;
  int32_t attributeLine;
  nsHtml5AtomTable* interner;
  bool viewingXmlSource;

 public:
  nsHtml5Tokenizer(nsHtml5TreeBuilder* tokenHandler, bool viewingXmlSource);
  void setInterner(nsHtml5AtomTable* interner);
  void initLocation(nsHtml5String newPublicId, nsHtml5String newSystemId);
  bool isViewingXmlSource();
  void setState(int32_t specialTokenizerState);
  void setStateAndEndTagExpectation(int32_t specialTokenizerState,
                                    nsHtml5ElementName* endTagExpectation);

 private:
  void endTagExpectationToArray();

 public:
  void setLineNumber(int32_t line);
  inline int32_t getLineNumber() { return line; }

  nsHtml5HtmlAttributes* emptyAttributes();

 private:
  inline void appendCharRefBuf(char16_t c) {
    MOZ_RELEASE_ASSERT(charRefBufLen < charRefBuf.length,
                       "Attempted to overrun charRefBuf!");
    charRefBuf[charRefBufLen++] = c;
  }

  void emitOrAppendCharRefBuf(int32_t returnState);
  inline void clearStrBufAfterUse() { strBufLen = 0; }

  inline void clearStrBufBeforeUse() {
    MOZ_ASSERT(!strBufLen, "strBufLen not reset after previous use!");
    strBufLen = 0;
  }

  inline void clearStrBufAfterOneHyphen() {
    MOZ_ASSERT(strBufLen == 1, "strBufLen length not one!");
    MOZ_ASSERT(strBuf[0] == '-', "strBuf does not start with a hyphen!");
    strBufLen = 0;
  }

  inline void appendStrBuf(char16_t c) {
    MOZ_ASSERT(strBufLen < strBuf.length,
               "Previous buffer length insufficient.");
    if (MOZ_UNLIKELY(strBufLen == strBuf.length)) {
      if (MOZ_UNLIKELY(!EnsureBufferSpace(1))) {
        MOZ_CRASH("Unable to recover from buffer reallocation failure");
      }
    }
    strBuf[strBufLen++] = c;
  }

 protected:
  nsHtml5String strBufToString();

 private:
  void strBufToDoctypeName();
  void emitStrBuf();
  inline void appendSecondHyphenToBogusComment() { appendStrBuf('-'); }

  inline void adjustDoubleHyphenAndAppendToStrBufAndErr(
      char16_t c, bool reportedConsecutiveHyphens) {
    appendStrBuf(c);
  }

  void appendStrBuf(char16_t* buffer, int32_t offset, int32_t length);
  inline void appendCharRefBufToStrBuf() {
    appendStrBuf(charRefBuf, 0, charRefBufLen);
    charRefBufLen = 0;
  }

  void emitComment(int32_t provisionalHyphens, int32_t pos);

 protected:
  void flushChars(char16_t* buf, int32_t pos);

 private:
  void strBufToElementNameString();
  int32_t emitCurrentTagToken(bool selfClosing, int32_t pos);
  void attributeNameComplete();
  void addAttributeWithoutValue();
  void addAttributeWithValue();

 public:
  void start();
  bool tokenizeBuffer(nsHtml5UTF16Buffer* buffer);

 private:
  template <class P>
  int32_t stateLoop(int32_t state, char16_t c, int32_t pos, char16_t* buf,
                    bool reconsume, int32_t returnState, int32_t endPos);
  void initDoctypeFields();
  inline void adjustDoubleHyphenAndAppendToStrBufCarriageReturn() {
    silentCarriageReturn();
    adjustDoubleHyphenAndAppendToStrBufAndErr('\n', false);
  }

  inline void adjustDoubleHyphenAndAppendToStrBufLineFeed() {
    silentLineFeed();
    adjustDoubleHyphenAndAppendToStrBufAndErr('\n', false);
  }

  inline void appendStrBufLineFeed() {
    silentLineFeed();
    appendStrBuf('\n');
  }

  inline void appendStrBufCarriageReturn() {
    silentCarriageReturn();
    appendStrBuf('\n');
  }

 protected:
  inline void silentCarriageReturn() {
    ++line;
    lastCR = true;
  }

  inline void silentLineFeed() { ++line; }

 private:
  void emitCarriageReturn(char16_t* buf, int32_t pos);
  void emitReplacementCharacter(char16_t* buf, int32_t pos);
  void emitPlaintextReplacementCharacter(char16_t* buf, int32_t pos);
  void setAdditionalAndRememberAmpersandLocation(char16_t add);
  void bogusDoctype();
  void bogusDoctypeWithoutQuirks();
  void handleNcrValue(int32_t returnState);

 public:
  void eof();

 private:
  void emitDoctypeToken(int32_t pos);

 protected:
  inline char16_t checkChar(char16_t* buf, int32_t pos) { return buf[pos]; }

 public:
  bool internalEncodingDeclaration(nsHtml5String internalCharset);

 private:
  void emitOrAppendTwo(const char16_t* val, int32_t returnState);
  void emitOrAppendOne(const char16_t* val, int32_t returnState);

 public:
  void end();
  void requestSuspension();
  bool isInDataState();
  void resetToDataState();
  void loadState(nsHtml5Tokenizer* other);
  void initializeWithoutStarting();
  void setEncodingDeclarationHandler(
      nsHtml5StreamParser* encodingDeclarationHandler);
  ~nsHtml5Tokenizer();
  static void initializeStatics();
  static void releaseStatics();

#include "nsHtml5TokenizerHSupplement.h"
};

#endif
