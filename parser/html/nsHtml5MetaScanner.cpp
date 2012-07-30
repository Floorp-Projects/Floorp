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

#define nsHtml5MetaScanner_cpp__

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

#include "nsHtml5Tokenizer.h"
#include "nsHtml5TreeBuilder.h"
#include "nsHtml5AttributeName.h"
#include "nsHtml5ElementName.h"
#include "nsHtml5HtmlAttributes.h"
#include "nsHtml5StackNode.h"
#include "nsHtml5UTF16Buffer.h"
#include "nsHtml5StateSnapshot.h"
#include "nsHtml5Portability.h"

#include "nsHtml5MetaScanner.h"

static PRUnichar const CHARSET_DATA[] = { 'h', 'a', 'r', 's', 'e', 't' };
staticJArray<PRUnichar,PRInt32> nsHtml5MetaScanner::CHARSET = { CHARSET_DATA, NS_ARRAY_LENGTH(CHARSET_DATA) };
static PRUnichar const CONTENT_DATA[] = { 'o', 'n', 't', 'e', 'n', 't' };
staticJArray<PRUnichar,PRInt32> nsHtml5MetaScanner::CONTENT = { CONTENT_DATA, NS_ARRAY_LENGTH(CONTENT_DATA) };
static PRUnichar const HTTP_EQUIV_DATA[] = { 't', 't', 'p', '-', 'e', 'q', 'u', 'i', 'v' };
staticJArray<PRUnichar,PRInt32> nsHtml5MetaScanner::HTTP_EQUIV = { HTTP_EQUIV_DATA, NS_ARRAY_LENGTH(HTTP_EQUIV_DATA) };
static PRUnichar const CONTENT_TYPE_DATA[] = { 'c', 'o', 'n', 't', 'e', 'n', 't', '-', 't', 'y', 'p', 'e' };
staticJArray<PRUnichar,PRInt32> nsHtml5MetaScanner::CONTENT_TYPE = { CONTENT_TYPE_DATA, NS_ARRAY_LENGTH(CONTENT_TYPE_DATA) };

nsHtml5MetaScanner::nsHtml5MetaScanner()
  : readable(nullptr),
    metaState(NS_HTML5META_SCANNER_NO),
    contentIndex(PR_INT32_MAX),
    charsetIndex(PR_INT32_MAX),
    httpEquivIndex(PR_INT32_MAX),
    contentTypeIndex(PR_INT32_MAX),
    stateSave(NS_HTML5META_SCANNER_DATA),
    strBufLen(0),
    strBuf(jArray<PRUnichar,PRInt32>::newJArray(36)),
    content(nullptr),
    charset(nullptr),
    httpEquivState(NS_HTML5META_SCANNER_HTTP_EQUIV_NOT_SEEN)
{
  MOZ_COUNT_CTOR(nsHtml5MetaScanner);
}


nsHtml5MetaScanner::~nsHtml5MetaScanner()
{
  MOZ_COUNT_DTOR(nsHtml5MetaScanner);
  nsHtml5Portability::releaseString(content);
  nsHtml5Portability::releaseString(charset);
}

void 
nsHtml5MetaScanner::stateLoop(PRInt32 state)
{
  PRInt32 c = -1;
  bool reconsume = false;
  stateloop: for (; ; ) {
    switch(state) {
      case NS_HTML5META_SCANNER_DATA: {
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
              state = NS_HTML5META_SCANNER_TAG_OPEN;
              NS_HTML5_BREAK(dataloop);
            }
            default: {
              continue;
            }
          }
        }
        dataloop_end: ;
      }
      case NS_HTML5META_SCANNER_TAG_OPEN: {
        for (; ; ) {
          c = read();
          switch(c) {
            case -1: {
              NS_HTML5_BREAK(stateloop);
            }
            case 'm':
            case 'M': {
              metaState = NS_HTML5META_SCANNER_M;
              state = NS_HTML5META_SCANNER_TAG_NAME;
              NS_HTML5_BREAK(tagopenloop);
            }
            case '!': {
              state = NS_HTML5META_SCANNER_MARKUP_DECLARATION_OPEN;
              NS_HTML5_CONTINUE(stateloop);
            }
            case '\?':
            case '/': {
              state = NS_HTML5META_SCANNER_SCAN_UNTIL_GT;
              NS_HTML5_CONTINUE(stateloop);
            }
            case '>': {
              state = NS_HTML5META_SCANNER_DATA;
              NS_HTML5_CONTINUE(stateloop);
            }
            default: {
              if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')) {
                metaState = NS_HTML5META_SCANNER_NO;
                state = NS_HTML5META_SCANNER_TAG_NAME;
                NS_HTML5_BREAK(tagopenloop);
              }
              state = NS_HTML5META_SCANNER_DATA;
              reconsume = true;
              NS_HTML5_CONTINUE(stateloop);
            }
          }
        }
        tagopenloop_end: ;
      }
      case NS_HTML5META_SCANNER_TAG_NAME: {
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
              state = NS_HTML5META_SCANNER_BEFORE_ATTRIBUTE_NAME;
              NS_HTML5_BREAK(tagnameloop);
            }
            case '/': {
              state = NS_HTML5META_SCANNER_SELF_CLOSING_START_TAG;
              NS_HTML5_CONTINUE(stateloop);
            }
            case '>': {
              state = NS_HTML5META_SCANNER_DATA;
              NS_HTML5_CONTINUE(stateloop);
            }
            case 'e':
            case 'E': {
              if (metaState == NS_HTML5META_SCANNER_M) {
                metaState = NS_HTML5META_SCANNER_E;
              } else {
                metaState = NS_HTML5META_SCANNER_NO;
              }
              continue;
            }
            case 't':
            case 'T': {
              if (metaState == NS_HTML5META_SCANNER_E) {
                metaState = NS_HTML5META_SCANNER_T;
              } else {
                metaState = NS_HTML5META_SCANNER_NO;
              }
              continue;
            }
            case 'a':
            case 'A': {
              if (metaState == NS_HTML5META_SCANNER_T) {
                metaState = NS_HTML5META_SCANNER_A;
              } else {
                metaState = NS_HTML5META_SCANNER_NO;
              }
              continue;
            }
            default: {
              metaState = NS_HTML5META_SCANNER_NO;
              continue;
            }
          }
        }
        tagnameloop_end: ;
      }
      case NS_HTML5META_SCANNER_BEFORE_ATTRIBUTE_NAME: {
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
              state = NS_HTML5META_SCANNER_SELF_CLOSING_START_TAG;
              NS_HTML5_CONTINUE(stateloop);
            }
            case '>': {
              if (handleTag()) {
                NS_HTML5_BREAK(stateloop);
              }
              state = NS_HTML5META_SCANNER_DATA;
              NS_HTML5_CONTINUE(stateloop);
            }
            case 'c':
            case 'C': {
              contentIndex = 0;
              charsetIndex = 0;
              httpEquivIndex = PR_INT32_MAX;
              contentTypeIndex = PR_INT32_MAX;
              state = NS_HTML5META_SCANNER_ATTRIBUTE_NAME;
              NS_HTML5_BREAK(beforeattributenameloop);
            }
            case 'h':
            case 'H': {
              contentIndex = PR_INT32_MAX;
              charsetIndex = PR_INT32_MAX;
              httpEquivIndex = 0;
              contentTypeIndex = PR_INT32_MAX;
              state = NS_HTML5META_SCANNER_ATTRIBUTE_NAME;
              NS_HTML5_BREAK(beforeattributenameloop);
            }
            default: {
              contentIndex = PR_INT32_MAX;
              charsetIndex = PR_INT32_MAX;
              httpEquivIndex = PR_INT32_MAX;
              contentTypeIndex = PR_INT32_MAX;
              state = NS_HTML5META_SCANNER_ATTRIBUTE_NAME;
              NS_HTML5_BREAK(beforeattributenameloop);
            }
          }
        }
        beforeattributenameloop_end: ;
      }
      case NS_HTML5META_SCANNER_ATTRIBUTE_NAME: {
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
              state = NS_HTML5META_SCANNER_AFTER_ATTRIBUTE_NAME;
              NS_HTML5_CONTINUE(stateloop);
            }
            case '/': {
              state = NS_HTML5META_SCANNER_SELF_CLOSING_START_TAG;
              NS_HTML5_CONTINUE(stateloop);
            }
            case '=': {
              strBufLen = 0;
              contentTypeIndex = 0;
              state = NS_HTML5META_SCANNER_BEFORE_ATTRIBUTE_VALUE;
              NS_HTML5_BREAK(attributenameloop);
            }
            case '>': {
              if (handleTag()) {
                NS_HTML5_BREAK(stateloop);
              }
              state = NS_HTML5META_SCANNER_DATA;
              NS_HTML5_CONTINUE(stateloop);
            }
            default: {
              if (metaState == NS_HTML5META_SCANNER_A) {
                if (c >= 'A' && c <= 'Z') {
                  c += 0x20;
                }
                if (contentIndex < CONTENT.length && c == CONTENT[contentIndex]) {
                  ++contentIndex;
                } else {
                  contentIndex = PR_INT32_MAX;
                }
                if (charsetIndex < CHARSET.length && c == CHARSET[charsetIndex]) {
                  ++charsetIndex;
                } else {
                  charsetIndex = PR_INT32_MAX;
                }
                if (httpEquivIndex < HTTP_EQUIV.length && c == HTTP_EQUIV[httpEquivIndex]) {
                  ++httpEquivIndex;
                } else {
                  httpEquivIndex = PR_INT32_MAX;
                }
              }
              continue;
            }
          }
        }
        attributenameloop_end: ;
      }
      case NS_HTML5META_SCANNER_BEFORE_ATTRIBUTE_VALUE: {
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
              state = NS_HTML5META_SCANNER_ATTRIBUTE_VALUE_DOUBLE_QUOTED;
              NS_HTML5_BREAK(beforeattributevalueloop);
            }
            case '\'': {
              state = NS_HTML5META_SCANNER_ATTRIBUTE_VALUE_SINGLE_QUOTED;
              NS_HTML5_CONTINUE(stateloop);
            }
            case '>': {
              if (handleTag()) {
                NS_HTML5_BREAK(stateloop);
              }
              state = NS_HTML5META_SCANNER_DATA;
              NS_HTML5_CONTINUE(stateloop);
            }
            default: {
              handleCharInAttributeValue(c);
              state = NS_HTML5META_SCANNER_ATTRIBUTE_VALUE_UNQUOTED;
              NS_HTML5_CONTINUE(stateloop);
            }
          }
        }
        beforeattributevalueloop_end: ;
      }
      case NS_HTML5META_SCANNER_ATTRIBUTE_VALUE_DOUBLE_QUOTED: {
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
              state = NS_HTML5META_SCANNER_AFTER_ATTRIBUTE_VALUE_QUOTED;
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
      case NS_HTML5META_SCANNER_AFTER_ATTRIBUTE_VALUE_QUOTED: {
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
              state = NS_HTML5META_SCANNER_BEFORE_ATTRIBUTE_NAME;
              NS_HTML5_CONTINUE(stateloop);
            }
            case '/': {
              state = NS_HTML5META_SCANNER_SELF_CLOSING_START_TAG;
              NS_HTML5_BREAK(afterattributevaluequotedloop);
            }
            case '>': {
              if (handleTag()) {
                NS_HTML5_BREAK(stateloop);
              }
              state = NS_HTML5META_SCANNER_DATA;
              NS_HTML5_CONTINUE(stateloop);
            }
            default: {
              state = NS_HTML5META_SCANNER_BEFORE_ATTRIBUTE_NAME;
              reconsume = true;
              NS_HTML5_CONTINUE(stateloop);
            }
          }
        }
        afterattributevaluequotedloop_end: ;
      }
      case NS_HTML5META_SCANNER_SELF_CLOSING_START_TAG: {
        c = read();
        switch(c) {
          case -1: {
            NS_HTML5_BREAK(stateloop);
          }
          case '>': {
            if (handleTag()) {
              NS_HTML5_BREAK(stateloop);
            }
            state = NS_HTML5META_SCANNER_DATA;
            NS_HTML5_CONTINUE(stateloop);
          }
          default: {
            state = NS_HTML5META_SCANNER_BEFORE_ATTRIBUTE_NAME;
            reconsume = true;
            NS_HTML5_CONTINUE(stateloop);
          }
        }
      }
      case NS_HTML5META_SCANNER_ATTRIBUTE_VALUE_UNQUOTED: {
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
              state = NS_HTML5META_SCANNER_BEFORE_ATTRIBUTE_NAME;
              NS_HTML5_CONTINUE(stateloop);
            }
            case '>': {
              handleAttributeValue();
              if (handleTag()) {
                NS_HTML5_BREAK(stateloop);
              }
              state = NS_HTML5META_SCANNER_DATA;
              NS_HTML5_CONTINUE(stateloop);
            }
            default: {
              handleCharInAttributeValue(c);
              continue;
            }
          }
        }
      }
      case NS_HTML5META_SCANNER_AFTER_ATTRIBUTE_NAME: {
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
              state = NS_HTML5META_SCANNER_SELF_CLOSING_START_TAG;
              NS_HTML5_CONTINUE(stateloop);
            }
            case '=': {
              strBufLen = 0;
              contentTypeIndex = 0;
              state = NS_HTML5META_SCANNER_BEFORE_ATTRIBUTE_VALUE;
              NS_HTML5_CONTINUE(stateloop);
            }
            case '>': {
              handleAttributeValue();
              if (handleTag()) {
                NS_HTML5_BREAK(stateloop);
              }
              state = NS_HTML5META_SCANNER_DATA;
              NS_HTML5_CONTINUE(stateloop);
            }
            case 'c':
            case 'C': {
              contentIndex = 0;
              charsetIndex = 0;
              state = NS_HTML5META_SCANNER_ATTRIBUTE_NAME;
              NS_HTML5_CONTINUE(stateloop);
            }
            default: {
              contentIndex = PR_INT32_MAX;
              charsetIndex = PR_INT32_MAX;
              state = NS_HTML5META_SCANNER_ATTRIBUTE_NAME;
              NS_HTML5_CONTINUE(stateloop);
            }
          }
        }
      }
      case NS_HTML5META_SCANNER_MARKUP_DECLARATION_OPEN: {
        for (; ; ) {
          c = read();
          switch(c) {
            case -1: {
              NS_HTML5_BREAK(stateloop);
            }
            case '-': {
              state = NS_HTML5META_SCANNER_MARKUP_DECLARATION_HYPHEN;
              NS_HTML5_BREAK(markupdeclarationopenloop);
            }
            default: {
              state = NS_HTML5META_SCANNER_SCAN_UNTIL_GT;
              reconsume = true;
              NS_HTML5_CONTINUE(stateloop);
            }
          }
        }
        markupdeclarationopenloop_end: ;
      }
      case NS_HTML5META_SCANNER_MARKUP_DECLARATION_HYPHEN: {
        for (; ; ) {
          c = read();
          switch(c) {
            case -1: {
              NS_HTML5_BREAK(stateloop);
            }
            case '-': {
              state = NS_HTML5META_SCANNER_COMMENT_START;
              NS_HTML5_BREAK(markupdeclarationhyphenloop);
            }
            default: {
              state = NS_HTML5META_SCANNER_SCAN_UNTIL_GT;
              reconsume = true;
              NS_HTML5_CONTINUE(stateloop);
            }
          }
        }
        markupdeclarationhyphenloop_end: ;
      }
      case NS_HTML5META_SCANNER_COMMENT_START: {
        for (; ; ) {
          c = read();
          switch(c) {
            case -1: {
              NS_HTML5_BREAK(stateloop);
            }
            case '-': {
              state = NS_HTML5META_SCANNER_COMMENT_START_DASH;
              NS_HTML5_CONTINUE(stateloop);
            }
            case '>': {
              state = NS_HTML5META_SCANNER_DATA;
              NS_HTML5_CONTINUE(stateloop);
            }
            default: {
              state = NS_HTML5META_SCANNER_COMMENT;
              NS_HTML5_BREAK(commentstartloop);
            }
          }
        }
        commentstartloop_end: ;
      }
      case NS_HTML5META_SCANNER_COMMENT: {
        for (; ; ) {
          c = read();
          switch(c) {
            case -1: {
              NS_HTML5_BREAK(stateloop);
            }
            case '-': {
              state = NS_HTML5META_SCANNER_COMMENT_END_DASH;
              NS_HTML5_BREAK(commentloop);
            }
            default: {
              continue;
            }
          }
        }
        commentloop_end: ;
      }
      case NS_HTML5META_SCANNER_COMMENT_END_DASH: {
        for (; ; ) {
          c = read();
          switch(c) {
            case -1: {
              NS_HTML5_BREAK(stateloop);
            }
            case '-': {
              state = NS_HTML5META_SCANNER_COMMENT_END;
              NS_HTML5_BREAK(commentenddashloop);
            }
            default: {
              state = NS_HTML5META_SCANNER_COMMENT;
              NS_HTML5_CONTINUE(stateloop);
            }
          }
        }
        commentenddashloop_end: ;
      }
      case NS_HTML5META_SCANNER_COMMENT_END: {
        for (; ; ) {
          c = read();
          switch(c) {
            case -1: {
              NS_HTML5_BREAK(stateloop);
            }
            case '>': {
              state = NS_HTML5META_SCANNER_DATA;
              NS_HTML5_CONTINUE(stateloop);
            }
            case '-': {
              continue;
            }
            default: {
              state = NS_HTML5META_SCANNER_COMMENT;
              NS_HTML5_CONTINUE(stateloop);
            }
          }
        }
      }
      case NS_HTML5META_SCANNER_COMMENT_START_DASH: {
        c = read();
        switch(c) {
          case -1: {
            NS_HTML5_BREAK(stateloop);
          }
          case '-': {
            state = NS_HTML5META_SCANNER_COMMENT_END;
            NS_HTML5_CONTINUE(stateloop);
          }
          case '>': {
            state = NS_HTML5META_SCANNER_DATA;
            NS_HTML5_CONTINUE(stateloop);
          }
          default: {
            state = NS_HTML5META_SCANNER_COMMENT;
            NS_HTML5_CONTINUE(stateloop);
          }
        }
      }
      case NS_HTML5META_SCANNER_ATTRIBUTE_VALUE_SINGLE_QUOTED: {
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
              state = NS_HTML5META_SCANNER_AFTER_ATTRIBUTE_VALUE_QUOTED;
              NS_HTML5_CONTINUE(stateloop);
            }
            default: {
              handleCharInAttributeValue(c);
              continue;
            }
          }
        }
      }
      case NS_HTML5META_SCANNER_SCAN_UNTIL_GT: {
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
              state = NS_HTML5META_SCANNER_DATA;
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
nsHtml5MetaScanner::handleCharInAttributeValue(PRInt32 c)
{
  if (metaState == NS_HTML5META_SCANNER_A) {
    if (contentIndex == CONTENT.length || charsetIndex == CHARSET.length) {
      addToBuffer(c);
    } else if (httpEquivIndex == HTTP_EQUIV.length) {
      if (contentTypeIndex < CONTENT_TYPE.length && toAsciiLowerCase(c) == CONTENT_TYPE[contentTypeIndex]) {
        ++contentTypeIndex;
      } else {
        contentTypeIndex = PR_INT32_MAX;
      }
    }
  }
}

void 
nsHtml5MetaScanner::addToBuffer(PRInt32 c)
{
  if (strBufLen == strBuf.length) {
    jArray<PRUnichar,PRInt32> newBuf = jArray<PRUnichar,PRInt32>::newJArray(strBuf.length + (strBuf.length << 1));
    nsHtml5ArrayCopy::arraycopy(strBuf, newBuf, strBuf.length);
    strBuf = newBuf;
  }
  strBuf[strBufLen++] = (PRUnichar) c;
}

void 
nsHtml5MetaScanner::handleAttributeValue()
{
  if (metaState != NS_HTML5META_SCANNER_A) {
    return;
  }
  if (contentIndex == CONTENT.length && !content) {
    content = nsHtml5Portability::newStringFromBuffer(strBuf, 0, strBufLen);
    return;
  }
  if (charsetIndex == CHARSET.length && !charset) {
    charset = nsHtml5Portability::newStringFromBuffer(strBuf, 0, strBufLen);
    return;
  }
  if (httpEquivIndex == HTTP_EQUIV.length && httpEquivState == NS_HTML5META_SCANNER_HTTP_EQUIV_NOT_SEEN) {
    httpEquivState = (contentTypeIndex == CONTENT_TYPE.length) ? NS_HTML5META_SCANNER_HTTP_EQUIV_CONTENT_TYPE : NS_HTML5META_SCANNER_HTTP_EQUIV_OTHER;
    return;
  }
}

bool 
nsHtml5MetaScanner::handleTag()
{
  bool stop = handleTagInner();
  nsHtml5Portability::releaseString(content);
  content = nullptr;
  nsHtml5Portability::releaseString(charset);
  charset = nullptr;
  httpEquivState = NS_HTML5META_SCANNER_HTTP_EQUIV_NOT_SEEN;
  return stop;
}

bool 
nsHtml5MetaScanner::handleTagInner()
{
  if (!!charset && tryCharset(charset)) {
    return true;
  }
  if (!!content && httpEquivState == NS_HTML5META_SCANNER_HTTP_EQUIV_CONTENT_TYPE) {
    nsString* extract = nsHtml5TreeBuilder::extractCharsetFromContent(content);
    if (!extract) {
      return false;
    }
    bool success = tryCharset(extract);
    nsHtml5Portability::releaseString(extract);
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

