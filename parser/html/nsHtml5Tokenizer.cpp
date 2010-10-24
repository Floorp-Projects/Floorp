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

#define nsHtml5Tokenizer_cpp__

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
    astralChar(jArray<PRUnichar,PRInt32>(2)),
    tagName(nsnull),
    attributeName(nsnull),
    doctypeName(nsnull),
    publicIdentifier(nsnull),
    systemIdentifier(nsnull),
    attributes(nsnull)
{
  MOZ_COUNT_CTOR(nsHtml5Tokenizer);
}

void 
nsHtml5Tokenizer::setInterner(nsHtml5AtomTable* interner)
{
  this->interner = interner;
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
nsHtml5Tokenizer::setStateAndEndTagExpectation(PRInt32 specialTokenizerState, nsIAtom* endTagExpectation)
{
  this->stateSave = specialTokenizerState;
  if (specialTokenizerState == NS_HTML5TOKENIZER_DATA) {
    return;
  }
  jArray<PRUnichar,PRInt32> asArray = nsHtml5Portability::newCharArrayFromLocal(endTagExpectation);
  this->endTagExpectation = nsHtml5ElementName::elementNameByBuffer(asArray, 0, asArray.length, interner);
  asArray.release();
  endTagExpectationToArray();
}

void 
nsHtml5Tokenizer::setStateAndEndTagExpectation(PRInt32 specialTokenizerState, nsHtml5ElementName* endTagExpectation)
{
  this->stateSave = specialTokenizerState;
  this->endTagExpectation = endTagExpectation;
  endTagExpectationToArray();
}

void 
nsHtml5Tokenizer::endTagExpectationToArray()
{
  switch(endTagExpectation->group) {
    case NS_HTML5TREE_BUILDER_TITLE: {
      endTagExpectationAsArray = TITLE_ARR;
      return;
    }
    case NS_HTML5TREE_BUILDER_SCRIPT: {
      endTagExpectationAsArray = SCRIPT_ARR;
      return;
    }
    case NS_HTML5TREE_BUILDER_STYLE: {
      endTagExpectationAsArray = STYLE_ARR;
      return;
    }
    case NS_HTML5TREE_BUILDER_PLAINTEXT: {
      endTagExpectationAsArray = PLAINTEXT_ARR;
      return;
    }
    case NS_HTML5TREE_BUILDER_XMP: {
      endTagExpectationAsArray = XMP_ARR;
      return;
    }
    case NS_HTML5TREE_BUILDER_TEXTAREA: {
      endTagExpectationAsArray = TEXTAREA_ARR;
      return;
    }
    case NS_HTML5TREE_BUILDER_IFRAME: {
      endTagExpectationAsArray = IFRAME_ARR;
      return;
    }
    case NS_HTML5TREE_BUILDER_NOEMBED: {
      endTagExpectationAsArray = NOEMBED_ARR;
      return;
    }
    case NS_HTML5TREE_BUILDER_NOSCRIPT: {
      endTagExpectationAsArray = NOSCRIPT_ARR;
      return;
    }
    case NS_HTML5TREE_BUILDER_NOFRAMES: {
      endTagExpectationAsArray = NOFRAMES_ARR;
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

nsString* 
nsHtml5Tokenizer::strBufToString()
{
  return nsHtml5Portability::newStringFromBuffer(strBuf, 0, strBufLen);
}

void 
nsHtml5Tokenizer::strBufToDoctypeName()
{
  doctypeName = nsHtml5Portability::newLocalNameFromBuffer(strBuf, 0, strBufLen, interner);
}

void 
nsHtml5Tokenizer::emitStrBuf()
{
  if (strBufLen > 0) {
    tokenHandler->characters(strBuf, 0, strBufLen);
  }
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
  cstart = PR_INT32_MAX;
}

void 
nsHtml5Tokenizer::resetAttributes()
{
  attributes = nsnull;
}

void 
nsHtml5Tokenizer::strBufToElementNameString()
{
  tagName = nsHtml5ElementName::elementNameByBuffer(strBuf, 0, strBufLen, interner);
}

PRInt32 
nsHtml5Tokenizer::emitCurrentTagToken(PRBool selfClosing, PRInt32 pos)
{
  cstart = pos + 1;

  stateSave = NS_HTML5TOKENIZER_DATA;
  nsHtml5HtmlAttributes* attrs = (!attributes ? nsHtml5HtmlAttributes::EMPTY_ATTRIBUTES : attributes);
  if (endTag) {

    tokenHandler->endTag(tagName);
    delete attributes;
  } else {
    tokenHandler->startTag(tagName, attrs, selfClosing);
  }
  tagName->release();
  tagName = nsnull;
  resetAttributes();
  return stateSave;
}

void 
nsHtml5Tokenizer::attributeNameComplete()
{
  attributeName = nsHtml5AttributeName::nameByBuffer(strBuf, 0, strBufLen, interner);
  if (!attributes) {
    attributes = new nsHtml5HtmlAttributes(0);
  }
  if (attributes->contains(attributeName)) {

    attributeName->release();
    attributeName = nsnull;
  }
}

void 
nsHtml5Tokenizer::addAttributeWithoutValue()
{

  if (attributeName) {
    attributes->addAttribute(attributeName, nsHtml5Portability::newEmptyString());
    attributeName = nsnull;
  }
}

void 
nsHtml5Tokenizer::addAttributeWithValue()
{
  if (attributeName) {
    nsString* val = longStrBufToString();
    attributes->addAttribute(attributeName, val);
    attributeName = nsnull;
  }
}

void 
nsHtml5Tokenizer::start()
{
  initializeWithoutStarting();
  tokenHandler->startTokenization(this);
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
    case NS_HTML5TOKENIZER_SCRIPT_DATA:
    case NS_HTML5TOKENIZER_PLAINTEXT:
    case NS_HTML5TOKENIZER_RAWTEXT:
    case NS_HTML5TOKENIZER_CDATA_SECTION:
    case NS_HTML5TOKENIZER_SCRIPT_DATA_ESCAPED:
    case NS_HTML5TOKENIZER_SCRIPT_DATA_ESCAPE_START:
    case NS_HTML5TOKENIZER_SCRIPT_DATA_ESCAPE_START_DASH:
    case NS_HTML5TOKENIZER_SCRIPT_DATA_ESCAPED_DASH:
    case NS_HTML5TOKENIZER_SCRIPT_DATA_ESCAPED_DASH_DASH:
    case NS_HTML5TOKENIZER_SCRIPT_DATA_DOUBLE_ESCAPE_START:
    case NS_HTML5TOKENIZER_SCRIPT_DATA_DOUBLE_ESCAPED:
    case NS_HTML5TOKENIZER_SCRIPT_DATA_DOUBLE_ESCAPED_LESS_THAN_SIGN:
    case NS_HTML5TOKENIZER_SCRIPT_DATA_DOUBLE_ESCAPED_DASH:
    case NS_HTML5TOKENIZER_SCRIPT_DATA_DOUBLE_ESCAPED_DASH_DASH:
    case NS_HTML5TOKENIZER_SCRIPT_DATA_DOUBLE_ESCAPE_END: {
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
              NS_HTML5_BREAK(stateloop);
            }
            c = checkChar(buf, pos);
          }
          switch(c) {
            case '&': {
              flushChars(buf, pos);
              clearStrBufAndAppend(c);
              setAdditionalAndRememberAmpersandLocation('\0');
              returnState = state;
              state = NS_HTML5TOKENIZER_CONSUME_CHARACTER_REFERENCE;
              NS_HTML5_CONTINUE(stateloop);
            }
            case '<': {
              flushChars(buf, pos);
              state = NS_HTML5TOKENIZER_TAG_OPEN;
              NS_HTML5_BREAK(dataloop);
            }
            case '\0': {
              emitReplacementCharacter(buf, pos);
              continue;
            }
            case '\r': {
              emitCarriageReturn(buf, pos);
              NS_HTML5_BREAK(stateloop);
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
            NS_HTML5_BREAK(stateloop);
          }
          c = checkChar(buf, pos);
          if (c >= 'A' && c <= 'Z') {
            endTag = PR_FALSE;
            clearStrBufAndAppend((PRUnichar) (c + 0x20));
            state = NS_HTML5TOKENIZER_TAG_NAME;
            NS_HTML5_BREAK(tagopenloop);
          } else if (c >= 'a' && c <= 'z') {
            endTag = PR_FALSE;
            clearStrBufAndAppend(c);
            state = NS_HTML5TOKENIZER_TAG_NAME;
            NS_HTML5_BREAK(tagopenloop);
          }
          switch(c) {
            case '!': {
              state = NS_HTML5TOKENIZER_MARKUP_DECLARATION_OPEN;
              NS_HTML5_CONTINUE(stateloop);
            }
            case '/': {
              state = NS_HTML5TOKENIZER_CLOSE_TAG_OPEN;
              NS_HTML5_CONTINUE(stateloop);
            }
            case '\?': {

              clearLongStrBufAndAppend(c);
              state = NS_HTML5TOKENIZER_BOGUS_COMMENT;
              NS_HTML5_CONTINUE(stateloop);
            }
            case '>': {

              tokenHandler->characters(nsHtml5Tokenizer::LT_GT, 0, 2);
              cstart = pos + 1;
              state = NS_HTML5TOKENIZER_DATA;
              NS_HTML5_CONTINUE(stateloop);
            }
            default: {

              tokenHandler->characters(nsHtml5Tokenizer::LT_GT, 0, 1);
              cstart = pos;
              state = NS_HTML5TOKENIZER_DATA;
              reconsume = PR_TRUE;
              NS_HTML5_CONTINUE(stateloop);
            }
          }
        }
        tagopenloop_end: ;
      }
      case NS_HTML5TOKENIZER_TAG_NAME: {
        for (; ; ) {
          if (++pos == endPos) {
            NS_HTML5_BREAK(stateloop);
          }
          c = checkChar(buf, pos);
          switch(c) {
            case '\r': {
              silentCarriageReturn();
              strBufToElementNameString();
              state = NS_HTML5TOKENIZER_BEFORE_ATTRIBUTE_NAME;
              NS_HTML5_BREAK(stateloop);
            }
            case '\n': {
              silentLineFeed();
            }
            case ' ':
            case '\t':
            case '\f': {
              strBufToElementNameString();
              state = NS_HTML5TOKENIZER_BEFORE_ATTRIBUTE_NAME;
              NS_HTML5_BREAK(tagnameloop);
            }
            case '/': {
              strBufToElementNameString();
              state = NS_HTML5TOKENIZER_SELF_CLOSING_START_TAG;
              NS_HTML5_CONTINUE(stateloop);
            }
            case '>': {
              strBufToElementNameString();
              state = emitCurrentTagToken(PR_FALSE, pos);
              if (shouldSuspend) {
                NS_HTML5_BREAK(stateloop);
              }
              NS_HTML5_CONTINUE(stateloop);
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
              NS_HTML5_BREAK(stateloop);
            }
            c = checkChar(buf, pos);
          }
          switch(c) {
            case '\r': {
              silentCarriageReturn();
              NS_HTML5_BREAK(stateloop);
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
              NS_HTML5_CONTINUE(stateloop);
            }
            case '>': {
              state = emitCurrentTagToken(PR_FALSE, pos);
              if (shouldSuspend) {
                NS_HTML5_BREAK(stateloop);
              }
              NS_HTML5_CONTINUE(stateloop);
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
              clearStrBufAndAppend(c);
              state = NS_HTML5TOKENIZER_ATTRIBUTE_NAME;
              NS_HTML5_BREAK(beforeattributenameloop);
            }
          }
        }
        beforeattributenameloop_end: ;
      }
      case NS_HTML5TOKENIZER_ATTRIBUTE_NAME: {
        for (; ; ) {
          if (++pos == endPos) {
            NS_HTML5_BREAK(stateloop);
          }
          c = checkChar(buf, pos);
          switch(c) {
            case '\r': {
              silentCarriageReturn();
              attributeNameComplete();
              state = NS_HTML5TOKENIZER_AFTER_ATTRIBUTE_NAME;
              NS_HTML5_BREAK(stateloop);
            }
            case '\n': {
              silentLineFeed();
            }
            case ' ':
            case '\t':
            case '\f': {
              attributeNameComplete();
              state = NS_HTML5TOKENIZER_AFTER_ATTRIBUTE_NAME;
              NS_HTML5_CONTINUE(stateloop);
            }
            case '/': {
              attributeNameComplete();
              addAttributeWithoutValue();
              state = NS_HTML5TOKENIZER_SELF_CLOSING_START_TAG;
              NS_HTML5_CONTINUE(stateloop);
            }
            case '=': {
              attributeNameComplete();
              state = NS_HTML5TOKENIZER_BEFORE_ATTRIBUTE_VALUE;
              NS_HTML5_BREAK(attributenameloop);
            }
            case '>': {
              attributeNameComplete();
              addAttributeWithoutValue();
              state = emitCurrentTagToken(PR_FALSE, pos);
              if (shouldSuspend) {
                NS_HTML5_BREAK(stateloop);
              }
              NS_HTML5_CONTINUE(stateloop);
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
            NS_HTML5_BREAK(stateloop);
          }
          c = checkChar(buf, pos);
          switch(c) {
            case '\r': {
              silentCarriageReturn();
              NS_HTML5_BREAK(stateloop);
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
              clearLongStrBuf();
              state = NS_HTML5TOKENIZER_ATTRIBUTE_VALUE_DOUBLE_QUOTED;
              NS_HTML5_BREAK(beforeattributevalueloop);
            }
            case '&': {
              clearLongStrBuf();
              state = NS_HTML5TOKENIZER_ATTRIBUTE_VALUE_UNQUOTED;

              reconsume = PR_TRUE;
              NS_HTML5_CONTINUE(stateloop);
            }
            case '\'': {
              clearLongStrBuf();
              state = NS_HTML5TOKENIZER_ATTRIBUTE_VALUE_SINGLE_QUOTED;
              NS_HTML5_CONTINUE(stateloop);
            }
            case '>': {

              addAttributeWithoutValue();
              state = emitCurrentTagToken(PR_FALSE, pos);
              if (shouldSuspend) {
                NS_HTML5_BREAK(stateloop);
              }
              NS_HTML5_CONTINUE(stateloop);
            }
            case '\0': {
              c = 0xfffd;
            }
            case '<':
            case '=':
            case '`':
            default: {
              clearLongStrBufAndAppend(c);
              state = NS_HTML5TOKENIZER_ATTRIBUTE_VALUE_UNQUOTED;

              NS_HTML5_CONTINUE(stateloop);
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
              NS_HTML5_BREAK(stateloop);
            }
            c = checkChar(buf, pos);
          }
          switch(c) {
            case '\"': {
              addAttributeWithValue();
              state = NS_HTML5TOKENIZER_AFTER_ATTRIBUTE_VALUE_QUOTED;
              NS_HTML5_BREAK(attributevaluedoublequotedloop);
            }
            case '&': {
              clearStrBufAndAppend(c);
              setAdditionalAndRememberAmpersandLocation('\"');
              returnState = state;
              state = NS_HTML5TOKENIZER_CONSUME_CHARACTER_REFERENCE;
              NS_HTML5_CONTINUE(stateloop);
            }
            case '\r': {
              appendLongStrBufCarriageReturn();
              NS_HTML5_BREAK(stateloop);
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
            NS_HTML5_BREAK(stateloop);
          }
          c = checkChar(buf, pos);
          switch(c) {
            case '\r': {
              silentCarriageReturn();
              state = NS_HTML5TOKENIZER_BEFORE_ATTRIBUTE_NAME;
              NS_HTML5_BREAK(stateloop);
            }
            case '\n': {
              silentLineFeed();
            }
            case ' ':
            case '\t':
            case '\f': {
              state = NS_HTML5TOKENIZER_BEFORE_ATTRIBUTE_NAME;
              NS_HTML5_CONTINUE(stateloop);
            }
            case '/': {
              state = NS_HTML5TOKENIZER_SELF_CLOSING_START_TAG;
              NS_HTML5_BREAK(afterattributevaluequotedloop);
            }
            case '>': {
              state = emitCurrentTagToken(PR_FALSE, pos);
              if (shouldSuspend) {
                NS_HTML5_BREAK(stateloop);
              }
              NS_HTML5_CONTINUE(stateloop);
            }
            default: {

              state = NS_HTML5TOKENIZER_BEFORE_ATTRIBUTE_NAME;
              reconsume = PR_TRUE;
              NS_HTML5_CONTINUE(stateloop);
            }
          }
        }
        afterattributevaluequotedloop_end: ;
      }
      case NS_HTML5TOKENIZER_SELF_CLOSING_START_TAG: {
        if (++pos == endPos) {
          NS_HTML5_BREAK(stateloop);
        }
        c = checkChar(buf, pos);
        switch(c) {
          case '>': {
            state = emitCurrentTagToken(PR_TRUE, pos);
            if (shouldSuspend) {
              NS_HTML5_BREAK(stateloop);
            }
            NS_HTML5_CONTINUE(stateloop);
          }
          default: {

            state = NS_HTML5TOKENIZER_BEFORE_ATTRIBUTE_NAME;
            reconsume = PR_TRUE;
            NS_HTML5_CONTINUE(stateloop);
          }
        }
      }
      case NS_HTML5TOKENIZER_ATTRIBUTE_VALUE_UNQUOTED: {
        for (; ; ) {
          if (reconsume) {
            reconsume = PR_FALSE;
          } else {
            if (++pos == endPos) {
              NS_HTML5_BREAK(stateloop);
            }
            c = checkChar(buf, pos);
          }
          switch(c) {
            case '\r': {
              silentCarriageReturn();
              addAttributeWithValue();
              state = NS_HTML5TOKENIZER_BEFORE_ATTRIBUTE_NAME;
              NS_HTML5_BREAK(stateloop);
            }
            case '\n': {
              silentLineFeed();
            }
            case ' ':
            case '\t':
            case '\f': {
              addAttributeWithValue();
              state = NS_HTML5TOKENIZER_BEFORE_ATTRIBUTE_NAME;
              NS_HTML5_CONTINUE(stateloop);
            }
            case '&': {
              clearStrBufAndAppend(c);
              setAdditionalAndRememberAmpersandLocation('>');
              returnState = state;
              state = NS_HTML5TOKENIZER_CONSUME_CHARACTER_REFERENCE;
              NS_HTML5_CONTINUE(stateloop);
            }
            case '>': {
              addAttributeWithValue();
              state = emitCurrentTagToken(PR_FALSE, pos);
              if (shouldSuspend) {
                NS_HTML5_BREAK(stateloop);
              }
              NS_HTML5_CONTINUE(stateloop);
            }
            case '\0': {
              c = 0xfffd;
            }
            case '<':
            case '\"':
            case '\'':
            case '=':
            case '`':
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
            NS_HTML5_BREAK(stateloop);
          }
          c = checkChar(buf, pos);
          switch(c) {
            case '\r': {
              silentCarriageReturn();
              NS_HTML5_BREAK(stateloop);
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
              NS_HTML5_CONTINUE(stateloop);
            }
            case '=': {
              state = NS_HTML5TOKENIZER_BEFORE_ATTRIBUTE_VALUE;
              NS_HTML5_CONTINUE(stateloop);
            }
            case '>': {
              addAttributeWithoutValue();
              state = emitCurrentTagToken(PR_FALSE, pos);
              if (shouldSuspend) {
                NS_HTML5_BREAK(stateloop);
              }
              NS_HTML5_CONTINUE(stateloop);
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
              clearStrBufAndAppend(c);
              state = NS_HTML5TOKENIZER_ATTRIBUTE_NAME;
              NS_HTML5_CONTINUE(stateloop);
            }
          }
        }
      }
      case NS_HTML5TOKENIZER_MARKUP_DECLARATION_OPEN: {
        for (; ; ) {
          if (++pos == endPos) {
            NS_HTML5_BREAK(stateloop);
          }
          c = checkChar(buf, pos);
          switch(c) {
            case '-': {
              clearLongStrBufAndAppend(c);
              state = NS_HTML5TOKENIZER_MARKUP_DECLARATION_HYPHEN;
              NS_HTML5_BREAK(markupdeclarationopenloop);
            }
            case 'd':
            case 'D': {
              clearLongStrBufAndAppend(c);
              index = 0;
              state = NS_HTML5TOKENIZER_MARKUP_DECLARATION_OCTYPE;
              NS_HTML5_CONTINUE(stateloop);
            }
            case '[': {
              if (tokenHandler->cdataSectionAllowed()) {
                clearLongStrBufAndAppend(c);
                index = 0;
                state = NS_HTML5TOKENIZER_CDATA_START;
                NS_HTML5_CONTINUE(stateloop);
              }
            }
            default: {

              clearLongStrBuf();
              state = NS_HTML5TOKENIZER_BOGUS_COMMENT;
              reconsume = PR_TRUE;
              NS_HTML5_CONTINUE(stateloop);
            }
          }
        }
        markupdeclarationopenloop_end: ;
      }
      case NS_HTML5TOKENIZER_MARKUP_DECLARATION_HYPHEN: {
        for (; ; ) {
          if (++pos == endPos) {
            NS_HTML5_BREAK(stateloop);
          }
          c = checkChar(buf, pos);
          switch(c) {
            case '\0': {
              NS_HTML5_BREAK(stateloop);
            }
            case '-': {
              clearLongStrBuf();
              state = NS_HTML5TOKENIZER_COMMENT_START;
              NS_HTML5_BREAK(markupdeclarationhyphenloop);
            }
            default: {

              state = NS_HTML5TOKENIZER_BOGUS_COMMENT;
              reconsume = PR_TRUE;
              NS_HTML5_CONTINUE(stateloop);
            }
          }
        }
        markupdeclarationhyphenloop_end: ;
      }
      case NS_HTML5TOKENIZER_COMMENT_START: {
        for (; ; ) {
          if (++pos == endPos) {
            NS_HTML5_BREAK(stateloop);
          }
          c = checkChar(buf, pos);
          switch(c) {
            case '-': {
              appendLongStrBuf(c);
              state = NS_HTML5TOKENIZER_COMMENT_START_DASH;
              NS_HTML5_CONTINUE(stateloop);
            }
            case '>': {

              emitComment(0, pos);
              state = NS_HTML5TOKENIZER_DATA;
              NS_HTML5_CONTINUE(stateloop);
            }
            case '\r': {
              appendLongStrBufCarriageReturn();
              state = NS_HTML5TOKENIZER_COMMENT;
              NS_HTML5_BREAK(stateloop);
            }
            case '\n': {
              appendLongStrBufLineFeed();
              state = NS_HTML5TOKENIZER_COMMENT;
              NS_HTML5_BREAK(commentstartloop);
            }
            case '\0': {
              c = 0xfffd;
            }
            default: {
              appendLongStrBuf(c);
              state = NS_HTML5TOKENIZER_COMMENT;
              NS_HTML5_BREAK(commentstartloop);
            }
          }
        }
        commentstartloop_end: ;
      }
      case NS_HTML5TOKENIZER_COMMENT: {
        for (; ; ) {
          if (++pos == endPos) {
            NS_HTML5_BREAK(stateloop);
          }
          c = checkChar(buf, pos);
          switch(c) {
            case '-': {
              appendLongStrBuf(c);
              state = NS_HTML5TOKENIZER_COMMENT_END_DASH;
              NS_HTML5_BREAK(commentloop);
            }
            case '\r': {
              appendLongStrBufCarriageReturn();
              NS_HTML5_BREAK(stateloop);
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
            NS_HTML5_BREAK(stateloop);
          }
          c = checkChar(buf, pos);
          switch(c) {
            case '-': {
              appendLongStrBuf(c);
              state = NS_HTML5TOKENIZER_COMMENT_END;
              NS_HTML5_BREAK(commentenddashloop);
            }
            case '\r': {
              appendLongStrBufCarriageReturn();
              state = NS_HTML5TOKENIZER_COMMENT;
              NS_HTML5_BREAK(stateloop);
            }
            case '\n': {
              appendLongStrBufLineFeed();
              state = NS_HTML5TOKENIZER_COMMENT;
              NS_HTML5_CONTINUE(stateloop);
            }
            case '\0': {
              c = 0xfffd;
            }
            default: {
              appendLongStrBuf(c);
              state = NS_HTML5TOKENIZER_COMMENT;
              NS_HTML5_CONTINUE(stateloop);
            }
          }
        }
        commentenddashloop_end: ;
      }
      case NS_HTML5TOKENIZER_COMMENT_END: {
        for (; ; ) {
          if (++pos == endPos) {
            NS_HTML5_BREAK(stateloop);
          }
          c = checkChar(buf, pos);
          switch(c) {
            case '>': {
              emitComment(2, pos);
              state = NS_HTML5TOKENIZER_DATA;
              NS_HTML5_CONTINUE(stateloop);
            }
            case '-': {
              adjustDoubleHyphenAndAppendToLongStrBufAndErr(c);
              continue;
            }
            case '\r': {
              adjustDoubleHyphenAndAppendToLongStrBufCarriageReturn();
              state = NS_HTML5TOKENIZER_COMMENT;
              NS_HTML5_BREAK(stateloop);
            }
            case '\n': {
              adjustDoubleHyphenAndAppendToLongStrBufLineFeed();
              state = NS_HTML5TOKENIZER_COMMENT;
              NS_HTML5_CONTINUE(stateloop);
            }
            case '!': {

              appendLongStrBuf(c);
              state = NS_HTML5TOKENIZER_COMMENT_END_BANG;
              NS_HTML5_CONTINUE(stateloop);
            }
            case '\0': {
              c = 0xfffd;
            }
            default: {
              adjustDoubleHyphenAndAppendToLongStrBufAndErr(c);
              state = NS_HTML5TOKENIZER_COMMENT;
              NS_HTML5_CONTINUE(stateloop);
            }
          }
        }

      }
      case NS_HTML5TOKENIZER_COMMENT_END_SPACE: {
        for (; ; ) {
          if (++pos == endPos) {
            NS_HTML5_BREAK(stateloop);
          }
          c = checkChar(buf, pos);
          switch(c) {
            case '>': {
              emitComment(0, pos);
              state = NS_HTML5TOKENIZER_DATA;
              NS_HTML5_CONTINUE(stateloop);
            }
            case '-': {
              appendLongStrBuf(c);
              state = NS_HTML5TOKENIZER_COMMENT_END_DASH;
              NS_HTML5_CONTINUE(stateloop);
            }
            case ' ':
            case '\t':
            case '\f': {
              appendLongStrBuf(c);
              continue;
            }
            case '\r': {
              appendLongStrBufCarriageReturn();
              NS_HTML5_BREAK(stateloop);
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
              NS_HTML5_CONTINUE(stateloop);
            }
          }
        }
      }
      case NS_HTML5TOKENIZER_COMMENT_END_BANG: {
        for (; ; ) {
          if (++pos == endPos) {
            NS_HTML5_BREAK(stateloop);
          }
          c = checkChar(buf, pos);
          switch(c) {
            case '>': {
              emitComment(3, pos);
              state = NS_HTML5TOKENIZER_DATA;
              NS_HTML5_CONTINUE(stateloop);
            }
            case '-': {
              appendLongStrBuf(c);
              state = NS_HTML5TOKENIZER_COMMENT_END_DASH;
              NS_HTML5_CONTINUE(stateloop);
            }
            case '\r': {
              appendLongStrBufCarriageReturn();
              NS_HTML5_BREAK(stateloop);
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
              NS_HTML5_CONTINUE(stateloop);
            }
          }
        }
      }
      case NS_HTML5TOKENIZER_COMMENT_START_DASH: {
        if (++pos == endPos) {
          NS_HTML5_BREAK(stateloop);
        }
        c = checkChar(buf, pos);
        switch(c) {
          case '-': {
            appendLongStrBuf(c);
            state = NS_HTML5TOKENIZER_COMMENT_END;
            NS_HTML5_CONTINUE(stateloop);
          }
          case '>': {

            emitComment(1, pos);
            state = NS_HTML5TOKENIZER_DATA;
            NS_HTML5_CONTINUE(stateloop);
          }
          case '\r': {
            appendLongStrBufCarriageReturn();
            state = NS_HTML5TOKENIZER_COMMENT;
            NS_HTML5_BREAK(stateloop);
          }
          case '\n': {
            appendLongStrBufLineFeed();
            state = NS_HTML5TOKENIZER_COMMENT;
            NS_HTML5_CONTINUE(stateloop);
          }
          case '\0': {
            c = 0xfffd;
          }
          default: {
            appendLongStrBuf(c);
            state = NS_HTML5TOKENIZER_COMMENT;
            NS_HTML5_CONTINUE(stateloop);
          }
        }
      }
      case NS_HTML5TOKENIZER_CDATA_START: {
        for (; ; ) {
          if (++pos == endPos) {
            NS_HTML5_BREAK(stateloop);
          }
          c = checkChar(buf, pos);
          if (index < 6) {
            if (c == nsHtml5Tokenizer::CDATA_LSQB[index]) {
              appendLongStrBuf(c);
            } else {

              state = NS_HTML5TOKENIZER_BOGUS_COMMENT;
              reconsume = PR_TRUE;
              NS_HTML5_CONTINUE(stateloop);
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
              NS_HTML5_BREAK(stateloop);
            }
            c = checkChar(buf, pos);
          }
          switch(c) {
            case ']': {
              flushChars(buf, pos);
              state = NS_HTML5TOKENIZER_CDATA_RSQB;
              NS_HTML5_BREAK(cdatasectionloop);
            }
            case '\0': {
              emitReplacementCharacter(buf, pos);
              continue;
            }
            case '\r': {
              emitCarriageReturn(buf, pos);
              NS_HTML5_BREAK(stateloop);
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
            NS_HTML5_BREAK(stateloop);
          }
          c = checkChar(buf, pos);
          switch(c) {
            case ']': {
              state = NS_HTML5TOKENIZER_CDATA_RSQB_RSQB;
              NS_HTML5_BREAK(cdatarsqb);
            }
            default: {
              tokenHandler->characters(nsHtml5Tokenizer::RSQB_RSQB, 0, 1);
              cstart = pos;
              state = NS_HTML5TOKENIZER_CDATA_SECTION;
              reconsume = PR_TRUE;
              NS_HTML5_CONTINUE(stateloop);
            }
          }
        }
        cdatarsqb_end: ;
      }
      case NS_HTML5TOKENIZER_CDATA_RSQB_RSQB: {
        if (++pos == endPos) {
          NS_HTML5_BREAK(stateloop);
        }
        c = checkChar(buf, pos);
        switch(c) {
          case '>': {
            cstart = pos + 1;
            state = NS_HTML5TOKENIZER_DATA;
            NS_HTML5_CONTINUE(stateloop);
          }
          default: {
            tokenHandler->characters(nsHtml5Tokenizer::RSQB_RSQB, 0, 2);
            cstart = pos;
            state = NS_HTML5TOKENIZER_CDATA_SECTION;
            reconsume = PR_TRUE;
            NS_HTML5_CONTINUE(stateloop);
          }
        }
      }
      case NS_HTML5TOKENIZER_ATTRIBUTE_VALUE_SINGLE_QUOTED: {
        for (; ; ) {
          if (reconsume) {
            reconsume = PR_FALSE;
          } else {
            if (++pos == endPos) {
              NS_HTML5_BREAK(stateloop);
            }
            c = checkChar(buf, pos);
          }
          switch(c) {
            case '\'': {
              addAttributeWithValue();
              state = NS_HTML5TOKENIZER_AFTER_ATTRIBUTE_VALUE_QUOTED;
              NS_HTML5_CONTINUE(stateloop);
            }
            case '&': {
              clearStrBufAndAppend(c);
              setAdditionalAndRememberAmpersandLocation('\'');
              returnState = state;
              state = NS_HTML5TOKENIZER_CONSUME_CHARACTER_REFERENCE;
              NS_HTML5_BREAK(attributevaluesinglequotedloop);
            }
            case '\r': {
              appendLongStrBufCarriageReturn();
              NS_HTML5_BREAK(stateloop);
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
          NS_HTML5_BREAK(stateloop);
        }
        c = checkChar(buf, pos);
        if (c == '\0') {
          NS_HTML5_BREAK(stateloop);
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
            if (!(returnState & NS_HTML5TOKENIZER_DATA_AND_RCDATA_MASK)) {
              cstart = pos;
            }
            state = returnState;
            reconsume = PR_TRUE;
            NS_HTML5_CONTINUE(stateloop);
          }
          case '#': {
            appendStrBuf('#');
            state = NS_HTML5TOKENIZER_CONSUME_NCR;
            NS_HTML5_CONTINUE(stateloop);
          }
          default: {
            if (c == additional) {
              emitOrAppendStrBuf(returnState);
              state = returnState;
              reconsume = PR_TRUE;
              NS_HTML5_CONTINUE(stateloop);
            }
            if (c >= 'a' && c <= 'z') {
              firstCharKey = c - 'a' + 26;
            } else if (c >= 'A' && c <= 'Z') {
              firstCharKey = c - 'A';
            } else {

              emitOrAppendStrBuf(returnState);
              if (!(returnState & NS_HTML5TOKENIZER_DATA_AND_RCDATA_MASK)) {
                cstart = pos;
              }
              state = returnState;
              reconsume = PR_TRUE;
              NS_HTML5_CONTINUE(stateloop);
            }
            appendStrBuf(c);
            state = NS_HTML5TOKENIZER_CHARACTER_REFERENCE_HILO_LOOKUP;
          }
        }
      }
      case NS_HTML5TOKENIZER_CHARACTER_REFERENCE_HILO_LOOKUP: {
        {
          if (++pos == endPos) {
            NS_HTML5_BREAK(stateloop);
          }
          c = checkChar(buf, pos);
          if (c == '\0') {
            NS_HTML5_BREAK(stateloop);
          }
          PRInt32 hilo = 0;
          if (c <= 'z') {
            const PRInt32* row = nsHtml5NamedCharactersAccel::HILO_ACCEL[c];
            if (row) {
              hilo = row[firstCharKey];
            }
          }
          if (!hilo) {

            emitOrAppendStrBuf(returnState);
            if (!(returnState & NS_HTML5TOKENIZER_DATA_AND_RCDATA_MASK)) {
              cstart = pos;
            }
            state = returnState;
            reconsume = PR_TRUE;
            NS_HTML5_CONTINUE(stateloop);
          }
          appendStrBuf(c);
          lo = hilo & 0xFFFF;
          hi = hilo >> 16;
          entCol = -1;
          candidate = -1;
          strBufMark = 0;
          state = NS_HTML5TOKENIZER_CHARACTER_REFERENCE_TAIL;
        }
      }
      case NS_HTML5TOKENIZER_CHARACTER_REFERENCE_TAIL: {
        for (; ; ) {
          if (++pos == endPos) {
            NS_HTML5_BREAK(stateloop);
          }
          c = checkChar(buf, pos);
          if (c == '\0') {
            NS_HTML5_BREAK(stateloop);
          }
          entCol++;
          for (; ; ) {
            if (hi < lo) {
              NS_HTML5_BREAK(outer);
            }
            if (entCol == nsHtml5NamedCharacters::NAMES[lo].length) {
              candidate = lo;
              strBufMark = strBufLen;
              lo++;
            } else if (entCol > nsHtml5NamedCharacters::NAMES[lo].length) {
              NS_HTML5_BREAK(outer);
            } else if (c > nsHtml5NamedCharacters::NAMES[lo][entCol]) {
              lo++;
            } else {
              NS_HTML5_BREAK(loloop);
            }
          }
          loloop_end: ;
          for (; ; ) {
            if (hi < lo) {
              NS_HTML5_BREAK(outer);
            }
            if (entCol == nsHtml5NamedCharacters::NAMES[hi].length) {
              NS_HTML5_BREAK(hiloop);
            }
            if (entCol > nsHtml5NamedCharacters::NAMES[hi].length) {
              NS_HTML5_BREAK(outer);
            } else if (c < nsHtml5NamedCharacters::NAMES[hi][entCol]) {
              hi--;
            } else {
              NS_HTML5_BREAK(hiloop);
            }
          }
          hiloop_end: ;
          if (hi < lo) {
            NS_HTML5_BREAK(outer);
          }
          appendStrBuf(c);
          continue;
        }
        outer_end: ;
        if (candidate == -1) {

          emitOrAppendStrBuf(returnState);
          if (!(returnState & NS_HTML5TOKENIZER_DATA_AND_RCDATA_MASK)) {
            cstart = pos;
          }
          state = returnState;
          reconsume = PR_TRUE;
          NS_HTML5_CONTINUE(stateloop);
        } else {
          jArray<PRInt8,PRInt32> candidateArr = nsHtml5NamedCharacters::NAMES[candidate];
          if (!candidateArr.length || candidateArr[candidateArr.length - 1] != ';') {
            if ((returnState & NS_HTML5TOKENIZER_DATA_AND_RCDATA_MASK)) {
              PRUnichar ch;
              if (strBufMark == strBufLen) {
                ch = c;
              } else {
                ch = strBuf[strBufMark];
              }
              if (ch == '=' || (ch >= '0' && ch <= '9') || (ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z')) {

                appendStrBufToLongStrBuf();
                state = returnState;
                reconsume = PR_TRUE;
                NS_HTML5_CONTINUE(stateloop);
              }
            }

          }
          const PRUnichar* val = nsHtml5NamedCharacters::VALUES[candidate];
          if ((val[0] & 0xFC00) == 0xD800) {
            emitOrAppendTwo(val, returnState);
          } else {
            emitOrAppendOne(val, returnState);
          }
          if (strBufMark < strBufLen) {
            if ((returnState & NS_HTML5TOKENIZER_DATA_AND_RCDATA_MASK)) {
              for (PRInt32 i = strBufMark; i < strBufLen; i++) {
                appendLongStrBuf(strBuf[i]);
              }
            } else {
              tokenHandler->characters(strBuf, strBufMark, strBufLen - strBufMark);
            }
          }
          if (!(returnState & NS_HTML5TOKENIZER_DATA_AND_RCDATA_MASK)) {
            cstart = pos;
          }
          state = returnState;
          reconsume = PR_TRUE;
          NS_HTML5_CONTINUE(stateloop);
        }
      }
      case NS_HTML5TOKENIZER_CONSUME_NCR: {
        if (++pos == endPos) {
          NS_HTML5_BREAK(stateloop);
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
            NS_HTML5_CONTINUE(stateloop);
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
              NS_HTML5_BREAK(stateloop);
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
              if (!(returnState & NS_HTML5TOKENIZER_DATA_AND_RCDATA_MASK)) {
                cstart = pos + 1;
              }
              state = NS_HTML5TOKENIZER_HANDLE_NCR_VALUE;
              NS_HTML5_BREAK(decimalloop);
            } else {

              appendStrBuf(';');
              emitOrAppendStrBuf(returnState);
              if (!(returnState & NS_HTML5TOKENIZER_DATA_AND_RCDATA_MASK)) {
                cstart = pos + 1;
              }
              state = returnState;
              NS_HTML5_CONTINUE(stateloop);
            }
          } else {
            if (!seenDigits) {

              emitOrAppendStrBuf(returnState);
              if (!(returnState & NS_HTML5TOKENIZER_DATA_AND_RCDATA_MASK)) {
                cstart = pos;
              }
              state = returnState;
              reconsume = PR_TRUE;
              NS_HTML5_CONTINUE(stateloop);
            } else {

              if (!(returnState & NS_HTML5TOKENIZER_DATA_AND_RCDATA_MASK)) {
                cstart = pos;
              }
              state = NS_HTML5TOKENIZER_HANDLE_NCR_VALUE;
              reconsume = PR_TRUE;
              NS_HTML5_BREAK(decimalloop);
            }
          }
        }
        decimalloop_end: ;
      }
      case NS_HTML5TOKENIZER_HANDLE_NCR_VALUE: {
        handleNcrValue(returnState);
        state = returnState;
        NS_HTML5_CONTINUE(stateloop);
      }
      case NS_HTML5TOKENIZER_HEX_NCR_LOOP: {
        for (; ; ) {
          if (++pos == endPos) {
            NS_HTML5_BREAK(stateloop);
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
              if (!(returnState & NS_HTML5TOKENIZER_DATA_AND_RCDATA_MASK)) {
                cstart = pos + 1;
              }
              state = NS_HTML5TOKENIZER_HANDLE_NCR_VALUE;
              NS_HTML5_CONTINUE(stateloop);
            } else {

              appendStrBuf(';');
              emitOrAppendStrBuf(returnState);
              if (!(returnState & NS_HTML5TOKENIZER_DATA_AND_RCDATA_MASK)) {
                cstart = pos + 1;
              }
              state = returnState;
              NS_HTML5_CONTINUE(stateloop);
            }
          } else {
            if (!seenDigits) {

              emitOrAppendStrBuf(returnState);
              if (!(returnState & NS_HTML5TOKENIZER_DATA_AND_RCDATA_MASK)) {
                cstart = pos;
              }
              state = returnState;
              reconsume = PR_TRUE;
              NS_HTML5_CONTINUE(stateloop);
            } else {

              if (!(returnState & NS_HTML5TOKENIZER_DATA_AND_RCDATA_MASK)) {
                cstart = pos;
              }
              state = NS_HTML5TOKENIZER_HANDLE_NCR_VALUE;
              reconsume = PR_TRUE;
              NS_HTML5_CONTINUE(stateloop);
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
              NS_HTML5_BREAK(stateloop);
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
              NS_HTML5_BREAK(stateloop);
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
      case NS_HTML5TOKENIZER_CLOSE_TAG_OPEN: {
        if (++pos == endPos) {
          NS_HTML5_BREAK(stateloop);
        }
        c = checkChar(buf, pos);
        switch(c) {
          case '>': {

            cstart = pos + 1;
            state = NS_HTML5TOKENIZER_DATA;
            NS_HTML5_CONTINUE(stateloop);
          }
          case '\r': {
            silentCarriageReturn();

            clearLongStrBufAndAppend('\n');
            state = NS_HTML5TOKENIZER_BOGUS_COMMENT;
            NS_HTML5_BREAK(stateloop);
          }
          case '\n': {
            silentLineFeed();

            clearLongStrBufAndAppend('\n');
            state = NS_HTML5TOKENIZER_BOGUS_COMMENT;
            NS_HTML5_CONTINUE(stateloop);
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
              clearStrBufAndAppend(c);
              state = NS_HTML5TOKENIZER_TAG_NAME;
              NS_HTML5_CONTINUE(stateloop);
            } else {

              clearLongStrBufAndAppend(c);
              state = NS_HTML5TOKENIZER_BOGUS_COMMENT;
              NS_HTML5_CONTINUE(stateloop);
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
              NS_HTML5_BREAK(stateloop);
            }
            c = checkChar(buf, pos);
          }
          switch(c) {
            case '&': {
              flushChars(buf, pos);
              clearStrBufAndAppend(c);
              additional = '\0';
              returnState = state;
              state = NS_HTML5TOKENIZER_CONSUME_CHARACTER_REFERENCE;
              NS_HTML5_CONTINUE(stateloop);
            }
            case '<': {
              flushChars(buf, pos);
              returnState = state;
              state = NS_HTML5TOKENIZER_RAWTEXT_RCDATA_LESS_THAN_SIGN;
              NS_HTML5_CONTINUE(stateloop);
            }
            case '\0': {
              emitReplacementCharacter(buf, pos);
              continue;
            }
            case '\r': {
              emitCarriageReturn(buf, pos);
              NS_HTML5_BREAK(stateloop);
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
      case NS_HTML5TOKENIZER_RAWTEXT: {
        for (; ; ) {
          if (reconsume) {
            reconsume = PR_FALSE;
          } else {
            if (++pos == endPos) {
              NS_HTML5_BREAK(stateloop);
            }
            c = checkChar(buf, pos);
          }
          switch(c) {
            case '<': {
              flushChars(buf, pos);
              returnState = state;
              state = NS_HTML5TOKENIZER_RAWTEXT_RCDATA_LESS_THAN_SIGN;
              NS_HTML5_BREAK(rawtextloop);
            }
            case '\0': {
              emitReplacementCharacter(buf, pos);
              continue;
            }
            case '\r': {
              emitCarriageReturn(buf, pos);
              NS_HTML5_BREAK(stateloop);
            }
            case '\n': {
              silentLineFeed();
            }
            default: {
              continue;
            }
          }
        }
        rawtextloop_end: ;
      }
      case NS_HTML5TOKENIZER_RAWTEXT_RCDATA_LESS_THAN_SIGN: {
        for (; ; ) {
          if (++pos == endPos) {
            NS_HTML5_BREAK(stateloop);
          }
          c = checkChar(buf, pos);
          switch(c) {
            case '/': {
              index = 0;
              clearStrBuf();
              state = NS_HTML5TOKENIZER_NON_DATA_END_TAG_NAME;
              NS_HTML5_BREAK(rawtextrcdatalessthansignloop);
            }
            default: {
              tokenHandler->characters(nsHtml5Tokenizer::LT_GT, 0, 1);
              cstart = pos;
              state = returnState;
              reconsume = PR_TRUE;
              NS_HTML5_CONTINUE(stateloop);
            }
          }
        }
        rawtextrcdatalessthansignloop_end: ;
      }
      case NS_HTML5TOKENIZER_NON_DATA_END_TAG_NAME: {
        for (; ; ) {
          if (++pos == endPos) {
            NS_HTML5_BREAK(stateloop);
          }
          c = checkChar(buf, pos);
          if (index < endTagExpectationAsArray.length) {
            PRUnichar e = endTagExpectationAsArray[index];
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
              NS_HTML5_CONTINUE(stateloop);
            }
            appendStrBuf(c);
            index++;
            continue;
          } else {
            endTag = PR_TRUE;
            tagName = endTagExpectation;
            switch(c) {
              case '\r': {
                silentCarriageReturn();
                state = NS_HTML5TOKENIZER_BEFORE_ATTRIBUTE_NAME;
                NS_HTML5_BREAK(stateloop);
              }
              case '\n': {
                silentLineFeed();
              }
              case ' ':
              case '\t':
              case '\f': {
                state = NS_HTML5TOKENIZER_BEFORE_ATTRIBUTE_NAME;
                NS_HTML5_CONTINUE(stateloop);
              }
              case '/': {
                state = NS_HTML5TOKENIZER_SELF_CLOSING_START_TAG;
                NS_HTML5_CONTINUE(stateloop);
              }
              case '>': {
                state = emitCurrentTagToken(PR_FALSE, pos);
                if (shouldSuspend) {
                  NS_HTML5_BREAK(stateloop);
                }
                NS_HTML5_CONTINUE(stateloop);
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
                NS_HTML5_CONTINUE(stateloop);
              }
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
              NS_HTML5_BREAK(stateloop);
            }
            c = checkChar(buf, pos);
          }
          switch(c) {
            case '>': {
              emitComment(0, pos);
              state = NS_HTML5TOKENIZER_DATA;
              NS_HTML5_CONTINUE(stateloop);
            }
            case '-': {
              appendLongStrBuf(c);
              state = NS_HTML5TOKENIZER_BOGUS_COMMENT_HYPHEN;
              NS_HTML5_BREAK(boguscommentloop);
            }
            case '\r': {
              appendLongStrBufCarriageReturn();
              NS_HTML5_BREAK(stateloop);
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
            NS_HTML5_BREAK(stateloop);
          }
          c = checkChar(buf, pos);
          switch(c) {
            case '>': {
              emitComment(0, pos);
              state = NS_HTML5TOKENIZER_DATA;
              NS_HTML5_CONTINUE(stateloop);
            }
            case '-': {
              appendSecondHyphenToBogusComment();
              NS_HTML5_CONTINUE(boguscommenthyphenloop);
            }
            case '\r': {
              appendLongStrBufCarriageReturn();
              state = NS_HTML5TOKENIZER_BOGUS_COMMENT;
              NS_HTML5_BREAK(stateloop);
            }
            case '\n': {
              appendLongStrBufLineFeed();
              state = NS_HTML5TOKENIZER_BOGUS_COMMENT;
              NS_HTML5_CONTINUE(stateloop);
            }
            case '\0': {
              c = 0xfffd;
            }
            default: {
              appendLongStrBuf(c);
              state = NS_HTML5TOKENIZER_BOGUS_COMMENT;
              NS_HTML5_CONTINUE(stateloop);
            }
          }
        }

      }
      case NS_HTML5TOKENIZER_SCRIPT_DATA: {
        for (; ; ) {
          if (reconsume) {
            reconsume = PR_FALSE;
          } else {
            if (++pos == endPos) {
              NS_HTML5_BREAK(stateloop);
            }
            c = checkChar(buf, pos);
          }
          switch(c) {
            case '<': {
              flushChars(buf, pos);
              returnState = state;
              state = NS_HTML5TOKENIZER_SCRIPT_DATA_LESS_THAN_SIGN;
              NS_HTML5_BREAK(scriptdataloop);
            }
            case '\0': {
              emitReplacementCharacter(buf, pos);
              continue;
            }
            case '\r': {
              emitCarriageReturn(buf, pos);
              NS_HTML5_BREAK(stateloop);
            }
            case '\n': {
              silentLineFeed();
            }
            default: {
              continue;
            }
          }
        }
        scriptdataloop_end: ;
      }
      case NS_HTML5TOKENIZER_SCRIPT_DATA_LESS_THAN_SIGN: {
        for (; ; ) {
          if (++pos == endPos) {
            NS_HTML5_BREAK(stateloop);
          }
          c = checkChar(buf, pos);
          switch(c) {
            case '/': {
              index = 0;
              clearStrBuf();
              state = NS_HTML5TOKENIZER_NON_DATA_END_TAG_NAME;
              NS_HTML5_CONTINUE(stateloop);
            }
            case '!': {
              tokenHandler->characters(nsHtml5Tokenizer::LT_GT, 0, 1);
              cstart = pos;
              state = NS_HTML5TOKENIZER_SCRIPT_DATA_ESCAPE_START;
              NS_HTML5_BREAK(scriptdatalessthansignloop);
            }
            default: {
              tokenHandler->characters(nsHtml5Tokenizer::LT_GT, 0, 1);
              cstart = pos;
              state = NS_HTML5TOKENIZER_SCRIPT_DATA;
              reconsume = PR_TRUE;
              NS_HTML5_CONTINUE(stateloop);
            }
          }
        }
        scriptdatalessthansignloop_end: ;
      }
      case NS_HTML5TOKENIZER_SCRIPT_DATA_ESCAPE_START: {
        for (; ; ) {
          if (++pos == endPos) {
            NS_HTML5_BREAK(stateloop);
          }
          c = checkChar(buf, pos);
          switch(c) {
            case '-': {
              state = NS_HTML5TOKENIZER_SCRIPT_DATA_ESCAPE_START_DASH;
              NS_HTML5_BREAK(scriptdataescapestartloop);
            }
            default: {
              state = NS_HTML5TOKENIZER_SCRIPT_DATA;
              reconsume = PR_TRUE;
              NS_HTML5_CONTINUE(stateloop);
            }
          }
        }
        scriptdataescapestartloop_end: ;
      }
      case NS_HTML5TOKENIZER_SCRIPT_DATA_ESCAPE_START_DASH: {
        for (; ; ) {
          if (++pos == endPos) {
            NS_HTML5_BREAK(stateloop);
          }
          c = checkChar(buf, pos);
          switch(c) {
            case '-': {
              state = NS_HTML5TOKENIZER_SCRIPT_DATA_ESCAPED_DASH_DASH;
              NS_HTML5_BREAK(scriptdataescapestartdashloop);
            }
            default: {
              state = NS_HTML5TOKENIZER_SCRIPT_DATA;
              reconsume = PR_TRUE;
              NS_HTML5_CONTINUE(stateloop);
            }
          }
        }
        scriptdataescapestartdashloop_end: ;
      }
      case NS_HTML5TOKENIZER_SCRIPT_DATA_ESCAPED_DASH_DASH: {
        for (; ; ) {
          if (++pos == endPos) {
            NS_HTML5_BREAK(stateloop);
          }
          c = checkChar(buf, pos);
          switch(c) {
            case '-': {
              continue;
            }
            case '<': {
              flushChars(buf, pos);
              state = NS_HTML5TOKENIZER_SCRIPT_DATA_ESCAPED_LESS_THAN_SIGN;
              NS_HTML5_CONTINUE(stateloop);
            }
            case '>': {
              state = NS_HTML5TOKENIZER_SCRIPT_DATA;
              NS_HTML5_CONTINUE(stateloop);
            }
            case '\0': {
              emitReplacementCharacter(buf, pos);
              state = NS_HTML5TOKENIZER_SCRIPT_DATA_ESCAPED;
              NS_HTML5_BREAK(scriptdataescapeddashdashloop);
            }
            case '\r': {
              emitCarriageReturn(buf, pos);
              state = NS_HTML5TOKENIZER_SCRIPT_DATA_ESCAPED;
              NS_HTML5_BREAK(stateloop);
            }
            case '\n': {
              silentLineFeed();
            }
            default: {
              state = NS_HTML5TOKENIZER_SCRIPT_DATA_ESCAPED;
              NS_HTML5_BREAK(scriptdataescapeddashdashloop);
            }
          }
        }
        scriptdataescapeddashdashloop_end: ;
      }
      case NS_HTML5TOKENIZER_SCRIPT_DATA_ESCAPED: {
        for (; ; ) {
          if (reconsume) {
            reconsume = PR_FALSE;
          } else {
            if (++pos == endPos) {
              NS_HTML5_BREAK(stateloop);
            }
            c = checkChar(buf, pos);
          }
          switch(c) {
            case '-': {
              state = NS_HTML5TOKENIZER_SCRIPT_DATA_ESCAPED_DASH;
              NS_HTML5_BREAK(scriptdataescapedloop);
            }
            case '<': {
              flushChars(buf, pos);
              state = NS_HTML5TOKENIZER_SCRIPT_DATA_ESCAPED_LESS_THAN_SIGN;
              NS_HTML5_CONTINUE(stateloop);
            }
            case '\0': {
              emitReplacementCharacter(buf, pos);
              continue;
            }
            case '\r': {
              emitCarriageReturn(buf, pos);
              NS_HTML5_BREAK(stateloop);
            }
            case '\n': {
              silentLineFeed();
            }
            default: {
              continue;
            }
          }
        }
        scriptdataescapedloop_end: ;
      }
      case NS_HTML5TOKENIZER_SCRIPT_DATA_ESCAPED_DASH: {
        for (; ; ) {
          if (++pos == endPos) {
            NS_HTML5_BREAK(stateloop);
          }
          c = checkChar(buf, pos);
          switch(c) {
            case '-': {
              state = NS_HTML5TOKENIZER_SCRIPT_DATA_ESCAPED_DASH_DASH;
              NS_HTML5_CONTINUE(stateloop);
            }
            case '<': {
              flushChars(buf, pos);
              state = NS_HTML5TOKENIZER_SCRIPT_DATA_ESCAPED_LESS_THAN_SIGN;
              NS_HTML5_BREAK(scriptdataescapeddashloop);
            }
            case '\0': {
              emitReplacementCharacter(buf, pos);
              state = NS_HTML5TOKENIZER_SCRIPT_DATA_ESCAPED;
              NS_HTML5_CONTINUE(stateloop);
            }
            case '\r': {
              emitCarriageReturn(buf, pos);
              state = NS_HTML5TOKENIZER_SCRIPT_DATA_ESCAPED;
              NS_HTML5_BREAK(stateloop);
            }
            case '\n': {
              silentLineFeed();
            }
            default: {
              state = NS_HTML5TOKENIZER_SCRIPT_DATA_ESCAPED;
              NS_HTML5_CONTINUE(stateloop);
            }
          }
        }
        scriptdataescapeddashloop_end: ;
      }
      case NS_HTML5TOKENIZER_SCRIPT_DATA_ESCAPED_LESS_THAN_SIGN: {
        for (; ; ) {
          if (++pos == endPos) {
            NS_HTML5_BREAK(stateloop);
          }
          c = checkChar(buf, pos);
          switch(c) {
            case '/': {
              index = 0;
              clearStrBuf();
              returnState = NS_HTML5TOKENIZER_SCRIPT_DATA_ESCAPED;
              state = NS_HTML5TOKENIZER_NON_DATA_END_TAG_NAME;
              NS_HTML5_CONTINUE(stateloop);
            }
            case 'S':
            case 's': {
              tokenHandler->characters(nsHtml5Tokenizer::LT_GT, 0, 1);
              cstart = pos;
              index = 1;
              state = NS_HTML5TOKENIZER_SCRIPT_DATA_DOUBLE_ESCAPE_START;
              NS_HTML5_BREAK(scriptdataescapedlessthanloop);
            }
            default: {
              tokenHandler->characters(nsHtml5Tokenizer::LT_GT, 0, 1);
              cstart = pos;
              reconsume = PR_TRUE;
              state = NS_HTML5TOKENIZER_SCRIPT_DATA_ESCAPED;
              NS_HTML5_CONTINUE(stateloop);
            }
          }
        }
        scriptdataescapedlessthanloop_end: ;
      }
      case NS_HTML5TOKENIZER_SCRIPT_DATA_DOUBLE_ESCAPE_START: {
        for (; ; ) {
          if (++pos == endPos) {
            NS_HTML5_BREAK(stateloop);
          }
          c = checkChar(buf, pos);

          if (index < 6) {
            PRUnichar folded = c;
            if (c >= 'A' && c <= 'Z') {
              folded += 0x20;
            }
            if (folded != nsHtml5Tokenizer::SCRIPT_ARR[index]) {
              reconsume = PR_TRUE;
              state = NS_HTML5TOKENIZER_SCRIPT_DATA_ESCAPED;
              NS_HTML5_CONTINUE(stateloop);
            }
            index++;
            continue;
          }
          switch(c) {
            case '\r': {
              emitCarriageReturn(buf, pos);
              state = NS_HTML5TOKENIZER_SCRIPT_DATA_DOUBLE_ESCAPED;
              NS_HTML5_BREAK(stateloop);
            }
            case '\n': {
              silentLineFeed();
            }
            case ' ':
            case '\t':
            case '\f':
            case '/':
            case '>': {
              state = NS_HTML5TOKENIZER_SCRIPT_DATA_DOUBLE_ESCAPED;
              NS_HTML5_BREAK(scriptdatadoubleescapestartloop);
            }
            default: {
              reconsume = PR_TRUE;
              state = NS_HTML5TOKENIZER_SCRIPT_DATA_ESCAPED;
              NS_HTML5_CONTINUE(stateloop);
            }
          }
        }
        scriptdatadoubleescapestartloop_end: ;
      }
      case NS_HTML5TOKENIZER_SCRIPT_DATA_DOUBLE_ESCAPED: {
        for (; ; ) {
          if (reconsume) {
            reconsume = PR_FALSE;
          } else {
            if (++pos == endPos) {
              NS_HTML5_BREAK(stateloop);
            }
            c = checkChar(buf, pos);
          }
          switch(c) {
            case '-': {
              state = NS_HTML5TOKENIZER_SCRIPT_DATA_DOUBLE_ESCAPED_DASH;
              NS_HTML5_BREAK(scriptdatadoubleescapedloop);
            }
            case '<': {
              state = NS_HTML5TOKENIZER_SCRIPT_DATA_DOUBLE_ESCAPED_LESS_THAN_SIGN;
              NS_HTML5_CONTINUE(stateloop);
            }
            case '\0': {
              emitReplacementCharacter(buf, pos);
              continue;
            }
            case '\r': {
              emitCarriageReturn(buf, pos);
              NS_HTML5_BREAK(stateloop);
            }
            case '\n': {
              silentLineFeed();
            }
            default: {
              continue;
            }
          }
        }
        scriptdatadoubleescapedloop_end: ;
      }
      case NS_HTML5TOKENIZER_SCRIPT_DATA_DOUBLE_ESCAPED_DASH: {
        for (; ; ) {
          if (++pos == endPos) {
            NS_HTML5_BREAK(stateloop);
          }
          c = checkChar(buf, pos);
          switch(c) {
            case '-': {
              state = NS_HTML5TOKENIZER_SCRIPT_DATA_DOUBLE_ESCAPED_DASH_DASH;
              NS_HTML5_BREAK(scriptdatadoubleescapeddashloop);
            }
            case '<': {
              state = NS_HTML5TOKENIZER_SCRIPT_DATA_DOUBLE_ESCAPED_LESS_THAN_SIGN;
              NS_HTML5_CONTINUE(stateloop);
            }
            case '\0': {
              emitReplacementCharacter(buf, pos);
              state = NS_HTML5TOKENIZER_SCRIPT_DATA_DOUBLE_ESCAPED;
              NS_HTML5_CONTINUE(stateloop);
            }
            case '\r': {
              emitCarriageReturn(buf, pos);
              state = NS_HTML5TOKENIZER_SCRIPT_DATA_DOUBLE_ESCAPED;
              NS_HTML5_BREAK(stateloop);
            }
            case '\n': {
              silentLineFeed();
            }
            default: {
              state = NS_HTML5TOKENIZER_SCRIPT_DATA_DOUBLE_ESCAPED;
              NS_HTML5_CONTINUE(stateloop);
            }
          }
        }
        scriptdatadoubleescapeddashloop_end: ;
      }
      case NS_HTML5TOKENIZER_SCRIPT_DATA_DOUBLE_ESCAPED_DASH_DASH: {
        for (; ; ) {
          if (++pos == endPos) {
            NS_HTML5_BREAK(stateloop);
          }
          c = checkChar(buf, pos);
          switch(c) {
            case '-': {
              continue;
            }
            case '<': {
              state = NS_HTML5TOKENIZER_SCRIPT_DATA_DOUBLE_ESCAPED_LESS_THAN_SIGN;
              NS_HTML5_BREAK(scriptdatadoubleescapeddashdashloop);
            }
            case '>': {
              state = NS_HTML5TOKENIZER_SCRIPT_DATA;
              NS_HTML5_CONTINUE(stateloop);
            }
            case '\0': {
              emitReplacementCharacter(buf, pos);
              state = NS_HTML5TOKENIZER_SCRIPT_DATA_DOUBLE_ESCAPED;
              NS_HTML5_CONTINUE(stateloop);
            }
            case '\r': {
              emitCarriageReturn(buf, pos);
              state = NS_HTML5TOKENIZER_SCRIPT_DATA_DOUBLE_ESCAPED;
              NS_HTML5_BREAK(stateloop);
            }
            case '\n': {
              silentLineFeed();
            }
            default: {
              state = NS_HTML5TOKENIZER_SCRIPT_DATA_DOUBLE_ESCAPED;
              NS_HTML5_CONTINUE(stateloop);
            }
          }
        }
        scriptdatadoubleescapeddashdashloop_end: ;
      }
      case NS_HTML5TOKENIZER_SCRIPT_DATA_DOUBLE_ESCAPED_LESS_THAN_SIGN: {
        for (; ; ) {
          if (++pos == endPos) {
            NS_HTML5_BREAK(stateloop);
          }
          c = checkChar(buf, pos);
          switch(c) {
            case '/': {
              index = 0;
              state = NS_HTML5TOKENIZER_SCRIPT_DATA_DOUBLE_ESCAPE_END;
              NS_HTML5_BREAK(scriptdatadoubleescapedlessthanloop);
            }
            default: {
              reconsume = PR_TRUE;
              state = NS_HTML5TOKENIZER_SCRIPT_DATA_DOUBLE_ESCAPED;
              NS_HTML5_CONTINUE(stateloop);
            }
          }
        }
        scriptdatadoubleescapedlessthanloop_end: ;
      }
      case NS_HTML5TOKENIZER_SCRIPT_DATA_DOUBLE_ESCAPE_END: {
        for (; ; ) {
          if (++pos == endPos) {
            NS_HTML5_BREAK(stateloop);
          }
          c = checkChar(buf, pos);
          if (index < 6) {
            PRUnichar folded = c;
            if (c >= 'A' && c <= 'Z') {
              folded += 0x20;
            }
            if (folded != nsHtml5Tokenizer::SCRIPT_ARR[index]) {
              reconsume = PR_TRUE;
              state = NS_HTML5TOKENIZER_SCRIPT_DATA_DOUBLE_ESCAPED;
              NS_HTML5_CONTINUE(stateloop);
            }
            index++;
            continue;
          }
          switch(c) {
            case '\r': {
              emitCarriageReturn(buf, pos);
              state = NS_HTML5TOKENIZER_SCRIPT_DATA_ESCAPED;
              NS_HTML5_BREAK(stateloop);
            }
            case '\n': {
              silentLineFeed();
            }
            case ' ':
            case '\t':
            case '\f':
            case '/':
            case '>': {
              state = NS_HTML5TOKENIZER_SCRIPT_DATA_ESCAPED;
              NS_HTML5_CONTINUE(stateloop);
            }
            default: {
              reconsume = PR_TRUE;
              state = NS_HTML5TOKENIZER_SCRIPT_DATA_DOUBLE_ESCAPED;
              NS_HTML5_CONTINUE(stateloop);
            }
          }
        }

      }
      case NS_HTML5TOKENIZER_MARKUP_DECLARATION_OCTYPE: {
        for (; ; ) {
          if (++pos == endPos) {
            NS_HTML5_BREAK(stateloop);
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
              NS_HTML5_CONTINUE(stateloop);
            }
            index++;
            continue;
          } else {
            state = NS_HTML5TOKENIZER_DOCTYPE;
            reconsume = PR_TRUE;
            NS_HTML5_BREAK(markupdeclarationdoctypeloop);
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
              NS_HTML5_BREAK(stateloop);
            }
            c = checkChar(buf, pos);
          }
          initDoctypeFields();
          switch(c) {
            case '\r': {
              silentCarriageReturn();
              state = NS_HTML5TOKENIZER_BEFORE_DOCTYPE_NAME;
              NS_HTML5_BREAK(stateloop);
            }
            case '\n': {
              silentLineFeed();
            }
            case ' ':
            case '\t':
            case '\f': {
              state = NS_HTML5TOKENIZER_BEFORE_DOCTYPE_NAME;
              NS_HTML5_BREAK(doctypeloop);
            }
            default: {

              state = NS_HTML5TOKENIZER_BEFORE_DOCTYPE_NAME;
              reconsume = PR_TRUE;
              NS_HTML5_BREAK(doctypeloop);
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
              NS_HTML5_BREAK(stateloop);
            }
            c = checkChar(buf, pos);
          }
          switch(c) {
            case '\r': {
              silentCarriageReturn();
              NS_HTML5_BREAK(stateloop);
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
              NS_HTML5_CONTINUE(stateloop);
            }
            case '\0': {
              c = 0xfffd;
            }
            default: {
              if (c >= 'A' && c <= 'Z') {
                c += 0x20;
              }
              clearStrBufAndAppend(c);
              state = NS_HTML5TOKENIZER_DOCTYPE_NAME;
              NS_HTML5_BREAK(beforedoctypenameloop);
            }
          }
        }
        beforedoctypenameloop_end: ;
      }
      case NS_HTML5TOKENIZER_DOCTYPE_NAME: {
        for (; ; ) {
          if (++pos == endPos) {
            NS_HTML5_BREAK(stateloop);
          }
          c = checkChar(buf, pos);
          switch(c) {
            case '\r': {
              silentCarriageReturn();
              strBufToDoctypeName();
              state = NS_HTML5TOKENIZER_AFTER_DOCTYPE_NAME;
              NS_HTML5_BREAK(stateloop);
            }
            case '\n': {
              silentLineFeed();
            }
            case ' ':
            case '\t':
            case '\f': {
              strBufToDoctypeName();
              state = NS_HTML5TOKENIZER_AFTER_DOCTYPE_NAME;
              NS_HTML5_BREAK(doctypenameloop);
            }
            case '>': {
              strBufToDoctypeName();
              emitDoctypeToken(pos);
              state = NS_HTML5TOKENIZER_DATA;
              NS_HTML5_CONTINUE(stateloop);
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
            NS_HTML5_BREAK(stateloop);
          }
          c = checkChar(buf, pos);
          switch(c) {
            case '\r': {
              silentCarriageReturn();
              NS_HTML5_BREAK(stateloop);
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
              NS_HTML5_CONTINUE(stateloop);
            }
            case 'p':
            case 'P': {
              index = 0;
              state = NS_HTML5TOKENIZER_DOCTYPE_UBLIC;
              NS_HTML5_BREAK(afterdoctypenameloop);
            }
            case 's':
            case 'S': {
              index = 0;
              state = NS_HTML5TOKENIZER_DOCTYPE_YSTEM;
              NS_HTML5_CONTINUE(stateloop);
            }
            default: {
              bogusDoctype();
              state = NS_HTML5TOKENIZER_BOGUS_DOCTYPE;
              NS_HTML5_CONTINUE(stateloop);
            }
          }
        }
        afterdoctypenameloop_end: ;
      }
      case NS_HTML5TOKENIZER_DOCTYPE_UBLIC: {
        for (; ; ) {
          if (++pos == endPos) {
            NS_HTML5_BREAK(stateloop);
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
              NS_HTML5_CONTINUE(stateloop);
            }
            index++;
            continue;
          } else {
            state = NS_HTML5TOKENIZER_AFTER_DOCTYPE_PUBLIC_KEYWORD;
            reconsume = PR_TRUE;
            NS_HTML5_BREAK(doctypeublicloop);
          }
        }
        doctypeublicloop_end: ;
      }
      case NS_HTML5TOKENIZER_AFTER_DOCTYPE_PUBLIC_KEYWORD: {
        for (; ; ) {
          if (reconsume) {
            reconsume = PR_FALSE;
          } else {
            if (++pos == endPos) {
              NS_HTML5_BREAK(stateloop);
            }
            c = checkChar(buf, pos);
          }
          switch(c) {
            case '\r': {
              silentCarriageReturn();
              state = NS_HTML5TOKENIZER_BEFORE_DOCTYPE_PUBLIC_IDENTIFIER;
              NS_HTML5_BREAK(stateloop);
            }
            case '\n': {
              silentLineFeed();
            }
            case ' ':
            case '\t':
            case '\f': {
              state = NS_HTML5TOKENIZER_BEFORE_DOCTYPE_PUBLIC_IDENTIFIER;
              NS_HTML5_BREAK(afterdoctypepublickeywordloop);
            }
            case '\"': {

              clearLongStrBuf();
              state = NS_HTML5TOKENIZER_DOCTYPE_PUBLIC_IDENTIFIER_DOUBLE_QUOTED;
              NS_HTML5_CONTINUE(stateloop);
            }
            case '\'': {

              clearLongStrBuf();
              state = NS_HTML5TOKENIZER_DOCTYPE_PUBLIC_IDENTIFIER_SINGLE_QUOTED;
              NS_HTML5_CONTINUE(stateloop);
            }
            case '>': {

              forceQuirks = PR_TRUE;
              emitDoctypeToken(pos);
              state = NS_HTML5TOKENIZER_DATA;
              NS_HTML5_CONTINUE(stateloop);
            }
            default: {
              bogusDoctype();
              state = NS_HTML5TOKENIZER_BOGUS_DOCTYPE;
              NS_HTML5_CONTINUE(stateloop);
            }
          }
        }
        afterdoctypepublickeywordloop_end: ;
      }
      case NS_HTML5TOKENIZER_BEFORE_DOCTYPE_PUBLIC_IDENTIFIER: {
        for (; ; ) {
          if (++pos == endPos) {
            NS_HTML5_BREAK(stateloop);
          }
          c = checkChar(buf, pos);
          switch(c) {
            case '\r': {
              silentCarriageReturn();
              NS_HTML5_BREAK(stateloop);
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
              clearLongStrBuf();
              state = NS_HTML5TOKENIZER_DOCTYPE_PUBLIC_IDENTIFIER_DOUBLE_QUOTED;
              NS_HTML5_BREAK(beforedoctypepublicidentifierloop);
            }
            case '\'': {
              clearLongStrBuf();
              state = NS_HTML5TOKENIZER_DOCTYPE_PUBLIC_IDENTIFIER_SINGLE_QUOTED;
              NS_HTML5_CONTINUE(stateloop);
            }
            case '>': {

              forceQuirks = PR_TRUE;
              emitDoctypeToken(pos);
              state = NS_HTML5TOKENIZER_DATA;
              NS_HTML5_CONTINUE(stateloop);
            }
            default: {
              bogusDoctype();
              state = NS_HTML5TOKENIZER_BOGUS_DOCTYPE;
              NS_HTML5_CONTINUE(stateloop);
            }
          }
        }
        beforedoctypepublicidentifierloop_end: ;
      }
      case NS_HTML5TOKENIZER_DOCTYPE_PUBLIC_IDENTIFIER_DOUBLE_QUOTED: {
        for (; ; ) {
          if (++pos == endPos) {
            NS_HTML5_BREAK(stateloop);
          }
          c = checkChar(buf, pos);
          switch(c) {
            case '\"': {
              publicIdentifier = longStrBufToString();
              state = NS_HTML5TOKENIZER_AFTER_DOCTYPE_PUBLIC_IDENTIFIER;
              NS_HTML5_BREAK(doctypepublicidentifierdoublequotedloop);
            }
            case '>': {

              forceQuirks = PR_TRUE;
              publicIdentifier = longStrBufToString();
              emitDoctypeToken(pos);
              state = NS_HTML5TOKENIZER_DATA;
              NS_HTML5_CONTINUE(stateloop);
            }
            case '\r': {
              appendLongStrBufCarriageReturn();
              NS_HTML5_BREAK(stateloop);
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
            NS_HTML5_BREAK(stateloop);
          }
          c = checkChar(buf, pos);
          switch(c) {
            case '\r': {
              silentCarriageReturn();
              state = NS_HTML5TOKENIZER_BETWEEN_DOCTYPE_PUBLIC_AND_SYSTEM_IDENTIFIERS;
              NS_HTML5_BREAK(stateloop);
            }
            case '\n': {
              silentLineFeed();
            }
            case ' ':
            case '\t':
            case '\f': {
              state = NS_HTML5TOKENIZER_BETWEEN_DOCTYPE_PUBLIC_AND_SYSTEM_IDENTIFIERS;
              NS_HTML5_BREAK(afterdoctypepublicidentifierloop);
            }
            case '>': {
              emitDoctypeToken(pos);
              state = NS_HTML5TOKENIZER_DATA;
              NS_HTML5_CONTINUE(stateloop);
            }
            case '\"': {

              clearLongStrBuf();
              state = NS_HTML5TOKENIZER_DOCTYPE_SYSTEM_IDENTIFIER_DOUBLE_QUOTED;
              NS_HTML5_CONTINUE(stateloop);
            }
            case '\'': {

              clearLongStrBuf();
              state = NS_HTML5TOKENIZER_DOCTYPE_SYSTEM_IDENTIFIER_SINGLE_QUOTED;
              NS_HTML5_CONTINUE(stateloop);
            }
            default: {
              bogusDoctype();
              state = NS_HTML5TOKENIZER_BOGUS_DOCTYPE;
              NS_HTML5_CONTINUE(stateloop);
            }
          }
        }
        afterdoctypepublicidentifierloop_end: ;
      }
      case NS_HTML5TOKENIZER_BETWEEN_DOCTYPE_PUBLIC_AND_SYSTEM_IDENTIFIERS: {
        for (; ; ) {
          if (++pos == endPos) {
            NS_HTML5_BREAK(stateloop);
          }
          c = checkChar(buf, pos);
          switch(c) {
            case '\r': {
              silentCarriageReturn();
              NS_HTML5_BREAK(stateloop);
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
              NS_HTML5_CONTINUE(stateloop);
            }
            case '\"': {
              clearLongStrBuf();
              state = NS_HTML5TOKENIZER_DOCTYPE_SYSTEM_IDENTIFIER_DOUBLE_QUOTED;
              NS_HTML5_BREAK(betweendoctypepublicandsystemidentifiersloop);
            }
            case '\'': {
              clearLongStrBuf();
              state = NS_HTML5TOKENIZER_DOCTYPE_SYSTEM_IDENTIFIER_SINGLE_QUOTED;
              NS_HTML5_CONTINUE(stateloop);
            }
            default: {
              bogusDoctype();
              state = NS_HTML5TOKENIZER_BOGUS_DOCTYPE;
              NS_HTML5_CONTINUE(stateloop);
            }
          }
        }
        betweendoctypepublicandsystemidentifiersloop_end: ;
      }
      case NS_HTML5TOKENIZER_DOCTYPE_SYSTEM_IDENTIFIER_DOUBLE_QUOTED: {
        for (; ; ) {
          if (++pos == endPos) {
            NS_HTML5_BREAK(stateloop);
          }
          c = checkChar(buf, pos);
          switch(c) {
            case '\"': {
              systemIdentifier = longStrBufToString();
              state = NS_HTML5TOKENIZER_AFTER_DOCTYPE_SYSTEM_IDENTIFIER;
              NS_HTML5_CONTINUE(stateloop);
            }
            case '>': {

              forceQuirks = PR_TRUE;
              systemIdentifier = longStrBufToString();
              emitDoctypeToken(pos);
              state = NS_HTML5TOKENIZER_DATA;
              NS_HTML5_CONTINUE(stateloop);
            }
            case '\r': {
              appendLongStrBufCarriageReturn();
              NS_HTML5_BREAK(stateloop);
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
            NS_HTML5_BREAK(stateloop);
          }
          c = checkChar(buf, pos);
          switch(c) {
            case '\r': {
              silentCarriageReturn();
              NS_HTML5_BREAK(stateloop);
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
              NS_HTML5_CONTINUE(stateloop);
            }
            default: {
              bogusDoctypeWithoutQuirks();
              state = NS_HTML5TOKENIZER_BOGUS_DOCTYPE;
              NS_HTML5_BREAK(afterdoctypesystemidentifierloop);
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
              NS_HTML5_BREAK(stateloop);
            }
            c = checkChar(buf, pos);
          }
          switch(c) {
            case '>': {
              emitDoctypeToken(pos);
              state = NS_HTML5TOKENIZER_DATA;
              NS_HTML5_CONTINUE(stateloop);
            }
            case '\r': {
              silentCarriageReturn();
              NS_HTML5_BREAK(stateloop);
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
            NS_HTML5_BREAK(stateloop);
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
              NS_HTML5_CONTINUE(stateloop);
            }
            index++;
            NS_HTML5_CONTINUE(stateloop);
          } else {
            state = NS_HTML5TOKENIZER_AFTER_DOCTYPE_SYSTEM_KEYWORD;
            reconsume = PR_TRUE;
            NS_HTML5_BREAK(doctypeystemloop);
          }
        }
        doctypeystemloop_end: ;
      }
      case NS_HTML5TOKENIZER_AFTER_DOCTYPE_SYSTEM_KEYWORD: {
        for (; ; ) {
          if (reconsume) {
            reconsume = PR_FALSE;
          } else {
            if (++pos == endPos) {
              NS_HTML5_BREAK(stateloop);
            }
            c = checkChar(buf, pos);
          }
          switch(c) {
            case '\r': {
              silentCarriageReturn();
              state = NS_HTML5TOKENIZER_BEFORE_DOCTYPE_SYSTEM_IDENTIFIER;
              NS_HTML5_BREAK(stateloop);
            }
            case '\n': {
              silentLineFeed();
            }
            case ' ':
            case '\t':
            case '\f': {
              state = NS_HTML5TOKENIZER_BEFORE_DOCTYPE_SYSTEM_IDENTIFIER;
              NS_HTML5_BREAK(afterdoctypesystemkeywordloop);
            }
            case '\"': {

              clearLongStrBuf();
              state = NS_HTML5TOKENIZER_DOCTYPE_SYSTEM_IDENTIFIER_DOUBLE_QUOTED;
              NS_HTML5_CONTINUE(stateloop);
            }
            case '\'': {

              clearLongStrBuf();
              state = NS_HTML5TOKENIZER_DOCTYPE_SYSTEM_IDENTIFIER_SINGLE_QUOTED;
              NS_HTML5_CONTINUE(stateloop);
            }
            case '>': {

              forceQuirks = PR_TRUE;
              emitDoctypeToken(pos);
              state = NS_HTML5TOKENIZER_DATA;
              NS_HTML5_CONTINUE(stateloop);
            }
            default: {
              bogusDoctype();
              state = NS_HTML5TOKENIZER_BOGUS_DOCTYPE;
              NS_HTML5_CONTINUE(stateloop);
            }
          }
        }
        afterdoctypesystemkeywordloop_end: ;
      }
      case NS_HTML5TOKENIZER_BEFORE_DOCTYPE_SYSTEM_IDENTIFIER: {
        for (; ; ) {
          if (++pos == endPos) {
            NS_HTML5_BREAK(stateloop);
          }
          c = checkChar(buf, pos);
          switch(c) {
            case '\r': {
              silentCarriageReturn();
              NS_HTML5_BREAK(stateloop);
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
              clearLongStrBuf();
              state = NS_HTML5TOKENIZER_DOCTYPE_SYSTEM_IDENTIFIER_DOUBLE_QUOTED;
              NS_HTML5_CONTINUE(stateloop);
            }
            case '\'': {
              clearLongStrBuf();
              state = NS_HTML5TOKENIZER_DOCTYPE_SYSTEM_IDENTIFIER_SINGLE_QUOTED;
              NS_HTML5_BREAK(beforedoctypesystemidentifierloop);
            }
            case '>': {

              forceQuirks = PR_TRUE;
              emitDoctypeToken(pos);
              state = NS_HTML5TOKENIZER_DATA;
              NS_HTML5_CONTINUE(stateloop);
            }
            default: {
              bogusDoctype();
              state = NS_HTML5TOKENIZER_BOGUS_DOCTYPE;
              NS_HTML5_CONTINUE(stateloop);
            }
          }
        }
        beforedoctypesystemidentifierloop_end: ;
      }
      case NS_HTML5TOKENIZER_DOCTYPE_SYSTEM_IDENTIFIER_SINGLE_QUOTED: {
        for (; ; ) {
          if (++pos == endPos) {
            NS_HTML5_BREAK(stateloop);
          }
          c = checkChar(buf, pos);
          switch(c) {
            case '\'': {
              systemIdentifier = longStrBufToString();
              state = NS_HTML5TOKENIZER_AFTER_DOCTYPE_SYSTEM_IDENTIFIER;
              NS_HTML5_CONTINUE(stateloop);
            }
            case '>': {

              forceQuirks = PR_TRUE;
              systemIdentifier = longStrBufToString();
              emitDoctypeToken(pos);
              state = NS_HTML5TOKENIZER_DATA;
              NS_HTML5_CONTINUE(stateloop);
            }
            case '\r': {
              appendLongStrBufCarriageReturn();
              NS_HTML5_BREAK(stateloop);
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
            NS_HTML5_BREAK(stateloop);
          }
          c = checkChar(buf, pos);
          switch(c) {
            case '\'': {
              publicIdentifier = longStrBufToString();
              state = NS_HTML5TOKENIZER_AFTER_DOCTYPE_PUBLIC_IDENTIFIER;
              NS_HTML5_CONTINUE(stateloop);
            }
            case '>': {

              forceQuirks = PR_TRUE;
              publicIdentifier = longStrBufToString();
              emitDoctypeToken(pos);
              state = NS_HTML5TOKENIZER_DATA;
              NS_HTML5_CONTINUE(stateloop);
            }
            case '\r': {
              appendLongStrBufCarriageReturn();
              NS_HTML5_BREAK(stateloop);
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
    }
  }
  stateloop_end: ;
  flushChars(buf, pos);
  stateSave = state;
  returnStateSave = returnState;
  return pos;
}

void 
nsHtml5Tokenizer::initDoctypeFields()
{
  nsHtml5Portability::releaseLocal(doctypeName);
  doctypeName = nsHtml5Atoms::emptystring;
  if (systemIdentifier) {
    nsHtml5Portability::releaseString(systemIdentifier);
    systemIdentifier = nsnull;
  }
  if (publicIdentifier) {
    nsHtml5Portability::releaseString(publicIdentifier);
    publicIdentifier = nsnull;
  }
  forceQuirks = PR_FALSE;
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
  flushChars(buf, pos);
  tokenHandler->zeroOriginatingReplacementCharacter();
  cstart = pos + 1;
}

void 
nsHtml5Tokenizer::setAdditionalAndRememberAmpersandLocation(PRUnichar add)
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
  if ((returnState & NS_HTML5TOKENIZER_DATA_AND_RCDATA_MASK)) {
    appendStrBufToLongStrBuf();
  } else {
    emitStrBuf();
  }
}

void 
nsHtml5Tokenizer::handleNcrValue(PRInt32 returnState)
{
  if (value <= 0xFFFF) {
    if (value >= 0x80 && value <= 0x9f) {

      PRUnichar* val = nsHtml5NamedCharacters::WINDOWS_1252[value - 0x80];
      emitOrAppendOne(val, returnState);
    } else if (value == 0x0) {

      emitOrAppendOne(nsHtml5Tokenizer::REPLACEMENT_CHARACTER, returnState);
    } else if ((value & 0xF800) == 0xD800) {

      emitOrAppendOne(nsHtml5Tokenizer::REPLACEMENT_CHARACTER, returnState);
    } else {
      PRUnichar ch = (PRUnichar) value;
      bmpChar[0] = ch;
      emitOrAppendOne(bmpChar, returnState);
    }
  } else if (value <= 0x10FFFF) {
    astralChar[0] = (PRUnichar) (NS_HTML5TOKENIZER_LEAD_OFFSET + (value >> 10));
    astralChar[1] = (PRUnichar) (0xDC00 + (value & 0x3FF));
    emitOrAppendTwo(astralChar, returnState);
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
      case NS_HTML5TOKENIZER_SCRIPT_DATA_LESS_THAN_SIGN:
      case NS_HTML5TOKENIZER_SCRIPT_DATA_ESCAPED_LESS_THAN_SIGN: {
        tokenHandler->characters(nsHtml5Tokenizer::LT_GT, 0, 1);
        NS_HTML5_BREAK(eofloop);
      }
      case NS_HTML5TOKENIZER_TAG_OPEN: {

        tokenHandler->characters(nsHtml5Tokenizer::LT_GT, 0, 1);
        NS_HTML5_BREAK(eofloop);
      }
      case NS_HTML5TOKENIZER_RAWTEXT_RCDATA_LESS_THAN_SIGN: {
        tokenHandler->characters(nsHtml5Tokenizer::LT_GT, 0, 1);
        NS_HTML5_BREAK(eofloop);
      }
      case NS_HTML5TOKENIZER_NON_DATA_END_TAG_NAME: {
        tokenHandler->characters(nsHtml5Tokenizer::LT_SOLIDUS, 0, 2);
        emitStrBuf();
        NS_HTML5_BREAK(eofloop);
      }
      case NS_HTML5TOKENIZER_CLOSE_TAG_OPEN: {

        tokenHandler->characters(nsHtml5Tokenizer::LT_SOLIDUS, 0, 2);
        NS_HTML5_BREAK(eofloop);
      }
      case NS_HTML5TOKENIZER_TAG_NAME: {

        NS_HTML5_BREAK(eofloop);
      }
      case NS_HTML5TOKENIZER_BEFORE_ATTRIBUTE_NAME:
      case NS_HTML5TOKENIZER_AFTER_ATTRIBUTE_VALUE_QUOTED:
      case NS_HTML5TOKENIZER_SELF_CLOSING_START_TAG: {

        NS_HTML5_BREAK(eofloop);
      }
      case NS_HTML5TOKENIZER_ATTRIBUTE_NAME: {

        NS_HTML5_BREAK(eofloop);
      }
      case NS_HTML5TOKENIZER_AFTER_ATTRIBUTE_NAME:
      case NS_HTML5TOKENIZER_BEFORE_ATTRIBUTE_VALUE: {

        NS_HTML5_BREAK(eofloop);
      }
      case NS_HTML5TOKENIZER_ATTRIBUTE_VALUE_DOUBLE_QUOTED:
      case NS_HTML5TOKENIZER_ATTRIBUTE_VALUE_SINGLE_QUOTED:
      case NS_HTML5TOKENIZER_ATTRIBUTE_VALUE_UNQUOTED: {

        NS_HTML5_BREAK(eofloop);
      }
      case NS_HTML5TOKENIZER_BOGUS_COMMENT: {
        emitComment(0, 0);
        NS_HTML5_BREAK(eofloop);
      }
      case NS_HTML5TOKENIZER_BOGUS_COMMENT_HYPHEN: {
        emitComment(0, 0);
        NS_HTML5_BREAK(eofloop);
      }
      case NS_HTML5TOKENIZER_MARKUP_DECLARATION_OPEN: {

        clearLongStrBuf();
        emitComment(0, 0);
        NS_HTML5_BREAK(eofloop);
      }
      case NS_HTML5TOKENIZER_MARKUP_DECLARATION_HYPHEN: {

        emitComment(0, 0);
        NS_HTML5_BREAK(eofloop);
      }
      case NS_HTML5TOKENIZER_MARKUP_DECLARATION_OCTYPE: {
        if (index < 6) {

          emitComment(0, 0);
        } else {

          nsHtml5Portability::releaseLocal(doctypeName);
          doctypeName = nsHtml5Atoms::emptystring;
          if (systemIdentifier) {
            nsHtml5Portability::releaseString(systemIdentifier);
            systemIdentifier = nsnull;
          }
          if (publicIdentifier) {
            nsHtml5Portability::releaseString(publicIdentifier);
            publicIdentifier = nsnull;
          }
          forceQuirks = PR_TRUE;
          emitDoctypeToken(0);
          NS_HTML5_BREAK(eofloop);
        }
        NS_HTML5_BREAK(eofloop);
      }
      case NS_HTML5TOKENIZER_COMMENT_START:
      case NS_HTML5TOKENIZER_COMMENT:
      case NS_HTML5TOKENIZER_COMMENT_END_SPACE: {

        emitComment(0, 0);
        NS_HTML5_BREAK(eofloop);
      }
      case NS_HTML5TOKENIZER_COMMENT_END: {

        emitComment(2, 0);
        NS_HTML5_BREAK(eofloop);
      }
      case NS_HTML5TOKENIZER_COMMENT_END_DASH:
      case NS_HTML5TOKENIZER_COMMENT_START_DASH: {

        emitComment(1, 0);
        NS_HTML5_BREAK(eofloop);
      }
      case NS_HTML5TOKENIZER_COMMENT_END_BANG: {

        emitComment(3, 0);
        NS_HTML5_BREAK(eofloop);
      }
      case NS_HTML5TOKENIZER_DOCTYPE:
      case NS_HTML5TOKENIZER_BEFORE_DOCTYPE_NAME: {

        forceQuirks = PR_TRUE;
        emitDoctypeToken(0);
        NS_HTML5_BREAK(eofloop);
      }
      case NS_HTML5TOKENIZER_DOCTYPE_NAME: {

        strBufToDoctypeName();
        forceQuirks = PR_TRUE;
        emitDoctypeToken(0);
        NS_HTML5_BREAK(eofloop);
      }
      case NS_HTML5TOKENIZER_DOCTYPE_UBLIC:
      case NS_HTML5TOKENIZER_DOCTYPE_YSTEM:
      case NS_HTML5TOKENIZER_AFTER_DOCTYPE_NAME:
      case NS_HTML5TOKENIZER_AFTER_DOCTYPE_PUBLIC_KEYWORD:
      case NS_HTML5TOKENIZER_AFTER_DOCTYPE_SYSTEM_KEYWORD:
      case NS_HTML5TOKENIZER_BEFORE_DOCTYPE_PUBLIC_IDENTIFIER: {

        forceQuirks = PR_TRUE;
        emitDoctypeToken(0);
        NS_HTML5_BREAK(eofloop);
      }
      case NS_HTML5TOKENIZER_DOCTYPE_PUBLIC_IDENTIFIER_DOUBLE_QUOTED:
      case NS_HTML5TOKENIZER_DOCTYPE_PUBLIC_IDENTIFIER_SINGLE_QUOTED: {

        forceQuirks = PR_TRUE;
        publicIdentifier = longStrBufToString();
        emitDoctypeToken(0);
        NS_HTML5_BREAK(eofloop);
      }
      case NS_HTML5TOKENIZER_AFTER_DOCTYPE_PUBLIC_IDENTIFIER:
      case NS_HTML5TOKENIZER_BEFORE_DOCTYPE_SYSTEM_IDENTIFIER:
      case NS_HTML5TOKENIZER_BETWEEN_DOCTYPE_PUBLIC_AND_SYSTEM_IDENTIFIERS: {

        forceQuirks = PR_TRUE;
        emitDoctypeToken(0);
        NS_HTML5_BREAK(eofloop);
      }
      case NS_HTML5TOKENIZER_DOCTYPE_SYSTEM_IDENTIFIER_DOUBLE_QUOTED:
      case NS_HTML5TOKENIZER_DOCTYPE_SYSTEM_IDENTIFIER_SINGLE_QUOTED: {

        forceQuirks = PR_TRUE;
        systemIdentifier = longStrBufToString();
        emitDoctypeToken(0);
        NS_HTML5_BREAK(eofloop);
      }
      case NS_HTML5TOKENIZER_AFTER_DOCTYPE_SYSTEM_IDENTIFIER: {

        forceQuirks = PR_TRUE;
        emitDoctypeToken(0);
        NS_HTML5_BREAK(eofloop);
      }
      case NS_HTML5TOKENIZER_BOGUS_DOCTYPE: {
        emitDoctypeToken(0);
        NS_HTML5_BREAK(eofloop);
      }
      case NS_HTML5TOKENIZER_CONSUME_CHARACTER_REFERENCE: {
        emitOrAppendStrBuf(returnState);
        state = returnState;
        continue;
      }
      case NS_HTML5TOKENIZER_CHARACTER_REFERENCE_HILO_LOOKUP: {

        emitOrAppendStrBuf(returnState);
        state = returnState;
        continue;
      }
      case NS_HTML5TOKENIZER_CHARACTER_REFERENCE_TAIL: {
        for (; ; ) {
          PRUnichar c = '\0';
          entCol++;
          for (; ; ) {
            if (hi == -1) {
              NS_HTML5_BREAK(hiloop);
            }
            if (entCol == nsHtml5NamedCharacters::NAMES[hi].length) {
              NS_HTML5_BREAK(hiloop);
            }
            if (entCol > nsHtml5NamedCharacters::NAMES[hi].length) {
              NS_HTML5_BREAK(outer);
            } else if (c < nsHtml5NamedCharacters::NAMES[hi][entCol]) {
              hi--;
            } else {
              NS_HTML5_BREAK(hiloop);
            }
          }
          hiloop_end: ;
          for (; ; ) {
            if (hi < lo) {
              NS_HTML5_BREAK(outer);
            }
            if (entCol == nsHtml5NamedCharacters::NAMES[lo].length) {
              candidate = lo;
              strBufMark = strBufLen;
              lo++;
            } else if (entCol > nsHtml5NamedCharacters::NAMES[lo].length) {
              NS_HTML5_BREAK(outer);
            } else if (c > nsHtml5NamedCharacters::NAMES[lo][entCol]) {
              lo++;
            } else {
              NS_HTML5_BREAK(loloop);
            }
          }
          loloop_end: ;
          if (hi < lo) {
            NS_HTML5_BREAK(outer);
          }
          continue;
        }
        outer_end: ;
        if (candidate == -1) {

          emitOrAppendStrBuf(returnState);
          state = returnState;
          NS_HTML5_CONTINUE(eofloop);
        } else {
          jArray<PRInt8,PRInt32> candidateArr = nsHtml5NamedCharacters::NAMES[candidate];
          if (!candidateArr.length || candidateArr[candidateArr.length - 1] != ';') {
            if ((returnState & NS_HTML5TOKENIZER_DATA_AND_RCDATA_MASK)) {
              PRUnichar ch;
              if (strBufMark == strBufLen) {
                ch = '\0';
              } else {
                ch = strBuf[strBufMark];
              }
              if ((ch >= '0' && ch <= '9') || (ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z')) {

                appendStrBufToLongStrBuf();
                state = returnState;
                NS_HTML5_CONTINUE(eofloop);
              }
            }

          }
          const PRUnichar* val = nsHtml5NamedCharacters::VALUES[candidate];
          if ((val[0] & 0xFC00) == 0xD800) {
            emitOrAppendTwo(val, returnState);
          } else {
            emitOrAppendOne(val, returnState);
          }
          if (strBufMark < strBufLen) {
            if ((returnState & NS_HTML5TOKENIZER_DATA_AND_RCDATA_MASK)) {
              for (PRInt32 i = strBufMark; i < strBufLen; i++) {
                appendLongStrBuf(strBuf[i]);
              }
            } else {
              tokenHandler->characters(strBuf, strBufMark, strBufLen - strBufMark);
            }
          }
          state = returnState;
          NS_HTML5_CONTINUE(eofloop);
        }
      }
      case NS_HTML5TOKENIZER_CONSUME_NCR:
      case NS_HTML5TOKENIZER_DECIMAL_NRC_LOOP:
      case NS_HTML5TOKENIZER_HEX_NCR_LOOP: {
        if (!seenDigits) {

          emitOrAppendStrBuf(returnState);
          state = returnState;
          continue;
        }
        handleNcrValue(returnState);
        state = returnState;
        continue;
      }
      case NS_HTML5TOKENIZER_CDATA_RSQB: {
        tokenHandler->characters(nsHtml5Tokenizer::RSQB_RSQB, 0, 1);
        NS_HTML5_BREAK(eofloop);
      }
      case NS_HTML5TOKENIZER_CDATA_RSQB_RSQB: {
        tokenHandler->characters(nsHtml5Tokenizer::RSQB_RSQB, 0, 2);
        NS_HTML5_BREAK(eofloop);
      }
      case NS_HTML5TOKENIZER_DATA:
      default: {
        NS_HTML5_BREAK(eofloop);
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
  doctypeName = nsnull;
  nsHtml5Portability::releaseString(publicIdentifier);
  publicIdentifier = nsnull;
  nsHtml5Portability::releaseString(systemIdentifier);
  systemIdentifier = nsnull;
}

void 
nsHtml5Tokenizer::internalEncodingDeclaration(nsString* internalCharset)
{
  if (encodingDeclarationHandler) {
    encodingDeclarationHandler->internalEncodingDeclaration(internalCharset);
  }
}

void 
nsHtml5Tokenizer::emitOrAppendTwo(const PRUnichar* val, PRInt32 returnState)
{
  if ((returnState & NS_HTML5TOKENIZER_DATA_AND_RCDATA_MASK)) {
    appendLongStrBuf(val[0]);
    appendLongStrBuf(val[1]);
  } else {
    tokenHandler->characters(val, 0, 2);
  }
}

void 
nsHtml5Tokenizer::emitOrAppendOne(const PRUnichar* val, PRInt32 returnState)
{
  if ((returnState & NS_HTML5TOKENIZER_DATA_AND_RCDATA_MASK)) {
    appendLongStrBuf(val[0]);
  } else {
    tokenHandler->characters(val, 0, 1);
  }
}

void 
nsHtml5Tokenizer::end()
{
  strBuf.release();
  strBuf = nsnull;
  longStrBuf.release();
  longStrBuf = nsnull;
  nsHtml5Portability::releaseLocal(doctypeName);
  doctypeName = nsnull;
  if (systemIdentifier) {
    nsHtml5Portability::releaseString(systemIdentifier);
    systemIdentifier = nsnull;
  }
  if (publicIdentifier) {
    nsHtml5Portability::releaseString(publicIdentifier);
    publicIdentifier = nsnull;
  }
  if (tagName) {
    tagName->release();
    tagName = nsnull;
  }
  if (attributeName) {
    attributeName->release();
    attributeName = nsnull;
  }
  tokenHandler->endTokenization();
  if (attributes) {
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

PRBool 
nsHtml5Tokenizer::isInDataState()
{
  return (stateSave == NS_HTML5TOKENIZER_DATA);
}

void 
nsHtml5Tokenizer::resetToDataState()
{
  strBufLen = 0;
  longStrBufLen = 0;
  stateSave = NS_HTML5TOKENIZER_DATA;
  lastCR = PR_FALSE;
  index = 0;
  forceQuirks = PR_FALSE;
  additional = '\0';
  entCol = -1;
  firstCharKey = -1;
  lo = 0;
  hi = (nsHtml5NamedCharacters::NAMES.length - 1);
  candidate = -1;
  strBufMark = 0;
  prevValue = -1;
  value = 0;
  seenDigits = PR_FALSE;
  endTag = PR_FALSE;
  shouldSuspend = PR_FALSE;
  initDoctypeFields();
  if (tagName) {
    tagName->release();
    tagName = nsnull;
  }
  if (attributeName) {
    attributeName->release();
    attributeName = nsnull;
  }
  if (attributes) {
    delete attributes;
    attributes = nsnull;
  }
}

void 
nsHtml5Tokenizer::loadState(nsHtml5Tokenizer* other)
{
  strBufLen = other->strBufLen;
  if (strBufLen > strBuf.length) {
    strBuf.release();
    strBuf = jArray<PRUnichar,PRInt32>(strBufLen);
  }
  nsHtml5ArrayCopy::arraycopy(other->strBuf, strBuf, strBufLen);
  longStrBufLen = other->longStrBufLen;
  if (longStrBufLen > longStrBuf.length) {
    longStrBuf.release();
    longStrBuf = jArray<PRUnichar,PRInt32>(longStrBufLen);
  }
  nsHtml5ArrayCopy::arraycopy(other->longStrBuf, longStrBuf, longStrBufLen);
  stateSave = other->stateSave;
  returnStateSave = other->returnStateSave;
  endTagExpectation = other->endTagExpectation;
  endTagExpectationAsArray = other->endTagExpectationAsArray;
  lastCR = other->lastCR;
  index = other->index;
  forceQuirks = other->forceQuirks;
  additional = other->additional;
  entCol = other->entCol;
  firstCharKey = other->firstCharKey;
  lo = other->lo;
  hi = other->hi;
  candidate = other->candidate;
  strBufMark = other->strBufMark;
  prevValue = other->prevValue;
  value = other->value;
  seenDigits = other->seenDigits;
  endTag = other->endTag;
  shouldSuspend = PR_FALSE;
  nsHtml5Portability::releaseLocal(doctypeName);
  if (!other->doctypeName) {
    doctypeName = nsnull;
  } else {
    doctypeName = nsHtml5Portability::newLocalFromLocal(other->doctypeName, interner);
  }
  nsHtml5Portability::releaseString(systemIdentifier);
  if (!other->systemIdentifier) {
    systemIdentifier = nsnull;
  } else {
    systemIdentifier = nsHtml5Portability::newStringFromString(other->systemIdentifier);
  }
  nsHtml5Portability::releaseString(publicIdentifier);
  if (!other->publicIdentifier) {
    publicIdentifier = nsnull;
  } else {
    publicIdentifier = nsHtml5Portability::newStringFromString(other->publicIdentifier);
  }
  if (tagName) {
    tagName->release();
  }
  if (!other->tagName) {
    tagName = nsnull;
  } else {
    tagName = other->tagName->cloneElementName(interner);
  }
  if (attributeName) {
    attributeName->release();
  }
  if (!other->attributeName) {
    attributeName = nsnull;
  } else {
    attributeName = other->attributeName->cloneAttributeName(interner);
  }
  if (attributes) {
    delete attributes;
  }
  if (!other->attributes) {
    attributes = nsnull;
  } else {
    attributes = other->attributes->cloneAttributes(interner);
  }
}

void 
nsHtml5Tokenizer::initializeWithoutStarting()
{
  confident = PR_FALSE;
  strBuf = jArray<PRUnichar,PRInt32>(64);
  longStrBuf = jArray<PRUnichar,PRInt32>(1024);
  line = 1;
  resetToDataState();
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


