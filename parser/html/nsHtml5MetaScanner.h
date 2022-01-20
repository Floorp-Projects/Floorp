/*
 * Copyright (c) 2007 Henri Sivonen
 * Copyright (c) 2008-2015 Mozilla Foundation
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

#ifndef nsHtml5MetaScanner_h
#define nsHtml5MetaScanner_h

#include "nsAtom.h"
#include "nsHtml5AtomTable.h"
#include "nsHtml5String.h"
#include "nsNameSpaceManager.h"
#include "nsIContent.h"
#include "nsTraceRefcnt.h"
#include "jArray.h"
#include "nsHtml5ArrayCopy.h"
#include "nsAHtml5TreeBuilderState.h"
#include "nsGkAtoms.h"
#include "nsHtml5ByteReadable.h"
#include "nsHtml5Macros.h"
#include "nsIContentHandle.h"
#include "nsHtml5Portability.h"
#include "nsHtml5ContentCreatorFunction.h"

class nsHtml5StreamParser;

class nsHtml5AttributeName;
class nsHtml5ElementName;
class nsHtml5Tokenizer;
class nsHtml5TreeBuilder;
class nsHtml5UTF16Buffer;
class nsHtml5StateSnapshot;
class nsHtml5Portability;

class nsHtml5MetaScanner {
 private:
  static staticJArray<char16_t, int32_t> CHARSET;
  static staticJArray<char16_t, int32_t> CONTENT;
  static staticJArray<char16_t, int32_t> HTTP_EQUIV;
  static staticJArray<char16_t, int32_t> CONTENT_TYPE;
  static const int32_t NO = 0;

  static const int32_t M = 1;

  static const int32_t E = 2;

  static const int32_t T = 3;

  static const int32_t A = 4;

  static const int32_t DATA = 0;

  static const int32_t TAG_OPEN = 1;

  static const int32_t SCAN_UNTIL_GT = 2;

  static const int32_t TAG_NAME = 3;

  static const int32_t BEFORE_ATTRIBUTE_NAME = 4;

  static const int32_t ATTRIBUTE_NAME = 5;

  static const int32_t AFTER_ATTRIBUTE_NAME = 6;

  static const int32_t BEFORE_ATTRIBUTE_VALUE = 7;

  static const int32_t ATTRIBUTE_VALUE_DOUBLE_QUOTED = 8;

  static const int32_t ATTRIBUTE_VALUE_SINGLE_QUOTED = 9;

  static const int32_t ATTRIBUTE_VALUE_UNQUOTED = 10;

  static const int32_t AFTER_ATTRIBUTE_VALUE_QUOTED = 11;

  static const int32_t MARKUP_DECLARATION_OPEN = 13;

  static const int32_t MARKUP_DECLARATION_HYPHEN = 14;

  static const int32_t COMMENT_START = 15;

  static const int32_t COMMENT_START_DASH = 16;

  static const int32_t COMMENT = 17;

  static const int32_t COMMENT_END_DASH = 18;

  static const int32_t COMMENT_END = 19;

  static const int32_t SELF_CLOSING_START_TAG = 20;

  static const int32_t HTTP_EQUIV_NOT_SEEN = 0;

  static const int32_t HTTP_EQUIV_CONTENT_TYPE = 1;

  static const int32_t HTTP_EQUIV_OTHER = 2;

 protected:
  nsHtml5ByteReadable* readable;

 private:
  int32_t metaState;
  int32_t contentIndex;
  int32_t charsetIndex;
  int32_t httpEquivIndex;
  int32_t contentTypeIndex;

 protected:
  int32_t stateSave;

 private:
  int32_t strBufLen;
  autoJArray<char16_t, int32_t> strBuf;
  nsHtml5String content;
  nsHtml5String charset;
  int32_t httpEquivState;
  nsHtml5TreeBuilder* treeBuilder;

 public:
  explicit nsHtml5MetaScanner(nsHtml5TreeBuilder* tb);
  ~nsHtml5MetaScanner();

 protected:
  void stateLoop(int32_t state);

 private:
  void handleCharInAttributeValue(int32_t c);
  inline int32_t toAsciiLowerCase(int32_t c) {
    if (c >= 'A' && c <= 'Z') {
      return c + 0x20;
    }
    return c;
  }

  void addToBuffer(int32_t c);
  void handleAttributeValue();
  bool handleTag();
  bool handleTagInner();

 protected:
  bool tryCharset(nsHtml5String encoding);

 public:
  static void initializeStatics();
  static void releaseStatics();

#include "nsHtml5MetaScannerHSupplement.h"
};

#endif
