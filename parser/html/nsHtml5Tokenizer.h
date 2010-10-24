/*
 * Copyright (c) 2005-2007 Henri Sivonen
 * Copyright (c) 2007-2010 Mozilla Foundation
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

#ifndef nsHtml5Tokenizer_h__
#define nsHtml5Tokenizer_h__

#include "prtypes.h"
#include "nsIAtom.h"
#include "nsHtml5AtomTable.h"
#include "nsString.h"
#include "nsINameSpaceManager.h"
#include "nsIContent.h"
#include "nsIDocument.h"
#include "nsTraceRefcnt.h"
#include "jArray.h"
#include "nsHtml5DocumentMode.h"
#include "nsHtml5ArrayCopy.h"
#include "nsHtml5NamedCharacters.h"
#include "nsHtml5NamedCharactersAccel.h"
#include "nsHtml5Atoms.h"
#include "nsHtml5ByteReadable.h"
#include "nsIUnicodeDecoder.h"
#include "nsAHtml5TreeBuilderState.h"
#include "nsHtml5Macros.h"

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
    static PRUnichar LT_GT[];
    static PRUnichar LT_SOLIDUS[];
    static PRUnichar RSQB_RSQB[];
    static PRUnichar REPLACEMENT_CHARACTER[];
    static PRUnichar LF[];
    static PRUnichar CDATA_LSQB[];
    static PRUnichar OCTYPE[];
    static PRUnichar UBLIC[];
    static PRUnichar YSTEM[];
    static jArray<PRUnichar,PRInt32> TITLE_ARR;
    static jArray<PRUnichar,PRInt32> SCRIPT_ARR;
    static jArray<PRUnichar,PRInt32> STYLE_ARR;
    static jArray<PRUnichar,PRInt32> PLAINTEXT_ARR;
    static jArray<PRUnichar,PRInt32> XMP_ARR;
    static jArray<PRUnichar,PRInt32> TEXTAREA_ARR;
    static jArray<PRUnichar,PRInt32> IFRAME_ARR;
    static jArray<PRUnichar,PRInt32> NOEMBED_ARR;
    static jArray<PRUnichar,PRInt32> NOSCRIPT_ARR;
    static jArray<PRUnichar,PRInt32> NOFRAMES_ARR;
  protected:
    nsHtml5TreeBuilder* tokenHandler;
    nsHtml5StreamParser* encodingDeclarationHandler;
    PRBool lastCR;
    PRInt32 stateSave;
  private:
    PRInt32 returnStateSave;
  protected:
    PRInt32 index;
  private:
    PRBool forceQuirks;
    PRUnichar additional;
    PRInt32 entCol;
    PRInt32 firstCharKey;
    PRInt32 lo;
    PRInt32 hi;
    PRInt32 candidate;
    PRInt32 strBufMark;
    PRInt32 prevValue;
  protected:
    PRInt32 value;
  private:
    PRBool seenDigits;
  protected:
    PRInt32 cstart;
  private:
    nsString* publicId;
    nsString* systemId;
    jArray<PRUnichar,PRInt32> strBuf;
    PRInt32 strBufLen;
    jArray<PRUnichar,PRInt32> longStrBuf;
    PRInt32 longStrBufLen;
    jArray<PRUnichar,PRInt32> bmpChar;
    jArray<PRUnichar,PRInt32> astralChar;
  protected:
    nsHtml5ElementName* endTagExpectation;
  private:
    jArray<PRUnichar,PRInt32> endTagExpectationAsArray;
  protected:
    PRBool endTag;
  private:
    nsHtml5ElementName* tagName;
  protected:
    nsHtml5AttributeName* attributeName;
  private:
    nsIAtom* doctypeName;
    nsString* publicIdentifier;
    nsString* systemIdentifier;
    nsHtml5HtmlAttributes* attributes;
    PRInt32 mappingLangToXmlLang;
    PRBool shouldSuspend;
  protected:
    PRBool confident;
  private:
    PRInt32 line;
    nsHtml5AtomTable* interner;
  public:
    nsHtml5Tokenizer(nsHtml5TreeBuilder* tokenHandler);
    void setInterner(nsHtml5AtomTable* interner);
    void initLocation(nsString* newPublicId, nsString* newSystemId);
    ~nsHtml5Tokenizer();
    void setStateAndEndTagExpectation(PRInt32 specialTokenizerState, nsIAtom* endTagExpectation);
    void setStateAndEndTagExpectation(PRInt32 specialTokenizerState, nsHtml5ElementName* endTagExpectation);
  private:
    void endTagExpectationToArray();
  public:
    void setLineNumber(PRInt32 line);
    inline PRInt32 getLineNumber()
    {
      return line;
    }

    nsHtml5HtmlAttributes* emptyAttributes();
  private:
    inline void clearStrBufAndAppend(PRUnichar c)
    {
      strBuf[0] = c;
      strBufLen = 1;
    }

    inline void clearStrBuf()
    {
      strBufLen = 0;
    }

    void appendStrBuf(PRUnichar c);
  protected:
    nsString* strBufToString();
  private:
    void strBufToDoctypeName();
    void emitStrBuf();
    inline void clearLongStrBuf()
    {
      longStrBufLen = 0;
    }

    inline void clearLongStrBufAndAppend(PRUnichar c)
    {
      longStrBuf[0] = c;
      longStrBufLen = 1;
    }

    void appendLongStrBuf(PRUnichar c);
    inline void appendSecondHyphenToBogusComment()
    {
      appendLongStrBuf('-');
    }

    inline void adjustDoubleHyphenAndAppendToLongStrBufAndErr(PRUnichar c)
    {

      appendLongStrBuf(c);
    }

    void appendLongStrBuf(jArray<PRUnichar,PRInt32> buffer, PRInt32 offset, PRInt32 length);
    inline void appendStrBufToLongStrBuf()
    {
      appendLongStrBuf(strBuf, 0, strBufLen);
    }

    nsString* longStrBufToString();
    void emitComment(PRInt32 provisionalHyphens, PRInt32 pos);
  protected:
    void flushChars(PRUnichar* buf, PRInt32 pos);
  private:
    void resetAttributes();
    void strBufToElementNameString();
    PRInt32 emitCurrentTagToken(PRBool selfClosing, PRInt32 pos);
    void attributeNameComplete();
    void addAttributeWithoutValue();
    void addAttributeWithValue();
  public:
    void start();
    PRBool tokenizeBuffer(nsHtml5UTF16Buffer* buffer);
  private:
    PRInt32 stateLoop(PRInt32 state, PRUnichar c, PRInt32 pos, PRUnichar* buf, PRBool reconsume, PRInt32 returnState, PRInt32 endPos);
    void initDoctypeFields();
    inline void adjustDoubleHyphenAndAppendToLongStrBufCarriageReturn()
    {
      silentCarriageReturn();
      adjustDoubleHyphenAndAppendToLongStrBufAndErr('\n');
    }

    inline void adjustDoubleHyphenAndAppendToLongStrBufLineFeed()
    {
      silentLineFeed();
      adjustDoubleHyphenAndAppendToLongStrBufAndErr('\n');
    }

    inline void appendLongStrBufLineFeed()
    {
      silentLineFeed();
      appendLongStrBuf('\n');
    }

    inline void appendLongStrBufCarriageReturn()
    {
      silentCarriageReturn();
      appendLongStrBuf('\n');
    }

  protected:
    inline void silentCarriageReturn()
    {
      ++line;
      lastCR = PR_TRUE;
    }

    inline void silentLineFeed()
    {
      ++line;
    }

  private:
    void emitCarriageReturn(PRUnichar* buf, PRInt32 pos);
    void emitReplacementCharacter(PRUnichar* buf, PRInt32 pos);
    void setAdditionalAndRememberAmpersandLocation(PRUnichar add);
    void bogusDoctype();
    void bogusDoctypeWithoutQuirks();
    void emitOrAppendStrBuf(PRInt32 returnState);
    void handleNcrValue(PRInt32 returnState);
  public:
    void eof();
  private:
    void emitDoctypeToken(PRInt32 pos);
  protected:
    inline PRUnichar checkChar(PRUnichar* buf, PRInt32 pos)
    {
      return buf[pos];
    }

  public:
    void internalEncodingDeclaration(nsString* internalCharset);
  private:
    void emitOrAppendTwo(const PRUnichar* val, PRInt32 returnState);
    void emitOrAppendOne(const PRUnichar* val, PRInt32 returnState);
  public:
    void end();
    void requestSuspension();
    PRBool isInDataState();
    void resetToDataState();
    void loadState(nsHtml5Tokenizer* other);
    void initializeWithoutStarting();
    void setEncodingDeclarationHandler(nsHtml5StreamParser* encodingDeclarationHandler);
    static void initializeStatics();
    static void releaseStatics();
};

#ifdef nsHtml5Tokenizer_cpp__
PRUnichar nsHtml5Tokenizer::LT_GT[] = { '<', '>' };
PRUnichar nsHtml5Tokenizer::LT_SOLIDUS[] = { '<', '/' };
PRUnichar nsHtml5Tokenizer::RSQB_RSQB[] = { ']', ']' };
PRUnichar nsHtml5Tokenizer::REPLACEMENT_CHARACTER[] = { 0xfffd };
PRUnichar nsHtml5Tokenizer::LF[] = { '\n' };
PRUnichar nsHtml5Tokenizer::CDATA_LSQB[] = { 'C', 'D', 'A', 'T', 'A', '[' };
PRUnichar nsHtml5Tokenizer::OCTYPE[] = { 'o', 'c', 't', 'y', 'p', 'e' };
PRUnichar nsHtml5Tokenizer::UBLIC[] = { 'u', 'b', 'l', 'i', 'c' };
PRUnichar nsHtml5Tokenizer::YSTEM[] = { 'y', 's', 't', 'e', 'm' };
jArray<PRUnichar,PRInt32> nsHtml5Tokenizer::TITLE_ARR = 0;
jArray<PRUnichar,PRInt32> nsHtml5Tokenizer::SCRIPT_ARR = 0;
jArray<PRUnichar,PRInt32> nsHtml5Tokenizer::STYLE_ARR = 0;
jArray<PRUnichar,PRInt32> nsHtml5Tokenizer::PLAINTEXT_ARR = 0;
jArray<PRUnichar,PRInt32> nsHtml5Tokenizer::XMP_ARR = 0;
jArray<PRUnichar,PRInt32> nsHtml5Tokenizer::TEXTAREA_ARR = 0;
jArray<PRUnichar,PRInt32> nsHtml5Tokenizer::IFRAME_ARR = 0;
jArray<PRUnichar,PRInt32> nsHtml5Tokenizer::NOEMBED_ARR = 0;
jArray<PRUnichar,PRInt32> nsHtml5Tokenizer::NOSCRIPT_ARR = 0;
jArray<PRUnichar,PRInt32> nsHtml5Tokenizer::NOFRAMES_ARR = 0;
#endif

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
#define NS_HTML5TOKENIZER_COMMENT_END_SPACE 37
#define NS_HTML5TOKENIZER_COMMENT_END_BANG 38
#define NS_HTML5TOKENIZER_NON_DATA_END_TAG_NAME 39
#define NS_HTML5TOKENIZER_MARKUP_DECLARATION_HYPHEN 40
#define NS_HTML5TOKENIZER_MARKUP_DECLARATION_OCTYPE 41
#define NS_HTML5TOKENIZER_DOCTYPE_UBLIC 42
#define NS_HTML5TOKENIZER_DOCTYPE_YSTEM 43
#define NS_HTML5TOKENIZER_AFTER_DOCTYPE_PUBLIC_KEYWORD 44
#define NS_HTML5TOKENIZER_BETWEEN_DOCTYPE_PUBLIC_AND_SYSTEM_IDENTIFIERS 45
#define NS_HTML5TOKENIZER_AFTER_DOCTYPE_SYSTEM_KEYWORD 46
#define NS_HTML5TOKENIZER_CONSUME_CHARACTER_REFERENCE 47
#define NS_HTML5TOKENIZER_CONSUME_NCR 48
#define NS_HTML5TOKENIZER_CHARACTER_REFERENCE_TAIL 49
#define NS_HTML5TOKENIZER_HEX_NCR_LOOP 50
#define NS_HTML5TOKENIZER_DECIMAL_NRC_LOOP 51
#define NS_HTML5TOKENIZER_HANDLE_NCR_VALUE 52
#define NS_HTML5TOKENIZER_HANDLE_NCR_VALUE_RECONSUME 53
#define NS_HTML5TOKENIZER_CHARACTER_REFERENCE_HILO_LOOKUP 54
#define NS_HTML5TOKENIZER_SELF_CLOSING_START_TAG 55
#define NS_HTML5TOKENIZER_CDATA_START 56
#define NS_HTML5TOKENIZER_CDATA_SECTION 57
#define NS_HTML5TOKENIZER_CDATA_RSQB 58
#define NS_HTML5TOKENIZER_CDATA_RSQB_RSQB 59
#define NS_HTML5TOKENIZER_SCRIPT_DATA_LESS_THAN_SIGN 60
#define NS_HTML5TOKENIZER_SCRIPT_DATA_ESCAPE_START 61
#define NS_HTML5TOKENIZER_SCRIPT_DATA_ESCAPE_START_DASH 62
#define NS_HTML5TOKENIZER_SCRIPT_DATA_ESCAPED_DASH 63
#define NS_HTML5TOKENIZER_SCRIPT_DATA_ESCAPED_DASH_DASH 64
#define NS_HTML5TOKENIZER_BOGUS_COMMENT_HYPHEN 65
#define NS_HTML5TOKENIZER_RAWTEXT_RCDATA_LESS_THAN_SIGN 66
#define NS_HTML5TOKENIZER_SCRIPT_DATA_ESCAPED_LESS_THAN_SIGN 67
#define NS_HTML5TOKENIZER_SCRIPT_DATA_DOUBLE_ESCAPE_START 68
#define NS_HTML5TOKENIZER_SCRIPT_DATA_DOUBLE_ESCAPED 69
#define NS_HTML5TOKENIZER_SCRIPT_DATA_DOUBLE_ESCAPED_LESS_THAN_SIGN 70
#define NS_HTML5TOKENIZER_SCRIPT_DATA_DOUBLE_ESCAPED_DASH 71
#define NS_HTML5TOKENIZER_SCRIPT_DATA_DOUBLE_ESCAPED_DASH_DASH 72
#define NS_HTML5TOKENIZER_SCRIPT_DATA_DOUBLE_ESCAPE_END 73
#define NS_HTML5TOKENIZER_LEAD_OFFSET (0xD800 - (0x10000 >> 10))
#define NS_HTML5TOKENIZER_BUFFER_GROW_BY 1024


#endif

