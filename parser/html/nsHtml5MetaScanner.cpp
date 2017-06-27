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

#define nsHtml5MetaScanner_cpp__

#include "nsIAtom.h"
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

#include "nsHtml5AttributeName.h"
#include "nsHtml5ElementName.h"
#include "nsHtml5Tokenizer.h"
#include "nsHtml5TreeBuilder.h"
#include "nsHtml5StackNode.h"
#include "nsHtml5UTF16Buffer.h"
#include "nsHtml5StateSnapshot.h"
#include "nsHtml5Portability.h"

#include "nsHtml5MetaScanner.h"

static char16_t const CHARSET_DATA[] = { 'h', 'a', 'r', 's', 'e', 't' };
staticJArray<char16_t,int32_t> nsHtml5MetaScanner::CHARSET = { CHARSET_DATA, MOZ_ARRAY_LENGTH(CHARSET_DATA) };
static char16_t const CONTENT_DATA[] = { 'o', 'n', 't', 'e', 'n', 't' };
staticJArray<char16_t,int32_t> nsHtml5MetaScanner::CONTENT = { CONTENT_DATA, MOZ_ARRAY_LENGTH(CONTENT_DATA) };
static char16_t const HTTP_EQUIV_DATA[] = { 't', 't', 'p', '-', 'e', 'q', 'u', 'i', 'v' };
staticJArray<char16_t,int32_t> nsHtml5MetaScanner::HTTP_EQUIV = { HTTP_EQUIV_DATA, MOZ_ARRAY_LENGTH(HTTP_EQUIV_DATA) };
static char16_t const CONTENT_TYPE_DATA[] = { 'c', 'o', 'n', 't', 'e', 'n', 't', '-', 't', 'y', 'p', 'e' };
staticJArray<char16_t,int32_t> nsHtml5MetaScanner::CONTENT_TYPE = { CONTENT_TYPE_DATA, MOZ_ARRAY_LENGTH(CONTENT_TYPE_DATA) };

nsHtml5MetaScanner::nsHtml5MetaScanner(nsHtml5TreeBuilder* tb)
  : readable(nullptr)
  , metaState(NO)
  , contentIndex(INT32_MAX)
  , charsetIndex(INT32_MAX)
  , httpEquivIndex(INT32_MAX)
  , contentTypeIndex(INT32_MAX)
  , stateSave(DATA)
  , strBufLen(0)
  , strBuf(jArray<char16_t, int32_t>::newJArray(36))
  , content(nullptr)
  , charset(nullptr)
  , httpEquivState(HTTP_EQUIV_NOT_SEEN)
  , treeBuilder(tb)
  , mEncoding(nullptr)
{
  MOZ_COUNT_CTOR(nsHtml5MetaScanner);
}


nsHtml5MetaScanner::~nsHtml5MetaScanner()
{
  MOZ_COUNT_DTOR(nsHtml5MetaScanner);
  content.Release();
  charset.Release();
}

void 
nsHtml5MetaScanner::stateLoop(int32_t state)
{
  int32_t c = -1;
  bool reconsume = false;
  stateloop: for (; ; ) {
    switch(state) {
      case DATA: {
        for (; ; ) {
          if (reconsume) {
            reconsume = false;
          } else {
            c = read();
          }
          switch(c) {
            case -1: {
              NS_HTML5_BREAK(stateloop);
            }
            case '<': {
              state = nsHtml5MetaScanner::TAG_OPEN;
              NS_HTML5_BREAK(dataloop);
            }
            default: {
              continue;
            }
          }
        }
        dataloop_end: ;
      }
      case TAG_OPEN: {
        for (; ; ) {
          c = read();
          switch(c) {
            case -1: {
              NS_HTML5_BREAK(stateloop);
            }
            case 'm':
            case 'M': {
              metaState = M;
              state = nsHtml5MetaScanner::TAG_NAME;
              NS_HTML5_BREAK(tagopenloop);
            }
            case '!': {
              state = nsHtml5MetaScanner::MARKUP_DECLARATION_OPEN;
              NS_HTML5_CONTINUE(stateloop);
            }
            case '\?':
            case '/': {
              state = nsHtml5MetaScanner::SCAN_UNTIL_GT;
              NS_HTML5_CONTINUE(stateloop);
            }
            case '>': {
              state = nsHtml5MetaScanner::DATA;
              NS_HTML5_CONTINUE(stateloop);
            }
            default: {
              if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')) {
                metaState = NO;
                state = nsHtml5MetaScanner::TAG_NAME;
                NS_HTML5_BREAK(tagopenloop);
              }
              state = nsHtml5MetaScanner::DATA;
              reconsume = true;
              NS_HTML5_CONTINUE(stateloop);
            }
          }
        }
        tagopenloop_end: ;
      }
      case TAG_NAME: {
        for (; ; ) {
          c = read();
          switch(c) {
            case -1: {
              NS_HTML5_BREAK(stateloop);
            }
            case ' ':
            case '\t':
            case '\n':
            case '\f': {
              state = nsHtml5MetaScanner::BEFORE_ATTRIBUTE_NAME;
              NS_HTML5_BREAK(tagnameloop);
            }
            case '/': {
              state = nsHtml5MetaScanner::SELF_CLOSING_START_TAG;
              NS_HTML5_CONTINUE(stateloop);
            }
            case '>': {
              state = nsHtml5MetaScanner::DATA;
              NS_HTML5_CONTINUE(stateloop);
            }
            case 'e':
            case 'E': {
              if (metaState == M) {
                metaState = E;
              } else {
                metaState = NO;
              }
              continue;
            }
            case 't':
            case 'T': {
              if (metaState == E) {
                metaState = T;
              } else {
                metaState = NO;
              }
              continue;
            }
            case 'a':
            case 'A': {
              if (metaState == T) {
                metaState = A;
              } else {
                metaState = NO;
              }
              continue;
            }
            default: {
              metaState = NO;
              continue;
            }
          }
        }
        tagnameloop_end: ;
      }
      case BEFORE_ATTRIBUTE_NAME: {
        for (; ; ) {
          if (reconsume) {
            reconsume = false;
          } else {
            c = read();
          }
          switch(c) {
            case -1: {
              NS_HTML5_BREAK(stateloop);
            }
            case ' ':
            case '\t':
            case '\n':
            case '\f': {
              continue;
            }
            case '/': {
              state = nsHtml5MetaScanner::SELF_CLOSING_START_TAG;
              NS_HTML5_CONTINUE(stateloop);
            }
            case '>': {
              if (handleTag()) {
                NS_HTML5_BREAK(stateloop);
              }
              state = DATA;
              NS_HTML5_CONTINUE(stateloop);
            }
            case 'c':
            case 'C': {
              contentIndex = 0;
              charsetIndex = 0;
              httpEquivIndex = INT32_MAX;
              contentTypeIndex = INT32_MAX;
              state = nsHtml5MetaScanner::ATTRIBUTE_NAME;
              NS_HTML5_BREAK(beforeattributenameloop);
            }
            case 'h':
            case 'H': {
              contentIndex = INT32_MAX;
              charsetIndex = INT32_MAX;
              httpEquivIndex = 0;
              contentTypeIndex = INT32_MAX;
              state = nsHtml5MetaScanner::ATTRIBUTE_NAME;
              NS_HTML5_BREAK(beforeattributenameloop);
            }
            default: {
              contentIndex = INT32_MAX;
              charsetIndex = INT32_MAX;
              httpEquivIndex = INT32_MAX;
              contentTypeIndex = INT32_MAX;
              state = nsHtml5MetaScanner::ATTRIBUTE_NAME;
              NS_HTML5_BREAK(beforeattributenameloop);
            }
          }
        }
        beforeattributenameloop_end: ;
      }
      case ATTRIBUTE_NAME: {
        for (; ; ) {
          c = read();
          switch(c) {
            case -1: {
              NS_HTML5_BREAK(stateloop);
            }
            case ' ':
            case '\t':
            case '\n':
            case '\f': {
              state = nsHtml5MetaScanner::AFTER_ATTRIBUTE_NAME;
              NS_HTML5_CONTINUE(stateloop);
            }
            case '/': {
              state = nsHtml5MetaScanner::SELF_CLOSING_START_TAG;
              NS_HTML5_CONTINUE(stateloop);
            }
            case '=': {
              strBufLen = 0;
              contentTypeIndex = 0;
              state = nsHtml5MetaScanner::BEFORE_ATTRIBUTE_VALUE;
              NS_HTML5_BREAK(attributenameloop);
            }
            case '>': {
              if (handleTag()) {
                NS_HTML5_BREAK(stateloop);
              }
              state = nsHtml5MetaScanner::DATA;
              NS_HTML5_CONTINUE(stateloop);
            }
            default: {
              if (metaState == A) {
                if (c >= 'A' && c <= 'Z') {
                  c += 0x20;
                }
                if (contentIndex < CONTENT.length && c == CONTENT[contentIndex]) {
                  ++contentIndex;
                } else {
                  contentIndex = INT32_MAX;
                }
                if (charsetIndex < CHARSET.length && c == CHARSET[charsetIndex]) {
                  ++charsetIndex;
                } else {
                  charsetIndex = INT32_MAX;
                }
                if (httpEquivIndex < HTTP_EQUIV.length && c == HTTP_EQUIV[httpEquivIndex]) {
                  ++httpEquivIndex;
                } else {
                  httpEquivIndex = INT32_MAX;
                }
              }
              continue;
            }
          }
        }
        attributenameloop_end: ;
      }
      case BEFORE_ATTRIBUTE_VALUE: {
        for (; ; ) {
          c = read();
          switch(c) {
            case -1: {
              NS_HTML5_BREAK(stateloop);
            }
            case ' ':
            case '\t':
            case '\n':
            case '\f': {
              continue;
            }
            case '\"': {
              state = nsHtml5MetaScanner::ATTRIBUTE_VALUE_DOUBLE_QUOTED;
              NS_HTML5_BREAK(beforeattributevalueloop);
            }
            case '\'': {
              state = nsHtml5MetaScanner::ATTRIBUTE_VALUE_SINGLE_QUOTED;
              NS_HTML5_CONTINUE(stateloop);
            }
            case '>': {
              if (handleTag()) {
                NS_HTML5_BREAK(stateloop);
              }
              state = nsHtml5MetaScanner::DATA;
              NS_HTML5_CONTINUE(stateloop);
            }
            default: {
              handleCharInAttributeValue(c);
              state = nsHtml5MetaScanner::ATTRIBUTE_VALUE_UNQUOTED;
              NS_HTML5_CONTINUE(stateloop);
            }
          }
        }
        beforeattributevalueloop_end: ;
      }
      case ATTRIBUTE_VALUE_DOUBLE_QUOTED: {
        for (; ; ) {
          if (reconsume) {
            reconsume = false;
          } else {
            c = read();
          }
          switch(c) {
            case -1: {
              NS_HTML5_BREAK(stateloop);
            }
            case '\"': {
              handleAttributeValue();
              state = nsHtml5MetaScanner::AFTER_ATTRIBUTE_VALUE_QUOTED;
              NS_HTML5_BREAK(attributevaluedoublequotedloop);
            }
            default: {
              handleCharInAttributeValue(c);
              continue;
            }
          }
        }
        attributevaluedoublequotedloop_end: ;
      }
      case AFTER_ATTRIBUTE_VALUE_QUOTED: {
        for (; ; ) {
          c = read();
          switch(c) {
            case -1: {
              NS_HTML5_BREAK(stateloop);
            }
            case ' ':
            case '\t':
            case '\n':
            case '\f': {
              state = nsHtml5MetaScanner::BEFORE_ATTRIBUTE_NAME;
              NS_HTML5_CONTINUE(stateloop);
            }
            case '/': {
              state = nsHtml5MetaScanner::SELF_CLOSING_START_TAG;
              NS_HTML5_BREAK(afterattributevaluequotedloop);
            }
            case '>': {
              if (handleTag()) {
                NS_HTML5_BREAK(stateloop);
              }
              state = nsHtml5MetaScanner::DATA;
              NS_HTML5_CONTINUE(stateloop);
            }
            default: {
              state = nsHtml5MetaScanner::BEFORE_ATTRIBUTE_NAME;
              reconsume = true;
              NS_HTML5_CONTINUE(stateloop);
            }
          }
        }
        afterattributevaluequotedloop_end: ;
      }
      case SELF_CLOSING_START_TAG: {
        c = read();
        switch(c) {
          case -1: {
            NS_HTML5_BREAK(stateloop);
          }
          case '>': {
            if (handleTag()) {
              NS_HTML5_BREAK(stateloop);
            }
            state = nsHtml5MetaScanner::DATA;
            NS_HTML5_CONTINUE(stateloop);
          }
          default: {
            state = nsHtml5MetaScanner::BEFORE_ATTRIBUTE_NAME;
            reconsume = true;
            NS_HTML5_CONTINUE(stateloop);
          }
        }
      }
      case ATTRIBUTE_VALUE_UNQUOTED: {
        for (; ; ) {
          if (reconsume) {
            reconsume = false;
          } else {
            c = read();
          }
          switch(c) {
            case -1: {
              NS_HTML5_BREAK(stateloop);
            }
            case ' ':
            case '\t':
            case '\n':
            case '\f': {
              handleAttributeValue();
              state = nsHtml5MetaScanner::BEFORE_ATTRIBUTE_NAME;
              NS_HTML5_CONTINUE(stateloop);
            }
            case '>': {
              handleAttributeValue();
              if (handleTag()) {
                NS_HTML5_BREAK(stateloop);
              }
              state = nsHtml5MetaScanner::DATA;
              NS_HTML5_CONTINUE(stateloop);
            }
            default: {
              handleCharInAttributeValue(c);
              continue;
            }
          }
        }
      }
      case AFTER_ATTRIBUTE_NAME: {
        for (; ; ) {
          c = read();
          switch(c) {
            case -1: {
              NS_HTML5_BREAK(stateloop);
            }
            case ' ':
            case '\t':
            case '\n':
            case '\f': {
              continue;
            }
            case '/': {
              handleAttributeValue();
              state = nsHtml5MetaScanner::SELF_CLOSING_START_TAG;
              NS_HTML5_CONTINUE(stateloop);
            }
            case '=': {
              strBufLen = 0;
              contentTypeIndex = 0;
              state = nsHtml5MetaScanner::BEFORE_ATTRIBUTE_VALUE;
              NS_HTML5_CONTINUE(stateloop);
            }
            case '>': {
              handleAttributeValue();
              if (handleTag()) {
                NS_HTML5_BREAK(stateloop);
              }
              state = nsHtml5MetaScanner::DATA;
              NS_HTML5_CONTINUE(stateloop);
            }
            case 'c':
            case 'C': {
              contentIndex = 0;
              charsetIndex = 0;
              state = nsHtml5MetaScanner::ATTRIBUTE_NAME;
              NS_HTML5_CONTINUE(stateloop);
            }
            default: {
              contentIndex = INT32_MAX;
              charsetIndex = INT32_MAX;
              state = nsHtml5MetaScanner::ATTRIBUTE_NAME;
              NS_HTML5_CONTINUE(stateloop);
            }
          }
        }
      }
      case MARKUP_DECLARATION_OPEN: {
        for (; ; ) {
          c = read();
          switch(c) {
            case -1: {
              NS_HTML5_BREAK(stateloop);
            }
            case '-': {
              state = nsHtml5MetaScanner::MARKUP_DECLARATION_HYPHEN;
              NS_HTML5_BREAK(markupdeclarationopenloop);
            }
            default: {
              state = nsHtml5MetaScanner::SCAN_UNTIL_GT;
              reconsume = true;
              NS_HTML5_CONTINUE(stateloop);
            }
          }
        }
        markupdeclarationopenloop_end: ;
      }
      case MARKUP_DECLARATION_HYPHEN: {
        for (; ; ) {
          c = read();
          switch(c) {
            case -1: {
              NS_HTML5_BREAK(stateloop);
            }
            case '-': {
              state = nsHtml5MetaScanner::COMMENT_START;
              NS_HTML5_BREAK(markupdeclarationhyphenloop);
            }
            default: {
              state = nsHtml5MetaScanner::SCAN_UNTIL_GT;
              reconsume = true;
              NS_HTML5_CONTINUE(stateloop);
            }
          }
        }
        markupdeclarationhyphenloop_end: ;
      }
      case COMMENT_START: {
        for (; ; ) {
          c = read();
          switch(c) {
            case -1: {
              NS_HTML5_BREAK(stateloop);
            }
            case '-': {
              state = nsHtml5MetaScanner::COMMENT_START_DASH;
              NS_HTML5_CONTINUE(stateloop);
            }
            case '>': {
              state = nsHtml5MetaScanner::DATA;
              NS_HTML5_CONTINUE(stateloop);
            }
            default: {
              state = nsHtml5MetaScanner::COMMENT;
              NS_HTML5_BREAK(commentstartloop);
            }
          }
        }
        commentstartloop_end: ;
      }
      case COMMENT: {
        for (; ; ) {
          c = read();
          switch(c) {
            case -1: {
              NS_HTML5_BREAK(stateloop);
            }
            case '-': {
              state = nsHtml5MetaScanner::COMMENT_END_DASH;
              NS_HTML5_BREAK(commentloop);
            }
            default: {
              continue;
            }
          }
        }
        commentloop_end: ;
      }
      case COMMENT_END_DASH: {
        for (; ; ) {
          c = read();
          switch(c) {
            case -1: {
              NS_HTML5_BREAK(stateloop);
            }
            case '-': {
              state = nsHtml5MetaScanner::COMMENT_END;
              NS_HTML5_BREAK(commentenddashloop);
            }
            default: {
              state = nsHtml5MetaScanner::COMMENT;
              NS_HTML5_CONTINUE(stateloop);
            }
          }
        }
        commentenddashloop_end: ;
      }
      case COMMENT_END: {
        for (; ; ) {
          c = read();
          switch(c) {
            case -1: {
              NS_HTML5_BREAK(stateloop);
            }
            case '>': {
              state = nsHtml5MetaScanner::DATA;
              NS_HTML5_CONTINUE(stateloop);
            }
            case '-': {
              continue;
            }
            default: {
              state = nsHtml5MetaScanner::COMMENT;
              NS_HTML5_CONTINUE(stateloop);
            }
          }
        }
      }
      case COMMENT_START_DASH: {
        c = read();
        switch(c) {
          case -1: {
            NS_HTML5_BREAK(stateloop);
          }
          case '-': {
            state = nsHtml5MetaScanner::COMMENT_END;
            NS_HTML5_CONTINUE(stateloop);
          }
          case '>': {
            state = nsHtml5MetaScanner::DATA;
            NS_HTML5_CONTINUE(stateloop);
          }
          default: {
            state = nsHtml5MetaScanner::COMMENT;
            NS_HTML5_CONTINUE(stateloop);
          }
        }
      }
      case ATTRIBUTE_VALUE_SINGLE_QUOTED: {
        for (; ; ) {
          if (reconsume) {
            reconsume = false;
          } else {
            c = read();
          }
          switch(c) {
            case -1: {
              NS_HTML5_BREAK(stateloop);
            }
            case '\'': {
              handleAttributeValue();
              state = nsHtml5MetaScanner::AFTER_ATTRIBUTE_VALUE_QUOTED;
              NS_HTML5_CONTINUE(stateloop);
            }
            default: {
              handleCharInAttributeValue(c);
              continue;
            }
          }
        }
      }
      case SCAN_UNTIL_GT: {
        for (; ; ) {
          if (reconsume) {
            reconsume = false;
          } else {
            c = read();
          }
          switch(c) {
            case -1: {
              NS_HTML5_BREAK(stateloop);
            }
            case '>': {
              state = nsHtml5MetaScanner::DATA;
              NS_HTML5_CONTINUE(stateloop);
            }
            default: {
              continue;
            }
          }
        }
      }
    }
  }
  stateloop_end: ;
  stateSave = state;
}

void 
nsHtml5MetaScanner::handleCharInAttributeValue(int32_t c)
{
  if (metaState == A) {
    if (contentIndex == CONTENT.length || charsetIndex == CHARSET.length) {
      addToBuffer(c);
    } else if (httpEquivIndex == HTTP_EQUIV.length) {
      if (contentTypeIndex < CONTENT_TYPE.length && toAsciiLowerCase(c) == CONTENT_TYPE[contentTypeIndex]) {
        ++contentTypeIndex;
      } else {
        contentTypeIndex = INT32_MAX;
      }
    }
  }
}

void 
nsHtml5MetaScanner::addToBuffer(int32_t c)
{
  if (strBufLen == strBuf.length) {
    jArray<char16_t,int32_t> newBuf = jArray<char16_t,int32_t>::newJArray(strBuf.length + (strBuf.length << 1));
    nsHtml5ArrayCopy::arraycopy(strBuf, newBuf, strBuf.length);
    strBuf = newBuf;
  }
  strBuf[strBufLen++] = (char16_t) c;
}

void 
nsHtml5MetaScanner::handleAttributeValue()
{
  if (metaState != A) {
    return;
  }
  if (contentIndex == CONTENT.length && !content) {
    content = nsHtml5Portability::newStringFromBuffer(strBuf, 0, strBufLen, treeBuilder);
    return;
  }
  if (charsetIndex == CHARSET.length && !charset) {
    charset = nsHtml5Portability::newStringFromBuffer(strBuf, 0, strBufLen, treeBuilder);
    return;
  }
  if (httpEquivIndex == HTTP_EQUIV.length &&
      httpEquivState == HTTP_EQUIV_NOT_SEEN) {
    httpEquivState = (contentTypeIndex == CONTENT_TYPE.length)
                       ? HTTP_EQUIV_CONTENT_TYPE
                       : HTTP_EQUIV_OTHER;
    return;
  }
}

bool 
nsHtml5MetaScanner::handleTag()
{
  bool stop = handleTagInner();
  content.Release();
  content = nullptr;
  charset.Release();
  charset = nullptr;
  httpEquivState = HTTP_EQUIV_NOT_SEEN;
  return stop;
}

bool 
nsHtml5MetaScanner::handleTagInner()
{
  if (!!charset && tryCharset(charset)) {
    return true;
  }
  if (!!content && httpEquivState == HTTP_EQUIV_CONTENT_TYPE) {
    nsHtml5String extract =
      nsHtml5TreeBuilder::extractCharsetFromContent(content, treeBuilder);
    if (!extract) {
      return false;
    }
    bool success = tryCharset(extract);
    extract.Release();
    return success;
  }
  return false;
}

void
nsHtml5MetaScanner::initializeStatics()
{
}

void
nsHtml5MetaScanner::releaseStatics()
{
}


#include "nsHtml5MetaScannerCppSupplement.h"

