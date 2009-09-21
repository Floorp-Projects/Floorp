/*
 * Copyright (c) 2005-2007 Henri Sivonen
 * Copyright (c) 2007-2009 Mozilla Foundation
 * Portions of comments Copyright 2004-2008 Apple Computer, Inc., Mozilla 
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

#define nsHtml5Tokenizer_cpp__

#include "prtypes.h"
#include "nsIAtom.h"
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

#include "nsHtml5TreeBuilder.h"
#include "nsHtml5MetaScanner.h"
#include "nsHtml5AttributeName.h"
#include "nsHtml5ElementName.h"
#include "nsHtml5HtmlAttributes.h"
#include "nsHtml5StackNode.h"
#include "nsHtml5UTF16Buffer.h"
#include "nsHtml5StateSnapshot.h"
#include "nsHtml5Portability.h"

#include "nsHtml5Tokenizer.h"

static PRUnichar const TITLE_ARR_DATA[] = { 't', 'i', 't', 'l', 'e' };
static PRUnichar const SCRIPT_ARR_DATA[] = { 's', 'c', 'r', 'i', 'p', 't' };
static PRUnichar const STYLE_ARR_DATA[] = { 's', 't', 'y', 'l', 'e' };
static PRUnichar const PLAINTEXT_ARR_DATA[] = { 'p', 'l', 'a', 'i', 'n', 't', 'e', 'x', 't' };
static PRUnichar const XMP_ARR_DATA[] = { 'x', 'm', 'p' };
static PRUnichar const TEXTAREA_ARR_DATA[] = { 't', 'e', 'x', 't', 'a', 'r', 'e', 'a' };
static PRUnichar const IFRAME_ARR_DATA[] = { 'i', 'f', 'r', 'a', 'm', 'e' };
static PRUnichar const NOEMBED_ARR_DATA[] = { 'n', 'o', 'e', 'm', 'b', 'e', 'd' };
static PRUnichar const NOSCRIPT_ARR_DATA[] = { 'n', 'o', 's', 'c', 'r', 'i', 'p', 't' };
static PRUnichar const NOFRAMES_ARR_DATA[] = { 'n', 'o', 'f', 'r', 'a', 'm', 'e', 's' };

nsHtml5Tokenizer::nsHtml5Tokenizer(nsHtml5TreeBuilder* tokenHandler)
  : tokenHandler(tokenHandler),
    encodingDeclarationHandler(nsnull),
    bmpChar(jArray<PRUnichar,PRInt32>(1)),
    astralChar(jArray<PRUnichar,PRInt32>(2))
{
  MOZ_COUNT_CTOR(nsHtml5Tokenizer);
}

void 
nsHtml5Tokenizer::initLocation(nsString* newPublicId, nsString* newSystemId)
{
  this->systemId = newSystemId;
  this->publicId = newPublicId;
}


nsHtml5Tokenizer::~nsHtml5Tokenizer()
{
  MOZ_COUNT_DTOR(nsHtml5Tokenizer);
  bmpChar.release();
  astralChar.release();
}

void 
nsHtml5Tokenizer::setContentModelFlag(PRInt32 contentModelFlag, nsIAtom* contentModelElement)
{
  this->stateSave = contentModelFlag;
  if (contentModelFlag == NS_HTML5TOKENIZER_DATA) {
    return;
  }
  jArray<PRUnichar,PRInt32> asArray = nsHtml5Portability::newCharArrayFromLocal(contentModelElement);
  this->contentModelElement = nsHtml5ElementName::elementNameByBuffer(asArray, 0, asArray.length);
  asArray.release();
  contentModelElementToArray();
}

void 
nsHtml5Tokenizer::setContentModelFlag(PRInt32 contentModelFlag, nsHtml5ElementName* contentModelElement)
{
  this->stateSave = contentModelFlag;
  this->contentModelElement = contentModelElement;
  contentModelElementToArray();
}

void 
nsHtml5Tokenizer::contentModelElementToArray()
{
  switch(contentModelElement->group) {
    case NS_HTML5TREE_BUILDER_TITLE: {
      contentModelElementNameAsArray = TITLE_ARR;
      return;
    }
    case NS_HTML5TREE_BUILDER_SCRIPT: {
      contentModelElementNameAsArray = SCRIPT_ARR;
      return;
    }
    case NS_HTML5TREE_BUILDER_STYLE: {
      contentModelElementNameAsArray = STYLE_ARR;
      return;
    }
    case NS_HTML5TREE_BUILDER_PLAINTEXT: {
      contentModelElementNameAsArray = PLAINTEXT_ARR;
      return;
    }
    case NS_HTML5TREE_BUILDER_XMP: {
      contentModelElementNameAsArray = XMP_ARR;
      return;
    }
    case NS_HTML5TREE_BUILDER_TEXTAREA: {
      contentModelElementNameAsArray = TEXTAREA_ARR;
      return;
    }
    case NS_HTML5TREE_BUILDER_IFRAME: {
      contentModelElementNameAsArray = IFRAME_ARR;
      return;
    }
    case NS_HTML5TREE_BUILDER_NOEMBED: {
      contentModelElementNameAsArray = NOEMBED_ARR;
      return;
    }
    case NS_HTML5TREE_BUILDER_NOSCRIPT: {
      contentModelElementNameAsArray = NOSCRIPT_ARR;
      return;
    }
    case NS_HTML5TREE_BUILDER_NOFRAMES: {
      contentModelElementNameAsArray = NOFRAMES_ARR;
      return;
    }
    default: {

      return;
    }
  }
}

void 
nsHtml5Tokenizer::setLineNumber(PRInt32 line)
{
  this->line = line;
}

nsHtml5HtmlAttributes* 
nsHtml5Tokenizer::emptyAttributes()
{
  return nsHtml5HtmlAttributes::EMPTY_ATTRIBUTES;
}

void 
nsHtml5Tokenizer::clearStrBufAndAppendCurrentC(PRUnichar c)
{
  strBuf[0] = c;
  strBufLen = 1;
}

void 
nsHtml5Tokenizer::clearStrBufAndAppendForceWrite(PRUnichar c)
{
  strBuf[0] = c;
  strBufLen = 1;
}

void 
nsHtml5Tokenizer::clearStrBufForNextState()
{
  strBufLen = 0;
}

void 
nsHtml5Tokenizer::appendStrBuf(PRUnichar c)
{
  if (strBufLen == strBuf.length) {
    jArray<PRUnichar,PRInt32> newBuf = jArray<PRUnichar,PRInt32>(strBuf.length + NS_HTML5TOKENIZER_BUFFER_GROW_BY);
    nsHtml5ArrayCopy::arraycopy(strBuf, newBuf, strBuf.length);
    strBuf.release();
    strBuf = newBuf;
  }
  strBuf[strBufLen++] = c;
}

void 
nsHtml5Tokenizer::appendStrBufForceWrite(PRUnichar c)
{
  if (strBufLen == strBuf.length) {
    jArray<PRUnichar,PRInt32> newBuf = jArray<PRUnichar,PRInt32>(strBuf.length + NS_HTML5TOKENIZER_BUFFER_GROW_BY);
    nsHtml5ArrayCopy::arraycopy(strBuf, newBuf, strBuf.length);
    strBuf.release();
    strBuf = newBuf;
  }
  strBuf[strBufLen++] = c;
}

nsString* 
nsHtml5Tokenizer::strBufToString()
{
  return nsHtml5Portability::newStringFromBuffer(strBuf, 0, strBufLen);
}

void 
nsHtml5Tokenizer::strBufToDoctypeName()
{
  doctypeName = nsHtml5Portability::newLocalNameFromBuffer(strBuf, 0, strBufLen);
}

void 
nsHtml5Tokenizer::emitStrBuf()
{
  if (strBufLen > 0) {
    tokenHandler->characters(strBuf, 0, strBufLen);
  }
}

void 
nsHtml5Tokenizer::clearLongStrBufForNextState()
{
  longStrBufLen = 0;
}

void 
nsHtml5Tokenizer::clearLongStrBuf()
{
  longStrBufLen = 0;
}

void 
nsHtml5Tokenizer::clearLongStrBufAndAppendCurrentC(PRUnichar c)
{
  longStrBuf[0] = c;
  longStrBufLen = 1;
}

void 
nsHtml5Tokenizer::clearLongStrBufAndAppendToComment(PRUnichar c)
{
  longStrBuf[0] = c;
  longStrBufLen = 1;
}

void 
nsHtml5Tokenizer::appendLongStrBuf(PRUnichar c)
{
  if (longStrBufLen == longStrBuf.length) {
    jArray<PRUnichar,PRInt32> newBuf = jArray<PRUnichar,PRInt32>(longStrBufLen + (longStrBufLen >> 1));
    nsHtml5ArrayCopy::arraycopy(longStrBuf, newBuf, longStrBuf.length);
    longStrBuf.release();
    longStrBuf = newBuf;
  }
  longStrBuf[longStrBufLen++] = c;
}

void 
nsHtml5Tokenizer::appendSecondHyphenToBogusComment()
{
  appendLongStrBuf('-');
}

void 
nsHtml5Tokenizer::adjustDoubleHyphenAndAppendToLongStrBufAndErr(PRUnichar c)
{

  appendLongStrBuf(c);
}

void 
nsHtml5Tokenizer::appendLongStrBuf(jArray<PRUnichar,PRInt32> buffer, PRInt32 offset, PRInt32 length)
{
  PRInt32 reqLen = longStrBufLen + length;
  if (longStrBuf.length < reqLen) {
    jArray<PRUnichar,PRInt32> newBuf = jArray<PRUnichar,PRInt32>(reqLen + (reqLen >> 1));
    nsHtml5ArrayCopy::arraycopy(longStrBuf, newBuf, longStrBuf.length);
    longStrBuf.release();
    longStrBuf = newBuf;
  }
  nsHtml5ArrayCopy::arraycopy(buffer, offset, longStrBuf, longStrBufLen, length);
  longStrBufLen = reqLen;
}

void 
nsHtml5Tokenizer::appendLongStrBuf(jArray<PRUnichar,PRInt32> arr)
{
  appendLongStrBuf(arr, 0, arr.length);
}

void 
nsHtml5Tokenizer::appendStrBufToLongStrBuf()
{
  appendLongStrBuf(strBuf, 0, strBufLen);
}

nsString* 
nsHtml5Tokenizer::longStrBufToString()
{
  return nsHtml5Portability::newStringFromBuffer(longStrBuf, 0, longStrBufLen);
}

void 
nsHtml5Tokenizer::emitComment(PRInt32 provisionalHyphens, PRInt32 pos)
{
  tokenHandler->comment(longStrBuf, 0, longStrBufLen - provisionalHyphens);
  cstart = pos + 1;
}

void 
nsHtml5Tokenizer::flushChars(PRUnichar* buf, PRInt32 pos)
{
  if (pos > cstart) {
    tokenHandler->characters(buf, cstart, pos - cstart);
  }
  cstart = 0x7fffffff;
}

void 
nsHtml5Tokenizer::resetAttributes()
{
  attributes->clear(0);
}

void 
nsHtml5Tokenizer::strBufToElementNameString()
{
  tagName = nsHtml5ElementName::elementNameByBuffer(strBuf, 0, strBufLen);
}

PRInt32 
nsHtml5Tokenizer::emitCurrentTagToken(PRBool selfClosing, PRInt32 pos)
{
  cstart = pos + 1;

  stateSave = NS_HTML5TOKENIZER_DATA;
  nsHtml5HtmlAttributes* attrs = (!attributes ? nsHtml5HtmlAttributes::EMPTY_ATTRIBUTES : attributes);
  if (endTag) {

    tokenHandler->endTag(tagName);
  } else {
    tokenHandler->startTag(tagName, attrs, selfClosing);
  }
  resetAttributes();
  return stateSave;
}

void 
nsHtml5Tokenizer::attributeNameComplete()
{
  attributeName = nsHtml5AttributeName::nameByBuffer(strBuf, 0, strBufLen);
  if (attributes->contains(attributeName)) {

    attributeName->release();
    attributeName = nsnull;
  }
}

void 
nsHtml5Tokenizer::addAttributeWithoutValue()
{
  if (!!attributeName) {
    attributes->addAttribute(attributeName, nsHtml5Portability::newEmptyString());
  }
}

void 
nsHtml5Tokenizer::addAttributeWithValue()
{
  if (!!attributeName) {
    nsString* value = longStrBufToString();
    attributes->addAttribute(attributeName, value);
  }
}

void 
nsHtml5Tokenizer::startErrorReporting()
{
}

void 
nsHtml5Tokenizer::start()
{
  confident = PR_FALSE;
  strBuf = jArray<PRUnichar,PRInt32>(64);
  strBufLen = 0;
  longStrBuf = jArray<PRUnichar,PRInt32>(1024);
  longStrBufLen = 0;
  stateSave = NS_HTML5TOKENIZER_DATA;
  line = 1;
  lastCR = PR_FALSE;
  tokenHandler->startTokenization(this);
  index = 0;
  forceQuirks = PR_FALSE;
  additional = '\0';
  entCol = -1;
  lo = 0;
  hi = (nsHtml5NamedCharacters::NAMES.length - 1);
  candidate = -1;
  strBufMark = 0;
  prevValue = -1;
  value = 0;
  seenDigits = PR_FALSE;
  shouldSuspend = PR_FALSE;
  attributes = new nsHtml5HtmlAttributes(0);
}

PRBool 
nsHtml5Tokenizer::tokenizeBuffer(nsHtml5UTF16Buffer* buffer)
{
  PRInt32 state = stateSave;
  PRInt32 returnState = returnStateSave;
  PRUnichar c = '\0';
  shouldSuspend = PR_FALSE;
  lastCR = PR_FALSE;
  PRInt32 start = buffer->getStart();
  PRInt32 pos = start - 1;
  switch(state) {
    case NS_HTML5TOKENIZER_DATA:
    case NS_HTML5TOKENIZER_RCDATA:
    case NS_HTML5TOKENIZER_CDATA:
    case NS_HTML5TOKENIZER_PLAINTEXT:
    case NS_HTML5TOKENIZER_CDATA_SECTION:
    case NS_HTML5TOKENIZER_ESCAPE:
    case NS_HTML5TOKENIZER_ESCAPE_EXCLAMATION:
    case NS_HTML5TOKENIZER_ESCAPE_EXCLAMATION_HYPHEN:
    case NS_HTML5TOKENIZER_ESCAPE_HYPHEN:
    case NS_HTML5TOKENIZER_ESCAPE_HYPHEN_HYPHEN: {
      cstart = start;
      break;
    }
    default: {
      cstart = PR_INT32_MAX;
      break;
    }
  }
  pos = stateLoop(state, c, pos, buffer->getBuffer(), PR_FALSE, returnState, buffer->getEnd());
  if (pos == buffer->getEnd()) {
    buffer->setStart(pos);
  } else {
    buffer->setStart(pos + 1);
  }
  return lastCR;
}

PRInt32 
nsHtml5Tokenizer::stateLoop(PRInt32 state, PRUnichar c, PRInt32 pos, PRUnichar* buf, PRBool reconsume, PRInt32 returnState, PRInt32 endPos)
{
  stateloop: for (; ; ) {
    switch(state) {
      case NS_HTML5TOKENIZER_DATA: {
        for (; ; ) {
          if (reconsume) {
            reconsume = PR_FALSE;
          } else {
            if (++pos == endPos) {
              goto stateloop_end;
            }
            c = checkChar(buf, pos);
          }
          switch(c) {
            case '&': {
              flushChars(buf, pos);
              clearStrBufAndAppendCurrentC(c);
              rememberAmpersandLocation('\0');
              returnState = state;
              state = NS_HTML5TOKENIZER_CONSUME_CHARACTER_REFERENCE;
              goto stateloop;
            }
            case '<': {
              flushChars(buf, pos);
              state = NS_HTML5TOKENIZER_TAG_OPEN;
              goto dataloop_end;
            }
            case '\0': {
              emitReplacementCharacter(buf, pos);
              continue;
            }
            case '\r': {
              emitCarriageReturn(buf, pos);
              goto stateloop_end;
            }
            case '\n': {
              silentLineFeed();
            }
            default: {
              continue;
            }
          }
        }
        dataloop_end: ;
      }
      case NS_HTML5TOKENIZER_TAG_OPEN: {
        for (; ; ) {
          if (++pos == endPos) {
            goto stateloop_end;
          }
          c = checkChar(buf, pos);
          if (c >= 'A' && c <= 'Z') {
            endTag = PR_FALSE;
            clearStrBufAndAppendForceWrite((PRUnichar) (c + 0x20));
            state = NS_HTML5TOKENIZER_TAG_NAME;
            goto tagopenloop_end;
          } else if (c >= 'a' && c <= 'z') {
            endTag = PR_FALSE;
            clearStrBufAndAppendCurrentC(c);
            state = NS_HTML5TOKENIZER_TAG_NAME;
            goto tagopenloop_end;
          }
          switch(c) {
            case '!': {
              state = NS_HTML5TOKENIZER_MARKUP_DECLARATION_OPEN;
              goto stateloop;
            }
            case '/': {
              state = NS_HTML5TOKENIZER_CLOSE_TAG_OPEN_PCDATA;
              goto stateloop;
            }
            case '\?': {

              clearLongStrBufAndAppendToComment(c);
              state = NS_HTML5TOKENIZER_BOGUS_COMMENT;
              goto stateloop;
            }
            case '>': {

              tokenHandler->characters(nsHtml5Tokenizer::LT_GT, 0, 2);
              cstart = pos + 1;
              state = NS_HTML5TOKENIZER_DATA;
              goto stateloop;
            }
            default: {

              tokenHandler->characters(nsHtml5Tokenizer::LT_GT, 0, 1);
              cstart = pos;
              state = NS_HTML5TOKENIZER_DATA;
              reconsume = PR_TRUE;
              goto stateloop;
            }
          }
        }
        tagopenloop_end: ;
      }
      case NS_HTML5TOKENIZER_TAG_NAME: {
        for (; ; ) {
          if (++pos == endPos) {
            goto stateloop_end;
          }
          c = checkChar(buf, pos);
          switch(c) {
            case '\r': {
              silentCarriageReturn();
              strBufToElementNameString();
              state = NS_HTML5TOKENIZER_BEFORE_ATTRIBUTE_NAME;
              goto stateloop_end;
            }
            case '\n': {
              silentLineFeed();
            }
            case ' ':
            case '\t':
            case '\f': {
              strBufToElementNameString();
              state = NS_HTML5TOKENIZER_BEFORE_ATTRIBUTE_NAME;
              goto tagnameloop_end;
            }
            case '/': {
              strBufToElementNameString();
              state = NS_HTML5TOKENIZER_SELF_CLOSING_START_TAG;
              goto stateloop;
            }
            case '>': {
              strBufToElementNameString();
              state = emitCurrentTagToken(PR_FALSE, pos);
              if (shouldSuspend) {
                goto stateloop_end;
              }
              goto stateloop;
            }
            case '\0': {
              c = 0xfffd;
            }
            default: {
              if (c >= 'A' && c <= 'Z') {
                c += 0x20;
              }
              appendStrBuf(c);
              continue;
            }
          }
        }
        tagnameloop_end: ;
      }
      case NS_HTML5TOKENIZER_BEFORE_ATTRIBUTE_NAME: {
        for (; ; ) {
          if (reconsume) {
            reconsume = PR_FALSE;
          } else {
            if (++pos == endPos) {
              goto stateloop_end;
            }
            c = checkChar(buf, pos);
          }
          switch(c) {
            case '\r': {
              silentCarriageReturn();
              goto stateloop_end;
            }
            case '\n': {
              silentLineFeed();
            }
            case ' ':
            case '\t':
            case '\f': {
              continue;
            }
            case '/': {
              state = NS_HTML5TOKENIZER_SELF_CLOSING_START_TAG;
              goto stateloop;
            }
            case '>': {
              state = emitCurrentTagToken(PR_FALSE, pos);
              if (shouldSuspend) {
                goto stateloop_end;
              }
              goto stateloop;
            }
            case '\0': {
              c = 0xfffd;
            }
            case '\"':
            case '\'':
            case '<':
            case '=':
            default: {
              if (c >= 'A' && c <= 'Z') {
                c += 0x20;
              }
              clearStrBufAndAppendCurrentC(c);
              state = NS_HTML5TOKENIZER_ATTRIBUTE_NAME;
              goto beforeattributenameloop_end;
            }
          }
        }
        beforeattributenameloop_end: ;
      }
      case NS_HTML5TOKENIZER_ATTRIBUTE_NAME: {
        for (; ; ) {
          if (++pos == endPos) {
            goto stateloop_end;
          }
          c = checkChar(buf, pos);
          switch(c) {
            case '\r': {
              silentCarriageReturn();
              attributeNameComplete();
              state = NS_HTML5TOKENIZER_AFTER_ATTRIBUTE_NAME;
              goto stateloop_end;
            }
            case '\n': {
              silentLineFeed();
            }
            case ' ':
            case '\t':
            case '\f': {
              attributeNameComplete();
              state = NS_HTML5TOKENIZER_AFTER_ATTRIBUTE_NAME;
              goto stateloop;
            }
            case '/': {
              attributeNameComplete();
              addAttributeWithoutValue();
              state = NS_HTML5TOKENIZER_SELF_CLOSING_START_TAG;
              goto stateloop;
            }
            case '=': {
              attributeNameComplete();
              state = NS_HTML5TOKENIZER_BEFORE_ATTRIBUTE_VALUE;
              goto attributenameloop_end;
            }
            case '>': {
              attributeNameComplete();
              addAttributeWithoutValue();
              state = emitCurrentTagToken(PR_FALSE, pos);
              if (shouldSuspend) {
                goto stateloop_end;
              }
              goto stateloop;
            }
            case '\0': {
              c = 0xfffd;
            }
            case '\"':
            case '\'':
            case '<':
            default: {
              if (c >= 'A' && c <= 'Z') {
                c += 0x20;
              }
              appendStrBuf(c);
              continue;
            }
          }
        }
        attributenameloop_end: ;
      }
      case NS_HTML5TOKENIZER_BEFORE_ATTRIBUTE_VALUE: {
        for (; ; ) {
          if (++pos == endPos) {
            goto stateloop_end;
          }
          c = checkChar(buf, pos);
          switch(c) {
            case '\r': {
              silentCarriageReturn();
              goto stateloop_end;
            }
            case '\n': {
              silentLineFeed();
            }
            case ' ':
            case '\t':
            case '\f': {
              continue;
            }
            case '\"': {
              clearLongStrBufForNextState();
              state = NS_HTML5TOKENIZER_ATTRIBUTE_VALUE_DOUBLE_QUOTED;
              goto beforeattributevalueloop_end;
            }
            case '&': {
              clearLongStrBuf();
              state = NS_HTML5TOKENIZER_ATTRIBUTE_VALUE_UNQUOTED;
              reconsume = PR_TRUE;
              goto stateloop;
            }
            case '\'': {
              clearLongStrBufForNextState();
              state = NS_HTML5TOKENIZER_ATTRIBUTE_VALUE_SINGLE_QUOTED;
              goto stateloop;
            }
            case '>': {

              addAttributeWithoutValue();
              state = emitCurrentTagToken(PR_FALSE, pos);
              if (shouldSuspend) {
                goto stateloop_end;
              }
              goto stateloop;
            }
            case '\0': {
              c = 0xfffd;
            }
            case '<':
            case '=':
            default: {
              clearLongStrBufAndAppendCurrentC(c);
              state = NS_HTML5TOKENIZER_ATTRIBUTE_VALUE_UNQUOTED;
              goto stateloop;
            }
          }
        }
        beforeattributevalueloop_end: ;
      }
      case NS_HTML5TOKENIZER_ATTRIBUTE_VALUE_DOUBLE_QUOTED: {
        for (; ; ) {
          if (reconsume) {
            reconsume = PR_FALSE;
          } else {
            if (++pos == endPos) {
              goto stateloop_end;
            }
            c = checkChar(buf, pos);
          }
          switch(c) {
            case '\"': {
              addAttributeWithValue();
              state = NS_HTML5TOKENIZER_AFTER_ATTRIBUTE_VALUE_QUOTED;
              goto attributevaluedoublequotedloop_end;
            }
            case '&': {
              clearStrBufAndAppendCurrentC(c);
              rememberAmpersandLocation('\"');
              returnState = state;
              state = NS_HTML5TOKENIZER_CONSUME_CHARACTER_REFERENCE;
              goto stateloop;
            }
            case '\r': {
              appendLongStrBufCarriageReturn();
              goto stateloop_end;
            }
            case '\n': {
              appendLongStrBufLineFeed();
              continue;
            }
            case '\0': {
              c = 0xfffd;
            }
            default: {
              appendLongStrBuf(c);
              continue;
            }
          }
        }
        attributevaluedoublequotedloop_end: ;
      }
      case NS_HTML5TOKENIZER_AFTER_ATTRIBUTE_VALUE_QUOTED: {
        for (; ; ) {
          if (++pos == endPos) {
            goto stateloop_end;
          }
          c = checkChar(buf, pos);
          switch(c) {
            case '\r': {
              silentCarriageReturn();
              state = NS_HTML5TOKENIZER_BEFORE_ATTRIBUTE_NAME;
              goto stateloop_end;
            }
            case '\n': {
              silentLineFeed();
            }
            case ' ':
            case '\t':
            case '\f': {
              state = NS_HTML5TOKENIZER_BEFORE_ATTRIBUTE_NAME;
              goto stateloop;
            }
            case '/': {
              state = NS_HTML5TOKENIZER_SELF_CLOSING_START_TAG;
              goto afterattributevaluequotedloop_end;
            }
            case '>': {
              state = emitCurrentTagToken(PR_FALSE, pos);
              if (shouldSuspend) {
                goto stateloop_end;
              }
              goto stateloop;
            }
            default: {

              state = NS_HTML5TOKENIZER_BEFORE_ATTRIBUTE_NAME;
              reconsume = PR_TRUE;
              goto stateloop;
            }
          }
        }
        afterattributevaluequotedloop_end: ;
      }
      case NS_HTML5TOKENIZER_SELF_CLOSING_START_TAG: {
        if (++pos == endPos) {
          goto stateloop_end;
        }
        c = checkChar(buf, pos);
        switch(c) {
          case '>': {
            state = emitCurrentTagToken(PR_TRUE, pos);
            if (shouldSuspend) {
              goto stateloop_end;
            }
            goto stateloop;
          }
          default: {

            state = NS_HTML5TOKENIZER_BEFORE_ATTRIBUTE_NAME;
            reconsume = PR_TRUE;
            goto stateloop;
          }
        }
      }
      case NS_HTML5TOKENIZER_ATTRIBUTE_VALUE_UNQUOTED: {
        for (; ; ) {
          if (reconsume) {
            reconsume = PR_FALSE;
          } else {
            if (++pos == endPos) {
              goto stateloop_end;
            }
            c = checkChar(buf, pos);
          }
          switch(c) {
            case '\r': {
              silentCarriageReturn();
              addAttributeWithValue();
              state = NS_HTML5TOKENIZER_BEFORE_ATTRIBUTE_NAME;
              goto stateloop_end;
            }
            case '\n': {
              silentLineFeed();
            }
            case ' ':
            case '\t':
            case '\f': {
              addAttributeWithValue();
              state = NS_HTML5TOKENIZER_BEFORE_ATTRIBUTE_NAME;
              goto stateloop;
            }
            case '&': {
              clearStrBufAndAppendCurrentC(c);
              rememberAmpersandLocation('\0');
              returnState = state;
              state = NS_HTML5TOKENIZER_CONSUME_CHARACTER_REFERENCE;
              goto stateloop;
            }
            case '>': {
              addAttributeWithValue();
              state = emitCurrentTagToken(PR_FALSE, pos);
              if (shouldSuspend) {
                goto stateloop_end;
              }
              goto stateloop;
            }
            case '\0': {
              c = 0xfffd;
            }
            case '<':
            case '\"':
            case '\'':
            case '=':
            default: {

              appendLongStrBuf(c);
              continue;
            }
          }
        }
      }
      case NS_HTML5TOKENIZER_AFTER_ATTRIBUTE_NAME: {
        for (; ; ) {
          if (++pos == endPos) {
            goto stateloop_end;
          }
          c = checkChar(buf, pos);
          switch(c) {
            case '\r': {
              silentCarriageReturn();
              goto stateloop_end;
            }
            case '\n': {
              silentLineFeed();
            }
            case ' ':
            case '\t':
            case '\f': {
              continue;
            }
            case '/': {
              addAttributeWithoutValue();
              state = NS_HTML5TOKENIZER_SELF_CLOSING_START_TAG;
              goto stateloop;
            }
            case '=': {
              state = NS_HTML5TOKENIZER_BEFORE_ATTRIBUTE_VALUE;
              goto stateloop;
            }
            case '>': {
              addAttributeWithoutValue();
              state = emitCurrentTagToken(PR_FALSE, pos);
              if (shouldSuspend) {
                goto stateloop_end;
              }
              goto stateloop;
            }
            case '\0': {
              c = 0xfffd;
            }
            case '\"':
            case '\'':
            case '<':
            default: {
              addAttributeWithoutValue();
              if (c >= 'A' && c <= 'Z') {
                c += 0x20;
              }
              clearStrBufAndAppendCurrentC(c);
              state = NS_HTML5TOKENIZER_ATTRIBUTE_NAME;
              goto stateloop;
            }
          }
        }
      }
      case NS_HTML5TOKENIZER_BOGUS_COMMENT: {
        for (; ; ) {
          if (reconsume) {
            reconsume = PR_FALSE;
          } else {
            if (++pos == endPos) {
              goto stateloop_end;
            }
            c = checkChar(buf, pos);
          }
          switch(c) {
            case '>': {
              emitComment(0, pos);
              state = NS_HTML5TOKENIZER_DATA;
              goto stateloop;
            }
            case '-': {
              appendLongStrBuf(c);
              state = NS_HTML5TOKENIZER_BOGUS_COMMENT_HYPHEN;
              goto boguscommentloop_end;
            }
            case '\r': {
              appendLongStrBufCarriageReturn();
              goto stateloop_end;
            }
            case '\n': {
              appendLongStrBufLineFeed();
              continue;
            }
            case '\0': {
              c = 0xfffd;
            }
            default: {
              appendLongStrBuf(c);
              continue;
            }
          }
        }
        boguscommentloop_end: ;
      }
      case NS_HTML5TOKENIZER_BOGUS_COMMENT_HYPHEN: {
        boguscommenthyphenloop: for (; ; ) {
          if (++pos == endPos) {
            goto stateloop_end;
          }
          c = checkChar(buf, pos);
          switch(c) {
            case '>': {
              emitComment(0, pos);
              state = NS_HTML5TOKENIZER_DATA;
              goto stateloop;
            }
            case '-': {
              appendSecondHyphenToBogusComment();
              goto boguscommenthyphenloop;
            }
            case '\r': {
              appendLongStrBufCarriageReturn();
              state = NS_HTML5TOKENIZER_BOGUS_COMMENT;
              goto stateloop_end;
            }
            case '\n': {
              appendLongStrBufLineFeed();
              state = NS_HTML5TOKENIZER_BOGUS_COMMENT;
              goto stateloop;
            }
            case '\0': {
              c = 0xfffd;
            }
            default: {
              appendLongStrBuf(c);
              state = NS_HTML5TOKENIZER_BOGUS_COMMENT;
              goto stateloop;
            }
          }
        }

      }
      case NS_HTML5TOKENIZER_MARKUP_DECLARATION_OPEN: {
        for (; ; ) {
          if (++pos == endPos) {
            goto stateloop_end;
          }
          c = checkChar(buf, pos);
          switch(c) {
            case '-': {
              clearLongStrBufAndAppendToComment(c);
              state = NS_HTML5TOKENIZER_MARKUP_DECLARATION_HYPHEN;
              goto markupdeclarationopenloop_end;
            }
            case 'd':
            case 'D': {
              clearLongStrBufAndAppendToComment(c);
              index = 0;
              state = NS_HTML5TOKENIZER_MARKUP_DECLARATION_OCTYPE;
              goto stateloop;
            }
            case '[': {
              if (tokenHandler->inForeign()) {
                clearLongStrBufAndAppendToComment(c);
                index = 0;
                state = NS_HTML5TOKENIZER_CDATA_START;
                goto stateloop;
              } else {
              }
            }
            default: {

              clearLongStrBuf();
              state = NS_HTML5TOKENIZER_BOGUS_COMMENT;
              reconsume = PR_TRUE;
              goto stateloop;
            }
          }
        }
        markupdeclarationopenloop_end: ;
      }
      case NS_HTML5TOKENIZER_MARKUP_DECLARATION_HYPHEN: {
        for (; ; ) {
          if (++pos == endPos) {
            goto stateloop_end;
          }
          c = checkChar(buf, pos);
          switch(c) {
            case '\0': {
              goto stateloop_end;
            }
            case '-': {
              clearLongStrBufForNextState();
              state = NS_HTML5TOKENIZER_COMMENT_START;
              goto markupdeclarationhyphenloop_end;
            }
            default: {

              state = NS_HTML5TOKENIZER_BOGUS_COMMENT;
              reconsume = PR_TRUE;
              goto stateloop;
            }
          }
        }
        markupdeclarationhyphenloop_end: ;
      }
      case NS_HTML5TOKENIZER_COMMENT_START: {
        for (; ; ) {
          if (++pos == endPos) {
            goto stateloop_end;
          }
          c = checkChar(buf, pos);
          switch(c) {
            case '-': {
              appendLongStrBuf(c);
              state = NS_HTML5TOKENIZER_COMMENT_START_DASH;
              goto stateloop;
            }
            case '>': {

              emitComment(0, pos);
              state = NS_HTML5TOKENIZER_DATA;
              goto stateloop;
            }
            case '\r': {
              appendLongStrBufCarriageReturn();
              state = NS_HTML5TOKENIZER_COMMENT;
              goto stateloop_end;
            }
            case '\n': {
              appendLongStrBufLineFeed();
              state = NS_HTML5TOKENIZER_COMMENT;
              goto commentstartloop_end;
            }
            case '\0': {
              c = 0xfffd;
            }
            default: {
              appendLongStrBuf(c);
              state = NS_HTML5TOKENIZER_COMMENT;
              goto commentstartloop_end;
            }
          }
        }
        commentstartloop_end: ;
      }
      case NS_HTML5TOKENIZER_COMMENT: {
        for (; ; ) {
          if (++pos == endPos) {
            goto stateloop_end;
          }
          c = checkChar(buf, pos);
          switch(c) {
            case '-': {
              appendLongStrBuf(c);
              state = NS_HTML5TOKENIZER_COMMENT_END_DASH;
              goto commentloop_end;
            }
            case '\r': {
              appendLongStrBufCarriageReturn();
              goto stateloop_end;
            }
            case '\n': {
              appendLongStrBufLineFeed();
              continue;
            }
            case '\0': {
              c = 0xfffd;
            }
            default: {
              appendLongStrBuf(c);
              continue;
            }
          }
        }
        commentloop_end: ;
      }
      case NS_HTML5TOKENIZER_COMMENT_END_DASH: {
        for (; ; ) {
          if (++pos == endPos) {
            goto stateloop_end;
          }
          c = checkChar(buf, pos);
          switch(c) {
            case '-': {
              appendLongStrBuf(c);
              state = NS_HTML5TOKENIZER_COMMENT_END;
              goto commentenddashloop_end;
            }
            case '\r': {
              appendLongStrBufCarriageReturn();
              state = NS_HTML5TOKENIZER_COMMENT;
              goto stateloop_end;
            }
            case '\n': {
              appendLongStrBufLineFeed();
              state = NS_HTML5TOKENIZER_COMMENT;
              goto stateloop;
            }
            case '\0': {
              c = 0xfffd;
            }
            default: {
              appendLongStrBuf(c);
              state = NS_HTML5TOKENIZER_COMMENT;
              goto stateloop;
            }
          }
        }
        commentenddashloop_end: ;
      }
      case NS_HTML5TOKENIZER_COMMENT_END: {
        for (; ; ) {
          if (++pos == endPos) {
            goto stateloop_end;
          }
          c = checkChar(buf, pos);
          switch(c) {
            case '>': {
              emitComment(2, pos);
              state = NS_HTML5TOKENIZER_DATA;
              goto stateloop;
            }
            case '-': {
              adjustDoubleHyphenAndAppendToLongStrBufAndErr(c);
              continue;
            }
            case ' ':
            case '\t':
            case '\f': {
              adjustDoubleHyphenAndAppendToLongStrBufAndErr(c);
              state = NS_HTML5TOKENIZER_COMMENT_END_SPACE;
              goto commentendloop_end;
            }
            case '\r': {
              adjustDoubleHyphenAndAppendToLongStrBufCarriageReturn();
              state = NS_HTML5TOKENIZER_COMMENT_END_SPACE;
              goto stateloop_end;
            }
            case '\n': {
              adjustDoubleHyphenAndAppendToLongStrBufLineFeed();
              state = NS_HTML5TOKENIZER_COMMENT_END_SPACE;
              goto commentendloop_end;
            }
            case '!': {

              appendLongStrBuf(c);
              state = NS_HTML5TOKENIZER_COMMENT_END_BANG;
              goto stateloop;
            }
            case '\0': {
              c = 0xfffd;
            }
            default: {
              adjustDoubleHyphenAndAppendToLongStrBufAndErr(c);
              state = NS_HTML5TOKENIZER_COMMENT;
              goto stateloop;
            }
          }
        }
        commentendloop_end: ;
      }
      case NS_HTML5TOKENIZER_COMMENT_END_SPACE: {
        for (; ; ) {
          if (++pos == endPos) {
            goto stateloop_end;
          }
          c = checkChar(buf, pos);
          switch(c) {
            case '>': {
              emitComment(0, pos);
              state = NS_HTML5TOKENIZER_DATA;
              goto stateloop;
            }
            case '-': {
              appendLongStrBuf(c);
              state = NS_HTML5TOKENIZER_COMMENT_END_DASH;
              goto stateloop;
            }
            case ' ':
            case '\t':
            case '\f': {
              appendLongStrBuf(c);
              continue;
            }
            case '\r': {
              appendLongStrBufCarriageReturn();
              goto stateloop_end;
            }
            case '\n': {
              appendLongStrBufLineFeed();
              continue;
            }
            case '\0': {
              c = 0xfffd;
            }
            default: {
              appendLongStrBuf(c);
              state = NS_HTML5TOKENIZER_COMMENT;
              goto stateloop;
            }
          }
        }
      }
      case NS_HTML5TOKENIZER_COMMENT_END_BANG: {
        for (; ; ) {
          if (++pos == endPos) {
            goto stateloop_end;
          }
          c = checkChar(buf, pos);
          switch(c) {
            case '>': {
              emitComment(3, pos);
              state = NS_HTML5TOKENIZER_DATA;
              goto stateloop;
            }
            case '-': {
              appendLongStrBuf(c);
              state = NS_HTML5TOKENIZER_COMMENT_END_DASH;
              goto stateloop;
            }
            case '\r': {
              appendLongStrBufCarriageReturn();
              goto stateloop_end;
            }
            case '\n': {
              appendLongStrBufLineFeed();
              continue;
            }
            case '\0': {
              c = 0xfffd;
            }
            default: {
              appendLongStrBuf(c);
              state = NS_HTML5TOKENIZER_COMMENT;
              goto stateloop;
            }
          }
        }
      }
      case NS_HTML5TOKENIZER_COMMENT_START_DASH: {
        if (++pos == endPos) {
          goto stateloop_end;
        }
        c = checkChar(buf, pos);
        switch(c) {
          case '-': {
            appendLongStrBuf(c);
            state = NS_HTML5TOKENIZER_COMMENT_END;
            goto stateloop;
          }
          case '>': {

            emitComment(1, pos);
            state = NS_HTML5TOKENIZER_DATA;
            goto stateloop;
          }
          case '\r': {
            appendLongStrBufCarriageReturn();
            state = NS_HTML5TOKENIZER_COMMENT;
            goto stateloop_end;
          }
          case '\n': {
            appendLongStrBufLineFeed();
            state = NS_HTML5TOKENIZER_COMMENT;
            goto stateloop;
          }
          case '\0': {
            c = 0xfffd;
          }
          default: {
            appendLongStrBuf(c);
            state = NS_HTML5TOKENIZER_COMMENT;
            goto stateloop;
          }
        }
      }
      case NS_HTML5TOKENIZER_MARKUP_DECLARATION_OCTYPE: {
        for (; ; ) {
          if (++pos == endPos) {
            goto stateloop_end;
          }
          c = checkChar(buf, pos);
          if (index < 6) {
            PRUnichar folded = c;
            if (c >= 'A' && c <= 'Z') {
              folded += 0x20;
            }
            if (folded == nsHtml5Tokenizer::OCTYPE[index]) {
              appendLongStrBuf(c);
            } else {

              state = NS_HTML5TOKENIZER_BOGUS_COMMENT;
              reconsume = PR_TRUE;
              goto stateloop;
            }
            index++;
            continue;
          } else {
            state = NS_HTML5TOKENIZER_DOCTYPE;
            reconsume = PR_TRUE;
            goto markupdeclarationdoctypeloop_end;
          }
        }
        markupdeclarationdoctypeloop_end: ;
      }
      case NS_HTML5TOKENIZER_DOCTYPE: {
        for (; ; ) {
          if (reconsume) {
            reconsume = PR_FALSE;
          } else {
            if (++pos == endPos) {
              goto stateloop_end;
            }
            c = checkChar(buf, pos);
          }
          initDoctypeFields();
          switch(c) {
            case '\r': {
              silentCarriageReturn();
              state = NS_HTML5TOKENIZER_BEFORE_DOCTYPE_NAME;
              goto stateloop_end;
            }
            case '\n': {
              silentLineFeed();
            }
            case ' ':
            case '\t':
            case '\f': {
              state = NS_HTML5TOKENIZER_BEFORE_DOCTYPE_NAME;
              goto doctypeloop_end;
            }
            default: {

              state = NS_HTML5TOKENIZER_BEFORE_DOCTYPE_NAME;
              reconsume = PR_TRUE;
              goto doctypeloop_end;
            }
          }
        }
        doctypeloop_end: ;
      }
      case NS_HTML5TOKENIZER_BEFORE_DOCTYPE_NAME: {
        for (; ; ) {
          if (reconsume) {
            reconsume = PR_FALSE;
          } else {
            if (++pos == endPos) {
              goto stateloop_end;
            }
            c = checkChar(buf, pos);
          }
          switch(c) {
            case '\r': {
              silentCarriageReturn();
              goto stateloop_end;
            }
            case '\n': {
              silentLineFeed();
            }
            case ' ':
            case '\t':
            case '\f': {
              continue;
            }
            case '>': {

              forceQuirks = PR_TRUE;
              emitDoctypeToken(pos);
              state = NS_HTML5TOKENIZER_DATA;
              goto stateloop;
            }
            case '\0': {
              c = 0xfffd;
            }
            default: {
              if (c >= 'A' && c <= 'Z') {
                c += 0x20;
              }
              clearStrBufAndAppendCurrentC(c);
              state = NS_HTML5TOKENIZER_DOCTYPE_NAME;
              goto beforedoctypenameloop_end;
            }
          }
        }
        beforedoctypenameloop_end: ;
      }
      case NS_HTML5TOKENIZER_DOCTYPE_NAME: {
        for (; ; ) {
          if (++pos == endPos) {
            goto stateloop_end;
          }
          c = checkChar(buf, pos);
          switch(c) {
            case '\r': {
              silentCarriageReturn();
              strBufToDoctypeName();
              state = NS_HTML5TOKENIZER_AFTER_DOCTYPE_NAME;
              goto stateloop_end;
            }
            case '\n': {
              silentLineFeed();
            }
            case ' ':
            case '\t':
            case '\f': {
              strBufToDoctypeName();
              state = NS_HTML5TOKENIZER_AFTER_DOCTYPE_NAME;
              goto doctypenameloop_end;
            }
            case '>': {
              strBufToDoctypeName();
              emitDoctypeToken(pos);
              state = NS_HTML5TOKENIZER_DATA;
              goto stateloop;
            }
            case '\0': {
              c = 0xfffd;
            }
            default: {
              if (c >= 'A' && c <= 'Z') {
                c += 0x0020;
              }
              appendStrBuf(c);
              continue;
            }
          }
        }
        doctypenameloop_end: ;
      }
      case NS_HTML5TOKENIZER_AFTER_DOCTYPE_NAME: {
        for (; ; ) {
          if (++pos == endPos) {
            goto stateloop_end;
          }
          c = checkChar(buf, pos);
          switch(c) {
            case '\r': {
              silentCarriageReturn();
              goto stateloop_end;
            }
            case '\n': {
              silentLineFeed();
            }
            case ' ':
            case '\t':
            case '\f': {
              continue;
            }
            case '>': {
              emitDoctypeToken(pos);
              state = NS_HTML5TOKENIZER_DATA;
              goto stateloop;
            }
            case 'p':
            case 'P': {
              index = 0;
              state = NS_HTML5TOKENIZER_DOCTYPE_UBLIC;
              goto afterdoctypenameloop_end;
            }
            case 's':
            case 'S': {
              index = 0;
              state = NS_HTML5TOKENIZER_DOCTYPE_YSTEM;
              goto stateloop;
            }
            default: {
              bogusDoctype();
              state = NS_HTML5TOKENIZER_BOGUS_DOCTYPE;
              goto stateloop;
            }
          }
        }
        afterdoctypenameloop_end: ;
      }
      case NS_HTML5TOKENIZER_DOCTYPE_UBLIC: {
        for (; ; ) {
          if (++pos == endPos) {
            goto stateloop_end;
          }
          c = checkChar(buf, pos);
          if (index < 5) {
            PRUnichar folded = c;
            if (c >= 'A' && c <= 'Z') {
              folded += 0x20;
            }
            if (folded != nsHtml5Tokenizer::UBLIC[index]) {
              bogusDoctype();
              state = NS_HTML5TOKENIZER_BOGUS_DOCTYPE;
              reconsume = PR_TRUE;
              goto stateloop;
            }
            index++;
            continue;
          } else {
            state = NS_HTML5TOKENIZER_BEFORE_DOCTYPE_PUBLIC_IDENTIFIER;
            reconsume = PR_TRUE;
            goto doctypeublicloop_end;
          }
        }
        doctypeublicloop_end: ;
      }
      case NS_HTML5TOKENIZER_BEFORE_DOCTYPE_PUBLIC_IDENTIFIER: {
        for (; ; ) {
          if (reconsume) {
            reconsume = PR_FALSE;
          } else {
            if (++pos == endPos) {
              goto stateloop_end;
            }
            c = checkChar(buf, pos);
          }
          switch(c) {
            case '\r': {
              silentCarriageReturn();
              goto stateloop_end;
            }
            case '\n': {
              silentLineFeed();
            }
            case ' ':
            case '\t':
            case '\f': {
              continue;
            }
            case '\"': {
              clearLongStrBufForNextState();
              state = NS_HTML5TOKENIZER_DOCTYPE_PUBLIC_IDENTIFIER_DOUBLE_QUOTED;
              goto beforedoctypepublicidentifierloop_end;
            }
            case '\'': {
              clearLongStrBufForNextState();
              state = NS_HTML5TOKENIZER_DOCTYPE_PUBLIC_IDENTIFIER_SINGLE_QUOTED;
              goto stateloop;
            }
            case '>': {

              forceQuirks = PR_TRUE;
              emitDoctypeToken(pos);
              state = NS_HTML5TOKENIZER_DATA;
              goto stateloop;
            }
            default: {
              bogusDoctype();
              state = NS_HTML5TOKENIZER_BOGUS_DOCTYPE;
              goto stateloop;
            }
          }
        }
        beforedoctypepublicidentifierloop_end: ;
      }
      case NS_HTML5TOKENIZER_DOCTYPE_PUBLIC_IDENTIFIER_DOUBLE_QUOTED: {
        for (; ; ) {
          if (++pos == endPos) {
            goto stateloop_end;
          }
          c = checkChar(buf, pos);
          switch(c) {
            case '\"': {
              publicIdentifier = longStrBufToString();
              state = NS_HTML5TOKENIZER_AFTER_DOCTYPE_PUBLIC_IDENTIFIER;
              goto doctypepublicidentifierdoublequotedloop_end;
            }
            case '>': {

              forceQuirks = PR_TRUE;
              publicIdentifier = longStrBufToString();
              emitDoctypeToken(pos);
              state = NS_HTML5TOKENIZER_DATA;
              goto stateloop;
            }
            case '\r': {
              appendLongStrBufCarriageReturn();
              goto stateloop_end;
            }
            case '\n': {
              appendLongStrBufLineFeed();
              continue;
            }
            case '\0': {
              c = 0xfffd;
            }
            default: {
              appendLongStrBuf(c);
              continue;
            }
          }
        }
        doctypepublicidentifierdoublequotedloop_end: ;
      }
      case NS_HTML5TOKENIZER_AFTER_DOCTYPE_PUBLIC_IDENTIFIER: {
        for (; ; ) {
          if (++pos == endPos) {
            goto stateloop_end;
          }
          c = checkChar(buf, pos);
          switch(c) {
            case '\r': {
              silentCarriageReturn();
              goto stateloop_end;
            }
            case '\n': {
              silentLineFeed();
            }
            case ' ':
            case '\t':
            case '\f': {
              continue;
            }
            case '\"': {
              clearLongStrBufForNextState();
              state = NS_HTML5TOKENIZER_DOCTYPE_SYSTEM_IDENTIFIER_DOUBLE_QUOTED;
              goto afterdoctypepublicidentifierloop_end;
            }
            case '\'': {
              clearLongStrBufForNextState();
              state = NS_HTML5TOKENIZER_DOCTYPE_SYSTEM_IDENTIFIER_SINGLE_QUOTED;
              goto stateloop;
            }
            case '>': {
              emitDoctypeToken(pos);
              state = NS_HTML5TOKENIZER_DATA;
              goto stateloop;
            }
            default: {
              bogusDoctype();
              state = NS_HTML5TOKENIZER_BOGUS_DOCTYPE;
              goto stateloop;
            }
          }
        }
        afterdoctypepublicidentifierloop_end: ;
      }
      case NS_HTML5TOKENIZER_DOCTYPE_SYSTEM_IDENTIFIER_DOUBLE_QUOTED: {
        for (; ; ) {
          if (++pos == endPos) {
            goto stateloop_end;
          }
          c = checkChar(buf, pos);
          switch(c) {
            case '\"': {
              systemIdentifier = longStrBufToString();
              state = NS_HTML5TOKENIZER_AFTER_DOCTYPE_SYSTEM_IDENTIFIER;
              goto stateloop;
            }
            case '>': {

              forceQuirks = PR_TRUE;
              systemIdentifier = longStrBufToString();
              emitDoctypeToken(pos);
              state = NS_HTML5TOKENIZER_DATA;
              goto stateloop;
            }
            case '\r': {
              appendLongStrBufCarriageReturn();
              goto stateloop_end;
            }
            case '\n': {
              appendLongStrBufLineFeed();
              continue;
            }
            case '\0': {
              c = 0xfffd;
            }
            default: {
              appendLongStrBuf(c);
              continue;
            }
          }
        }

      }
      case NS_HTML5TOKENIZER_AFTER_DOCTYPE_SYSTEM_IDENTIFIER: {
        for (; ; ) {
          if (++pos == endPos) {
            goto stateloop_end;
          }
          c = checkChar(buf, pos);
          switch(c) {
            case '\r': {
              silentCarriageReturn();
              goto stateloop_end;
            }
            case '\n': {
              silentLineFeed();
            }
            case ' ':
            case '\t':
            case '\f': {
              continue;
            }
            case '>': {
              emitDoctypeToken(pos);
              state = NS_HTML5TOKENIZER_DATA;
              goto stateloop;
            }
            default: {
              bogusDoctypeWithoutQuirks();
              state = NS_HTML5TOKENIZER_BOGUS_DOCTYPE;
              goto afterdoctypesystemidentifierloop_end;
            }
          }
        }
        afterdoctypesystemidentifierloop_end: ;
      }
      case NS_HTML5TOKENIZER_BOGUS_DOCTYPE: {
        for (; ; ) {
          if (reconsume) {
            reconsume = PR_FALSE;
          } else {
            if (++pos == endPos) {
              goto stateloop_end;
            }
            c = checkChar(buf, pos);
          }
          switch(c) {
            case '>': {
              emitDoctypeToken(pos);
              state = NS_HTML5TOKENIZER_DATA;
              goto stateloop;
            }
            case '\r': {
              silentCarriageReturn();
              goto stateloop_end;
            }
            case '\n': {
              silentLineFeed();
            }
            default: {
              continue;
            }
          }
        }
      }
      case NS_HTML5TOKENIZER_DOCTYPE_YSTEM: {
        for (; ; ) {
          if (++pos == endPos) {
            goto stateloop_end;
          }
          c = checkChar(buf, pos);
          if (index < 5) {
            PRUnichar folded = c;
            if (c >= 'A' && c <= 'Z') {
              folded += 0x20;
            }
            if (folded != nsHtml5Tokenizer::YSTEM[index]) {
              bogusDoctype();
              state = NS_HTML5TOKENIZER_BOGUS_DOCTYPE;
              reconsume = PR_TRUE;
              goto stateloop;
            }
            index++;
            goto stateloop;
          } else {
            state = NS_HTML5TOKENIZER_BEFORE_DOCTYPE_SYSTEM_IDENTIFIER;
            reconsume = PR_TRUE;
            goto doctypeystemloop_end;
          }
        }
        doctypeystemloop_end: ;
      }
      case NS_HTML5TOKENIZER_BEFORE_DOCTYPE_SYSTEM_IDENTIFIER: {
        for (; ; ) {
          if (reconsume) {
            reconsume = PR_FALSE;
          } else {
            if (++pos == endPos) {
              goto stateloop_end;
            }
            c = checkChar(buf, pos);
          }
          switch(c) {
            case '\r': {
              silentCarriageReturn();
              goto stateloop_end;
            }
            case '\n': {
              silentLineFeed();
            }
            case ' ':
            case '\t':
            case '\f': {
              continue;
            }
            case '\"': {
              clearLongStrBufForNextState();
              state = NS_HTML5TOKENIZER_DOCTYPE_SYSTEM_IDENTIFIER_DOUBLE_QUOTED;
              goto stateloop;
            }
            case '\'': {
              clearLongStrBufForNextState();
              state = NS_HTML5TOKENIZER_DOCTYPE_SYSTEM_IDENTIFIER_SINGLE_QUOTED;
              goto beforedoctypesystemidentifierloop_end;
            }
            case '>': {

              forceQuirks = PR_TRUE;
              emitDoctypeToken(pos);
              state = NS_HTML5TOKENIZER_DATA;
              goto stateloop;
            }
            default: {
              bogusDoctype();
              state = NS_HTML5TOKENIZER_BOGUS_DOCTYPE;
              goto stateloop;
            }
          }
        }
        beforedoctypesystemidentifierloop_end: ;
      }
      case NS_HTML5TOKENIZER_DOCTYPE_SYSTEM_IDENTIFIER_SINGLE_QUOTED: {
        for (; ; ) {
          if (++pos == endPos) {
            goto stateloop_end;
          }
          c = checkChar(buf, pos);
          switch(c) {
            case '\'': {
              systemIdentifier = longStrBufToString();
              state = NS_HTML5TOKENIZER_AFTER_DOCTYPE_SYSTEM_IDENTIFIER;
              goto stateloop;
            }
            case '>': {

              forceQuirks = PR_TRUE;
              systemIdentifier = longStrBufToString();
              emitDoctypeToken(pos);
              state = NS_HTML5TOKENIZER_DATA;
              goto stateloop;
            }
            case '\r': {
              appendLongStrBufCarriageReturn();
              goto stateloop_end;
            }
            case '\n': {
              appendLongStrBufLineFeed();
              continue;
            }
            case '\0': {
              c = 0xfffd;
            }
            default: {
              appendLongStrBuf(c);
              continue;
            }
          }
        }
      }
      case NS_HTML5TOKENIZER_DOCTYPE_PUBLIC_IDENTIFIER_SINGLE_QUOTED: {
        for (; ; ) {
          if (++pos == endPos) {
            goto stateloop_end;
          }
          c = checkChar(buf, pos);
          switch(c) {
            case '\'': {
              publicIdentifier = longStrBufToString();
              state = NS_HTML5TOKENIZER_AFTER_DOCTYPE_PUBLIC_IDENTIFIER;
              goto stateloop;
            }
            case '>': {

              forceQuirks = PR_TRUE;
              publicIdentifier = longStrBufToString();
              emitDoctypeToken(pos);
              state = NS_HTML5TOKENIZER_DATA;
              goto stateloop;
            }
            case '\r': {
              appendLongStrBufCarriageReturn();
              goto stateloop_end;
            }
            case '\n': {
              appendLongStrBufLineFeed();
              continue;
            }
            case '\0': {
              c = 0xfffd;
            }
            default: {
              appendLongStrBuf(c);
              continue;
            }
          }
        }
      }
      case NS_HTML5TOKENIZER_CDATA_START: {
        for (; ; ) {
          if (++pos == endPos) {
            goto stateloop_end;
          }
          c = checkChar(buf, pos);
          if (index < 6) {
            if (c == nsHtml5Tokenizer::CDATA_LSQB[index]) {
              appendLongStrBuf(c);
            } else {

              state = NS_HTML5TOKENIZER_BOGUS_COMMENT;
              reconsume = PR_TRUE;
              goto stateloop;
            }
            index++;
            continue;
          } else {
            cstart = pos;
            state = NS_HTML5TOKENIZER_CDATA_SECTION;
            reconsume = PR_TRUE;
            break;
          }
        }
      }
      case NS_HTML5TOKENIZER_CDATA_SECTION: {
        for (; ; ) {
          if (reconsume) {
            reconsume = PR_FALSE;
          } else {
            if (++pos == endPos) {
              goto stateloop_end;
            }
            c = checkChar(buf, pos);
          }
          switch(c) {
            case ']': {
              flushChars(buf, pos);
              state = NS_HTML5TOKENIZER_CDATA_RSQB;
              goto cdatasectionloop_end;
            }
            case '\0': {
              emitReplacementCharacter(buf, pos);
              continue;
            }
            case '\r': {
              emitCarriageReturn(buf, pos);
              goto stateloop_end;
            }
            case '\n': {
              silentLineFeed();
            }
            default: {
              continue;
            }
          }
        }
        cdatasectionloop_end: ;
      }
      case NS_HTML5TOKENIZER_CDATA_RSQB: {
        for (; ; ) {
          if (++pos == endPos) {
            goto stateloop_end;
          }
          c = checkChar(buf, pos);
          switch(c) {
            case ']': {
              state = NS_HTML5TOKENIZER_CDATA_RSQB_RSQB;
              goto cdatarsqb_end;
            }
            default: {
              tokenHandler->characters(nsHtml5Tokenizer::RSQB_RSQB, 0, 1);
              cstart = pos;
              state = NS_HTML5TOKENIZER_CDATA_SECTION;
              reconsume = PR_TRUE;
              goto stateloop;
            }
          }
        }
        cdatarsqb_end: ;
      }
      case NS_HTML5TOKENIZER_CDATA_RSQB_RSQB: {
        if (++pos == endPos) {
          goto stateloop_end;
        }
        c = checkChar(buf, pos);
        switch(c) {
          case '>': {
            cstart = pos + 1;
            state = NS_HTML5TOKENIZER_DATA;
            goto stateloop;
          }
          default: {
            tokenHandler->characters(nsHtml5Tokenizer::RSQB_RSQB, 0, 2);
            cstart = pos;
            state = NS_HTML5TOKENIZER_CDATA_SECTION;
            reconsume = PR_TRUE;
            goto stateloop;
          }
        }
      }
      case NS_HTML5TOKENIZER_ATTRIBUTE_VALUE_SINGLE_QUOTED: {
        for (; ; ) {
          if (reconsume) {
            reconsume = PR_FALSE;
          } else {
            if (++pos == endPos) {
              goto stateloop_end;
            }
            c = checkChar(buf, pos);
          }
          switch(c) {
            case '\'': {
              addAttributeWithValue();
              state = NS_HTML5TOKENIZER_AFTER_ATTRIBUTE_VALUE_QUOTED;
              goto stateloop;
            }
            case '&': {
              clearStrBufAndAppendCurrentC(c);
              rememberAmpersandLocation('\'');
              returnState = state;
              state = NS_HTML5TOKENIZER_CONSUME_CHARACTER_REFERENCE;
              goto attributevaluesinglequotedloop_end;
            }
            case '\r': {
              appendLongStrBufCarriageReturn();
              goto stateloop_end;
            }
            case '\n': {
              appendLongStrBufLineFeed();
              continue;
            }
            case '\0': {
              c = 0xfffd;
            }
            default: {
              appendLongStrBuf(c);
              continue;
            }
          }
        }
        attributevaluesinglequotedloop_end: ;
      }
      case NS_HTML5TOKENIZER_CONSUME_CHARACTER_REFERENCE: {
        if (++pos == endPos) {
          goto stateloop_end;
        }
        c = checkChar(buf, pos);
        if (c == '\0') {
          goto stateloop_end;
        }
        switch(c) {
          case ' ':
          case '\t':
          case '\n':
          case '\r':
          case '\f':
          case '<':
          case '&': {
            emitOrAppendStrBuf(returnState);
            if (!(returnState & (~1))) {
              cstart = pos;
            }
            state = returnState;
            reconsume = PR_TRUE;
            goto stateloop;
          }
          case '#': {
            appendStrBuf('#');
            state = NS_HTML5TOKENIZER_CONSUME_NCR;
            goto stateloop;
          }
          default: {
            if (c == additional) {
              emitOrAppendStrBuf(returnState);
              state = returnState;
              reconsume = PR_TRUE;
              goto stateloop;
            }
            entCol = -1;
            lo = 0;
            hi = (nsHtml5NamedCharacters::NAMES.length - 1);
            candidate = -1;
            strBufMark = 0;
            state = NS_HTML5TOKENIZER_CHARACTER_REFERENCE_LOOP;
            reconsume = PR_TRUE;
          }
        }
      }
      case NS_HTML5TOKENIZER_CHARACTER_REFERENCE_LOOP: {
        for (; ; ) {
          if (reconsume) {
            reconsume = PR_FALSE;
          } else {
            if (++pos == endPos) {
              goto stateloop_end;
            }
            c = checkChar(buf, pos);
          }
          if (c == '\0') {
            goto stateloop_end;
          }
          entCol++;
          for (; ; ) {
            if (hi == -1) {
              goto hiloop_end;
            }
            if (entCol == nsHtml5NamedCharacters::NAMES[hi].length) {
              goto hiloop_end;
            }
            if (entCol > nsHtml5NamedCharacters::NAMES[hi].length) {
              goto outer_end;
            } else if (c < nsHtml5NamedCharacters::NAMES[hi][entCol]) {
              hi--;
            } else {
              goto hiloop_end;
            }
          }
          hiloop_end: ;
          for (; ; ) {
            if (hi < lo) {
              goto outer_end;
            }
            if (entCol == nsHtml5NamedCharacters::NAMES[lo].length) {
              candidate = lo;
              strBufMark = strBufLen;
              lo++;
            } else if (entCol > nsHtml5NamedCharacters::NAMES[lo].length) {
              goto outer_end;
            } else if (c > nsHtml5NamedCharacters::NAMES[lo][entCol]) {
              lo++;
            } else {
              goto loloop_end;
            }
          }
          loloop_end: ;
          if (hi < lo) {
            goto outer_end;
          }
          appendStrBuf(c);
          continue;
        }
        outer_end: ;
        if (candidate == -1) {

          emitOrAppendStrBuf(returnState);
          if (!(returnState & (~1))) {
            cstart = pos;
          }
          state = returnState;
          reconsume = PR_TRUE;
          goto stateloop;
        } else {
          jArray<PRUnichar,PRInt32> candidateArr = nsHtml5NamedCharacters::NAMES[candidate];
          if (candidateArr[candidateArr.length - 1] != ';') {
            if ((returnState & (~1))) {
              PRUnichar ch;
              if (strBufMark == strBufLen) {
                ch = c;
              } else {
                ch = strBuf[strBufMark];
              }
              if ((ch >= '0' && ch <= '9') || (ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z')) {

                appendStrBufToLongStrBuf();
                state = returnState;
                reconsume = PR_TRUE;
                goto stateloop;
              }
            }
            if ((returnState & (~1))) {

            } else {

            }
          }
          jArray<PRUnichar,PRInt32> val = nsHtml5NamedCharacters::VALUES[candidate];
          emitOrAppend(val, returnState);
          if (strBufMark < strBufLen) {
            if ((returnState & (~1))) {
              for (PRInt32 i = strBufMark; i < strBufLen; i++) {
                appendLongStrBuf(strBuf[i]);
              }
            } else {
              tokenHandler->characters(strBuf, strBufMark, strBufLen - strBufMark);
            }
          }
          if (!(returnState & (~1))) {
            cstart = pos;
          }
          state = returnState;
          reconsume = PR_TRUE;
          goto stateloop;
        }
      }
      case NS_HTML5TOKENIZER_CONSUME_NCR: {
        if (++pos == endPos) {
          goto stateloop_end;
        }
        c = checkChar(buf, pos);
        prevValue = -1;
        value = 0;
        seenDigits = PR_FALSE;
        switch(c) {
          case 'x':
          case 'X': {
            appendStrBuf(c);
            state = NS_HTML5TOKENIZER_HEX_NCR_LOOP;
            goto stateloop;
          }
          default: {
            state = NS_HTML5TOKENIZER_DECIMAL_NRC_LOOP;
            reconsume = PR_TRUE;
          }
        }
      }
      case NS_HTML5TOKENIZER_DECIMAL_NRC_LOOP: {
        for (; ; ) {
          if (reconsume) {
            reconsume = PR_FALSE;
          } else {
            if (++pos == endPos) {
              goto stateloop_end;
            }
            c = checkChar(buf, pos);
          }
          if (value < prevValue) {
            value = 0x110000;
          }
          prevValue = value;
          if (c >= '0' && c <= '9') {
            seenDigits = PR_TRUE;
            value *= 10;
            value += c - '0';
            continue;
          } else if (c == ';') {
            if (seenDigits) {
              if (!(returnState & (~1))) {
                cstart = pos + 1;
              }
              state = NS_HTML5TOKENIZER_HANDLE_NCR_VALUE;
              goto decimalloop_end;
            } else {

              appendStrBuf(';');
              emitOrAppendStrBuf(returnState);
              if (!(returnState & (~1))) {
                cstart = pos + 1;
              }
              state = returnState;
              goto stateloop;
            }
          } else {
            if (!seenDigits) {

              emitOrAppendStrBuf(returnState);
              if (!(returnState & (~1))) {
                cstart = pos;
              }
              state = returnState;
              reconsume = PR_TRUE;
              goto stateloop;
            } else {

              if (!(returnState & (~1))) {
                cstart = pos;
              }
              state = NS_HTML5TOKENIZER_HANDLE_NCR_VALUE;
              reconsume = PR_TRUE;
              goto decimalloop_end;
            }
          }
        }
        decimalloop_end: ;
      }
      case NS_HTML5TOKENIZER_HANDLE_NCR_VALUE: {
        handleNcrValue(returnState);
        state = returnState;
        goto stateloop;
      }
      case NS_HTML5TOKENIZER_HEX_NCR_LOOP: {
        for (; ; ) {
          if (++pos == endPos) {
            goto stateloop_end;
          }
          c = checkChar(buf, pos);
          if (value < prevValue) {
            value = 0x110000;
          }
          prevValue = value;
          if (c >= '0' && c <= '9') {
            seenDigits = PR_TRUE;
            value *= 16;
            value += c - '0';
            continue;
          } else if (c >= 'A' && c <= 'F') {
            seenDigits = PR_TRUE;
            value *= 16;
            value += c - 'A' + 10;
            continue;
          } else if (c >= 'a' && c <= 'f') {
            seenDigits = PR_TRUE;
            value *= 16;
            value += c - 'a' + 10;
            continue;
          } else if (c == ';') {
            if (seenDigits) {
              if (!(returnState & (~1))) {
                cstart = pos + 1;
              }
              state = NS_HTML5TOKENIZER_HANDLE_NCR_VALUE;
              goto stateloop;
            } else {

              appendStrBuf(';');
              emitOrAppendStrBuf(returnState);
              if (!(returnState & (~1))) {
                cstart = pos + 1;
              }
              state = returnState;
              goto stateloop;
            }
          } else {
            if (!seenDigits) {

              emitOrAppendStrBuf(returnState);
              if (!(returnState & (~1))) {
                cstart = pos;
              }
              state = returnState;
              reconsume = PR_TRUE;
              goto stateloop;
            } else {

              if (!(returnState & (~1))) {
                cstart = pos;
              }
              state = NS_HTML5TOKENIZER_HANDLE_NCR_VALUE;
              reconsume = PR_TRUE;
              goto stateloop;
            }
          }
        }
      }
      case NS_HTML5TOKENIZER_PLAINTEXT: {
        for (; ; ) {
          if (reconsume) {
            reconsume = PR_FALSE;
          } else {
            if (++pos == endPos) {
              goto stateloop_end;
            }
            c = checkChar(buf, pos);
          }
          switch(c) {
            case '\0': {
              emitReplacementCharacter(buf, pos);
              continue;
            }
            case '\r': {
              emitCarriageReturn(buf, pos);
              goto stateloop_end;
            }
            case '\n': {
              silentLineFeed();
            }
            default: {
              continue;
            }
          }
        }

      }
      case NS_HTML5TOKENIZER_CDATA: {
        for (; ; ) {
          if (reconsume) {
            reconsume = PR_FALSE;
          } else {
            if (++pos == endPos) {
              goto stateloop_end;
            }
            c = checkChar(buf, pos);
          }
          switch(c) {
            case '<': {
              flushChars(buf, pos);
              returnState = state;
              state = NS_HTML5TOKENIZER_TAG_OPEN_NON_PCDATA;
              goto cdataloop_end;
            }
            case '\0': {
              emitReplacementCharacter(buf, pos);
              continue;
            }
            case '\r': {
              emitCarriageReturn(buf, pos);
              goto stateloop_end;
            }
            case '\n': {
              silentLineFeed();
            }
            default: {
              continue;
            }
          }
        }
        cdataloop_end: ;
      }
      case NS_HTML5TOKENIZER_TAG_OPEN_NON_PCDATA: {
        for (; ; ) {
          if (++pos == endPos) {
            goto stateloop_end;
          }
          c = checkChar(buf, pos);
          switch(c) {
            case '!': {
              tokenHandler->characters(nsHtml5Tokenizer::LT_GT, 0, 1);
              cstart = pos;
              state = NS_HTML5TOKENIZER_ESCAPE_EXCLAMATION;
              goto tagopennonpcdataloop_end;
            }
            case '/': {
              if (!!contentModelElement) {
                index = 0;
                clearStrBufForNextState();
                state = NS_HTML5TOKENIZER_CLOSE_TAG_OPEN_NOT_PCDATA;
                goto stateloop;
              }
            }
            default: {
              tokenHandler->characters(nsHtml5Tokenizer::LT_GT, 0, 1);
              cstart = pos;
              state = returnState;
              reconsume = PR_TRUE;
              goto stateloop;
            }
          }
        }
        tagopennonpcdataloop_end: ;
      }
      case NS_HTML5TOKENIZER_ESCAPE_EXCLAMATION: {
        for (; ; ) {
          if (++pos == endPos) {
            goto stateloop_end;
          }
          c = checkChar(buf, pos);
          switch(c) {
            case '-': {
              state = NS_HTML5TOKENIZER_ESCAPE_EXCLAMATION_HYPHEN;
              goto escapeexclamationloop_end;
            }
            default: {
              state = returnState;
              reconsume = PR_TRUE;
              goto stateloop;
            }
          }
        }
        escapeexclamationloop_end: ;
      }
      case NS_HTML5TOKENIZER_ESCAPE_EXCLAMATION_HYPHEN: {
        for (; ; ) {
          if (++pos == endPos) {
            goto stateloop_end;
          }
          c = checkChar(buf, pos);
          switch(c) {
            case '-': {
              state = NS_HTML5TOKENIZER_ESCAPE_HYPHEN_HYPHEN;
              goto escapeexclamationhyphenloop_end;
            }
            default: {
              state = returnState;
              reconsume = PR_TRUE;
              goto stateloop;
            }
          }
        }
        escapeexclamationhyphenloop_end: ;
      }
      case NS_HTML5TOKENIZER_ESCAPE_HYPHEN_HYPHEN: {
        for (; ; ) {
          if (++pos == endPos) {
            goto stateloop_end;
          }
          c = checkChar(buf, pos);
          switch(c) {
            case '-': {
              continue;
            }
            case '>': {
              state = returnState;
              goto stateloop;
            }
            case '\0': {
              emitReplacementCharacter(buf, pos);
              state = NS_HTML5TOKENIZER_ESCAPE;
              goto escapehyphenhyphenloop_end;
            }
            case '\r': {
              emitCarriageReturn(buf, pos);
              state = NS_HTML5TOKENIZER_ESCAPE;
              goto stateloop_end;
            }
            case '\n': {
              silentLineFeed();
            }
            default: {
              state = NS_HTML5TOKENIZER_ESCAPE;
              goto escapehyphenhyphenloop_end;
            }
          }
        }
        escapehyphenhyphenloop_end: ;
      }
      case NS_HTML5TOKENIZER_ESCAPE: {
        for (; ; ) {
          if (++pos == endPos) {
            goto stateloop_end;
          }
          c = checkChar(buf, pos);
          switch(c) {
            case '-': {
              state = NS_HTML5TOKENIZER_ESCAPE_HYPHEN;
              goto escapeloop_end;
            }
            case '\0': {
              emitReplacementCharacter(buf, pos);
              continue;
            }
            case '\r': {
              emitCarriageReturn(buf, pos);
              goto stateloop_end;
            }
            case '\n': {
              silentLineFeed();
            }
            default: {
              continue;
            }
          }
        }
        escapeloop_end: ;
      }
      case NS_HTML5TOKENIZER_ESCAPE_HYPHEN: {
        for (; ; ) {
          if (++pos == endPos) {
            goto stateloop_end;
          }
          c = checkChar(buf, pos);
          switch(c) {
            case '-': {
              state = NS_HTML5TOKENIZER_ESCAPE_HYPHEN_HYPHEN;
              goto stateloop;
            }
            case '\0': {
              emitReplacementCharacter(buf, pos);
              state = NS_HTML5TOKENIZER_ESCAPE;
              goto stateloop;
            }
            case '\r': {
              emitCarriageReturn(buf, pos);
              state = NS_HTML5TOKENIZER_ESCAPE;
              goto stateloop;
            }
            case '\n': {
              silentLineFeed();
            }
            default: {
              state = NS_HTML5TOKENIZER_ESCAPE;
              goto stateloop;
            }
          }
        }

      }
      case NS_HTML5TOKENIZER_CLOSE_TAG_OPEN_NOT_PCDATA: {
        for (; ; ) {
          if (++pos == endPos) {
            goto stateloop_end;
          }
          c = checkChar(buf, pos);
          if (index < contentModelElementNameAsArray.length) {
            PRUnichar e = contentModelElementNameAsArray[index];
            PRUnichar folded = c;
            if (c >= 'A' && c <= 'Z') {
              folded += 0x20;
            }
            if (folded != e) {
              tokenHandler->characters(nsHtml5Tokenizer::LT_SOLIDUS, 0, 2);
              emitStrBuf();
              cstart = pos;
              state = returnState;
              reconsume = PR_TRUE;
              goto stateloop;
            }
            appendStrBuf(c);
            index++;
            continue;
          } else {
            endTag = PR_TRUE;
            tagName = contentModelElement;
            switch(c) {
              case '\r': {
                silentCarriageReturn();
                state = NS_HTML5TOKENIZER_BEFORE_ATTRIBUTE_NAME;
                goto stateloop_end;
              }
              case '\n': {
                silentLineFeed();
              }
              case ' ':
              case '\t':
              case '\f': {
                state = NS_HTML5TOKENIZER_BEFORE_ATTRIBUTE_NAME;
                goto stateloop;
              }
              case '>': {
                state = emitCurrentTagToken(PR_FALSE, pos);
                if (shouldSuspend) {
                  goto stateloop_end;
                }
                goto stateloop;
              }
              case '/': {
                state = NS_HTML5TOKENIZER_SELF_CLOSING_START_TAG;
                goto stateloop;
              }
              default: {
                tokenHandler->characters(nsHtml5Tokenizer::LT_SOLIDUS, 0, 2);
                emitStrBuf();
                if (c == '\0') {
                  emitReplacementCharacter(buf, pos);
                } else {
                  cstart = pos;
                }
                state = returnState;
                goto stateloop;
              }
            }
          }
        }
      }
      case NS_HTML5TOKENIZER_CLOSE_TAG_OPEN_PCDATA: {
        if (++pos == endPos) {
          goto stateloop_end;
        }
        c = checkChar(buf, pos);
        switch(c) {
          case '>': {

            cstart = pos + 1;
            state = NS_HTML5TOKENIZER_DATA;
            goto stateloop;
          }
          case '\r': {
            silentCarriageReturn();

            clearLongStrBufAndAppendToComment('\n');
            state = NS_HTML5TOKENIZER_BOGUS_COMMENT;
            goto stateloop_end;
          }
          case '\n': {
            silentLineFeed();

            clearLongStrBufAndAppendToComment('\n');
            state = NS_HTML5TOKENIZER_BOGUS_COMMENT;
            goto stateloop;
          }
          case '\0': {
            c = 0xfffd;
          }
          default: {
            if (c >= 'A' && c <= 'Z') {
              c += 0x20;
            }
            if (c >= 'a' && c <= 'z') {
              endTag = PR_TRUE;
              clearStrBufAndAppendCurrentC(c);
              state = NS_HTML5TOKENIZER_TAG_NAME;
              goto stateloop;
            } else {

              clearLongStrBufAndAppendToComment(c);
              state = NS_HTML5TOKENIZER_BOGUS_COMMENT;
              goto stateloop;
            }
          }
        }
      }
      case NS_HTML5TOKENIZER_RCDATA: {
        for (; ; ) {
          if (reconsume) {
            reconsume = PR_FALSE;
          } else {
            if (++pos == endPos) {
              goto stateloop_end;
            }
            c = checkChar(buf, pos);
          }
          switch(c) {
            case '&': {
              flushChars(buf, pos);
              clearStrBufAndAppendCurrentC(c);
              additional = '\0';
              returnState = state;
              state = NS_HTML5TOKENIZER_CONSUME_CHARACTER_REFERENCE;
              goto stateloop;
            }
            case '<': {
              flushChars(buf, pos);
              returnState = state;
              state = NS_HTML5TOKENIZER_TAG_OPEN_NON_PCDATA;
              goto stateloop;
            }
            case '\0': {
              emitReplacementCharacter(buf, pos);
              continue;
            }
            case '\r': {
              emitCarriageReturn(buf, pos);
              goto stateloop_end;
            }
            case '\n': {
              silentLineFeed();
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
  flushChars(buf, pos);
  stateSave = state;
  returnStateSave = returnState;
  return pos;
}

void 
nsHtml5Tokenizer::emitCarriageReturn(PRUnichar* buf, PRInt32 pos)
{
  silentCarriageReturn();
  flushChars(buf, pos);
  tokenHandler->characters(nsHtml5Tokenizer::LF, 0, 1);
  cstart = PR_INT32_MAX;
}

void 
nsHtml5Tokenizer::emitReplacementCharacter(PRUnichar* buf, PRInt32 pos)
{
  silentCarriageReturn();
  flushChars(buf, pos);
  tokenHandler->characters(nsHtml5Tokenizer::REPLACEMENT_CHARACTER, 0, 1);
  cstart = PR_INT32_MAX;
}

void 
nsHtml5Tokenizer::rememberAmpersandLocation(PRUnichar add)
{
  additional = add;
}

void 
nsHtml5Tokenizer::bogusDoctype()
{

  forceQuirks = PR_TRUE;
}

void 
nsHtml5Tokenizer::bogusDoctypeWithoutQuirks()
{

  forceQuirks = PR_FALSE;
}

void 
nsHtml5Tokenizer::emitOrAppendStrBuf(PRInt32 returnState)
{
  if ((returnState & (~1))) {
    appendStrBufToLongStrBuf();
  } else {
    emitStrBuf();
  }
}

void 
nsHtml5Tokenizer::handleNcrValue(PRInt32 returnState)
{
  if (value >= 0x80 && value <= 0x9f) {

    PRUnichar* val = nsHtml5NamedCharacters::WINDOWS_1252[value - 0x80];
    emitOrAppendOne(val, returnState);
  } else if (value == 0x0D) {

    emitOrAppendOne(nsHtml5Tokenizer::LF, returnState);
  } else if ((value >= 0x0000 && value <= 0x0008) || (value == 0x000B) || (value >= 0x000E && value <= 0x001F) || value == 0x007F) {

    emitOrAppendOne(nsHtml5Tokenizer::REPLACEMENT_CHARACTER, returnState);
  } else if ((value & 0xF800) == 0xD800) {

    emitOrAppendOne(nsHtml5Tokenizer::REPLACEMENT_CHARACTER, returnState);
  } else if ((value & 0xFFFE) == 0xFFFE) {

    emitOrAppendOne(nsHtml5Tokenizer::REPLACEMENT_CHARACTER, returnState);
  } else if (value >= 0xFDD0 && value <= 0xFDEF) {

    emitOrAppendOne(nsHtml5Tokenizer::REPLACEMENT_CHARACTER, returnState);
  } else if (value <= 0xFFFF) {
    PRUnichar ch = (PRUnichar) value;
    bmpChar[0] = ch;
    emitOrAppendOne(bmpChar, returnState);
  } else if (value <= 0x10FFFF) {
    astralChar[0] = (PRUnichar) (NS_HTML5TOKENIZER_LEAD_OFFSET + (value >> 10));
    astralChar[1] = (PRUnichar) (0xDC00 + (value & 0x3FF));
    emitOrAppend(astralChar, returnState);
  } else {

    emitOrAppendOne(nsHtml5Tokenizer::REPLACEMENT_CHARACTER, returnState);
  }
}

void 
nsHtml5Tokenizer::eof()
{
  PRInt32 state = stateSave;
  PRInt32 returnState = returnStateSave;
  eofloop: for (; ; ) {
    switch(state) {
      case NS_HTML5TOKENIZER_TAG_OPEN_NON_PCDATA: {
        tokenHandler->characters(nsHtml5Tokenizer::LT_GT, 0, 1);
        goto eofloop_end;
      }
      case NS_HTML5TOKENIZER_TAG_OPEN: {

        tokenHandler->characters(nsHtml5Tokenizer::LT_GT, 0, 1);
        goto eofloop_end;
      }
      case NS_HTML5TOKENIZER_CLOSE_TAG_OPEN_NOT_PCDATA: {
        if (index < contentModelElementNameAsArray.length) {
          goto eofloop_end;
        } else {

          goto eofloop_end;
        }
      }
      case NS_HTML5TOKENIZER_CLOSE_TAG_OPEN_PCDATA: {

        tokenHandler->characters(nsHtml5Tokenizer::LT_SOLIDUS, 0, 2);
        goto eofloop_end;
      }
      case NS_HTML5TOKENIZER_TAG_NAME: {

        goto eofloop_end;
      }
      case NS_HTML5TOKENIZER_BEFORE_ATTRIBUTE_NAME:
      case NS_HTML5TOKENIZER_AFTER_ATTRIBUTE_VALUE_QUOTED:
      case NS_HTML5TOKENIZER_SELF_CLOSING_START_TAG: {

        goto eofloop_end;
      }
      case NS_HTML5TOKENIZER_ATTRIBUTE_NAME: {

        goto eofloop_end;
      }
      case NS_HTML5TOKENIZER_AFTER_ATTRIBUTE_NAME:
      case NS_HTML5TOKENIZER_BEFORE_ATTRIBUTE_VALUE: {

        goto eofloop_end;
      }
      case NS_HTML5TOKENIZER_ATTRIBUTE_VALUE_DOUBLE_QUOTED:
      case NS_HTML5TOKENIZER_ATTRIBUTE_VALUE_SINGLE_QUOTED:
      case NS_HTML5TOKENIZER_ATTRIBUTE_VALUE_UNQUOTED: {

        goto eofloop_end;
      }
      case NS_HTML5TOKENIZER_BOGUS_COMMENT: {
        emitComment(0, 0);
        goto eofloop_end;
      }
      case NS_HTML5TOKENIZER_BOGUS_COMMENT_HYPHEN: {
        emitComment(0, 0);
        goto eofloop_end;
      }
      case NS_HTML5TOKENIZER_MARKUP_DECLARATION_OPEN: {

        clearLongStrBuf();
        emitComment(0, 0);
        goto eofloop_end;
      }
      case NS_HTML5TOKENIZER_MARKUP_DECLARATION_HYPHEN: {

        emitComment(0, 0);
        goto eofloop_end;
      }
      case NS_HTML5TOKENIZER_MARKUP_DECLARATION_OCTYPE: {
        if (index < 6) {

          emitComment(0, 0);
        } else {

          doctypeName = nsHtml5Atoms::emptystring;
          publicIdentifier = nsnull;
          systemIdentifier = nsnull;
          forceQuirks = PR_TRUE;
          emitDoctypeToken(0);
          goto eofloop_end;
        }
        goto eofloop_end;
      }
      case NS_HTML5TOKENIZER_COMMENT_START:
      case NS_HTML5TOKENIZER_COMMENT:
      case NS_HTML5TOKENIZER_COMMENT_END_SPACE: {

        emitComment(0, 0);
        goto eofloop_end;
      }
      case NS_HTML5TOKENIZER_COMMENT_END: {

        emitComment(2, 0);
        goto eofloop_end;
      }
      case NS_HTML5TOKENIZER_COMMENT_END_DASH:
      case NS_HTML5TOKENIZER_COMMENT_START_DASH: {

        emitComment(1, 0);
        goto eofloop_end;
      }
      case NS_HTML5TOKENIZER_COMMENT_END_BANG: {

        emitComment(3, 0);
        goto eofloop_end;
      }
      case NS_HTML5TOKENIZER_DOCTYPE:
      case NS_HTML5TOKENIZER_BEFORE_DOCTYPE_NAME: {

        forceQuirks = PR_TRUE;
        emitDoctypeToken(0);
        goto eofloop_end;
      }
      case NS_HTML5TOKENIZER_DOCTYPE_NAME: {

        strBufToDoctypeName();
        forceQuirks = PR_TRUE;
        emitDoctypeToken(0);
        goto eofloop_end;
      }
      case NS_HTML5TOKENIZER_DOCTYPE_UBLIC:
      case NS_HTML5TOKENIZER_DOCTYPE_YSTEM:
      case NS_HTML5TOKENIZER_AFTER_DOCTYPE_NAME:
      case NS_HTML5TOKENIZER_BEFORE_DOCTYPE_PUBLIC_IDENTIFIER: {

        forceQuirks = PR_TRUE;
        emitDoctypeToken(0);
        goto eofloop_end;
      }
      case NS_HTML5TOKENIZER_DOCTYPE_PUBLIC_IDENTIFIER_DOUBLE_QUOTED:
      case NS_HTML5TOKENIZER_DOCTYPE_PUBLIC_IDENTIFIER_SINGLE_QUOTED: {

        forceQuirks = PR_TRUE;
        publicIdentifier = longStrBufToString();
        emitDoctypeToken(0);
        goto eofloop_end;
      }
      case NS_HTML5TOKENIZER_AFTER_DOCTYPE_PUBLIC_IDENTIFIER:
      case NS_HTML5TOKENIZER_BEFORE_DOCTYPE_SYSTEM_IDENTIFIER: {

        forceQuirks = PR_TRUE;
        emitDoctypeToken(0);
        goto eofloop_end;
      }
      case NS_HTML5TOKENIZER_DOCTYPE_SYSTEM_IDENTIFIER_DOUBLE_QUOTED:
      case NS_HTML5TOKENIZER_DOCTYPE_SYSTEM_IDENTIFIER_SINGLE_QUOTED: {

        forceQuirks = PR_TRUE;
        systemIdentifier = longStrBufToString();
        emitDoctypeToken(0);
        goto eofloop_end;
      }
      case NS_HTML5TOKENIZER_AFTER_DOCTYPE_SYSTEM_IDENTIFIER: {

        forceQuirks = PR_TRUE;
        emitDoctypeToken(0);
        goto eofloop_end;
      }
      case NS_HTML5TOKENIZER_BOGUS_DOCTYPE: {
        emitDoctypeToken(0);
        goto eofloop_end;
      }
      case NS_HTML5TOKENIZER_CONSUME_CHARACTER_REFERENCE: {
        emitOrAppendStrBuf(returnState);
        state = returnState;
        continue;
      }
      case NS_HTML5TOKENIZER_CHARACTER_REFERENCE_LOOP: {
        for (; ; ) {
          PRUnichar c = '\0';
          entCol++;
          for (; ; ) {
            if (hi == -1) {
              goto hiloop_end;
            }
            if (entCol == nsHtml5NamedCharacters::NAMES[hi].length) {
              goto hiloop_end;
            }
            if (entCol > nsHtml5NamedCharacters::NAMES[hi].length) {
              goto outer_end;
            } else if (c < nsHtml5NamedCharacters::NAMES[hi][entCol]) {
              hi--;
            } else {
              goto hiloop_end;
            }
          }
          hiloop_end: ;
          for (; ; ) {
            if (hi < lo) {
              goto outer_end;
            }
            if (entCol == nsHtml5NamedCharacters::NAMES[lo].length) {
              candidate = lo;
              strBufMark = strBufLen;
              lo++;
            } else if (entCol > nsHtml5NamedCharacters::NAMES[lo].length) {
              goto outer_end;
            } else if (c > nsHtml5NamedCharacters::NAMES[lo][entCol]) {
              lo++;
            } else {
              goto loloop_end;
            }
          }
          loloop_end: ;
          if (hi < lo) {
            goto outer_end;
          }
          continue;
        }
        outer_end: ;
        if (candidate == -1) {

          emitOrAppendStrBuf(returnState);
          state = returnState;
          goto eofloop;
        } else {
          jArray<PRUnichar,PRInt32> candidateArr = nsHtml5NamedCharacters::NAMES[candidate];
          if (candidateArr[candidateArr.length - 1] != ';') {
            if ((returnState & (~1))) {
              PRUnichar ch;
              if (strBufMark == strBufLen) {
                ch = '\0';
              } else {
                ch = strBuf[strBufMark];
              }
              if ((ch >= '0' && ch <= '9') || (ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z')) {

                appendStrBufToLongStrBuf();
                state = returnState;
                goto eofloop;
              }
            }
            if ((returnState & (~1))) {

            } else {

            }
          }
          jArray<PRUnichar,PRInt32> val = nsHtml5NamedCharacters::VALUES[candidate];
          emitOrAppend(val, returnState);
          if (strBufMark < strBufLen) {
            if ((returnState & (~1))) {
              for (PRInt32 i = strBufMark; i < strBufLen; i++) {
                appendLongStrBuf(strBuf[i]);
              }
            } else {
              tokenHandler->characters(strBuf, strBufMark, strBufLen - strBufMark);
            }
          }
          state = returnState;
          goto eofloop;
        }
      }
      case NS_HTML5TOKENIZER_CONSUME_NCR:
      case NS_HTML5TOKENIZER_DECIMAL_NRC_LOOP:
      case NS_HTML5TOKENIZER_HEX_NCR_LOOP: {
        if (!seenDigits) {

          emitOrAppendStrBuf(returnState);
          state = returnState;
          continue;
        } else {

        }
        handleNcrValue(returnState);
        state = returnState;
        continue;
      }
      case NS_HTML5TOKENIZER_DATA:
      default: {
        goto eofloop_end;
      }
    }
  }
  eofloop_end: ;
  tokenHandler->eof();
  return;
}

void 
nsHtml5Tokenizer::emitDoctypeToken(PRInt32 pos)
{
  cstart = pos + 1;
  tokenHandler->doctype(doctypeName, publicIdentifier, systemIdentifier, forceQuirks);
  nsHtml5Portability::releaseLocal(doctypeName);
  nsHtml5Portability::releaseString(publicIdentifier);
  nsHtml5Portability::releaseString(systemIdentifier);
}

void 
nsHtml5Tokenizer::internalEncodingDeclaration(nsString* internalCharset)
{
  if (!!encodingDeclarationHandler) {
    encodingDeclarationHandler->internalEncodingDeclaration(internalCharset);
  }
}

void 
nsHtml5Tokenizer::emitOrAppend(jArray<PRUnichar,PRInt32> val, PRInt32 returnState)
{
  if ((returnState & (~1))) {
    appendLongStrBuf(val);
  } else {
    tokenHandler->characters(val, 0, val.length);
  }
}

void 
nsHtml5Tokenizer::emitOrAppendOne(PRUnichar* val, PRInt32 returnState)
{
  if ((returnState & (~1))) {
    appendLongStrBuf(val[0]);
  } else {
    tokenHandler->characters(val, 0, 1);
  }
}

void 
nsHtml5Tokenizer::end()
{
  strBuf = nsnull;
  longStrBuf = nsnull;
  systemIdentifier = nsnull;
  publicIdentifier = nsnull;
  doctypeName = nsnull;
  tagName = nsnull;
  attributeName = nsnull;
  tokenHandler->endTokenization();
  if (!!attributes) {
    attributes->clear(0);
    delete attributes;
    attributes = nsnull;
  }
}

void 
nsHtml5Tokenizer::requestSuspension()
{
  shouldSuspend = PR_TRUE;
}

void 
nsHtml5Tokenizer::becomeConfident()
{
  confident = PR_TRUE;
}

PRBool 
nsHtml5Tokenizer::isNextCharOnNewLine()
{
  return PR_FALSE;
}

PRBool 
nsHtml5Tokenizer::isPrevCR()
{
  return lastCR;
}

PRInt32 
nsHtml5Tokenizer::getLine()
{
  return -1;
}

PRInt32 
nsHtml5Tokenizer::getCol()
{
  return -1;
}

PRBool 
nsHtml5Tokenizer::isInDataState()
{
  return (stateSave == NS_HTML5TOKENIZER_DATA);
}

void 
nsHtml5Tokenizer::setEncodingDeclarationHandler(nsHtml5StreamParser* encodingDeclarationHandler)
{
  this->encodingDeclarationHandler = encodingDeclarationHandler;
}

void
nsHtml5Tokenizer::initializeStatics()
{
  TITLE_ARR = jArray<PRUnichar,PRInt32>((PRUnichar*)TITLE_ARR_DATA, 5);
  SCRIPT_ARR = jArray<PRUnichar,PRInt32>((PRUnichar*)SCRIPT_ARR_DATA, 6);
  STYLE_ARR = jArray<PRUnichar,PRInt32>((PRUnichar*)STYLE_ARR_DATA, 5);
  PLAINTEXT_ARR = jArray<PRUnichar,PRInt32>((PRUnichar*)PLAINTEXT_ARR_DATA, 9);
  XMP_ARR = jArray<PRUnichar,PRInt32>((PRUnichar*)XMP_ARR_DATA, 3);
  TEXTAREA_ARR = jArray<PRUnichar,PRInt32>((PRUnichar*)TEXTAREA_ARR_DATA, 8);
  IFRAME_ARR = jArray<PRUnichar,PRInt32>((PRUnichar*)IFRAME_ARR_DATA, 6);
  NOEMBED_ARR = jArray<PRUnichar,PRInt32>((PRUnichar*)NOEMBED_ARR_DATA, 7);
  NOSCRIPT_ARR = jArray<PRUnichar,PRInt32>((PRUnichar*)NOSCRIPT_ARR_DATA, 8);
  NOFRAMES_ARR = jArray<PRUnichar,PRInt32>((PRUnichar*)NOFRAMES_ARR_DATA, 8);
}

void
nsHtml5Tokenizer::releaseStatics()
{
}


