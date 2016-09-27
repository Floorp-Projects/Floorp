/*
 * Copyright (c) 2005-2007 Henri Sivonen
 * Copyright (c) 2007-2015 Mozilla Foundation
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

#include "nsIAtom.h"
#include "nsHtml5AtomTable.h"
#include "nsString.h"
#include "nsIContent.h"
#include "nsTraceRefcnt.h"
#include "jArray.h"
#include "nsHtml5DocumentMode.h"
#include "nsHtml5ArrayCopy.h"
#include "nsHtml5NamedCharacters.h"
#include "nsHtml5NamedCharactersAccel.h"
#include "nsHtml5Atoms.h"
#include "nsAHtml5TreeBuilderState.h"
#include "nsHtml5Macros.h"
#include "nsHtml5Highlighter.h"
#include "nsHtml5TokenizerLoopPolicies.h"

class nsHtml5StreamParser;

class nsHtml5TreeBuilder;
class nsHtml5MetaScanner;
class nsHtml5AttributeName;
class nsHtml5ElementName;
class nsHtml5HtmlAttributes;
class nsHtml5UTF16Buffer;
class nsHtml5StateSnapshot;
class nsHtml5Portability;


class nsHtml5Tokenizer
{
  private:
    static char16_t LT_GT[];
    static char16_t LT_SOLIDUS[];
    static char16_t RSQB_RSQB[];
    static char16_t REPLACEMENT_CHARACTER[];
    static char16_t LF[];
    static char16_t CDATA_LSQB[];
    static char16_t OCTYPE[];
    static char16_t UBLIC[];
    static char16_t YSTEM[];
    static staticJArray<char16_t,int32_t> TITLE_ARR;
    static staticJArray<char16_t,int32_t> SCRIPT_ARR;
    static staticJArray<char16_t,int32_t> STYLE_ARR;
    static staticJArray<char16_t,int32_t> PLAINTEXT_ARR;
    static staticJArray<char16_t,int32_t> XMP_ARR;
    static staticJArray<char16_t,int32_t> TEXTAREA_ARR;
    static staticJArray<char16_t,int32_t> IFRAME_ARR;
    static staticJArray<char16_t,int32_t> NOEMBED_ARR;
    static staticJArray<char16_t,int32_t> NOSCRIPT_ARR;
    static staticJArray<char16_t,int32_t> NOFRAMES_ARR;
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
    nsString* publicId;
    nsString* systemId;
    autoJArray<char16_t,int32_t> strBuf;
    int32_t strBufLen;
    autoJArray<char16_t,int32_t> charRefBuf;
    int32_t charRefBufLen;
    autoJArray<char16_t,int32_t> bmpChar;
    autoJArray<char16_t,int32_t> astralChar;
  protected:
    nsHtml5ElementName* endTagExpectation;
  private:
    jArray<char16_t,int32_t> endTagExpectationAsArray;
  protected:
    bool endTag;
  private:
    nsHtml5ElementName* tagName;
  protected:
    nsHtml5AttributeName* attributeName;
  private:
    nsIAtom* doctypeName;
    nsString* publicIdentifier;
    nsString* systemIdentifier;
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
    void initLocation(nsString* newPublicId, nsString* newSystemId);
    bool isViewingXmlSource();
    void setStateAndEndTagExpectation(int32_t specialTokenizerState, nsIAtom* endTagExpectation);
    void setStateAndEndTagExpectation(int32_t specialTokenizerState, nsHtml5ElementName* endTagExpectation);
  private:
    void endTagExpectationToArray();
  public:
    void setLineNumber(int32_t line);
    inline int32_t getLineNumber()
    {
      return line;
    }

    nsHtml5HtmlAttributes* emptyAttributes();
  private:
    inline void appendCharRefBuf(char16_t c)
    {
      MOZ_RELEASE_ASSERT(charRefBufLen < charRefBuf.length, "Attempted to overrun charRefBuf!");
      charRefBuf[charRefBufLen++] = c;
    }

    void emitOrAppendCharRefBuf(int32_t returnState);
    inline void clearStrBufAfterUse()
    {
      strBufLen = 0;
    }

    inline void clearStrBufBeforeUse()
    {
      MOZ_ASSERT(!strBufLen, "strBufLen not reset after previous use!");
      strBufLen = 0;
    }

    inline void clearStrBufAfterOneHyphen()
    {
      MOZ_ASSERT(strBufLen == 1, "strBufLen length not one!");
      MOZ_ASSERT(strBuf[0] == '-', "strBuf does not start with a hyphen!");
      strBufLen = 0;
    }

    inline void appendStrBuf(char16_t c)
    {
      MOZ_ASSERT(strBufLen < strBuf.length, "Previous buffer length insufficient.");
      if (MOZ_UNLIKELY(strBufLen == strBuf.length)) {
        if (MOZ_UNLIKELY(!EnsureBufferSpace(1))) {
          MOZ_CRASH("Unable to recover from buffer reallocation failure");
        }
      }
      strBuf[strBufLen++] = c;
    }

  protected:
    nsString* strBufToString();
  private:
    void strBufToDoctypeName();
    void emitStrBuf();
    inline void appendSecondHyphenToBogusComment()
    {
      appendStrBuf('-');
    }

    inline void adjustDoubleHyphenAndAppendToStrBufAndErr(char16_t c)
    {
      errConsecutiveHyphens();
      appendStrBuf(c);
    }

    void appendStrBuf(char16_t* buffer, int32_t offset, int32_t length);
    inline void appendCharRefBufToStrBuf()
    {
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
    template<class P> int32_t stateLoop(int32_t state, char16_t c, int32_t pos, char16_t* buf, bool reconsume, int32_t returnState, int32_t endPos);
    void initDoctypeFields();
    inline void adjustDoubleHyphenAndAppendToStrBufCarriageReturn()
    {
      silentCarriageReturn();
      adjustDoubleHyphenAndAppendToStrBufAndErr('\n');
    }

    inline void adjustDoubleHyphenAndAppendToStrBufLineFeed()
    {
      silentLineFeed();
      adjustDoubleHyphenAndAppendToStrBufAndErr('\n');
    }

    inline void appendStrBufLineFeed()
    {
      silentLineFeed();
      appendStrBuf('\n');
    }

    inline void appendStrBufCarriageReturn()
    {
      silentCarriageReturn();
      appendStrBuf('\n');
    }

  protected:
    inline void silentCarriageReturn()
    {
      ++line;
      lastCR = true;
    }

    inline void silentLineFeed()
    {
      ++line;
    }

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
    inline char16_t checkChar(char16_t* buf, int32_t pos)
    {
      return buf[pos];
    }

  public:
    bool internalEncodingDeclaration(nsString* internalCharset);
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
    void setEncodingDeclarationHandler(nsHtml5StreamParser* encodingDeclarationHandler);
    ~nsHtml5Tokenizer();
    static void initializeStatics();
    static void releaseStatics();

#include "nsHtml5TokenizerHSupplement.h"
};

#define NS_HTML5TOKENIZER_DATA_AND_RCDATA_MASK ~1
#define NS_HTML5TOKENIZER_DATA 0
#define NS_HTML5TOKENIZER_RCDATA 1
#define NS_HTML5TOKENIZER_SCRIPT_DATA 2
#define NS_HTML5TOKENIZER_RAWTEXT 3
#define NS_HTML5TOKENIZER_SCRIPT_DATA_ESCAPED 4
#define NS_HTML5TOKENIZER_ATTRIBUTE_VALUE_DOUBLE_QUOTED 5
#define NS_HTML5TOKENIZER_ATTRIBUTE_VALUE_SINGLE_QUOTED 6
#define NS_HTML5TOKENIZER_ATTRIBUTE_VALUE_UNQUOTED 7
#define NS_HTML5TOKENIZER_PLAINTEXT 8
#define NS_HTML5TOKENIZER_TAG_OPEN 9
#define NS_HTML5TOKENIZER_CLOSE_TAG_OPEN 10
#define NS_HTML5TOKENIZER_TAG_NAME 11
#define NS_HTML5TOKENIZER_BEFORE_ATTRIBUTE_NAME 12
#define NS_HTML5TOKENIZER_ATTRIBUTE_NAME 13
#define NS_HTML5TOKENIZER_AFTER_ATTRIBUTE_NAME 14
#define NS_HTML5TOKENIZER_BEFORE_ATTRIBUTE_VALUE 15
#define NS_HTML5TOKENIZER_AFTER_ATTRIBUTE_VALUE_QUOTED 16
#define NS_HTML5TOKENIZER_BOGUS_COMMENT 17
#define NS_HTML5TOKENIZER_MARKUP_DECLARATION_OPEN 18
#define NS_HTML5TOKENIZER_DOCTYPE 19
#define NS_HTML5TOKENIZER_BEFORE_DOCTYPE_NAME 20
#define NS_HTML5TOKENIZER_DOCTYPE_NAME 21
#define NS_HTML5TOKENIZER_AFTER_DOCTYPE_NAME 22
#define NS_HTML5TOKENIZER_BEFORE_DOCTYPE_PUBLIC_IDENTIFIER 23
#define NS_HTML5TOKENIZER_DOCTYPE_PUBLIC_IDENTIFIER_DOUBLE_QUOTED 24
#define NS_HTML5TOKENIZER_DOCTYPE_PUBLIC_IDENTIFIER_SINGLE_QUOTED 25
#define NS_HTML5TOKENIZER_AFTER_DOCTYPE_PUBLIC_IDENTIFIER 26
#define NS_HTML5TOKENIZER_BEFORE_DOCTYPE_SYSTEM_IDENTIFIER 27
#define NS_HTML5TOKENIZER_DOCTYPE_SYSTEM_IDENTIFIER_DOUBLE_QUOTED 28
#define NS_HTML5TOKENIZER_DOCTYPE_SYSTEM_IDENTIFIER_SINGLE_QUOTED 29
#define NS_HTML5TOKENIZER_AFTER_DOCTYPE_SYSTEM_IDENTIFIER 30
#define NS_HTML5TOKENIZER_BOGUS_DOCTYPE 31
#define NS_HTML5TOKENIZER_COMMENT_START 32
#define NS_HTML5TOKENIZER_COMMENT_START_DASH 33
#define NS_HTML5TOKENIZER_COMMENT 34
#define NS_HTML5TOKENIZER_COMMENT_END_DASH 35
#define NS_HTML5TOKENIZER_COMMENT_END 36
#define NS_HTML5TOKENIZER_COMMENT_END_BANG 37
#define NS_HTML5TOKENIZER_NON_DATA_END_TAG_NAME 38
#define NS_HTML5TOKENIZER_MARKUP_DECLARATION_HYPHEN 39
#define NS_HTML5TOKENIZER_MARKUP_DECLARATION_OCTYPE 40
#define NS_HTML5TOKENIZER_DOCTYPE_UBLIC 41
#define NS_HTML5TOKENIZER_DOCTYPE_YSTEM 42
#define NS_HTML5TOKENIZER_AFTER_DOCTYPE_PUBLIC_KEYWORD 43
#define NS_HTML5TOKENIZER_BETWEEN_DOCTYPE_PUBLIC_AND_SYSTEM_IDENTIFIERS 44
#define NS_HTML5TOKENIZER_AFTER_DOCTYPE_SYSTEM_KEYWORD 45
#define NS_HTML5TOKENIZER_CONSUME_CHARACTER_REFERENCE 46
#define NS_HTML5TOKENIZER_CONSUME_NCR 47
#define NS_HTML5TOKENIZER_CHARACTER_REFERENCE_TAIL 48
#define NS_HTML5TOKENIZER_HEX_NCR_LOOP 49
#define NS_HTML5TOKENIZER_DECIMAL_NRC_LOOP 50
#define NS_HTML5TOKENIZER_HANDLE_NCR_VALUE 51
#define NS_HTML5TOKENIZER_HANDLE_NCR_VALUE_RECONSUME 52
#define NS_HTML5TOKENIZER_CHARACTER_REFERENCE_HILO_LOOKUP 53
#define NS_HTML5TOKENIZER_SELF_CLOSING_START_TAG 54
#define NS_HTML5TOKENIZER_CDATA_START 55
#define NS_HTML5TOKENIZER_CDATA_SECTION 56
#define NS_HTML5TOKENIZER_CDATA_RSQB 57
#define NS_HTML5TOKENIZER_CDATA_RSQB_RSQB 58
#define NS_HTML5TOKENIZER_SCRIPT_DATA_LESS_THAN_SIGN 59
#define NS_HTML5TOKENIZER_SCRIPT_DATA_ESCAPE_START 60
#define NS_HTML5TOKENIZER_SCRIPT_DATA_ESCAPE_START_DASH 61
#define NS_HTML5TOKENIZER_SCRIPT_DATA_ESCAPED_DASH 62
#define NS_HTML5TOKENIZER_SCRIPT_DATA_ESCAPED_DASH_DASH 63
#define NS_HTML5TOKENIZER_BOGUS_COMMENT_HYPHEN 64
#define NS_HTML5TOKENIZER_RAWTEXT_RCDATA_LESS_THAN_SIGN 65
#define NS_HTML5TOKENIZER_SCRIPT_DATA_ESCAPED_LESS_THAN_SIGN 66
#define NS_HTML5TOKENIZER_SCRIPT_DATA_DOUBLE_ESCAPE_START 67
#define NS_HTML5TOKENIZER_SCRIPT_DATA_DOUBLE_ESCAPED 68
#define NS_HTML5TOKENIZER_SCRIPT_DATA_DOUBLE_ESCAPED_LESS_THAN_SIGN 69
#define NS_HTML5TOKENIZER_SCRIPT_DATA_DOUBLE_ESCAPED_DASH 70
#define NS_HTML5TOKENIZER_SCRIPT_DATA_DOUBLE_ESCAPED_DASH_DASH 71
#define NS_HTML5TOKENIZER_SCRIPT_DATA_DOUBLE_ESCAPE_END 72
#define NS_HTML5TOKENIZER_PROCESSING_INSTRUCTION 73
#define NS_HTML5TOKENIZER_PROCESSING_INSTRUCTION_QUESTION_MARK 74
#define NS_HTML5TOKENIZER_LEAD_OFFSET (0xD800 - (0x10000 >> 10))


#endif

