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
#include "nsIDocument.h"
#include "nsTraceRefcnt.h"
#include "jArray.h"
#include "nsHtml5DocumentMode.h"
#include "nsHtml5ArrayCopy.h"
#include "nsHtml5NamedCharacters.h"
#include "nsHtml5Atoms.h"
#include "nsHtml5ByteReadable.h"
#include "nsIUnicodeDecoder.h"
#include "nsAHtml5TreeBuilderState.h"
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

void 
nsHtml5MetaScanner::stateLoop(PRInt32 state)
{
  PRInt32 c = -1;
  PRBool reconsume = PR_FALSE;
  stateloop: for (; ; ) {
    switch(state) {
      case NS_HTML5META_SCANNER_DATA: {
        for (; ; ) {
          if (reconsume) {
            reconsume = PR_FALSE;
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
              reconsume = PR_TRUE;
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
            reconsume = PR_FALSE;
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
              state = NS_HTML5META_SCANNER_DATA;
              NS_HTML5_CONTINUE(stateloop);
            }
            case 'c':
            case 'C': {
              contentIndex = 0;
              charsetIndex = 0;
              state = NS_HTML5META_SCANNER_ATTRIBUTE_NAME;
              NS_HTML5_BREAK(beforeattributenameloop);
            }
            default: {
              contentIndex = -1;
              charsetIndex = -1;
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
              state = NS_HTML5META_SCANNER_BEFORE_ATTRIBUTE_VALUE;
              NS_HTML5_BREAK(attributenameloop);
            }
            case '>': {
              state = NS_HTML5META_SCANNER_DATA;
              NS_HTML5_CONTINUE(stateloop);
            }
            default: {
              if (metaState == NS_HTML5META_SCANNER_A) {
                if (c >= 'A' && c <= 'Z') {
                  c += 0x20;
                }
                if (contentIndex == 6) {
                  contentIndex = -1;
                } else if (contentIndex > -1 && contentIndex < 6 && (c == CONTENT[contentIndex + 1])) {
                  contentIndex++;
                }
                if (charsetIndex == 6) {
                  charsetIndex = -1;
                } else if (charsetIndex > -1 && charsetIndex < 6 && (c == CHARSET[charsetIndex + 1])) {
                  charsetIndex++;
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
              state = NS_HTML5META_SCANNER_DATA;
              NS_HTML5_CONTINUE(stateloop);
            }
            default: {
              if (charsetIndex == 6 || contentIndex == 6) {
                addToBuffer(c);
              }
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
            reconsume = PR_FALSE;
          } else {
            c = read();
          }
          switch(c) {
            case -1: {
              NS_HTML5_BREAK(stateloop);
            }
            case '\"': {
              if (tryCharset()) {
                NS_HTML5_BREAK(stateloop);
              }
              state = NS_HTML5META_SCANNER_AFTER_ATTRIBUTE_VALUE_QUOTED;
              NS_HTML5_BREAK(attributevaluedoublequotedloop);
            }
            default: {
              if (metaState == NS_HTML5META_SCANNER_A && (contentIndex == 6 || charsetIndex == 6)) {
                addToBuffer(c);
              }
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
              state = NS_HTML5META_SCANNER_DATA;
              NS_HTML5_CONTINUE(stateloop);
            }
            default: {
              state = NS_HTML5META_SCANNER_BEFORE_ATTRIBUTE_NAME;
              reconsume = PR_TRUE;
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
            state = NS_HTML5META_SCANNER_DATA;
            NS_HTML5_CONTINUE(stateloop);
          }
          default: {
            state = NS_HTML5META_SCANNER_BEFORE_ATTRIBUTE_NAME;
            reconsume = PR_TRUE;
            NS_HTML5_CONTINUE(stateloop);
          }
        }
      }
      case NS_HTML5META_SCANNER_ATTRIBUTE_VALUE_UNQUOTED: {
        for (; ; ) {
          if (reconsume) {
            reconsume = PR_FALSE;
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
              if (tryCharset()) {
                NS_HTML5_BREAK(stateloop);
              }
              state = NS_HTML5META_SCANNER_BEFORE_ATTRIBUTE_NAME;
              NS_HTML5_CONTINUE(stateloop);
            }
            case '>': {
              if (tryCharset()) {
                NS_HTML5_BREAK(stateloop);
              }
              state = NS_HTML5META_SCANNER_DATA;
              NS_HTML5_CONTINUE(stateloop);
            }
            default: {
              if (metaState == NS_HTML5META_SCANNER_A && (contentIndex == 6 || charsetIndex == 6)) {
                addToBuffer(c);
              }
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
              if (tryCharset()) {
                NS_HTML5_BREAK(stateloop);
              }
              state = NS_HTML5META_SCANNER_SELF_CLOSING_START_TAG;
              NS_HTML5_CONTINUE(stateloop);
            }
            case '=': {
              state = NS_HTML5META_SCANNER_BEFORE_ATTRIBUTE_VALUE;
              NS_HTML5_CONTINUE(stateloop);
            }
            case '>': {
              if (tryCharset()) {
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
              contentIndex = -1;
              charsetIndex = -1;
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
              reconsume = PR_TRUE;
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
              reconsume = PR_TRUE;
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
            reconsume = PR_FALSE;
          } else {
            c = read();
          }
          switch(c) {
            case -1: {
              NS_HTML5_BREAK(stateloop);
            }
            case '\'': {
              if (tryCharset()) {
                NS_HTML5_BREAK(stateloop);
              }
              state = NS_HTML5META_SCANNER_AFTER_ATTRIBUTE_VALUE_QUOTED;
              NS_HTML5_CONTINUE(stateloop);
            }
            default: {
              if (metaState == NS_HTML5META_SCANNER_A && (contentIndex == 6 || charsetIndex == 6)) {
                addToBuffer(c);
              }
              continue;
            }
          }
        }
      }
      case NS_HTML5META_SCANNER_SCAN_UNTIL_GT: {
        for (; ; ) {
          if (reconsume) {
            reconsume = PR_FALSE;
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
nsHtml5MetaScanner::addToBuffer(PRInt32 c)
{
  if (strBufLen == strBuf.length) {
    jArray<PRUnichar,PRInt32> newBuf = jArray<PRUnichar,PRInt32>(strBuf.length + (strBuf.length << 1));
    nsHtml5ArrayCopy::arraycopy(strBuf, newBuf, strBuf.length);
    strBuf.release();
    strBuf = newBuf;
  }
  strBuf[strBufLen++] = (PRUnichar) c;
}

PRBool 
nsHtml5MetaScanner::tryCharset()
{
  if (metaState != NS_HTML5META_SCANNER_A || !(contentIndex == 6 || charsetIndex == 6)) {
    return PR_FALSE;
  }
  nsString* attVal = nsHtml5Portability::newStringFromBuffer(strBuf, 0, strBufLen);
  nsString* candidateEncoding;
  if (contentIndex == 6) {
    candidateEncoding = nsHtml5TreeBuilder::extractCharsetFromContent(attVal);
    nsHtml5Portability::releaseString(attVal);
  } else {
    candidateEncoding = attVal;
  }
  if (!candidateEncoding) {
    return PR_FALSE;
  }
  PRBool success = tryCharset(candidateEncoding);
  nsHtml5Portability::releaseString(candidateEncoding);
  contentIndex = -1;
  charsetIndex = -1;
  return success;
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

