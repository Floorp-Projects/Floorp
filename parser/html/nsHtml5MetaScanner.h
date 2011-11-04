/*
 * Copyright (c) 2007 Henri Sivonen
 * Copyright (c) 2008-2010 Mozilla Foundation
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
 * Please edit MetaScanner.java instead and regenerate.
 */

#ifndef nsHtml5MetaScanner_h__
#define nsHtml5MetaScanner_h__

#include "prtypes.h"
#include "nsIAtom.h"
#include "nsHtml5AtomTable.h"
#include "nsString.h"
#include "nsINameSpaceManager.h"
#include "nsIContent.h"
#include "nsTraceRefcnt.h"
#include "jArray.h"
#include "nsHtml5ArrayCopy.h"
#include "nsAHtml5TreeBuilderState.h"
#include "nsHtml5Atoms.h"
#include "nsHtml5ByteReadable.h"
#include "nsIUnicodeDecoder.h"
#include "nsHtml5Macros.h"

class nsHtml5StreamParser;

class nsHtml5Tokenizer;
class nsHtml5TreeBuilder;
class nsHtml5AttributeName;
class nsHtml5ElementName;
class nsHtml5HtmlAttributes;
class nsHtml5UTF16Buffer;
class nsHtml5StateSnapshot;
class nsHtml5Portability;


class nsHtml5MetaScanner
{
  private:
    static staticJArray<PRUnichar,PRInt32> CHARSET;
    static staticJArray<PRUnichar,PRInt32> CONTENT;
    static staticJArray<PRUnichar,PRInt32> HTTP_EQUIV;
    static staticJArray<PRUnichar,PRInt32> CONTENT_TYPE;
  protected:
    nsHtml5ByteReadable* readable;
  private:
    PRInt32 metaState;
    PRInt32 contentIndex;
    PRInt32 charsetIndex;
    PRInt32 httpEquivIndex;
    PRInt32 contentTypeIndex;
  protected:
    PRInt32 stateSave;
  private:
    PRInt32 strBufLen;
    autoJArray<PRUnichar,PRInt32> strBuf;
    nsString* content;
    nsString* charset;
    PRInt32 httpEquivState;
  public:
    nsHtml5MetaScanner();
    ~nsHtml5MetaScanner();
  protected:
    void stateLoop(PRInt32 state);
  private:
    void handleCharInAttributeValue(PRInt32 c);
    inline PRInt32 toAsciiLowerCase(PRInt32 c)
    {
      if (c >= 'A' && c <= 'Z') {
        return c + 0x20;
      }
      return c;
    }

    void addToBuffer(PRInt32 c);
    void handleAttributeValue();
    bool handleTag();
    bool handleTagInner();
  protected:
    bool tryCharset(nsString* encoding);
  public:
    static void initializeStatics();
    static void releaseStatics();

#include "nsHtml5MetaScannerHSupplement.h"
};

#define NS_HTML5META_SCANNER_NO 0
#define NS_HTML5META_SCANNER_M 1
#define NS_HTML5META_SCANNER_E 2
#define NS_HTML5META_SCANNER_T 3
#define NS_HTML5META_SCANNER_A 4
#define NS_HTML5META_SCANNER_DATA 0
#define NS_HTML5META_SCANNER_TAG_OPEN 1
#define NS_HTML5META_SCANNER_SCAN_UNTIL_GT 2
#define NS_HTML5META_SCANNER_TAG_NAME 3
#define NS_HTML5META_SCANNER_BEFORE_ATTRIBUTE_NAME 4
#define NS_HTML5META_SCANNER_ATTRIBUTE_NAME 5
#define NS_HTML5META_SCANNER_AFTER_ATTRIBUTE_NAME 6
#define NS_HTML5META_SCANNER_BEFORE_ATTRIBUTE_VALUE 7
#define NS_HTML5META_SCANNER_ATTRIBUTE_VALUE_DOUBLE_QUOTED 8
#define NS_HTML5META_SCANNER_ATTRIBUTE_VALUE_SINGLE_QUOTED 9
#define NS_HTML5META_SCANNER_ATTRIBUTE_VALUE_UNQUOTED 10
#define NS_HTML5META_SCANNER_AFTER_ATTRIBUTE_VALUE_QUOTED 11
#define NS_HTML5META_SCANNER_MARKUP_DECLARATION_OPEN 13
#define NS_HTML5META_SCANNER_MARKUP_DECLARATION_HYPHEN 14
#define NS_HTML5META_SCANNER_COMMENT_START 15
#define NS_HTML5META_SCANNER_COMMENT_START_DASH 16
#define NS_HTML5META_SCANNER_COMMENT 17
#define NS_HTML5META_SCANNER_COMMENT_END_DASH 18
#define NS_HTML5META_SCANNER_COMMENT_END 19
#define NS_HTML5META_SCANNER_SELF_CLOSING_START_TAG 20
#define NS_HTML5META_SCANNER_HTTP_EQUIV_NOT_SEEN 0
#define NS_HTML5META_SCANNER_HTTP_EQUIV_CONTENT_TYPE 1
#define NS_HTML5META_SCANNER_HTTP_EQUIV_OTHER 2


#endif

