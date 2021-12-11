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

#define nsHtml5Tokenizer_cpp__

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

#include "nsHtml5AttributeName.h"
#include "nsHtml5ElementName.h"
#include "nsHtml5TreeBuilder.h"
#include "nsHtml5MetaScanner.h"
#include "nsHtml5StackNode.h"
#include "nsHtml5UTF16Buffer.h"
#include "nsHtml5StateSnapshot.h"
#include "nsHtml5Portability.h"

#include "nsHtml5Tokenizer.h"

char16_t nsHtml5Tokenizer::LT_GT[] = {'<', '>'};
char16_t nsHtml5Tokenizer::LT_SOLIDUS[] = {'<', '/'};
char16_t nsHtml5Tokenizer::RSQB_RSQB[] = {']', ']'};
char16_t nsHtml5Tokenizer::REPLACEMENT_CHARACTER[] = {0xfffd};
char16_t nsHtml5Tokenizer::LF[] = {'\n'};
char16_t nsHtml5Tokenizer::CDATA_LSQB[] = {'C', 'D', 'A', 'T', 'A', '['};
char16_t nsHtml5Tokenizer::OCTYPE[] = {'o', 'c', 't', 'y', 'p', 'e'};
char16_t nsHtml5Tokenizer::UBLIC[] = {'u', 'b', 'l', 'i', 'c'};
char16_t nsHtml5Tokenizer::YSTEM[] = {'y', 's', 't', 'e', 'm'};
static char16_t const TITLE_ARR_DATA[] = {'t', 'i', 't', 'l', 'e'};
staticJArray<char16_t, int32_t> nsHtml5Tokenizer::TITLE_ARR = {
    TITLE_ARR_DATA, MOZ_ARRAY_LENGTH(TITLE_ARR_DATA)};
static char16_t const SCRIPT_ARR_DATA[] = {'s', 'c', 'r', 'i', 'p', 't'};
staticJArray<char16_t, int32_t> nsHtml5Tokenizer::SCRIPT_ARR = {
    SCRIPT_ARR_DATA, MOZ_ARRAY_LENGTH(SCRIPT_ARR_DATA)};
static char16_t const STYLE_ARR_DATA[] = {'s', 't', 'y', 'l', 'e'};
staticJArray<char16_t, int32_t> nsHtml5Tokenizer::STYLE_ARR = {
    STYLE_ARR_DATA, MOZ_ARRAY_LENGTH(STYLE_ARR_DATA)};
static char16_t const PLAINTEXT_ARR_DATA[] = {'p', 'l', 'a', 'i', 'n',
                                              't', 'e', 'x', 't'};
staticJArray<char16_t, int32_t> nsHtml5Tokenizer::PLAINTEXT_ARR = {
    PLAINTEXT_ARR_DATA, MOZ_ARRAY_LENGTH(PLAINTEXT_ARR_DATA)};
static char16_t const XMP_ARR_DATA[] = {'x', 'm', 'p'};
staticJArray<char16_t, int32_t> nsHtml5Tokenizer::XMP_ARR = {
    XMP_ARR_DATA, MOZ_ARRAY_LENGTH(XMP_ARR_DATA)};
static char16_t const TEXTAREA_ARR_DATA[] = {'t', 'e', 'x', 't',
                                             'a', 'r', 'e', 'a'};
staticJArray<char16_t, int32_t> nsHtml5Tokenizer::TEXTAREA_ARR = {
    TEXTAREA_ARR_DATA, MOZ_ARRAY_LENGTH(TEXTAREA_ARR_DATA)};
static char16_t const IFRAME_ARR_DATA[] = {'i', 'f', 'r', 'a', 'm', 'e'};
staticJArray<char16_t, int32_t> nsHtml5Tokenizer::IFRAME_ARR = {
    IFRAME_ARR_DATA, MOZ_ARRAY_LENGTH(IFRAME_ARR_DATA)};
static char16_t const NOEMBED_ARR_DATA[] = {'n', 'o', 'e', 'm', 'b', 'e', 'd'};
staticJArray<char16_t, int32_t> nsHtml5Tokenizer::NOEMBED_ARR = {
    NOEMBED_ARR_DATA, MOZ_ARRAY_LENGTH(NOEMBED_ARR_DATA)};
static char16_t const NOSCRIPT_ARR_DATA[] = {'n', 'o', 's', 'c',
                                             'r', 'i', 'p', 't'};
staticJArray<char16_t, int32_t> nsHtml5Tokenizer::NOSCRIPT_ARR = {
    NOSCRIPT_ARR_DATA, MOZ_ARRAY_LENGTH(NOSCRIPT_ARR_DATA)};
static char16_t const NOFRAMES_ARR_DATA[] = {'n', 'o', 'f', 'r',
                                             'a', 'm', 'e', 's'};
staticJArray<char16_t, int32_t> nsHtml5Tokenizer::NOFRAMES_ARR = {
    NOFRAMES_ARR_DATA, MOZ_ARRAY_LENGTH(NOFRAMES_ARR_DATA)};

nsHtml5Tokenizer::nsHtml5Tokenizer(nsHtml5TreeBuilder* tokenHandler,
                                   bool viewingXmlSource)
    : tokenHandler(tokenHandler),
      encodingDeclarationHandler(nullptr),
      lastCR(false),
      stateSave(0),
      returnStateSave(0),
      index(0),
      forceQuirks(false),
      additional('\0'),
      entCol(0),
      firstCharKey(0),
      lo(0),
      hi(0),
      candidate(0),
      charRefBufMark(0),
      value(0),
      seenDigits(false),
      cstart(0),
      strBufLen(0),
      charRefBuf(jArray<char16_t, int32_t>::newJArray(32)),
      charRefBufLen(0),
      bmpChar(jArray<char16_t, int32_t>::newJArray(1)),
      astralChar(jArray<char16_t, int32_t>::newJArray(2)),
      endTagExpectation(nullptr),
      endTagExpectationAsArray(nullptr),
      endTag(false),
      containsHyphen(false),
      tagName(nullptr),
      nonInternedTagName(new nsHtml5ElementName()),
      attributeName(nullptr),
      nonInternedAttributeName(new nsHtml5AttributeName()),
      doctypeName(nullptr),
      publicIdentifier(nullptr),
      systemIdentifier(nullptr),
      attributes(tokenHandler->HasBuilder() ? new nsHtml5HtmlAttributes(0)
                                            : nullptr),
      newAttributesEachTime(!tokenHandler->HasBuilder()),
      shouldSuspend(false),
      confident(false),
      line(0),
      attributeLine(0),
      interner(nullptr),
      viewingXmlSource(viewingXmlSource) {
  MOZ_COUNT_CTOR(nsHtml5Tokenizer);
}

void nsHtml5Tokenizer::setInterner(nsHtml5AtomTable* interner) {
  this->interner = interner;
}

void nsHtml5Tokenizer::initLocation(nsHtml5String newPublicId,
                                    nsHtml5String newSystemId) {
  this->systemId = newSystemId;
  this->publicId = newPublicId;
}

bool nsHtml5Tokenizer::isViewingXmlSource() { return viewingXmlSource; }

void nsHtml5Tokenizer::setState(int32_t specialTokenizerState) {
  this->stateSave = specialTokenizerState;
  this->endTagExpectation = nullptr;
  this->endTagExpectationAsArray = nullptr;
}

void nsHtml5Tokenizer::setStateAndEndTagExpectation(
    int32_t specialTokenizerState, nsHtml5ElementName* endTagExpectation) {
  this->stateSave = specialTokenizerState;
  this->endTagExpectation = endTagExpectation;
  endTagExpectationToArray();
}

void nsHtml5Tokenizer::endTagExpectationToArray() {
  switch (endTagExpectation->getGroup()) {
    case nsHtml5TreeBuilder::TITLE: {
      endTagExpectationAsArray = TITLE_ARR;
      return;
    }
    case nsHtml5TreeBuilder::SCRIPT: {
      endTagExpectationAsArray = SCRIPT_ARR;
      return;
    }
    case nsHtml5TreeBuilder::STYLE: {
      endTagExpectationAsArray = STYLE_ARR;
      return;
    }
    case nsHtml5TreeBuilder::PLAINTEXT: {
      endTagExpectationAsArray = PLAINTEXT_ARR;
      return;
    }
    case nsHtml5TreeBuilder::XMP: {
      endTagExpectationAsArray = XMP_ARR;
      return;
    }
    case nsHtml5TreeBuilder::TEXTAREA: {
      endTagExpectationAsArray = TEXTAREA_ARR;
      return;
    }
    case nsHtml5TreeBuilder::IFRAME: {
      endTagExpectationAsArray = IFRAME_ARR;
      return;
    }
    case nsHtml5TreeBuilder::NOEMBED: {
      endTagExpectationAsArray = NOEMBED_ARR;
      return;
    }
    case nsHtml5TreeBuilder::NOSCRIPT: {
      endTagExpectationAsArray = NOSCRIPT_ARR;
      return;
    }
    case nsHtml5TreeBuilder::NOFRAMES: {
      endTagExpectationAsArray = NOFRAMES_ARR;
      return;
    }
    default: {
      MOZ_ASSERT(false, "Bad end tag expectation.");
      return;
    }
  }
}

void nsHtml5Tokenizer::setLineNumber(int32_t line) {
  this->attributeLine = line;
  this->line = line;
}

nsHtml5HtmlAttributes* nsHtml5Tokenizer::emptyAttributes() {
  return nsHtml5HtmlAttributes::EMPTY_ATTRIBUTES;
}

void nsHtml5Tokenizer::emitOrAppendCharRefBuf(int32_t returnState) {
  if ((returnState & DATA_AND_RCDATA_MASK)) {
    appendCharRefBufToStrBuf();
  } else {
    if (charRefBufLen > 0) {
      tokenHandler->characters(charRefBuf, 0, charRefBufLen);
      charRefBufLen = 0;
    }
  }
}

nsHtml5String nsHtml5Tokenizer::strBufToString() {
  nsHtml5String str = nsHtml5Portability::newStringFromBuffer(
      strBuf, 0, strBufLen, tokenHandler,
      !newAttributesEachTime &&
          attributeName == nsHtml5AttributeName::ATTR_CLASS);
  clearStrBufAfterUse();
  return str;
}

void nsHtml5Tokenizer::strBufToDoctypeName() {
  doctypeName =
      nsHtml5Portability::newLocalNameFromBuffer(strBuf, strBufLen, interner);
  clearStrBufAfterUse();
}

void nsHtml5Tokenizer::emitStrBuf() {
  if (strBufLen > 0) {
    tokenHandler->characters(strBuf, 0, strBufLen);
    clearStrBufAfterUse();
  }
}

void nsHtml5Tokenizer::appendStrBuf(char16_t* buffer, int32_t offset,
                                    int32_t length) {
  int32_t newLen = nsHtml5Portability::checkedAdd(strBufLen, length);
  MOZ_ASSERT(newLen <= strBuf.length, "Previous buffer length insufficient.");
  if (MOZ_UNLIKELY(strBuf.length < newLen)) {
    if (MOZ_UNLIKELY(!EnsureBufferSpace(length))) {
      MOZ_CRASH("Unable to recover from buffer reallocation failure");
    }
  }
  nsHtml5ArrayCopy::arraycopy(buffer, offset, strBuf, strBufLen, length);
  strBufLen = newLen;
}

void nsHtml5Tokenizer::emitComment(int32_t provisionalHyphens, int32_t pos) {
  tokenHandler->comment(strBuf, 0, strBufLen - provisionalHyphens);
  clearStrBufAfterUse();
  cstart = pos + 1;
}

void nsHtml5Tokenizer::flushChars(char16_t* buf, int32_t pos) {
  if (pos > cstart) {
    tokenHandler->characters(buf, cstart, pos - cstart);
  }
  cstart = INT32_MAX;
}

void nsHtml5Tokenizer::strBufToElementNameString() {
  if (containsHyphen) {
    nsAtom* annotationName = nsHtml5ElementName::ELT_ANNOTATION_XML->getName();
    if (nsHtml5Portability::localEqualsBuffer(annotationName, strBuf,
                                              strBufLen)) {
      tagName = nsHtml5ElementName::ELT_ANNOTATION_XML;
    } else {
      nonInternedTagName->setNameForNonInterned(
          nsHtml5Portability::newLocalNameFromBuffer(strBuf, strBufLen,
                                                     interner),
          true);
      tagName = nonInternedTagName;
    }
  } else {
    tagName =
        nsHtml5ElementName::elementNameByBuffer(strBuf, strBufLen, interner);
    if (!tagName) {
      nonInternedTagName->setNameForNonInterned(
          nsHtml5Portability::newLocalNameFromBuffer(strBuf, strBufLen,
                                                     interner),
          false);
      tagName = nonInternedTagName;
    }
  }
  containsHyphen = false;
  clearStrBufAfterUse();
}

int32_t nsHtml5Tokenizer::emitCurrentTagToken(bool selfClosing, int32_t pos) {
  cstart = pos + 1;
  maybeErrSlashInEndTag(selfClosing);
  stateSave = nsHtml5Tokenizer::DATA;
  nsHtml5HtmlAttributes* attrs =
      (!attributes ? nsHtml5HtmlAttributes::EMPTY_ATTRIBUTES : attributes);
  if (endTag) {
    maybeErrAttributesOnEndTag(attrs);
    if (!viewingXmlSource) {
      tokenHandler->endTag(tagName);
    }
    if (newAttributesEachTime) {
      delete attributes;
      attributes = nullptr;
    }
  } else {
    if (viewingXmlSource) {
      MOZ_ASSERT(newAttributesEachTime);
      delete attributes;
      attributes = nullptr;
    } else {
      tokenHandler->startTag(tagName, attrs, selfClosing);
    }
  }
  tagName = nullptr;
  if (newAttributesEachTime) {
    attributes = nullptr;
  } else {
    attributes->clear(0);
  }
  return stateSave;
}

void nsHtml5Tokenizer::attributeNameComplete() {
  attributeName =
      nsHtml5AttributeName::nameByBuffer(strBuf, strBufLen, interner);
  if (!attributeName) {
    nonInternedAttributeName->setNameForNonInterned(
        nsHtml5Portability::newLocalNameFromBuffer(strBuf, strBufLen,
                                                   interner));
    attributeName = nonInternedAttributeName;
  }
  clearStrBufAfterUse();
  if (!attributes) {
    attributes = new nsHtml5HtmlAttributes(0);
  }
  if (attributes->contains(attributeName)) {
    errDuplicateAttribute();
    attributeName = nullptr;
  }
}

void nsHtml5Tokenizer::addAttributeWithoutValue() {
  if (attributeName) {
    attributes->addAttribute(
        attributeName, nsHtml5Portability::newEmptyString(), attributeLine);
    attributeName = nullptr;
  } else {
    clearStrBufAfterUse();
  }
}

void nsHtml5Tokenizer::addAttributeWithValue() {
  if (attributeName) {
    nsHtml5String val = strBufToString();
    if (mViewSource) {
      mViewSource->MaybeLinkifyAttributeValue(attributeName, val);
    }
    attributes->addAttribute(attributeName, val, attributeLine);
    attributeName = nullptr;
  } else {
    clearStrBufAfterUse();
  }
}

void nsHtml5Tokenizer::start() {
  initializeWithoutStarting();
  tokenHandler->startTokenization(this);
}

bool nsHtml5Tokenizer::tokenizeBuffer(nsHtml5UTF16Buffer* buffer) {
  int32_t state = stateSave;
  int32_t returnState = returnStateSave;
  char16_t c = '\0';
  shouldSuspend = false;
  lastCR = false;
  int32_t start = buffer->getStart();
  int32_t end = buffer->getEnd();
  int32_t pos = start - 1;
  switch (state) {
    case DATA:
    case RCDATA:
    case SCRIPT_DATA:
    case PLAINTEXT:
    case RAWTEXT:
    case CDATA_SECTION:
    case SCRIPT_DATA_ESCAPED:
    case SCRIPT_DATA_ESCAPE_START:
    case SCRIPT_DATA_ESCAPE_START_DASH:
    case SCRIPT_DATA_ESCAPED_DASH:
    case SCRIPT_DATA_ESCAPED_DASH_DASH:
    case SCRIPT_DATA_DOUBLE_ESCAPE_START:
    case SCRIPT_DATA_DOUBLE_ESCAPED:
    case SCRIPT_DATA_DOUBLE_ESCAPED_LESS_THAN_SIGN:
    case SCRIPT_DATA_DOUBLE_ESCAPED_DASH:
    case SCRIPT_DATA_DOUBLE_ESCAPED_DASH_DASH:
    case SCRIPT_DATA_DOUBLE_ESCAPE_END: {
      cstart = start;
      break;
    }
    default: {
      cstart = INT32_MAX;
      break;
    }
  }
  if (mViewSource) {
    mViewSource->SetBuffer(buffer);
    pos = stateLoop<nsHtml5ViewSourcePolicy>(state, c, pos, buffer->getBuffer(),
                                             false, returnState,
                                             buffer->getEnd());
    mViewSource->DropBuffer((pos == buffer->getEnd()) ? pos : pos + 1);
  } else {
    pos = stateLoop<nsHtml5SilentPolicy>(state, c, pos, buffer->getBuffer(),
                                         false, returnState, buffer->getEnd());
  }
  if (pos == end) {
    buffer->setStart(pos);
  } else {
    buffer->setStart(pos + 1);
  }
  return lastCR;
}

template <class P>
int32_t nsHtml5Tokenizer::stateLoop(int32_t state, char16_t c, int32_t pos,
                                    char16_t* buf, bool reconsume,
                                    int32_t returnState, int32_t endPos) {
  bool reportedConsecutiveHyphens = false;
stateloop:
  for (;;) {
    switch (state) {
      case DATA: {
        for (;;) {
          if (reconsume) {
            reconsume = false;
          } else {
            if (++pos == endPos) {
              NS_HTML5_BREAK(stateloop);
            }
            c = checkChar(buf, pos);
          }
          switch (c) {
            case '&': {
              flushChars(buf, pos);
              MOZ_ASSERT(!charRefBufLen,
                         "charRefBufLen not reset after previous use!");
              appendCharRefBuf(c);
              setAdditionalAndRememberAmpersandLocation('\0');
              returnState = state;
              state =
                  P::transition(mViewSource.get(),
                                nsHtml5Tokenizer::CONSUME_CHARACTER_REFERENCE,
                                reconsume, pos);
              NS_HTML5_CONTINUE(stateloop);
            }
            case '<': {
              flushChars(buf, pos);
              state = P::transition(mViewSource.get(),
                                    nsHtml5Tokenizer::TAG_OPEN, reconsume, pos);
              NS_HTML5_BREAK(dataloop);
            }
            case '\0': {
              maybeEmitReplacementCharacter(buf, pos);
              continue;
            }
            case '\r': {
              emitCarriageReturn(buf, pos);
              NS_HTML5_BREAK(stateloop);
            }
            case '\n': {
              silentLineFeed();
              [[fallthrough]];
            }
            default: {
              continue;
            }
          }
        }
      dataloop_end:;
        [[fallthrough]];
      }
      case TAG_OPEN: {
        for (;;) {
          if (++pos == endPos) {
            NS_HTML5_BREAK(stateloop);
          }
          c = checkChar(buf, pos);
          if (c >= 'A' && c <= 'Z') {
            endTag = false;
            clearStrBufBeforeUse();
            appendStrBuf((char16_t)(c + 0x20));
            containsHyphen = false;
            state = P::transition(mViewSource.get(), nsHtml5Tokenizer::TAG_NAME,
                                  reconsume, pos);
            NS_HTML5_BREAK(tagopenloop);
          } else if (c >= 'a' && c <= 'z') {
            endTag = false;
            clearStrBufBeforeUse();
            appendStrBuf(c);
            containsHyphen = false;
            state = P::transition(mViewSource.get(), nsHtml5Tokenizer::TAG_NAME,
                                  reconsume, pos);
            NS_HTML5_BREAK(tagopenloop);
          }
          switch (c) {
            case '!': {
              state = P::transition(mViewSource.get(),
                                    nsHtml5Tokenizer::MARKUP_DECLARATION_OPEN,
                                    reconsume, pos);
              NS_HTML5_CONTINUE(stateloop);
            }
            case '/': {
              state = P::transition(mViewSource.get(),
                                    nsHtml5Tokenizer::CLOSE_TAG_OPEN, reconsume,
                                    pos);
              NS_HTML5_CONTINUE(stateloop);
            }
            case '\?': {
              if (viewingXmlSource) {
                state = P::transition(mViewSource.get(),
                                      nsHtml5Tokenizer::PROCESSING_INSTRUCTION,
                                      reconsume, pos);
                NS_HTML5_CONTINUE(stateloop);
              }
              if (P::reportErrors) {
                errProcessingInstruction();
              }
              clearStrBufBeforeUse();
              appendStrBuf(c);
              state = P::transition(mViewSource.get(),
                                    nsHtml5Tokenizer::BOGUS_COMMENT, reconsume,
                                    pos);
              NS_HTML5_CONTINUE(stateloop);
            }
            case '>': {
              if (P::reportErrors) {
                errLtGt();
              }
              tokenHandler->characters(nsHtml5Tokenizer::LT_GT, 0, 2);
              cstart = pos + 1;
              state = P::transition(mViewSource.get(), nsHtml5Tokenizer::DATA,
                                    reconsume, pos);
              NS_HTML5_CONTINUE(stateloop);
            }
            default: {
              if (P::reportErrors) {
                errBadCharAfterLt(c);
              }
              tokenHandler->characters(nsHtml5Tokenizer::LT_GT, 0, 1);
              cstart = pos;
              reconsume = true;
              state = P::transition(mViewSource.get(), nsHtml5Tokenizer::DATA,
                                    reconsume, pos);
              NS_HTML5_CONTINUE(stateloop);
            }
          }
        }
      tagopenloop_end:;
        [[fallthrough]];
      }
      case TAG_NAME: {
        for (;;) {
          if (++pos == endPos) {
            NS_HTML5_BREAK(stateloop);
          }
          c = checkChar(buf, pos);
          switch (c) {
            case '\r': {
              silentCarriageReturn();
              strBufToElementNameString();
              state = P::transition(mViewSource.get(),
                                    nsHtml5Tokenizer::BEFORE_ATTRIBUTE_NAME,
                                    reconsume, pos);
              NS_HTML5_BREAK(stateloop);
            }
            case '\n': {
              silentLineFeed();
              [[fallthrough]];
            }
            case ' ':
            case '\t':
            case '\f': {
              strBufToElementNameString();
              state = P::transition(mViewSource.get(),
                                    nsHtml5Tokenizer::BEFORE_ATTRIBUTE_NAME,
                                    reconsume, pos);
              NS_HTML5_BREAK(tagnameloop);
            }
            case '/': {
              strBufToElementNameString();
              state = P::transition(mViewSource.get(),
                                    nsHtml5Tokenizer::SELF_CLOSING_START_TAG,
                                    reconsume, pos);
              NS_HTML5_CONTINUE(stateloop);
            }
            case '>': {
              strBufToElementNameString();
              state = P::transition(mViewSource.get(),
                                    emitCurrentTagToken(false, pos), reconsume,
                                    pos);
              if (shouldSuspend) {
                NS_HTML5_BREAK(stateloop);
              }
              NS_HTML5_CONTINUE(stateloop);
            }
            case '\0': {
              c = 0xfffd;
              [[fallthrough]];
            }
            default: {
              if (c >= 'A' && c <= 'Z') {
                c += 0x20;
              } else if (c == '-') {
                containsHyphen = true;
              }
              appendStrBuf(c);
              continue;
            }
          }
        }
      tagnameloop_end:;
        [[fallthrough]];
      }
      case BEFORE_ATTRIBUTE_NAME: {
        for (;;) {
          if (reconsume) {
            reconsume = false;
          } else {
            if (++pos == endPos) {
              NS_HTML5_BREAK(stateloop);
            }
            c = checkChar(buf, pos);
          }
          switch (c) {
            case '\r': {
              silentCarriageReturn();
              NS_HTML5_BREAK(stateloop);
            }
            case '\n': {
              silentLineFeed();
              [[fallthrough]];
            }
            case ' ':
            case '\t':
            case '\f': {
              continue;
            }
            case '/': {
              state = P::transition(mViewSource.get(),
                                    nsHtml5Tokenizer::SELF_CLOSING_START_TAG,
                                    reconsume, pos);
              NS_HTML5_CONTINUE(stateloop);
            }
            case '>': {
              state = P::transition(mViewSource.get(),
                                    emitCurrentTagToken(false, pos), reconsume,
                                    pos);
              if (shouldSuspend) {
                NS_HTML5_BREAK(stateloop);
              }
              NS_HTML5_CONTINUE(stateloop);
            }
            case '\0': {
              c = 0xfffd;
              [[fallthrough]];
            }
            case '\"':
            case '\'':
            case '<':
            case '=': {
              if (P::reportErrors) {
                errBadCharBeforeAttributeNameOrNull(c);
              }
              [[fallthrough]];
            }
            default: {
              if (c >= 'A' && c <= 'Z') {
                c += 0x20;
              }
              attributeLine = line;
              clearStrBufBeforeUse();
              appendStrBuf(c);
              state = P::transition(mViewSource.get(),
                                    nsHtml5Tokenizer::ATTRIBUTE_NAME, reconsume,
                                    pos);
              NS_HTML5_BREAK(beforeattributenameloop);
            }
          }
        }
      beforeattributenameloop_end:;
        [[fallthrough]];
      }
      case ATTRIBUTE_NAME: {
        for (;;) {
          if (++pos == endPos) {
            NS_HTML5_BREAK(stateloop);
          }
          c = checkChar(buf, pos);
          switch (c) {
            case '\r': {
              silentCarriageReturn();
              attributeNameComplete();
              state = P::transition(mViewSource.get(),
                                    nsHtml5Tokenizer::AFTER_ATTRIBUTE_NAME,
                                    reconsume, pos);
              NS_HTML5_BREAK(stateloop);
            }
            case '\n': {
              silentLineFeed();
              [[fallthrough]];
            }
            case ' ':
            case '\t':
            case '\f': {
              attributeNameComplete();
              state = P::transition(mViewSource.get(),
                                    nsHtml5Tokenizer::AFTER_ATTRIBUTE_NAME,
                                    reconsume, pos);
              NS_HTML5_CONTINUE(stateloop);
            }
            case '/': {
              attributeNameComplete();
              addAttributeWithoutValue();
              state = P::transition(mViewSource.get(),
                                    nsHtml5Tokenizer::SELF_CLOSING_START_TAG,
                                    reconsume, pos);
              NS_HTML5_CONTINUE(stateloop);
            }
            case '=': {
              attributeNameComplete();
              state = P::transition(mViewSource.get(),
                                    nsHtml5Tokenizer::BEFORE_ATTRIBUTE_VALUE,
                                    reconsume, pos);
              NS_HTML5_BREAK(attributenameloop);
            }
            case '>': {
              attributeNameComplete();
              addAttributeWithoutValue();
              state = P::transition(mViewSource.get(),
                                    emitCurrentTagToken(false, pos), reconsume,
                                    pos);
              if (shouldSuspend) {
                NS_HTML5_BREAK(stateloop);
              }
              NS_HTML5_CONTINUE(stateloop);
            }
            case '\0': {
              c = 0xfffd;
              [[fallthrough]];
            }
            case '\"':
            case '\'':
            case '<': {
              if (P::reportErrors) {
                errQuoteOrLtInAttributeNameOrNull(c);
              }
              [[fallthrough]];
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
      attributenameloop_end:;
        [[fallthrough]];
      }
      case BEFORE_ATTRIBUTE_VALUE: {
        for (;;) {
          if (++pos == endPos) {
            NS_HTML5_BREAK(stateloop);
          }
          c = checkChar(buf, pos);
          switch (c) {
            case '\r': {
              silentCarriageReturn();
              NS_HTML5_BREAK(stateloop);
            }
            case '\n': {
              silentLineFeed();
              [[fallthrough]];
            }
            case ' ':
            case '\t':
            case '\f': {
              continue;
            }
            case '\"': {
              attributeLine = line;
              clearStrBufBeforeUse();
              state =
                  P::transition(mViewSource.get(),
                                nsHtml5Tokenizer::ATTRIBUTE_VALUE_DOUBLE_QUOTED,
                                reconsume, pos);
              NS_HTML5_BREAK(beforeattributevalueloop);
            }
            case '&': {
              attributeLine = line;
              clearStrBufBeforeUse();
              reconsume = true;
              state = P::transition(mViewSource.get(),
                                    nsHtml5Tokenizer::ATTRIBUTE_VALUE_UNQUOTED,
                                    reconsume, pos);

              NS_HTML5_CONTINUE(stateloop);
            }
            case '\'': {
              attributeLine = line;
              clearStrBufBeforeUse();
              state =
                  P::transition(mViewSource.get(),
                                nsHtml5Tokenizer::ATTRIBUTE_VALUE_SINGLE_QUOTED,
                                reconsume, pos);
              NS_HTML5_CONTINUE(stateloop);
            }
            case '>': {
              if (P::reportErrors) {
                errAttributeValueMissing();
              }
              addAttributeWithoutValue();
              state = P::transition(mViewSource.get(),
                                    emitCurrentTagToken(false, pos), reconsume,
                                    pos);
              if (shouldSuspend) {
                NS_HTML5_BREAK(stateloop);
              }
              NS_HTML5_CONTINUE(stateloop);
            }
            case '\0': {
              c = 0xfffd;
              [[fallthrough]];
            }
            case '<':
            case '=':
            case '`': {
              if (P::reportErrors) {
                errLtOrEqualsOrGraveInUnquotedAttributeOrNull(c);
              }
              [[fallthrough]];
            }
            default: {
              attributeLine = line;
              clearStrBufBeforeUse();
              appendStrBuf(c);
              state = P::transition(mViewSource.get(),
                                    nsHtml5Tokenizer::ATTRIBUTE_VALUE_UNQUOTED,
                                    reconsume, pos);

              NS_HTML5_CONTINUE(stateloop);
            }
          }
        }
      beforeattributevalueloop_end:;
        [[fallthrough]];
      }
      case ATTRIBUTE_VALUE_DOUBLE_QUOTED: {
        for (;;) {
          if (reconsume) {
            reconsume = false;
          } else {
            if (++pos == endPos) {
              NS_HTML5_BREAK(stateloop);
            }
            c = checkChar(buf, pos);
          }
          switch (c) {
            case '\"': {
              addAttributeWithValue();
              state =
                  P::transition(mViewSource.get(),
                                nsHtml5Tokenizer::AFTER_ATTRIBUTE_VALUE_QUOTED,
                                reconsume, pos);
              NS_HTML5_BREAK(attributevaluedoublequotedloop);
            }
            case '&': {
              MOZ_ASSERT(!charRefBufLen,
                         "charRefBufLen not reset after previous use!");
              appendCharRefBuf(c);
              setAdditionalAndRememberAmpersandLocation('\"');
              returnState = state;
              state =
                  P::transition(mViewSource.get(),
                                nsHtml5Tokenizer::CONSUME_CHARACTER_REFERENCE,
                                reconsume, pos);
              NS_HTML5_CONTINUE(stateloop);
            }
            case '\r': {
              appendStrBufCarriageReturn();
              NS_HTML5_BREAK(stateloop);
            }
            case '\n': {
              appendStrBufLineFeed();
              continue;
            }
            case '\0': {
              c = 0xfffd;
              [[fallthrough]];
            }
            default: {
              appendStrBuf(c);
              continue;
            }
          }
        }
      attributevaluedoublequotedloop_end:;
        [[fallthrough]];
      }
      case AFTER_ATTRIBUTE_VALUE_QUOTED: {
        for (;;) {
          if (++pos == endPos) {
            NS_HTML5_BREAK(stateloop);
          }
          c = checkChar(buf, pos);
          switch (c) {
            case '\r': {
              silentCarriageReturn();
              state = P::transition(mViewSource.get(),
                                    nsHtml5Tokenizer::BEFORE_ATTRIBUTE_NAME,
                                    reconsume, pos);
              NS_HTML5_BREAK(stateloop);
            }
            case '\n': {
              silentLineFeed();
              [[fallthrough]];
            }
            case ' ':
            case '\t':
            case '\f': {
              state = P::transition(mViewSource.get(),
                                    nsHtml5Tokenizer::BEFORE_ATTRIBUTE_NAME,
                                    reconsume, pos);
              NS_HTML5_CONTINUE(stateloop);
            }
            case '/': {
              state = P::transition(mViewSource.get(),
                                    nsHtml5Tokenizer::SELF_CLOSING_START_TAG,
                                    reconsume, pos);
              NS_HTML5_BREAK(afterattributevaluequotedloop);
            }
            case '>': {
              state = P::transition(mViewSource.get(),
                                    emitCurrentTagToken(false, pos), reconsume,
                                    pos);
              if (shouldSuspend) {
                NS_HTML5_BREAK(stateloop);
              }
              NS_HTML5_CONTINUE(stateloop);
            }
            default: {
              if (P::reportErrors) {
                errNoSpaceBetweenAttributes();
              }
              reconsume = true;
              state = P::transition(mViewSource.get(),
                                    nsHtml5Tokenizer::BEFORE_ATTRIBUTE_NAME,
                                    reconsume, pos);
              NS_HTML5_CONTINUE(stateloop);
            }
          }
        }
      afterattributevaluequotedloop_end:;
        [[fallthrough]];
      }
      case SELF_CLOSING_START_TAG: {
        if (++pos == endPos) {
          NS_HTML5_BREAK(stateloop);
        }
        c = checkChar(buf, pos);
        switch (c) {
          case '>': {
            state =
                P::transition(mViewSource.get(), emitCurrentTagToken(true, pos),
                              reconsume, pos);
            if (shouldSuspend) {
              NS_HTML5_BREAK(stateloop);
            }
            NS_HTML5_CONTINUE(stateloop);
          }
          default: {
            if (P::reportErrors) {
              errSlashNotFollowedByGt();
            }
            reconsume = true;
            state = P::transition(mViewSource.get(),
                                  nsHtml5Tokenizer::BEFORE_ATTRIBUTE_NAME,
                                  reconsume, pos);
            NS_HTML5_CONTINUE(stateloop);
          }
        }
      }
      case ATTRIBUTE_VALUE_UNQUOTED: {
        for (;;) {
          if (reconsume) {
            reconsume = false;
          } else {
            if (++pos == endPos) {
              NS_HTML5_BREAK(stateloop);
            }
            c = checkChar(buf, pos);
          }
          switch (c) {
            case '\r': {
              silentCarriageReturn();
              addAttributeWithValue();
              state = P::transition(mViewSource.get(),
                                    nsHtml5Tokenizer::BEFORE_ATTRIBUTE_NAME,
                                    reconsume, pos);
              NS_HTML5_BREAK(stateloop);
            }
            case '\n': {
              silentLineFeed();
              [[fallthrough]];
            }
            case ' ':
            case '\t':
            case '\f': {
              addAttributeWithValue();
              state = P::transition(mViewSource.get(),
                                    nsHtml5Tokenizer::BEFORE_ATTRIBUTE_NAME,
                                    reconsume, pos);
              NS_HTML5_CONTINUE(stateloop);
            }
            case '&': {
              MOZ_ASSERT(!charRefBufLen,
                         "charRefBufLen not reset after previous use!");
              appendCharRefBuf(c);
              setAdditionalAndRememberAmpersandLocation('>');
              returnState = state;
              state =
                  P::transition(mViewSource.get(),
                                nsHtml5Tokenizer::CONSUME_CHARACTER_REFERENCE,
                                reconsume, pos);
              NS_HTML5_CONTINUE(stateloop);
            }
            case '>': {
              addAttributeWithValue();
              state = P::transition(mViewSource.get(),
                                    emitCurrentTagToken(false, pos), reconsume,
                                    pos);
              if (shouldSuspend) {
                NS_HTML5_BREAK(stateloop);
              }
              NS_HTML5_CONTINUE(stateloop);
            }
            case '\0': {
              c = 0xfffd;
              [[fallthrough]];
            }
            case '<':
            case '\"':
            case '\'':
            case '=':
            case '`': {
              if (P::reportErrors) {
                errUnquotedAttributeValOrNull(c);
              }
              [[fallthrough]];
            }
            default: {
              appendStrBuf(c);
              continue;
            }
          }
        }
      }
      case AFTER_ATTRIBUTE_NAME: {
        for (;;) {
          if (++pos == endPos) {
            NS_HTML5_BREAK(stateloop);
          }
          c = checkChar(buf, pos);
          switch (c) {
            case '\r': {
              silentCarriageReturn();
              NS_HTML5_BREAK(stateloop);
            }
            case '\n': {
              silentLineFeed();
              [[fallthrough]];
            }
            case ' ':
            case '\t':
            case '\f': {
              continue;
            }
            case '/': {
              addAttributeWithoutValue();
              state = P::transition(mViewSource.get(),
                                    nsHtml5Tokenizer::SELF_CLOSING_START_TAG,
                                    reconsume, pos);
              NS_HTML5_CONTINUE(stateloop);
            }
            case '=': {
              state = P::transition(mViewSource.get(),
                                    nsHtml5Tokenizer::BEFORE_ATTRIBUTE_VALUE,
                                    reconsume, pos);
              NS_HTML5_CONTINUE(stateloop);
            }
            case '>': {
              addAttributeWithoutValue();
              state = P::transition(mViewSource.get(),
                                    emitCurrentTagToken(false, pos), reconsume,
                                    pos);
              if (shouldSuspend) {
                NS_HTML5_BREAK(stateloop);
              }
              NS_HTML5_CONTINUE(stateloop);
            }
            case '\0': {
              c = 0xfffd;
              [[fallthrough]];
            }
            case '\"':
            case '\'':
            case '<': {
              if (P::reportErrors) {
                errQuoteOrLtInAttributeNameOrNull(c);
              }
              [[fallthrough]];
            }
            default: {
              addAttributeWithoutValue();
              if (c >= 'A' && c <= 'Z') {
                c += 0x20;
              }
              clearStrBufBeforeUse();
              appendStrBuf(c);
              state = P::transition(mViewSource.get(),
                                    nsHtml5Tokenizer::ATTRIBUTE_NAME, reconsume,
                                    pos);
              NS_HTML5_CONTINUE(stateloop);
            }
          }
        }
      }
      case MARKUP_DECLARATION_OPEN: {
        for (;;) {
          if (++pos == endPos) {
            NS_HTML5_BREAK(stateloop);
          }
          c = checkChar(buf, pos);
          switch (c) {
            case '-': {
              clearStrBufBeforeUse();
              appendStrBuf(c);
              state = P::transition(mViewSource.get(),
                                    nsHtml5Tokenizer::MARKUP_DECLARATION_HYPHEN,
                                    reconsume, pos);
              NS_HTML5_BREAK(markupdeclarationopenloop);
            }
            case 'd':
            case 'D': {
              clearStrBufBeforeUse();
              appendStrBuf(c);
              index = 0;
              state = P::transition(mViewSource.get(),
                                    nsHtml5Tokenizer::MARKUP_DECLARATION_OCTYPE,
                                    reconsume, pos);
              NS_HTML5_CONTINUE(stateloop);
            }
            case '[': {
              if (tokenHandler->cdataSectionAllowed()) {
                clearStrBufBeforeUse();
                appendStrBuf(c);
                index = 0;
                state = P::transition(mViewSource.get(),
                                      nsHtml5Tokenizer::CDATA_START, reconsume,
                                      pos);
                NS_HTML5_CONTINUE(stateloop);
              }
              [[fallthrough]];
            }
            default: {
              if (P::reportErrors) {
                errBogusComment();
              }
              clearStrBufBeforeUse();
              reconsume = true;
              state = P::transition(mViewSource.get(),
                                    nsHtml5Tokenizer::BOGUS_COMMENT, reconsume,
                                    pos);
              NS_HTML5_CONTINUE(stateloop);
            }
          }
        }
      markupdeclarationopenloop_end:;
        [[fallthrough]];
      }
      case MARKUP_DECLARATION_HYPHEN: {
        for (;;) {
          if (++pos == endPos) {
            NS_HTML5_BREAK(stateloop);
          }
          c = checkChar(buf, pos);
          switch (c) {
            case '-': {
              clearStrBufAfterOneHyphen();
              state = P::transition(mViewSource.get(),
                                    nsHtml5Tokenizer::COMMENT_START, reconsume,
                                    pos);
              NS_HTML5_BREAK(markupdeclarationhyphenloop);
            }
            default: {
              if (P::reportErrors) {
                errBogusComment();
              }
              reconsume = true;
              state = P::transition(mViewSource.get(),
                                    nsHtml5Tokenizer::BOGUS_COMMENT, reconsume,
                                    pos);
              NS_HTML5_CONTINUE(stateloop);
            }
          }
        }
      markupdeclarationhyphenloop_end:;
        [[fallthrough]];
      }
      case COMMENT_START: {
        reportedConsecutiveHyphens = false;
        for (;;) {
          if (++pos == endPos) {
            NS_HTML5_BREAK(stateloop);
          }
          c = checkChar(buf, pos);
          switch (c) {
            case '-': {
              appendStrBuf(c);
              state = P::transition(mViewSource.get(),
                                    nsHtml5Tokenizer::COMMENT_START_DASH,
                                    reconsume, pos);
              NS_HTML5_CONTINUE(stateloop);
            }
            case '>': {
              if (P::reportErrors) {
                errPrematureEndOfComment();
              }
              emitComment(0, pos);
              state = P::transition(mViewSource.get(), nsHtml5Tokenizer::DATA,
                                    reconsume, pos);
              NS_HTML5_CONTINUE(stateloop);
            }
            case '<': {
              appendStrBuf(c);
              state = P::transition(mViewSource.get(),
                                    nsHtml5Tokenizer::COMMENT_LESSTHAN,
                                    reconsume, pos);
              NS_HTML5_CONTINUE(stateloop);
            }
            case '\r': {
              appendStrBufCarriageReturn();
              state = P::transition(mViewSource.get(),
                                    nsHtml5Tokenizer::COMMENT, reconsume, pos);
              NS_HTML5_BREAK(stateloop);
            }
            case '\n': {
              appendStrBufLineFeed();
              state = P::transition(mViewSource.get(),
                                    nsHtml5Tokenizer::COMMENT, reconsume, pos);
              NS_HTML5_BREAK(commentstartloop);
            }
            case '\0': {
              c = 0xfffd;
              [[fallthrough]];
            }
            default: {
              appendStrBuf(c);
              state = P::transition(mViewSource.get(),
                                    nsHtml5Tokenizer::COMMENT, reconsume, pos);
              NS_HTML5_BREAK(commentstartloop);
            }
          }
        }
      commentstartloop_end:;
        [[fallthrough]];
      }
      case COMMENT: {
        for (;;) {
          if (++pos == endPos) {
            NS_HTML5_BREAK(stateloop);
          }
          c = checkChar(buf, pos);
          switch (c) {
            case '-': {
              appendStrBuf(c);
              state = P::transition(mViewSource.get(),
                                    nsHtml5Tokenizer::COMMENT_END_DASH,
                                    reconsume, pos);
              NS_HTML5_BREAK(commentloop);
            }
            case '<': {
              appendStrBuf(c);
              state = P::transition(mViewSource.get(),
                                    nsHtml5Tokenizer::COMMENT_LESSTHAN,
                                    reconsume, pos);
              NS_HTML5_CONTINUE(stateloop);
            }
            case '\r': {
              appendStrBufCarriageReturn();
              NS_HTML5_BREAK(stateloop);
            }
            case '\n': {
              appendStrBufLineFeed();
              continue;
            }
            case '\0': {
              c = 0xfffd;
              [[fallthrough]];
            }
            default: {
              appendStrBuf(c);
              continue;
            }
          }
        }
      commentloop_end:;
        [[fallthrough]];
      }
      case COMMENT_END_DASH: {
        for (;;) {
          if (++pos == endPos) {
            NS_HTML5_BREAK(stateloop);
          }
          c = checkChar(buf, pos);
          switch (c) {
            case '-': {
              appendStrBuf(c);
              state =
                  P::transition(mViewSource.get(),
                                nsHtml5Tokenizer::COMMENT_END, reconsume, pos);
              NS_HTML5_BREAK(commentenddashloop);
            }
            case '<': {
              appendStrBuf(c);
              state = P::transition(mViewSource.get(),
                                    nsHtml5Tokenizer::COMMENT_LESSTHAN,
                                    reconsume, pos);
              NS_HTML5_CONTINUE(stateloop);
            }
            case '\r': {
              appendStrBufCarriageReturn();
              state = P::transition(mViewSource.get(),
                                    nsHtml5Tokenizer::COMMENT, reconsume, pos);
              NS_HTML5_BREAK(stateloop);
            }
            case '\n': {
              appendStrBufLineFeed();
              state = P::transition(mViewSource.get(),
                                    nsHtml5Tokenizer::COMMENT, reconsume, pos);
              NS_HTML5_CONTINUE(stateloop);
            }
            case '\0': {
              c = 0xfffd;
              [[fallthrough]];
            }
            default: {
              appendStrBuf(c);
              state = P::transition(mViewSource.get(),
                                    nsHtml5Tokenizer::COMMENT, reconsume, pos);
              NS_HTML5_CONTINUE(stateloop);
            }
          }
        }
      commentenddashloop_end:;
        [[fallthrough]];
      }
      case COMMENT_END: {
        for (;;) {
          if (++pos == endPos) {
            NS_HTML5_BREAK(stateloop);
          }
          c = checkChar(buf, pos);
          switch (c) {
            case '>': {
              emitComment(2, pos);
              state = P::transition(mViewSource.get(), nsHtml5Tokenizer::DATA,
                                    reconsume, pos);
              NS_HTML5_CONTINUE(stateloop);
            }
            case '-': {
              adjustDoubleHyphenAndAppendToStrBufAndErr(
                  c, reportedConsecutiveHyphens);
              reportedConsecutiveHyphens = true;
              continue;
            }
            case '<': {
              appendStrBuf(c);
              state = P::transition(mViewSource.get(),
                                    nsHtml5Tokenizer::COMMENT_LESSTHAN,
                                    reconsume, pos);
              NS_HTML5_CONTINUE(stateloop);
            }
            case '\r': {
              adjustDoubleHyphenAndAppendToStrBufCarriageReturn();
              state = P::transition(mViewSource.get(),
                                    nsHtml5Tokenizer::COMMENT, reconsume, pos);
              NS_HTML5_BREAK(stateloop);
            }
            case '\n': {
              adjustDoubleHyphenAndAppendToStrBufLineFeed();
              state = P::transition(mViewSource.get(),
                                    nsHtml5Tokenizer::COMMENT, reconsume, pos);
              NS_HTML5_CONTINUE(stateloop);
            }
            case '!': {
              appendStrBuf(c);
              state = P::transition(mViewSource.get(),
                                    nsHtml5Tokenizer::COMMENT_END_BANG,
                                    reconsume, pos);
              NS_HTML5_CONTINUE(stateloop);
            }
            case '\0': {
              c = 0xfffd;
              [[fallthrough]];
            }
            default: {
              adjustDoubleHyphenAndAppendToStrBufAndErr(
                  c, reportedConsecutiveHyphens);
              reportedConsecutiveHyphens = true;
              state = P::transition(mViewSource.get(),
                                    nsHtml5Tokenizer::COMMENT, reconsume, pos);
              NS_HTML5_CONTINUE(stateloop);
            }
          }
        }
      }
      case COMMENT_END_BANG: {
        for (;;) {
          if (++pos == endPos) {
            NS_HTML5_BREAK(stateloop);
          }
          c = checkChar(buf, pos);
          switch (c) {
            case '>': {
              emitComment(3, pos);
              state = P::transition(mViewSource.get(), nsHtml5Tokenizer::DATA,
                                    reconsume, pos);
              NS_HTML5_CONTINUE(stateloop);
            }
            case '-': {
              appendStrBuf(c);
              state = P::transition(mViewSource.get(),
                                    nsHtml5Tokenizer::COMMENT_END_DASH,
                                    reconsume, pos);
              NS_HTML5_CONTINUE(stateloop);
            }
            case '\r': {
              appendStrBufCarriageReturn();
              state = P::transition(mViewSource.get(),
                                    nsHtml5Tokenizer::COMMENT, reconsume, pos);
              NS_HTML5_BREAK(stateloop);
            }
            case '\n': {
              appendStrBufLineFeed();
              state = P::transition(mViewSource.get(),
                                    nsHtml5Tokenizer::COMMENT, reconsume, pos);
              NS_HTML5_CONTINUE(stateloop);
            }
            case '\0': {
              c = 0xfffd;
              [[fallthrough]];
            }
            default: {
              appendStrBuf(c);
              state = P::transition(mViewSource.get(),
                                    nsHtml5Tokenizer::COMMENT, reconsume, pos);
              NS_HTML5_CONTINUE(stateloop);
            }
          }
        }
      }
      case COMMENT_LESSTHAN: {
        for (;;) {
          if (++pos == endPos) {
            NS_HTML5_BREAK(stateloop);
          }
          c = checkChar(buf, pos);
          switch (c) {
            case '!': {
              appendStrBuf(c);
              state = P::transition(mViewSource.get(),
                                    nsHtml5Tokenizer::COMMENT_LESSTHAN_BANG,
                                    reconsume, pos);
              NS_HTML5_CONTINUE(stateloop);
            }
            case '<': {
              appendStrBuf(c);
              continue;
            }
            case '-': {
              appendStrBuf(c);
              state = P::transition(mViewSource.get(),
                                    nsHtml5Tokenizer::COMMENT_END_DASH,
                                    reconsume, pos);
              NS_HTML5_CONTINUE(stateloop);
            }
            case '\r': {
              appendStrBufCarriageReturn();
              NS_HTML5_BREAK(stateloop);
            }
            case '\n': {
              appendStrBufLineFeed();
              continue;
            }
            case '\0': {
              c = 0xfffd;
              [[fallthrough]];
            }
            default: {
              appendStrBuf(c);
              state = P::transition(mViewSource.get(),
                                    nsHtml5Tokenizer::COMMENT, reconsume, pos);
              NS_HTML5_CONTINUE(stateloop);
            }
          }
        }
        [[fallthrough]];
      }
      case COMMENT_LESSTHAN_BANG: {
        for (;;) {
          if (++pos == endPos) {
            NS_HTML5_BREAK(stateloop);
          }
          c = checkChar(buf, pos);
          switch (c) {
            case '-': {
              appendStrBuf(c);
              state = P::transition(
                  mViewSource.get(),
                  nsHtml5Tokenizer::COMMENT_LESSTHAN_BANG_DASH, reconsume, pos);
              NS_HTML5_CONTINUE(stateloop);
            }
            case '<': {
              appendStrBuf(c);
              state = P::transition(mViewSource.get(),
                                    nsHtml5Tokenizer::COMMENT_LESSTHAN,
                                    reconsume, pos);
              NS_HTML5_CONTINUE(stateloop);
            }
            case '\r': {
              appendStrBufCarriageReturn();
              NS_HTML5_BREAK(stateloop);
            }
            case '\n': {
              appendStrBufLineFeed();
              continue;
            }
            case '\0': {
              c = 0xfffd;
              [[fallthrough]];
            }
            default: {
              appendStrBuf(c);
              state = P::transition(mViewSource.get(),
                                    nsHtml5Tokenizer::COMMENT, reconsume, pos);
              NS_HTML5_CONTINUE(stateloop);
            }
          }
        }
        [[fallthrough]];
      }
      case COMMENT_LESSTHAN_BANG_DASH: {
        for (;;) {
          if (++pos == endPos) {
            NS_HTML5_BREAK(stateloop);
          }
          c = checkChar(buf, pos);
          switch (c) {
            case '-': {
              appendStrBuf(c);
              state = P::transition(
                  mViewSource.get(),
                  nsHtml5Tokenizer::COMMENT_LESSTHAN_BANG_DASH_DASH, reconsume,
                  pos);
              NS_HTML5_CONTINUE(stateloop);
            }
            case '<': {
              appendStrBuf(c);
              state = P::transition(mViewSource.get(),
                                    nsHtml5Tokenizer::COMMENT_LESSTHAN,
                                    reconsume, pos);
              NS_HTML5_CONTINUE(stateloop);
            }
            case '\r': {
              appendStrBufCarriageReturn();
              NS_HTML5_BREAK(stateloop);
            }
            case '\n': {
              appendStrBufLineFeed();
              continue;
            }
            case '\0': {
              c = 0xfffd;
              [[fallthrough]];
            }
            default: {
              appendStrBuf(c);
              state = P::transition(mViewSource.get(),
                                    nsHtml5Tokenizer::COMMENT, reconsume, pos);
              NS_HTML5_CONTINUE(stateloop);
            }
          }
        }
        [[fallthrough]];
      }
      case COMMENT_LESSTHAN_BANG_DASH_DASH: {
        for (;;) {
          if (++pos == endPos) {
            NS_HTML5_BREAK(stateloop);
          }
          c = checkChar(buf, pos);
          switch (c) {
            case '>': {
              appendStrBuf(c);
              emitComment(3, pos);
              state = P::transition(mViewSource.get(), nsHtml5Tokenizer::DATA,
                                    reconsume, pos);
              NS_HTML5_CONTINUE(stateloop);
            }
            case '-': {
              if (P::reportErrors) {
                errNestedComment();
              }
              adjustDoubleHyphenAndAppendToStrBufAndErr(
                  c, reportedConsecutiveHyphens);
              reportedConsecutiveHyphens = true;
              state =
                  P::transition(mViewSource.get(),
                                nsHtml5Tokenizer::COMMENT_END, reconsume, pos);
              NS_HTML5_CONTINUE(stateloop);
            }
            case '\r': {
              if (P::reportErrors) {
                errNestedComment();
              }
              adjustDoubleHyphenAndAppendToStrBufAndErr(
                  c, reportedConsecutiveHyphens);
              reportedConsecutiveHyphens = true;
              state = P::transition(mViewSource.get(),
                                    nsHtml5Tokenizer::COMMENT, reconsume, pos);
              NS_HTML5_BREAK(stateloop);
            }
            case '\n': {
              if (P::reportErrors) {
                errNestedComment();
              }
              adjustDoubleHyphenAndAppendToStrBufAndErr(
                  c, reportedConsecutiveHyphens);
              reportedConsecutiveHyphens = true;
              state = P::transition(mViewSource.get(),
                                    nsHtml5Tokenizer::COMMENT, reconsume, pos);
              continue;
            }
            case '\0': {
              c = 0xfffd;
              [[fallthrough]];
            }
            case '!': {
              if (P::reportErrors) {
                errNestedComment();
              }
              adjustDoubleHyphenAndAppendToStrBufAndErr(
                  c, reportedConsecutiveHyphens);
              reportedConsecutiveHyphens = true;
              state = P::transition(mViewSource.get(),
                                    nsHtml5Tokenizer::COMMENT_END_BANG,
                                    reconsume, pos);
              NS_HTML5_CONTINUE(stateloop);
            }
            default: {
              if (P::reportErrors) {
                errNestedComment();
              }
              adjustDoubleHyphenAndAppendToStrBufAndErr(
                  c, reportedConsecutiveHyphens);
              reportedConsecutiveHyphens = true;
              state =
                  P::transition(mViewSource.get(),
                                nsHtml5Tokenizer::COMMENT_END, reconsume, pos);
              NS_HTML5_CONTINUE(stateloop);
            }
          }
        }
        [[fallthrough]];
      }
      case COMMENT_START_DASH: {
        if (++pos == endPos) {
          NS_HTML5_BREAK(stateloop);
        }
        c = checkChar(buf, pos);
        switch (c) {
          case '-': {
            appendStrBuf(c);
            state =
                P::transition(mViewSource.get(), nsHtml5Tokenizer::COMMENT_END,
                              reconsume, pos);
            NS_HTML5_CONTINUE(stateloop);
          }
          case '>': {
            if (P::reportErrors) {
              errPrematureEndOfComment();
            }
            emitComment(1, pos);
            state = P::transition(mViewSource.get(), nsHtml5Tokenizer::DATA,
                                  reconsume, pos);
            NS_HTML5_CONTINUE(stateloop);
          }
          case '<': {
            appendStrBuf(c);
            state = P::transition(mViewSource.get(),
                                  nsHtml5Tokenizer::COMMENT_LESSTHAN, reconsume,
                                  pos);
            NS_HTML5_CONTINUE(stateloop);
          }
          case '\r': {
            appendStrBufCarriageReturn();
            state = P::transition(mViewSource.get(), nsHtml5Tokenizer::COMMENT,
                                  reconsume, pos);
            NS_HTML5_BREAK(stateloop);
          }
          case '\n': {
            appendStrBufLineFeed();
            state = P::transition(mViewSource.get(), nsHtml5Tokenizer::COMMENT,
                                  reconsume, pos);
            NS_HTML5_CONTINUE(stateloop);
          }
          case '\0': {
            c = 0xfffd;
            [[fallthrough]];
          }
          default: {
            appendStrBuf(c);
            state = P::transition(mViewSource.get(), nsHtml5Tokenizer::COMMENT,
                                  reconsume, pos);
            NS_HTML5_CONTINUE(stateloop);
          }
        }
      }
      case CDATA_START: {
        for (;;) {
          if (++pos == endPos) {
            NS_HTML5_BREAK(stateloop);
          }
          c = checkChar(buf, pos);
          if (index < 6) {
            if (c == nsHtml5Tokenizer::CDATA_LSQB[index]) {
              appendStrBuf(c);
            } else {
              if (P::reportErrors) {
                errBogusComment();
              }
              reconsume = true;
              state = P::transition(mViewSource.get(),
                                    nsHtml5Tokenizer::BOGUS_COMMENT, reconsume,
                                    pos);
              NS_HTML5_CONTINUE(stateloop);
            }
            index++;
            continue;
          } else {
            clearStrBufAfterUse();
            cstart = pos;
            reconsume = true;
            state =
                P::transition(mViewSource.get(),
                              nsHtml5Tokenizer::CDATA_SECTION, reconsume, pos);
            break;
          }
        }
        [[fallthrough]];
      }
      case CDATA_SECTION: {
        for (;;) {
          if (reconsume) {
            reconsume = false;
          } else {
            if (++pos == endPos) {
              NS_HTML5_BREAK(stateloop);
            }
            c = checkChar(buf, pos);
          }
          switch (c) {
            case ']': {
              flushChars(buf, pos);
              state =
                  P::transition(mViewSource.get(), nsHtml5Tokenizer::CDATA_RSQB,
                                reconsume, pos);
              NS_HTML5_BREAK(cdatasectionloop);
            }
            case '\0': {
              maybeEmitReplacementCharacter(buf, pos);
              continue;
            }
            case '\r': {
              emitCarriageReturn(buf, pos);
              NS_HTML5_BREAK(stateloop);
            }
            case '\n': {
              silentLineFeed();
              [[fallthrough]];
            }
            default: {
              continue;
            }
          }
        }
      cdatasectionloop_end:;
        [[fallthrough]];
      }
      case CDATA_RSQB: {
        for (;;) {
          if (++pos == endPos) {
            NS_HTML5_BREAK(stateloop);
          }
          c = checkChar(buf, pos);
          switch (c) {
            case ']': {
              state = P::transition(mViewSource.get(),
                                    nsHtml5Tokenizer::CDATA_RSQB_RSQB,
                                    reconsume, pos);
              NS_HTML5_BREAK(cdatarsqb);
            }
            default: {
              tokenHandler->characters(nsHtml5Tokenizer::RSQB_RSQB, 0, 1);
              cstart = pos;
              reconsume = true;
              state = P::transition(mViewSource.get(),
                                    nsHtml5Tokenizer::CDATA_SECTION, reconsume,
                                    pos);
              NS_HTML5_CONTINUE(stateloop);
            }
          }
        }
      cdatarsqb_end:;
        [[fallthrough]];
      }
      case CDATA_RSQB_RSQB: {
        for (;;) {
          if (++pos == endPos) {
            NS_HTML5_BREAK(stateloop);
          }
          c = checkChar(buf, pos);
          switch (c) {
            case ']': {
              tokenHandler->characters(nsHtml5Tokenizer::RSQB_RSQB, 0, 1);
              continue;
            }
            case '>': {
              cstart = pos + 1;
              state = P::transition(mViewSource.get(), nsHtml5Tokenizer::DATA,
                                    reconsume, pos);
              NS_HTML5_CONTINUE(stateloop);
            }
            default: {
              tokenHandler->characters(nsHtml5Tokenizer::RSQB_RSQB, 0, 2);
              cstart = pos;
              reconsume = true;
              state = P::transition(mViewSource.get(),
                                    nsHtml5Tokenizer::CDATA_SECTION, reconsume,
                                    pos);
              NS_HTML5_CONTINUE(stateloop);
            }
          }
        }
      }
      case ATTRIBUTE_VALUE_SINGLE_QUOTED: {
        for (;;) {
          if (reconsume) {
            reconsume = false;
          } else {
            if (++pos == endPos) {
              NS_HTML5_BREAK(stateloop);
            }
            c = checkChar(buf, pos);
          }
          switch (c) {
            case '\'': {
              addAttributeWithValue();
              state =
                  P::transition(mViewSource.get(),
                                nsHtml5Tokenizer::AFTER_ATTRIBUTE_VALUE_QUOTED,
                                reconsume, pos);
              NS_HTML5_CONTINUE(stateloop);
            }
            case '&': {
              MOZ_ASSERT(!charRefBufLen,
                         "charRefBufLen not reset after previous use!");
              appendCharRefBuf(c);
              setAdditionalAndRememberAmpersandLocation('\'');
              returnState = state;
              state =
                  P::transition(mViewSource.get(),
                                nsHtml5Tokenizer::CONSUME_CHARACTER_REFERENCE,
                                reconsume, pos);
              NS_HTML5_BREAK(attributevaluesinglequotedloop);
            }
            case '\r': {
              appendStrBufCarriageReturn();
              NS_HTML5_BREAK(stateloop);
            }
            case '\n': {
              appendStrBufLineFeed();
              continue;
            }
            case '\0': {
              c = 0xfffd;
              [[fallthrough]];
            }
            default: {
              appendStrBuf(c);
              continue;
            }
          }
        }
      attributevaluesinglequotedloop_end:;
        [[fallthrough]];
      }
      case CONSUME_CHARACTER_REFERENCE: {
        if (++pos == endPos) {
          NS_HTML5_BREAK(stateloop);
        }
        c = checkChar(buf, pos);
        switch (c) {
          case ' ':
          case '\t':
          case '\n':
          case '\r':
          case '\f':
          case '<':
          case '&':
          case '\0':
          case ';': {
            emitOrAppendCharRefBuf(returnState);
            if (!(returnState & DATA_AND_RCDATA_MASK)) {
              cstart = pos;
            }
            reconsume = true;
            state =
                P::transition(mViewSource.get(), returnState, reconsume, pos);
            NS_HTML5_CONTINUE(stateloop);
          }
          case '#': {
            appendCharRefBuf('#');
            state =
                P::transition(mViewSource.get(), nsHtml5Tokenizer::CONSUME_NCR,
                              reconsume, pos);
            NS_HTML5_CONTINUE(stateloop);
          }
          default: {
            if (c == additional) {
              emitOrAppendCharRefBuf(returnState);
              reconsume = true;
              state =
                  P::transition(mViewSource.get(), returnState, reconsume, pos);
              NS_HTML5_CONTINUE(stateloop);
            }
            if (c >= 'a' && c <= 'z') {
              firstCharKey = c - 'a' + 26;
            } else if (c >= 'A' && c <= 'Z') {
              firstCharKey = c - 'A';
            } else {
              if (c == ';') {
                if (P::reportErrors) {
                  errNoNamedCharacterMatch();
                }
              }
              emitOrAppendCharRefBuf(returnState);
              if (!(returnState & DATA_AND_RCDATA_MASK)) {
                cstart = pos;
              }
              reconsume = true;
              state =
                  P::transition(mViewSource.get(), returnState, reconsume, pos);
              NS_HTML5_CONTINUE(stateloop);
            }
            appendCharRefBuf(c);
            state =
                P::transition(mViewSource.get(),
                              nsHtml5Tokenizer::CHARACTER_REFERENCE_HILO_LOOKUP,
                              reconsume, pos);
          }
        }
        [[fallthrough]];
      }
      case CHARACTER_REFERENCE_HILO_LOOKUP: {
        {
          if (++pos == endPos) {
            NS_HTML5_BREAK(stateloop);
          }
          c = checkChar(buf, pos);
          int32_t hilo = 0;
          if (c <= 'z') {
            const int32_t* row = nsHtml5NamedCharactersAccel::HILO_ACCEL[c];
            if (row) {
              hilo = row[firstCharKey];
            }
          }
          if (!hilo) {
            if (c == ';') {
              if (P::reportErrors) {
                errNoNamedCharacterMatch();
              }
            }
            emitOrAppendCharRefBuf(returnState);
            if (!(returnState & DATA_AND_RCDATA_MASK)) {
              cstart = pos;
            }
            reconsume = true;
            state =
                P::transition(mViewSource.get(), returnState, reconsume, pos);
            NS_HTML5_CONTINUE(stateloop);
          }
          appendCharRefBuf(c);
          lo = hilo & 0xFFFF;
          hi = hilo >> 16;
          entCol = -1;
          candidate = -1;
          charRefBufMark = 0;
          state = P::transition(mViewSource.get(),
                                nsHtml5Tokenizer::CHARACTER_REFERENCE_TAIL,
                                reconsume, pos);
        }
        [[fallthrough]];
      }
      case CHARACTER_REFERENCE_TAIL: {
        for (;;) {
          if (++pos == endPos) {
            NS_HTML5_BREAK(stateloop);
          }
          c = checkChar(buf, pos);
          entCol++;
          for (;;) {
            if (hi < lo) {
              NS_HTML5_BREAK(outer);
            }
            if (entCol == nsHtml5NamedCharacters::NAMES[lo].length()) {
              candidate = lo;
              charRefBufMark = charRefBufLen;
              lo++;
            } else if (entCol > nsHtml5NamedCharacters::NAMES[lo].length()) {
              NS_HTML5_BREAK(outer);
            } else if (c > nsHtml5NamedCharacters::NAMES[lo].charAt(entCol)) {
              lo++;
            } else {
              NS_HTML5_BREAK(loloop);
            }
          }
        loloop_end:;
          for (;;) {
            if (hi < lo) {
              NS_HTML5_BREAK(outer);
            }
            if (entCol == nsHtml5NamedCharacters::NAMES[hi].length()) {
              NS_HTML5_BREAK(hiloop);
            }
            if (entCol > nsHtml5NamedCharacters::NAMES[hi].length()) {
              NS_HTML5_BREAK(outer);
            } else if (c < nsHtml5NamedCharacters::NAMES[hi].charAt(entCol)) {
              hi--;
            } else {
              NS_HTML5_BREAK(hiloop);
            }
          }
        hiloop_end:;
          if (c == ';') {
            if (entCol + 1 == nsHtml5NamedCharacters::NAMES[lo].length()) {
              candidate = lo;
              charRefBufMark = charRefBufLen;
            }
            NS_HTML5_BREAK(outer);
          }
          if (hi < lo) {
            NS_HTML5_BREAK(outer);
          }
          appendCharRefBuf(c);
          continue;
        }
      outer_end:;
        if (candidate == -1) {
          if (c == ';') {
            if (P::reportErrors) {
              errNoNamedCharacterMatch();
            }
          }
          emitOrAppendCharRefBuf(returnState);
          if (!(returnState & DATA_AND_RCDATA_MASK)) {
            cstart = pos;
          }
          reconsume = true;
          state = P::transition(mViewSource.get(), returnState, reconsume, pos);
          NS_HTML5_CONTINUE(stateloop);
        } else {
          const nsHtml5CharacterName& candidateName =
              nsHtml5NamedCharacters::NAMES[candidate];
          if (!candidateName.length() ||
              candidateName.charAt(candidateName.length() - 1) != ';') {
            if ((returnState & DATA_AND_RCDATA_MASK)) {
              char16_t ch;
              if (charRefBufMark == charRefBufLen) {
                ch = c;
              } else {
                ch = charRefBuf[charRefBufMark];
              }
              if (ch == '=' || (ch >= '0' && ch <= '9') ||
                  (ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z')) {
                if (c == ';') {
                  if (P::reportErrors) {
                    errNoNamedCharacterMatch();
                  }
                }
                appendCharRefBufToStrBuf();
                reconsume = true;
                state = P::transition(mViewSource.get(), returnState, reconsume,
                                      pos);
                NS_HTML5_CONTINUE(stateloop);
              }
            }
            if ((returnState & DATA_AND_RCDATA_MASK)) {
              if (P::reportErrors) {
                errUnescapedAmpersandInterpretedAsCharacterReference();
              }
            } else {
              if (P::reportErrors) {
                errNotSemicolonTerminated();
              }
            }
          }
          P::completedNamedCharacterReference(mViewSource.get());
          const char16_t* val = nsHtml5NamedCharacters::VALUES[candidate];
          if (!val[1]) {
            emitOrAppendOne(val, returnState);
          } else {
            emitOrAppendTwo(val, returnState);
          }
          if (charRefBufMark < charRefBufLen) {
            if ((returnState & DATA_AND_RCDATA_MASK)) {
              appendStrBuf(charRefBuf, charRefBufMark,
                           charRefBufLen - charRefBufMark);
            } else {
              tokenHandler->characters(charRefBuf, charRefBufMark,
                                       charRefBufLen - charRefBufMark);
            }
          }
          bool earlyBreak = (c == ';' && charRefBufMark == charRefBufLen);
          charRefBufLen = 0;
          if (!(returnState & DATA_AND_RCDATA_MASK)) {
            cstart = earlyBreak ? pos + 1 : pos;
          }
          reconsume = !earlyBreak;
          state = P::transition(mViewSource.get(), returnState, reconsume, pos);
          NS_HTML5_CONTINUE(stateloop);
        }
      }
      case CONSUME_NCR: {
        if (++pos == endPos) {
          NS_HTML5_BREAK(stateloop);
        }
        c = checkChar(buf, pos);
        value = 0;
        seenDigits = false;
        switch (c) {
          case 'x':
          case 'X': {
            appendCharRefBuf(c);
            state =
                P::transition(mViewSource.get(), nsHtml5Tokenizer::HEX_NCR_LOOP,
                              reconsume, pos);
            NS_HTML5_CONTINUE(stateloop);
          }
          default: {
            reconsume = true;
            state = P::transition(mViewSource.get(),
                                  nsHtml5Tokenizer::DECIMAL_NRC_LOOP, reconsume,
                                  pos);
          }
        }
        [[fallthrough]];
      }
      case DECIMAL_NRC_LOOP: {
        for (;;) {
          if (reconsume) {
            reconsume = false;
          } else {
            if (++pos == endPos) {
              NS_HTML5_BREAK(stateloop);
            }
            c = checkChar(buf, pos);
          }
          MOZ_ASSERT(value >= 0, "value must not become negative.");
          if (c >= '0' && c <= '9') {
            seenDigits = true;
            if (value <= 0x10FFFF) {
              value *= 10;
              value += c - '0';
            }
            continue;
          } else if (c == ';') {
            if (seenDigits) {
              if (!(returnState & DATA_AND_RCDATA_MASK)) {
                cstart = pos + 1;
              }
              state = P::transition(mViewSource.get(),
                                    nsHtml5Tokenizer::HANDLE_NCR_VALUE,
                                    reconsume, pos);
              NS_HTML5_BREAK(decimalloop);
            } else {
              if (P::reportErrors) {
                errNoDigitsInNCR();
              }
              appendCharRefBuf(';');
              emitOrAppendCharRefBuf(returnState);
              if (!(returnState & DATA_AND_RCDATA_MASK)) {
                cstart = pos + 1;
              }
              state =
                  P::transition(mViewSource.get(), returnState, reconsume, pos);
              NS_HTML5_CONTINUE(stateloop);
            }
          } else {
            if (!seenDigits) {
              if (P::reportErrors) {
                errNoDigitsInNCR();
              }
              emitOrAppendCharRefBuf(returnState);
              if (!(returnState & DATA_AND_RCDATA_MASK)) {
                cstart = pos;
              }
              reconsume = true;
              state =
                  P::transition(mViewSource.get(), returnState, reconsume, pos);
              NS_HTML5_CONTINUE(stateloop);
            } else {
              if (P::reportErrors) {
                errCharRefLacksSemicolon();
              }
              if (!(returnState & DATA_AND_RCDATA_MASK)) {
                cstart = pos;
              }
              reconsume = true;
              state = P::transition(mViewSource.get(),
                                    nsHtml5Tokenizer::HANDLE_NCR_VALUE,
                                    reconsume, pos);
              NS_HTML5_BREAK(decimalloop);
            }
          }
        }
      decimalloop_end:;
        [[fallthrough]];
      }
      case HANDLE_NCR_VALUE: {
        charRefBufLen = 0;
        handleNcrValue(returnState);
        state = P::transition(mViewSource.get(), returnState, reconsume, pos);
        NS_HTML5_CONTINUE(stateloop);
      }
      case HEX_NCR_LOOP: {
        for (;;) {
          if (++pos == endPos) {
            NS_HTML5_BREAK(stateloop);
          }
          c = checkChar(buf, pos);
          MOZ_ASSERT(value >= 0, "value must not become negative.");
          if (c >= '0' && c <= '9') {
            seenDigits = true;
            if (value <= 0x10FFFF) {
              value *= 16;
              value += c - '0';
            }
            continue;
          } else if (c >= 'A' && c <= 'F') {
            seenDigits = true;
            if (value <= 0x10FFFF) {
              value *= 16;
              value += c - 'A' + 10;
            }
            continue;
          } else if (c >= 'a' && c <= 'f') {
            seenDigits = true;
            if (value <= 0x10FFFF) {
              value *= 16;
              value += c - 'a' + 10;
            }
            continue;
          } else if (c == ';') {
            if (seenDigits) {
              if (!(returnState & DATA_AND_RCDATA_MASK)) {
                cstart = pos + 1;
              }
              state = P::transition(mViewSource.get(),
                                    nsHtml5Tokenizer::HANDLE_NCR_VALUE,
                                    reconsume, pos);
              NS_HTML5_CONTINUE(stateloop);
            } else {
              if (P::reportErrors) {
                errNoDigitsInNCR();
              }
              appendCharRefBuf(';');
              emitOrAppendCharRefBuf(returnState);
              if (!(returnState & DATA_AND_RCDATA_MASK)) {
                cstart = pos + 1;
              }
              state =
                  P::transition(mViewSource.get(), returnState, reconsume, pos);
              NS_HTML5_CONTINUE(stateloop);
            }
          } else {
            if (!seenDigits) {
              if (P::reportErrors) {
                errNoDigitsInNCR();
              }
              emitOrAppendCharRefBuf(returnState);
              if (!(returnState & DATA_AND_RCDATA_MASK)) {
                cstart = pos;
              }
              reconsume = true;
              state =
                  P::transition(mViewSource.get(), returnState, reconsume, pos);
              NS_HTML5_CONTINUE(stateloop);
            } else {
              if (P::reportErrors) {
                errCharRefLacksSemicolon();
              }
              if (!(returnState & DATA_AND_RCDATA_MASK)) {
                cstart = pos;
              }
              reconsume = true;
              state = P::transition(mViewSource.get(),
                                    nsHtml5Tokenizer::HANDLE_NCR_VALUE,
                                    reconsume, pos);
              NS_HTML5_CONTINUE(stateloop);
            }
          }
        }
      }
      case PLAINTEXT: {
        for (;;) {
          if (reconsume) {
            reconsume = false;
          } else {
            if (++pos == endPos) {
              NS_HTML5_BREAK(stateloop);
            }
            c = checkChar(buf, pos);
          }
          switch (c) {
            case '\0': {
              emitPlaintextReplacementCharacter(buf, pos);
              continue;
            }
            case '\r': {
              emitCarriageReturn(buf, pos);
              NS_HTML5_BREAK(stateloop);
            }
            case '\n': {
              silentLineFeed();
              [[fallthrough]];
            }
            default: {
              continue;
            }
          }
        }
      }
      case CLOSE_TAG_OPEN: {
        if (++pos == endPos) {
          NS_HTML5_BREAK(stateloop);
        }
        c = checkChar(buf, pos);
        switch (c) {
          case '>': {
            if (P::reportErrors) {
              errLtSlashGt();
            }
            cstart = pos + 1;
            state = P::transition(mViewSource.get(), nsHtml5Tokenizer::DATA,
                                  reconsume, pos);
            NS_HTML5_CONTINUE(stateloop);
          }
          case '\r': {
            silentCarriageReturn();
            if (P::reportErrors) {
              errGarbageAfterLtSlash();
            }
            clearStrBufBeforeUse();
            appendStrBuf('\n');
            state =
                P::transition(mViewSource.get(),
                              nsHtml5Tokenizer::BOGUS_COMMENT, reconsume, pos);
            NS_HTML5_BREAK(stateloop);
          }
          case '\n': {
            silentLineFeed();
            if (P::reportErrors) {
              errGarbageAfterLtSlash();
            }
            clearStrBufBeforeUse();
            appendStrBuf(c);
            state =
                P::transition(mViewSource.get(),
                              nsHtml5Tokenizer::BOGUS_COMMENT, reconsume, pos);
            NS_HTML5_CONTINUE(stateloop);
          }
          case '\0': {
            c = 0xfffd;
            [[fallthrough]];
          }
          default: {
            if (c >= 'A' && c <= 'Z') {
              c += 0x20;
            }
            if (c >= 'a' && c <= 'z') {
              endTag = true;
              clearStrBufBeforeUse();
              appendStrBuf(c);
              containsHyphen = false;
              state = P::transition(mViewSource.get(),
                                    nsHtml5Tokenizer::TAG_NAME, reconsume, pos);
              NS_HTML5_CONTINUE(stateloop);
            } else {
              if (P::reportErrors) {
                errGarbageAfterLtSlash();
              }
              clearStrBufBeforeUse();
              appendStrBuf(c);
              state = P::transition(mViewSource.get(),
                                    nsHtml5Tokenizer::BOGUS_COMMENT, reconsume,
                                    pos);
              NS_HTML5_CONTINUE(stateloop);
            }
          }
        }
      }
      case RCDATA: {
        for (;;) {
          if (reconsume) {
            reconsume = false;
          } else {
            if (++pos == endPos) {
              NS_HTML5_BREAK(stateloop);
            }
            c = checkChar(buf, pos);
          }
          switch (c) {
            case '&': {
              flushChars(buf, pos);
              MOZ_ASSERT(!charRefBufLen,
                         "charRefBufLen not reset after previous use!");
              appendCharRefBuf(c);
              setAdditionalAndRememberAmpersandLocation('\0');
              returnState = state;
              state =
                  P::transition(mViewSource.get(),
                                nsHtml5Tokenizer::CONSUME_CHARACTER_REFERENCE,
                                reconsume, pos);
              NS_HTML5_CONTINUE(stateloop);
            }
            case '<': {
              flushChars(buf, pos);
              returnState = state;
              state =
                  P::transition(mViewSource.get(),
                                nsHtml5Tokenizer::RAWTEXT_RCDATA_LESS_THAN_SIGN,
                                reconsume, pos);
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
              [[fallthrough]];
            }
            default: {
              continue;
            }
          }
        }
      }
      case RAWTEXT: {
        for (;;) {
          if (reconsume) {
            reconsume = false;
          } else {
            if (++pos == endPos) {
              NS_HTML5_BREAK(stateloop);
            }
            c = checkChar(buf, pos);
          }
          switch (c) {
            case '<': {
              flushChars(buf, pos);
              returnState = state;
              state =
                  P::transition(mViewSource.get(),
                                nsHtml5Tokenizer::RAWTEXT_RCDATA_LESS_THAN_SIGN,
                                reconsume, pos);
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
              [[fallthrough]];
            }
            default: {
              continue;
            }
          }
        }
      rawtextloop_end:;
        [[fallthrough]];
      }
      case RAWTEXT_RCDATA_LESS_THAN_SIGN: {
        for (;;) {
          if (++pos == endPos) {
            NS_HTML5_BREAK(stateloop);
          }
          c = checkChar(buf, pos);
          switch (c) {
            case '/': {
              index = 0;
              clearStrBufBeforeUse();
              state = P::transition(mViewSource.get(),
                                    nsHtml5Tokenizer::NON_DATA_END_TAG_NAME,
                                    reconsume, pos);
              NS_HTML5_BREAK(rawtextrcdatalessthansignloop);
            }
            default: {
              tokenHandler->characters(nsHtml5Tokenizer::LT_GT, 0, 1);
              cstart = pos;
              reconsume = true;
              state =
                  P::transition(mViewSource.get(), returnState, reconsume, pos);
              NS_HTML5_CONTINUE(stateloop);
            }
          }
        }
      rawtextrcdatalessthansignloop_end:;
        [[fallthrough]];
      }
      case NON_DATA_END_TAG_NAME: {
        for (;;) {
          if (++pos == endPos) {
            NS_HTML5_BREAK(stateloop);
          }
          c = checkChar(buf, pos);
          if (!endTagExpectationAsArray) {
            tokenHandler->characters(nsHtml5Tokenizer::LT_SOLIDUS, 0, 2);
            cstart = pos;
            reconsume = true;
            state =
                P::transition(mViewSource.get(), returnState, reconsume, pos);
            NS_HTML5_CONTINUE(stateloop);
          } else if (index < endTagExpectationAsArray.length) {
            char16_t e = endTagExpectationAsArray[index];
            char16_t folded = c;
            if (c >= 'A' && c <= 'Z') {
              folded += 0x20;
            }
            if (folded != e) {
              tokenHandler->characters(nsHtml5Tokenizer::LT_SOLIDUS, 0, 2);
              emitStrBuf();
              cstart = pos;
              reconsume = true;
              state =
                  P::transition(mViewSource.get(), returnState, reconsume, pos);
              NS_HTML5_CONTINUE(stateloop);
            }
            appendStrBuf(c);
            index++;
            continue;
          } else {
            endTag = true;
            tagName = endTagExpectation;
            switch (c) {
              case '\r': {
                silentCarriageReturn();
                clearStrBufAfterUse();
                state = P::transition(mViewSource.get(),
                                      nsHtml5Tokenizer::BEFORE_ATTRIBUTE_NAME,
                                      reconsume, pos);
                NS_HTML5_BREAK(stateloop);
              }
              case '\n': {
                silentLineFeed();
                [[fallthrough]];
              }
              case ' ':
              case '\t':
              case '\f': {
                clearStrBufAfterUse();
                state = P::transition(mViewSource.get(),
                                      nsHtml5Tokenizer::BEFORE_ATTRIBUTE_NAME,
                                      reconsume, pos);
                NS_HTML5_CONTINUE(stateloop);
              }
              case '/': {
                clearStrBufAfterUse();
                state = P::transition(mViewSource.get(),
                                      nsHtml5Tokenizer::SELF_CLOSING_START_TAG,
                                      reconsume, pos);
                NS_HTML5_CONTINUE(stateloop);
              }
              case '>': {
                clearStrBufAfterUse();
                state = P::transition(mViewSource.get(),
                                      emitCurrentTagToken(false, pos),
                                      reconsume, pos);
                if (shouldSuspend) {
                  NS_HTML5_BREAK(stateloop);
                }
                NS_HTML5_CONTINUE(stateloop);
              }
              default: {
                tokenHandler->characters(nsHtml5Tokenizer::LT_SOLIDUS, 0, 2);
                emitStrBuf();
                cstart = pos;
                reconsume = true;
                state = P::transition(mViewSource.get(), returnState, reconsume,
                                      pos);
                NS_HTML5_CONTINUE(stateloop);
              }
            }
          }
        }
      }
      case BOGUS_COMMENT: {
        for (;;) {
          if (reconsume) {
            reconsume = false;
          } else {
            if (++pos == endPos) {
              NS_HTML5_BREAK(stateloop);
            }
            c = checkChar(buf, pos);
          }
          switch (c) {
            case '>': {
              emitComment(0, pos);
              state = P::transition(mViewSource.get(), nsHtml5Tokenizer::DATA,
                                    reconsume, pos);
              NS_HTML5_CONTINUE(stateloop);
            }
            case '-': {
              appendStrBuf(c);
              state = P::transition(mViewSource.get(),
                                    nsHtml5Tokenizer::BOGUS_COMMENT_HYPHEN,
                                    reconsume, pos);
              NS_HTML5_BREAK(boguscommentloop);
            }
            case '\r': {
              appendStrBufCarriageReturn();
              NS_HTML5_BREAK(stateloop);
            }
            case '\n': {
              appendStrBufLineFeed();
              continue;
            }
            case '\0': {
              c = 0xfffd;
              [[fallthrough]];
            }
            default: {
              appendStrBuf(c);
              continue;
            }
          }
        }
      boguscommentloop_end:;
        [[fallthrough]];
      }
      case BOGUS_COMMENT_HYPHEN: {
      boguscommenthyphenloop:
        for (;;) {
          if (++pos == endPos) {
            NS_HTML5_BREAK(stateloop);
          }
          c = checkChar(buf, pos);
          switch (c) {
            case '>': {
              emitComment(0, pos);
              state = P::transition(mViewSource.get(), nsHtml5Tokenizer::DATA,
                                    reconsume, pos);
              NS_HTML5_CONTINUE(stateloop);
            }
            case '-': {
              appendSecondHyphenToBogusComment();
              NS_HTML5_CONTINUE(boguscommenthyphenloop);
            }
            case '\r': {
              appendStrBufCarriageReturn();
              state = P::transition(mViewSource.get(),
                                    nsHtml5Tokenizer::BOGUS_COMMENT, reconsume,
                                    pos);
              NS_HTML5_BREAK(stateloop);
            }
            case '\n': {
              appendStrBufLineFeed();
              state = P::transition(mViewSource.get(),
                                    nsHtml5Tokenizer::BOGUS_COMMENT, reconsume,
                                    pos);
              NS_HTML5_CONTINUE(stateloop);
            }
            case '\0': {
              c = 0xfffd;
              [[fallthrough]];
            }
            default: {
              appendStrBuf(c);
              state = P::transition(mViewSource.get(),
                                    nsHtml5Tokenizer::BOGUS_COMMENT, reconsume,
                                    pos);
              NS_HTML5_CONTINUE(stateloop);
            }
          }
        }
      }
      case SCRIPT_DATA: {
        for (;;) {
          if (reconsume) {
            reconsume = false;
          } else {
            if (++pos == endPos) {
              NS_HTML5_BREAK(stateloop);
            }
            c = checkChar(buf, pos);
          }
          switch (c) {
            case '<': {
              flushChars(buf, pos);
              returnState = state;
              state = P::transition(
                  mViewSource.get(),
                  nsHtml5Tokenizer::SCRIPT_DATA_LESS_THAN_SIGN, reconsume, pos);
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
              [[fallthrough]];
            }
            default: {
              continue;
            }
          }
        }
      scriptdataloop_end:;
        [[fallthrough]];
      }
      case SCRIPT_DATA_LESS_THAN_SIGN: {
        for (;;) {
          if (++pos == endPos) {
            NS_HTML5_BREAK(stateloop);
          }
          c = checkChar(buf, pos);
          switch (c) {
            case '/': {
              index = 0;
              clearStrBufBeforeUse();
              state = P::transition(mViewSource.get(),
                                    nsHtml5Tokenizer::NON_DATA_END_TAG_NAME,
                                    reconsume, pos);
              NS_HTML5_CONTINUE(stateloop);
            }
            case '!': {
              tokenHandler->characters(nsHtml5Tokenizer::LT_GT, 0, 1);
              cstart = pos;
              state = P::transition(mViewSource.get(),
                                    nsHtml5Tokenizer::SCRIPT_DATA_ESCAPE_START,
                                    reconsume, pos);
              NS_HTML5_BREAK(scriptdatalessthansignloop);
            }
            default: {
              tokenHandler->characters(nsHtml5Tokenizer::LT_GT, 0, 1);
              cstart = pos;
              reconsume = true;
              state =
                  P::transition(mViewSource.get(),
                                nsHtml5Tokenizer::SCRIPT_DATA, reconsume, pos);
              NS_HTML5_CONTINUE(stateloop);
            }
          }
        }
      scriptdatalessthansignloop_end:;
        [[fallthrough]];
      }
      case SCRIPT_DATA_ESCAPE_START: {
        for (;;) {
          if (++pos == endPos) {
            NS_HTML5_BREAK(stateloop);
          }
          c = checkChar(buf, pos);
          switch (c) {
            case '-': {
              state =
                  P::transition(mViewSource.get(),
                                nsHtml5Tokenizer::SCRIPT_DATA_ESCAPE_START_DASH,
                                reconsume, pos);
              NS_HTML5_BREAK(scriptdataescapestartloop);
            }
            default: {
              reconsume = true;
              state =
                  P::transition(mViewSource.get(),
                                nsHtml5Tokenizer::SCRIPT_DATA, reconsume, pos);
              NS_HTML5_CONTINUE(stateloop);
            }
          }
        }
      scriptdataescapestartloop_end:;
        [[fallthrough]];
      }
      case SCRIPT_DATA_ESCAPE_START_DASH: {
        for (;;) {
          if (++pos == endPos) {
            NS_HTML5_BREAK(stateloop);
          }
          c = checkChar(buf, pos);
          switch (c) {
            case '-': {
              state =
                  P::transition(mViewSource.get(),
                                nsHtml5Tokenizer::SCRIPT_DATA_ESCAPED_DASH_DASH,
                                reconsume, pos);
              NS_HTML5_BREAK(scriptdataescapestartdashloop);
            }
            default: {
              reconsume = true;
              state =
                  P::transition(mViewSource.get(),
                                nsHtml5Tokenizer::SCRIPT_DATA, reconsume, pos);
              NS_HTML5_CONTINUE(stateloop);
            }
          }
        }
      scriptdataescapestartdashloop_end:;
        [[fallthrough]];
      }
      case SCRIPT_DATA_ESCAPED_DASH_DASH: {
        for (;;) {
          if (++pos == endPos) {
            NS_HTML5_BREAK(stateloop);
          }
          c = checkChar(buf, pos);
          switch (c) {
            case '-': {
              continue;
            }
            case '<': {
              flushChars(buf, pos);
              state = P::transition(
                  mViewSource.get(),
                  nsHtml5Tokenizer::SCRIPT_DATA_ESCAPED_LESS_THAN_SIGN,
                  reconsume, pos);
              NS_HTML5_CONTINUE(stateloop);
            }
            case '>': {
              state =
                  P::transition(mViewSource.get(),
                                nsHtml5Tokenizer::SCRIPT_DATA, reconsume, pos);
              NS_HTML5_CONTINUE(stateloop);
            }
            case '\0': {
              emitReplacementCharacter(buf, pos);
              state = P::transition(mViewSource.get(),
                                    nsHtml5Tokenizer::SCRIPT_DATA_ESCAPED,
                                    reconsume, pos);
              NS_HTML5_BREAK(scriptdataescapeddashdashloop);
            }
            case '\r': {
              emitCarriageReturn(buf, pos);
              state = P::transition(mViewSource.get(),
                                    nsHtml5Tokenizer::SCRIPT_DATA_ESCAPED,
                                    reconsume, pos);
              NS_HTML5_BREAK(stateloop);
            }
            case '\n': {
              silentLineFeed();
              [[fallthrough]];
            }
            default: {
              state = P::transition(mViewSource.get(),
                                    nsHtml5Tokenizer::SCRIPT_DATA_ESCAPED,
                                    reconsume, pos);
              NS_HTML5_BREAK(scriptdataescapeddashdashloop);
            }
          }
        }
      scriptdataescapeddashdashloop_end:;
        [[fallthrough]];
      }
      case SCRIPT_DATA_ESCAPED: {
        for (;;) {
          if (reconsume) {
            reconsume = false;
          } else {
            if (++pos == endPos) {
              NS_HTML5_BREAK(stateloop);
            }
            c = checkChar(buf, pos);
          }
          switch (c) {
            case '-': {
              state = P::transition(mViewSource.get(),
                                    nsHtml5Tokenizer::SCRIPT_DATA_ESCAPED_DASH,
                                    reconsume, pos);
              NS_HTML5_BREAK(scriptdataescapedloop);
            }
            case '<': {
              flushChars(buf, pos);
              state = P::transition(
                  mViewSource.get(),
                  nsHtml5Tokenizer::SCRIPT_DATA_ESCAPED_LESS_THAN_SIGN,
                  reconsume, pos);
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
              [[fallthrough]];
            }
            default: {
              continue;
            }
          }
        }
      scriptdataescapedloop_end:;
        [[fallthrough]];
      }
      case SCRIPT_DATA_ESCAPED_DASH: {
        for (;;) {
          if (++pos == endPos) {
            NS_HTML5_BREAK(stateloop);
          }
          c = checkChar(buf, pos);
          switch (c) {
            case '-': {
              state =
                  P::transition(mViewSource.get(),
                                nsHtml5Tokenizer::SCRIPT_DATA_ESCAPED_DASH_DASH,
                                reconsume, pos);
              NS_HTML5_CONTINUE(stateloop);
            }
            case '<': {
              flushChars(buf, pos);
              state = P::transition(
                  mViewSource.get(),
                  nsHtml5Tokenizer::SCRIPT_DATA_ESCAPED_LESS_THAN_SIGN,
                  reconsume, pos);
              NS_HTML5_BREAK(scriptdataescapeddashloop);
            }
            case '\0': {
              emitReplacementCharacter(buf, pos);
              state = P::transition(mViewSource.get(),
                                    nsHtml5Tokenizer::SCRIPT_DATA_ESCAPED,
                                    reconsume, pos);
              NS_HTML5_CONTINUE(stateloop);
            }
            case '\r': {
              emitCarriageReturn(buf, pos);
              state = P::transition(mViewSource.get(),
                                    nsHtml5Tokenizer::SCRIPT_DATA_ESCAPED,
                                    reconsume, pos);
              NS_HTML5_BREAK(stateloop);
            }
            case '\n': {
              silentLineFeed();
              [[fallthrough]];
            }
            default: {
              state = P::transition(mViewSource.get(),
                                    nsHtml5Tokenizer::SCRIPT_DATA_ESCAPED,
                                    reconsume, pos);
              NS_HTML5_CONTINUE(stateloop);
            }
          }
        }
      scriptdataescapeddashloop_end:;
        [[fallthrough]];
      }
      case SCRIPT_DATA_ESCAPED_LESS_THAN_SIGN: {
        for (;;) {
          if (++pos == endPos) {
            NS_HTML5_BREAK(stateloop);
          }
          c = checkChar(buf, pos);
          switch (c) {
            case '/': {
              index = 0;
              clearStrBufBeforeUse();
              returnState = nsHtml5Tokenizer::SCRIPT_DATA_ESCAPED;
              state = P::transition(mViewSource.get(),
                                    nsHtml5Tokenizer::NON_DATA_END_TAG_NAME,
                                    reconsume, pos);
              NS_HTML5_CONTINUE(stateloop);
            }
            case 'S':
            case 's': {
              tokenHandler->characters(nsHtml5Tokenizer::LT_GT, 0, 1);
              cstart = pos;
              index = 1;
              state = P::transition(
                  mViewSource.get(),
                  nsHtml5Tokenizer::SCRIPT_DATA_DOUBLE_ESCAPE_START, reconsume,
                  pos);
              NS_HTML5_BREAK(scriptdataescapedlessthanloop);
            }
            default: {
              tokenHandler->characters(nsHtml5Tokenizer::LT_GT, 0, 1);
              cstart = pos;
              reconsume = true;
              state = P::transition(mViewSource.get(),
                                    nsHtml5Tokenizer::SCRIPT_DATA_ESCAPED,
                                    reconsume, pos);
              NS_HTML5_CONTINUE(stateloop);
            }
          }
        }
      scriptdataescapedlessthanloop_end:;
        [[fallthrough]];
      }
      case SCRIPT_DATA_DOUBLE_ESCAPE_START: {
        for (;;) {
          if (++pos == endPos) {
            NS_HTML5_BREAK(stateloop);
          }
          c = checkChar(buf, pos);
          MOZ_ASSERT(index > 0);
          if (index < 6) {
            char16_t folded = c;
            if (c >= 'A' && c <= 'Z') {
              folded += 0x20;
            }
            if (folded != nsHtml5Tokenizer::SCRIPT_ARR[index]) {
              reconsume = true;
              state = P::transition(mViewSource.get(),
                                    nsHtml5Tokenizer::SCRIPT_DATA_ESCAPED,
                                    reconsume, pos);
              NS_HTML5_CONTINUE(stateloop);
            }
            index++;
            continue;
          }
          switch (c) {
            case '\r': {
              emitCarriageReturn(buf, pos);
              state = P::transition(
                  mViewSource.get(),
                  nsHtml5Tokenizer::SCRIPT_DATA_DOUBLE_ESCAPED, reconsume, pos);
              NS_HTML5_BREAK(stateloop);
            }
            case '\n': {
              silentLineFeed();
              [[fallthrough]];
            }
            case ' ':
            case '\t':
            case '\f':
            case '/':
            case '>': {
              state = P::transition(
                  mViewSource.get(),
                  nsHtml5Tokenizer::SCRIPT_DATA_DOUBLE_ESCAPED, reconsume, pos);
              NS_HTML5_BREAK(scriptdatadoubleescapestartloop);
            }
            default: {
              reconsume = true;
              state = P::transition(mViewSource.get(),
                                    nsHtml5Tokenizer::SCRIPT_DATA_ESCAPED,
                                    reconsume, pos);
              NS_HTML5_CONTINUE(stateloop);
            }
          }
        }
      scriptdatadoubleescapestartloop_end:;
        [[fallthrough]];
      }
      case SCRIPT_DATA_DOUBLE_ESCAPED: {
        for (;;) {
          if (reconsume) {
            reconsume = false;
          } else {
            if (++pos == endPos) {
              NS_HTML5_BREAK(stateloop);
            }
            c = checkChar(buf, pos);
          }
          switch (c) {
            case '-': {
              state = P::transition(
                  mViewSource.get(),
                  nsHtml5Tokenizer::SCRIPT_DATA_DOUBLE_ESCAPED_DASH, reconsume,
                  pos);
              NS_HTML5_BREAK(scriptdatadoubleescapedloop);
            }
            case '<': {
              state = P::transition(
                  mViewSource.get(),
                  nsHtml5Tokenizer::SCRIPT_DATA_DOUBLE_ESCAPED_LESS_THAN_SIGN,
                  reconsume, pos);
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
              [[fallthrough]];
            }
            default: {
              continue;
            }
          }
        }
      scriptdatadoubleescapedloop_end:;
        [[fallthrough]];
      }
      case SCRIPT_DATA_DOUBLE_ESCAPED_DASH: {
        for (;;) {
          if (++pos == endPos) {
            NS_HTML5_BREAK(stateloop);
          }
          c = checkChar(buf, pos);
          switch (c) {
            case '-': {
              state = P::transition(
                  mViewSource.get(),
                  nsHtml5Tokenizer::SCRIPT_DATA_DOUBLE_ESCAPED_DASH_DASH,
                  reconsume, pos);
              NS_HTML5_BREAK(scriptdatadoubleescapeddashloop);
            }
            case '<': {
              state = P::transition(
                  mViewSource.get(),
                  nsHtml5Tokenizer::SCRIPT_DATA_DOUBLE_ESCAPED_LESS_THAN_SIGN,
                  reconsume, pos);
              NS_HTML5_CONTINUE(stateloop);
            }
            case '\0': {
              emitReplacementCharacter(buf, pos);
              state = P::transition(
                  mViewSource.get(),
                  nsHtml5Tokenizer::SCRIPT_DATA_DOUBLE_ESCAPED, reconsume, pos);
              NS_HTML5_CONTINUE(stateloop);
            }
            case '\r': {
              emitCarriageReturn(buf, pos);
              state = P::transition(
                  mViewSource.get(),
                  nsHtml5Tokenizer::SCRIPT_DATA_DOUBLE_ESCAPED, reconsume, pos);
              NS_HTML5_BREAK(stateloop);
            }
            case '\n': {
              silentLineFeed();
              [[fallthrough]];
            }
            default: {
              state = P::transition(
                  mViewSource.get(),
                  nsHtml5Tokenizer::SCRIPT_DATA_DOUBLE_ESCAPED, reconsume, pos);
              NS_HTML5_CONTINUE(stateloop);
            }
          }
        }
      scriptdatadoubleescapeddashloop_end:;
        [[fallthrough]];
      }
      case SCRIPT_DATA_DOUBLE_ESCAPED_DASH_DASH: {
        for (;;) {
          if (++pos == endPos) {
            NS_HTML5_BREAK(stateloop);
          }
          c = checkChar(buf, pos);
          switch (c) {
            case '-': {
              continue;
            }
            case '<': {
              state = P::transition(
                  mViewSource.get(),
                  nsHtml5Tokenizer::SCRIPT_DATA_DOUBLE_ESCAPED_LESS_THAN_SIGN,
                  reconsume, pos);
              NS_HTML5_BREAK(scriptdatadoubleescapeddashdashloop);
            }
            case '>': {
              state =
                  P::transition(mViewSource.get(),
                                nsHtml5Tokenizer::SCRIPT_DATA, reconsume, pos);
              NS_HTML5_CONTINUE(stateloop);
            }
            case '\0': {
              emitReplacementCharacter(buf, pos);
              state = P::transition(
                  mViewSource.get(),
                  nsHtml5Tokenizer::SCRIPT_DATA_DOUBLE_ESCAPED, reconsume, pos);
              NS_HTML5_CONTINUE(stateloop);
            }
            case '\r': {
              emitCarriageReturn(buf, pos);
              state = P::transition(
                  mViewSource.get(),
                  nsHtml5Tokenizer::SCRIPT_DATA_DOUBLE_ESCAPED, reconsume, pos);
              NS_HTML5_BREAK(stateloop);
            }
            case '\n': {
              silentLineFeed();
              [[fallthrough]];
            }
            default: {
              state = P::transition(
                  mViewSource.get(),
                  nsHtml5Tokenizer::SCRIPT_DATA_DOUBLE_ESCAPED, reconsume, pos);
              NS_HTML5_CONTINUE(stateloop);
            }
          }
        }
      scriptdatadoubleescapeddashdashloop_end:;
        [[fallthrough]];
      }
      case SCRIPT_DATA_DOUBLE_ESCAPED_LESS_THAN_SIGN: {
        for (;;) {
          if (++pos == endPos) {
            NS_HTML5_BREAK(stateloop);
          }
          c = checkChar(buf, pos);
          switch (c) {
            case '/': {
              index = 0;
              state =
                  P::transition(mViewSource.get(),
                                nsHtml5Tokenizer::SCRIPT_DATA_DOUBLE_ESCAPE_END,
                                reconsume, pos);
              NS_HTML5_BREAK(scriptdatadoubleescapedlessthanloop);
            }
            default: {
              reconsume = true;
              state = P::transition(
                  mViewSource.get(),
                  nsHtml5Tokenizer::SCRIPT_DATA_DOUBLE_ESCAPED, reconsume, pos);
              NS_HTML5_CONTINUE(stateloop);
            }
          }
        }
      scriptdatadoubleescapedlessthanloop_end:;
        [[fallthrough]];
      }
      case SCRIPT_DATA_DOUBLE_ESCAPE_END: {
        for (;;) {
          if (++pos == endPos) {
            NS_HTML5_BREAK(stateloop);
          }
          c = checkChar(buf, pos);
          if (index < 6) {
            char16_t folded = c;
            if (c >= 'A' && c <= 'Z') {
              folded += 0x20;
            }
            if (folded != nsHtml5Tokenizer::SCRIPT_ARR[index]) {
              reconsume = true;
              state = P::transition(
                  mViewSource.get(),
                  nsHtml5Tokenizer::SCRIPT_DATA_DOUBLE_ESCAPED, reconsume, pos);
              NS_HTML5_CONTINUE(stateloop);
            }
            index++;
            continue;
          }
          switch (c) {
            case '\r': {
              emitCarriageReturn(buf, pos);
              state = P::transition(mViewSource.get(),
                                    nsHtml5Tokenizer::SCRIPT_DATA_ESCAPED,
                                    reconsume, pos);
              NS_HTML5_BREAK(stateloop);
            }
            case '\n': {
              silentLineFeed();
              [[fallthrough]];
            }
            case ' ':
            case '\t':
            case '\f':
            case '/':
            case '>': {
              state = P::transition(mViewSource.get(),
                                    nsHtml5Tokenizer::SCRIPT_DATA_ESCAPED,
                                    reconsume, pos);
              NS_HTML5_CONTINUE(stateloop);
            }
            default: {
              reconsume = true;
              state = P::transition(
                  mViewSource.get(),
                  nsHtml5Tokenizer::SCRIPT_DATA_DOUBLE_ESCAPED, reconsume, pos);
              NS_HTML5_CONTINUE(stateloop);
            }
          }
        }
      }
      case MARKUP_DECLARATION_OCTYPE: {
        for (;;) {
          if (++pos == endPos) {
            NS_HTML5_BREAK(stateloop);
          }
          c = checkChar(buf, pos);
          if (index < 6) {
            char16_t folded = c;
            if (c >= 'A' && c <= 'Z') {
              folded += 0x20;
            }
            if (folded == nsHtml5Tokenizer::OCTYPE[index]) {
              appendStrBuf(c);
            } else {
              if (P::reportErrors) {
                errBogusComment();
              }
              reconsume = true;
              state = P::transition(mViewSource.get(),
                                    nsHtml5Tokenizer::BOGUS_COMMENT, reconsume,
                                    pos);
              NS_HTML5_CONTINUE(stateloop);
            }
            index++;
            continue;
          } else {
            reconsume = true;
            state = P::transition(mViewSource.get(), nsHtml5Tokenizer::DOCTYPE,
                                  reconsume, pos);
            NS_HTML5_BREAK(markupdeclarationdoctypeloop);
          }
        }
      markupdeclarationdoctypeloop_end:;
        [[fallthrough]];
      }
      case DOCTYPE: {
        for (;;) {
          if (reconsume) {
            reconsume = false;
          } else {
            if (++pos == endPos) {
              NS_HTML5_BREAK(stateloop);
            }
            c = checkChar(buf, pos);
          }
          initDoctypeFields();
          switch (c) {
            case '\r': {
              silentCarriageReturn();
              state = P::transition(mViewSource.get(),
                                    nsHtml5Tokenizer::BEFORE_DOCTYPE_NAME,
                                    reconsume, pos);
              NS_HTML5_BREAK(stateloop);
            }
            case '\n': {
              silentLineFeed();
              [[fallthrough]];
            }
            case ' ':
            case '\t':
            case '\f': {
              state = P::transition(mViewSource.get(),
                                    nsHtml5Tokenizer::BEFORE_DOCTYPE_NAME,
                                    reconsume, pos);
              NS_HTML5_BREAK(doctypeloop);
            }
            default: {
              if (P::reportErrors) {
                errMissingSpaceBeforeDoctypeName();
              }
              reconsume = true;
              state = P::transition(mViewSource.get(),
                                    nsHtml5Tokenizer::BEFORE_DOCTYPE_NAME,
                                    reconsume, pos);
              NS_HTML5_BREAK(doctypeloop);
            }
          }
        }
      doctypeloop_end:;
        [[fallthrough]];
      }
      case BEFORE_DOCTYPE_NAME: {
        for (;;) {
          if (reconsume) {
            reconsume = false;
          } else {
            if (++pos == endPos) {
              NS_HTML5_BREAK(stateloop);
            }
            c = checkChar(buf, pos);
          }
          switch (c) {
            case '\r': {
              silentCarriageReturn();
              NS_HTML5_BREAK(stateloop);
            }
            case '\n': {
              silentLineFeed();
              [[fallthrough]];
            }
            case ' ':
            case '\t':
            case '\f': {
              continue;
            }
            case '>': {
              if (P::reportErrors) {
                errNamelessDoctype();
              }
              forceQuirks = true;
              emitDoctypeToken(pos);
              state = P::transition(mViewSource.get(), nsHtml5Tokenizer::DATA,
                                    reconsume, pos);
              NS_HTML5_CONTINUE(stateloop);
            }
            case '\0': {
              c = 0xfffd;
              [[fallthrough]];
            }
            default: {
              if (c >= 'A' && c <= 'Z') {
                c += 0x20;
              }
              clearStrBufBeforeUse();
              appendStrBuf(c);
              state =
                  P::transition(mViewSource.get(),
                                nsHtml5Tokenizer::DOCTYPE_NAME, reconsume, pos);
              NS_HTML5_BREAK(beforedoctypenameloop);
            }
          }
        }
      beforedoctypenameloop_end:;
        [[fallthrough]];
      }
      case DOCTYPE_NAME: {
        for (;;) {
          if (++pos == endPos) {
            NS_HTML5_BREAK(stateloop);
          }
          c = checkChar(buf, pos);
          switch (c) {
            case '\r': {
              silentCarriageReturn();
              strBufToDoctypeName();
              state = P::transition(mViewSource.get(),
                                    nsHtml5Tokenizer::AFTER_DOCTYPE_NAME,
                                    reconsume, pos);
              NS_HTML5_BREAK(stateloop);
            }
            case '\n': {
              silentLineFeed();
              [[fallthrough]];
            }
            case ' ':
            case '\t':
            case '\f': {
              strBufToDoctypeName();
              state = P::transition(mViewSource.get(),
                                    nsHtml5Tokenizer::AFTER_DOCTYPE_NAME,
                                    reconsume, pos);
              NS_HTML5_BREAK(doctypenameloop);
            }
            case '>': {
              strBufToDoctypeName();
              emitDoctypeToken(pos);
              state = P::transition(mViewSource.get(), nsHtml5Tokenizer::DATA,
                                    reconsume, pos);
              NS_HTML5_CONTINUE(stateloop);
            }
            case '\0': {
              c = 0xfffd;
              [[fallthrough]];
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
      doctypenameloop_end:;
        [[fallthrough]];
      }
      case AFTER_DOCTYPE_NAME: {
        for (;;) {
          if (++pos == endPos) {
            NS_HTML5_BREAK(stateloop);
          }
          c = checkChar(buf, pos);
          switch (c) {
            case '\r': {
              silentCarriageReturn();
              NS_HTML5_BREAK(stateloop);
            }
            case '\n': {
              silentLineFeed();
              [[fallthrough]];
            }
            case ' ':
            case '\t':
            case '\f': {
              continue;
            }
            case '>': {
              emitDoctypeToken(pos);
              state = P::transition(mViewSource.get(), nsHtml5Tokenizer::DATA,
                                    reconsume, pos);
              NS_HTML5_CONTINUE(stateloop);
            }
            case 'p':
            case 'P': {
              index = 0;
              state = P::transition(mViewSource.get(),
                                    nsHtml5Tokenizer::DOCTYPE_UBLIC, reconsume,
                                    pos);
              NS_HTML5_BREAK(afterdoctypenameloop);
            }
            case 's':
            case 'S': {
              index = 0;
              state = P::transition(mViewSource.get(),
                                    nsHtml5Tokenizer::DOCTYPE_YSTEM, reconsume,
                                    pos);
              NS_HTML5_CONTINUE(stateloop);
            }
            default: {
              bogusDoctype();
              state = P::transition(mViewSource.get(),
                                    nsHtml5Tokenizer::BOGUS_DOCTYPE, reconsume,
                                    pos);
              NS_HTML5_CONTINUE(stateloop);
            }
          }
        }
      afterdoctypenameloop_end:;
        [[fallthrough]];
      }
      case DOCTYPE_UBLIC: {
        for (;;) {
          if (++pos == endPos) {
            NS_HTML5_BREAK(stateloop);
          }
          c = checkChar(buf, pos);
          if (index < 5) {
            char16_t folded = c;
            if (c >= 'A' && c <= 'Z') {
              folded += 0x20;
            }
            if (folded != nsHtml5Tokenizer::UBLIC[index]) {
              bogusDoctype();
              reconsume = true;
              state = P::transition(mViewSource.get(),
                                    nsHtml5Tokenizer::BOGUS_DOCTYPE, reconsume,
                                    pos);
              NS_HTML5_CONTINUE(stateloop);
            }
            index++;
            continue;
          } else {
            reconsume = true;
            state = P::transition(
                mViewSource.get(),
                nsHtml5Tokenizer::AFTER_DOCTYPE_PUBLIC_KEYWORD, reconsume, pos);
            NS_HTML5_BREAK(doctypeublicloop);
          }
        }
      doctypeublicloop_end:;
        [[fallthrough]];
      }
      case AFTER_DOCTYPE_PUBLIC_KEYWORD: {
        for (;;) {
          if (reconsume) {
            reconsume = false;
          } else {
            if (++pos == endPos) {
              NS_HTML5_BREAK(stateloop);
            }
            c = checkChar(buf, pos);
          }
          switch (c) {
            case '\r': {
              silentCarriageReturn();
              state = P::transition(
                  mViewSource.get(),
                  nsHtml5Tokenizer::BEFORE_DOCTYPE_PUBLIC_IDENTIFIER, reconsume,
                  pos);
              NS_HTML5_BREAK(stateloop);
            }
            case '\n': {
              silentLineFeed();
              [[fallthrough]];
            }
            case ' ':
            case '\t':
            case '\f': {
              state = P::transition(
                  mViewSource.get(),
                  nsHtml5Tokenizer::BEFORE_DOCTYPE_PUBLIC_IDENTIFIER, reconsume,
                  pos);
              NS_HTML5_BREAK(afterdoctypepublickeywordloop);
            }
            case '\"': {
              if (P::reportErrors) {
                errNoSpaceBetweenDoctypePublicKeywordAndQuote();
              }
              clearStrBufBeforeUse();
              state = P::transition(
                  mViewSource.get(),
                  nsHtml5Tokenizer::DOCTYPE_PUBLIC_IDENTIFIER_DOUBLE_QUOTED,
                  reconsume, pos);
              NS_HTML5_CONTINUE(stateloop);
            }
            case '\'': {
              if (P::reportErrors) {
                errNoSpaceBetweenDoctypePublicKeywordAndQuote();
              }
              clearStrBufBeforeUse();
              state = P::transition(
                  mViewSource.get(),
                  nsHtml5Tokenizer::DOCTYPE_PUBLIC_IDENTIFIER_SINGLE_QUOTED,
                  reconsume, pos);
              NS_HTML5_CONTINUE(stateloop);
            }
            case '>': {
              if (P::reportErrors) {
                errExpectedPublicId();
              }
              forceQuirks = true;
              emitDoctypeToken(pos);
              state = P::transition(mViewSource.get(), nsHtml5Tokenizer::DATA,
                                    reconsume, pos);
              NS_HTML5_CONTINUE(stateloop);
            }
            default: {
              bogusDoctype();
              state = P::transition(mViewSource.get(),
                                    nsHtml5Tokenizer::BOGUS_DOCTYPE, reconsume,
                                    pos);
              NS_HTML5_CONTINUE(stateloop);
            }
          }
        }
      afterdoctypepublickeywordloop_end:;
        [[fallthrough]];
      }
      case BEFORE_DOCTYPE_PUBLIC_IDENTIFIER: {
        for (;;) {
          if (++pos == endPos) {
            NS_HTML5_BREAK(stateloop);
          }
          c = checkChar(buf, pos);
          switch (c) {
            case '\r': {
              silentCarriageReturn();
              NS_HTML5_BREAK(stateloop);
            }
            case '\n': {
              silentLineFeed();
              [[fallthrough]];
            }
            case ' ':
            case '\t':
            case '\f': {
              continue;
            }
            case '\"': {
              clearStrBufBeforeUse();
              state = P::transition(
                  mViewSource.get(),
                  nsHtml5Tokenizer::DOCTYPE_PUBLIC_IDENTIFIER_DOUBLE_QUOTED,
                  reconsume, pos);
              NS_HTML5_BREAK(beforedoctypepublicidentifierloop);
            }
            case '\'': {
              clearStrBufBeforeUse();
              state = P::transition(
                  mViewSource.get(),
                  nsHtml5Tokenizer::DOCTYPE_PUBLIC_IDENTIFIER_SINGLE_QUOTED,
                  reconsume, pos);
              NS_HTML5_CONTINUE(stateloop);
            }
            case '>': {
              if (P::reportErrors) {
                errExpectedPublicId();
              }
              forceQuirks = true;
              emitDoctypeToken(pos);
              state = P::transition(mViewSource.get(), nsHtml5Tokenizer::DATA,
                                    reconsume, pos);
              NS_HTML5_CONTINUE(stateloop);
            }
            default: {
              bogusDoctype();
              state = P::transition(mViewSource.get(),
                                    nsHtml5Tokenizer::BOGUS_DOCTYPE, reconsume,
                                    pos);
              NS_HTML5_CONTINUE(stateloop);
            }
          }
        }
      beforedoctypepublicidentifierloop_end:;
        [[fallthrough]];
      }
      case DOCTYPE_PUBLIC_IDENTIFIER_DOUBLE_QUOTED: {
        for (;;) {
          if (++pos == endPos) {
            NS_HTML5_BREAK(stateloop);
          }
          c = checkChar(buf, pos);
          switch (c) {
            case '\"': {
              publicIdentifier = strBufToString();
              state = P::transition(
                  mViewSource.get(),
                  nsHtml5Tokenizer::AFTER_DOCTYPE_PUBLIC_IDENTIFIER, reconsume,
                  pos);
              NS_HTML5_BREAK(doctypepublicidentifierdoublequotedloop);
            }
            case '>': {
              if (P::reportErrors) {
                errGtInPublicId();
              }
              forceQuirks = true;
              publicIdentifier = strBufToString();
              emitDoctypeToken(pos);
              state = P::transition(mViewSource.get(), nsHtml5Tokenizer::DATA,
                                    reconsume, pos);
              NS_HTML5_CONTINUE(stateloop);
            }
            case '\r': {
              appendStrBufCarriageReturn();
              NS_HTML5_BREAK(stateloop);
            }
            case '\n': {
              appendStrBufLineFeed();
              continue;
            }
            case '\0': {
              c = 0xfffd;
              [[fallthrough]];
            }
            default: {
              appendStrBuf(c);
              continue;
            }
          }
        }
      doctypepublicidentifierdoublequotedloop_end:;
        [[fallthrough]];
      }
      case AFTER_DOCTYPE_PUBLIC_IDENTIFIER: {
        for (;;) {
          if (++pos == endPos) {
            NS_HTML5_BREAK(stateloop);
          }
          c = checkChar(buf, pos);
          switch (c) {
            case '\r': {
              silentCarriageReturn();
              state = P::transition(
                  mViewSource.get(),
                  nsHtml5Tokenizer::
                      BETWEEN_DOCTYPE_PUBLIC_AND_SYSTEM_IDENTIFIERS,
                  reconsume, pos);
              NS_HTML5_BREAK(stateloop);
            }
            case '\n': {
              silentLineFeed();
              [[fallthrough]];
            }
            case ' ':
            case '\t':
            case '\f': {
              state = P::transition(
                  mViewSource.get(),
                  nsHtml5Tokenizer::
                      BETWEEN_DOCTYPE_PUBLIC_AND_SYSTEM_IDENTIFIERS,
                  reconsume, pos);
              NS_HTML5_BREAK(afterdoctypepublicidentifierloop);
            }
            case '>': {
              emitDoctypeToken(pos);
              state = P::transition(mViewSource.get(), nsHtml5Tokenizer::DATA,
                                    reconsume, pos);
              NS_HTML5_CONTINUE(stateloop);
            }
            case '\"': {
              if (P::reportErrors) {
                errNoSpaceBetweenPublicAndSystemIds();
              }
              clearStrBufBeforeUse();
              state = P::transition(
                  mViewSource.get(),
                  nsHtml5Tokenizer::DOCTYPE_SYSTEM_IDENTIFIER_DOUBLE_QUOTED,
                  reconsume, pos);
              NS_HTML5_CONTINUE(stateloop);
            }
            case '\'': {
              if (P::reportErrors) {
                errNoSpaceBetweenPublicAndSystemIds();
              }
              clearStrBufBeforeUse();
              state = P::transition(
                  mViewSource.get(),
                  nsHtml5Tokenizer::DOCTYPE_SYSTEM_IDENTIFIER_SINGLE_QUOTED,
                  reconsume, pos);
              NS_HTML5_CONTINUE(stateloop);
            }
            default: {
              bogusDoctype();
              state = P::transition(mViewSource.get(),
                                    nsHtml5Tokenizer::BOGUS_DOCTYPE, reconsume,
                                    pos);
              NS_HTML5_CONTINUE(stateloop);
            }
          }
        }
      afterdoctypepublicidentifierloop_end:;
        [[fallthrough]];
      }
      case BETWEEN_DOCTYPE_PUBLIC_AND_SYSTEM_IDENTIFIERS: {
        for (;;) {
          if (++pos == endPos) {
            NS_HTML5_BREAK(stateloop);
          }
          c = checkChar(buf, pos);
          switch (c) {
            case '\r': {
              silentCarriageReturn();
              NS_HTML5_BREAK(stateloop);
            }
            case '\n': {
              silentLineFeed();
              [[fallthrough]];
            }
            case ' ':
            case '\t':
            case '\f': {
              continue;
            }
            case '>': {
              emitDoctypeToken(pos);
              state = P::transition(mViewSource.get(), nsHtml5Tokenizer::DATA,
                                    reconsume, pos);
              NS_HTML5_CONTINUE(stateloop);
            }
            case '\"': {
              clearStrBufBeforeUse();
              state = P::transition(
                  mViewSource.get(),
                  nsHtml5Tokenizer::DOCTYPE_SYSTEM_IDENTIFIER_DOUBLE_QUOTED,
                  reconsume, pos);
              NS_HTML5_BREAK(betweendoctypepublicandsystemidentifiersloop);
            }
            case '\'': {
              clearStrBufBeforeUse();
              state = P::transition(
                  mViewSource.get(),
                  nsHtml5Tokenizer::DOCTYPE_SYSTEM_IDENTIFIER_SINGLE_QUOTED,
                  reconsume, pos);
              NS_HTML5_CONTINUE(stateloop);
            }
            default: {
              bogusDoctype();
              state = P::transition(mViewSource.get(),
                                    nsHtml5Tokenizer::BOGUS_DOCTYPE, reconsume,
                                    pos);
              NS_HTML5_CONTINUE(stateloop);
            }
          }
        }
      betweendoctypepublicandsystemidentifiersloop_end:;
        [[fallthrough]];
      }
      case DOCTYPE_SYSTEM_IDENTIFIER_DOUBLE_QUOTED: {
        for (;;) {
          if (++pos == endPos) {
            NS_HTML5_BREAK(stateloop);
          }
          c = checkChar(buf, pos);
          switch (c) {
            case '\"': {
              systemIdentifier = strBufToString();
              state = P::transition(
                  mViewSource.get(),
                  nsHtml5Tokenizer::AFTER_DOCTYPE_SYSTEM_IDENTIFIER, reconsume,
                  pos);
              NS_HTML5_CONTINUE(stateloop);
            }
            case '>': {
              if (P::reportErrors) {
                errGtInSystemId();
              }
              forceQuirks = true;
              systemIdentifier = strBufToString();
              emitDoctypeToken(pos);
              state = P::transition(mViewSource.get(), nsHtml5Tokenizer::DATA,
                                    reconsume, pos);
              NS_HTML5_CONTINUE(stateloop);
            }
            case '\r': {
              appendStrBufCarriageReturn();
              NS_HTML5_BREAK(stateloop);
            }
            case '\n': {
              appendStrBufLineFeed();
              continue;
            }
            case '\0': {
              c = 0xfffd;
              [[fallthrough]];
            }
            default: {
              appendStrBuf(c);
              continue;
            }
          }
        }
      }
      case AFTER_DOCTYPE_SYSTEM_IDENTIFIER: {
        for (;;) {
          if (++pos == endPos) {
            NS_HTML5_BREAK(stateloop);
          }
          c = checkChar(buf, pos);
          switch (c) {
            case '\r': {
              silentCarriageReturn();
              NS_HTML5_BREAK(stateloop);
            }
            case '\n': {
              silentLineFeed();
              [[fallthrough]];
            }
            case ' ':
            case '\t':
            case '\f': {
              continue;
            }
            case '>': {
              emitDoctypeToken(pos);
              state = P::transition(mViewSource.get(), nsHtml5Tokenizer::DATA,
                                    reconsume, pos);
              NS_HTML5_CONTINUE(stateloop);
            }
            default: {
              bogusDoctypeWithoutQuirks();
              state = P::transition(mViewSource.get(),
                                    nsHtml5Tokenizer::BOGUS_DOCTYPE, reconsume,
                                    pos);
              NS_HTML5_BREAK(afterdoctypesystemidentifierloop);
            }
          }
        }
      afterdoctypesystemidentifierloop_end:;
        [[fallthrough]];
      }
      case BOGUS_DOCTYPE: {
        for (;;) {
          if (reconsume) {
            reconsume = false;
          } else {
            if (++pos == endPos) {
              NS_HTML5_BREAK(stateloop);
            }
            c = checkChar(buf, pos);
          }
          switch (c) {
            case '>': {
              emitDoctypeToken(pos);
              state = P::transition(mViewSource.get(), nsHtml5Tokenizer::DATA,
                                    reconsume, pos);
              NS_HTML5_CONTINUE(stateloop);
            }
            case '\r': {
              silentCarriageReturn();
              NS_HTML5_BREAK(stateloop);
            }
            case '\n': {
              silentLineFeed();
              [[fallthrough]];
            }
            default: {
              continue;
            }
          }
        }
      }
      case DOCTYPE_YSTEM: {
        for (;;) {
          if (++pos == endPos) {
            NS_HTML5_BREAK(stateloop);
          }
          c = checkChar(buf, pos);
          if (index < 5) {
            char16_t folded = c;
            if (c >= 'A' && c <= 'Z') {
              folded += 0x20;
            }
            if (folded != nsHtml5Tokenizer::YSTEM[index]) {
              bogusDoctype();
              reconsume = true;
              state = P::transition(mViewSource.get(),
                                    nsHtml5Tokenizer::BOGUS_DOCTYPE, reconsume,
                                    pos);
              NS_HTML5_CONTINUE(stateloop);
            }
            index++;
            NS_HTML5_CONTINUE(stateloop);
          } else {
            reconsume = true;
            state = P::transition(
                mViewSource.get(),
                nsHtml5Tokenizer::AFTER_DOCTYPE_SYSTEM_KEYWORD, reconsume, pos);
            NS_HTML5_BREAK(doctypeystemloop);
          }
        }
      doctypeystemloop_end:;
        [[fallthrough]];
      }
      case AFTER_DOCTYPE_SYSTEM_KEYWORD: {
        for (;;) {
          if (reconsume) {
            reconsume = false;
          } else {
            if (++pos == endPos) {
              NS_HTML5_BREAK(stateloop);
            }
            c = checkChar(buf, pos);
          }
          switch (c) {
            case '\r': {
              silentCarriageReturn();
              state = P::transition(
                  mViewSource.get(),
                  nsHtml5Tokenizer::BEFORE_DOCTYPE_SYSTEM_IDENTIFIER, reconsume,
                  pos);
              NS_HTML5_BREAK(stateloop);
            }
            case '\n': {
              silentLineFeed();
              [[fallthrough]];
            }
            case ' ':
            case '\t':
            case '\f': {
              state = P::transition(
                  mViewSource.get(),
                  nsHtml5Tokenizer::BEFORE_DOCTYPE_SYSTEM_IDENTIFIER, reconsume,
                  pos);
              NS_HTML5_BREAK(afterdoctypesystemkeywordloop);
            }
            case '\"': {
              if (P::reportErrors) {
                errNoSpaceBetweenDoctypeSystemKeywordAndQuote();
              }
              clearStrBufBeforeUse();
              state = P::transition(
                  mViewSource.get(),
                  nsHtml5Tokenizer::DOCTYPE_SYSTEM_IDENTIFIER_DOUBLE_QUOTED,
                  reconsume, pos);
              NS_HTML5_CONTINUE(stateloop);
            }
            case '\'': {
              if (P::reportErrors) {
                errNoSpaceBetweenDoctypeSystemKeywordAndQuote();
              }
              clearStrBufBeforeUse();
              state = P::transition(
                  mViewSource.get(),
                  nsHtml5Tokenizer::DOCTYPE_SYSTEM_IDENTIFIER_SINGLE_QUOTED,
                  reconsume, pos);
              NS_HTML5_CONTINUE(stateloop);
            }
            case '>': {
              if (P::reportErrors) {
                errExpectedPublicId();
              }
              forceQuirks = true;
              emitDoctypeToken(pos);
              state = P::transition(mViewSource.get(), nsHtml5Tokenizer::DATA,
                                    reconsume, pos);
              NS_HTML5_CONTINUE(stateloop);
            }
            default: {
              bogusDoctype();
              state = P::transition(mViewSource.get(),
                                    nsHtml5Tokenizer::BOGUS_DOCTYPE, reconsume,
                                    pos);
              NS_HTML5_CONTINUE(stateloop);
            }
          }
        }
      afterdoctypesystemkeywordloop_end:;
        [[fallthrough]];
      }
      case BEFORE_DOCTYPE_SYSTEM_IDENTIFIER: {
        for (;;) {
          if (++pos == endPos) {
            NS_HTML5_BREAK(stateloop);
          }
          c = checkChar(buf, pos);
          switch (c) {
            case '\r': {
              silentCarriageReturn();
              NS_HTML5_BREAK(stateloop);
            }
            case '\n': {
              silentLineFeed();
              [[fallthrough]];
            }
            case ' ':
            case '\t':
            case '\f': {
              continue;
            }
            case '\"': {
              clearStrBufBeforeUse();
              state = P::transition(
                  mViewSource.get(),
                  nsHtml5Tokenizer::DOCTYPE_SYSTEM_IDENTIFIER_DOUBLE_QUOTED,
                  reconsume, pos);
              NS_HTML5_CONTINUE(stateloop);
            }
            case '\'': {
              clearStrBufBeforeUse();
              state = P::transition(
                  mViewSource.get(),
                  nsHtml5Tokenizer::DOCTYPE_SYSTEM_IDENTIFIER_SINGLE_QUOTED,
                  reconsume, pos);
              NS_HTML5_BREAK(beforedoctypesystemidentifierloop);
            }
            case '>': {
              if (P::reportErrors) {
                errExpectedSystemId();
              }
              forceQuirks = true;
              emitDoctypeToken(pos);
              state = P::transition(mViewSource.get(), nsHtml5Tokenizer::DATA,
                                    reconsume, pos);
              NS_HTML5_CONTINUE(stateloop);
            }
            default: {
              bogusDoctype();
              state = P::transition(mViewSource.get(),
                                    nsHtml5Tokenizer::BOGUS_DOCTYPE, reconsume,
                                    pos);
              NS_HTML5_CONTINUE(stateloop);
            }
          }
        }
      beforedoctypesystemidentifierloop_end:;
        [[fallthrough]];
      }
      case DOCTYPE_SYSTEM_IDENTIFIER_SINGLE_QUOTED: {
        for (;;) {
          if (++pos == endPos) {
            NS_HTML5_BREAK(stateloop);
          }
          c = checkChar(buf, pos);
          switch (c) {
            case '\'': {
              systemIdentifier = strBufToString();
              state = P::transition(
                  mViewSource.get(),
                  nsHtml5Tokenizer::AFTER_DOCTYPE_SYSTEM_IDENTIFIER, reconsume,
                  pos);
              NS_HTML5_CONTINUE(stateloop);
            }
            case '>': {
              if (P::reportErrors) {
                errGtInSystemId();
              }
              forceQuirks = true;
              systemIdentifier = strBufToString();
              emitDoctypeToken(pos);
              state = P::transition(mViewSource.get(), nsHtml5Tokenizer::DATA,
                                    reconsume, pos);
              NS_HTML5_CONTINUE(stateloop);
            }
            case '\r': {
              appendStrBufCarriageReturn();
              NS_HTML5_BREAK(stateloop);
            }
            case '\n': {
              appendStrBufLineFeed();
              continue;
            }
            case '\0': {
              c = 0xfffd;
              [[fallthrough]];
            }
            default: {
              appendStrBuf(c);
              continue;
            }
          }
        }
      }
      case DOCTYPE_PUBLIC_IDENTIFIER_SINGLE_QUOTED: {
        for (;;) {
          if (++pos == endPos) {
            NS_HTML5_BREAK(stateloop);
          }
          c = checkChar(buf, pos);
          switch (c) {
            case '\'': {
              publicIdentifier = strBufToString();
              state = P::transition(
                  mViewSource.get(),
                  nsHtml5Tokenizer::AFTER_DOCTYPE_PUBLIC_IDENTIFIER, reconsume,
                  pos);
              NS_HTML5_CONTINUE(stateloop);
            }
            case '>': {
              if (P::reportErrors) {
                errGtInPublicId();
              }
              forceQuirks = true;
              publicIdentifier = strBufToString();
              emitDoctypeToken(pos);
              state = P::transition(mViewSource.get(), nsHtml5Tokenizer::DATA,
                                    reconsume, pos);
              NS_HTML5_CONTINUE(stateloop);
            }
            case '\r': {
              appendStrBufCarriageReturn();
              NS_HTML5_BREAK(stateloop);
            }
            case '\n': {
              appendStrBufLineFeed();
              continue;
            }
            case '\0': {
              c = 0xfffd;
              [[fallthrough]];
            }
            default: {
              appendStrBuf(c);
              continue;
            }
          }
        }
      }
      case PROCESSING_INSTRUCTION: {
        for (;;) {
          if (++pos == endPos) {
            NS_HTML5_BREAK(stateloop);
          }
          c = checkChar(buf, pos);
          switch (c) {
            case '\?': {
              state = P::transition(
                  mViewSource.get(),
                  nsHtml5Tokenizer::PROCESSING_INSTRUCTION_QUESTION_MARK,
                  reconsume, pos);
              NS_HTML5_BREAK(processinginstructionloop);
            }
            default: {
              continue;
            }
          }
        }
      processinginstructionloop_end:;
        [[fallthrough]];
      }
      case PROCESSING_INSTRUCTION_QUESTION_MARK: {
        if (++pos == endPos) {
          NS_HTML5_BREAK(stateloop);
        }
        c = checkChar(buf, pos);
        switch (c) {
          case '>': {
            state = P::transition(mViewSource.get(), nsHtml5Tokenizer::DATA,
                                  reconsume, pos);
            NS_HTML5_CONTINUE(stateloop);
          }
          default: {
            state = P::transition(mViewSource.get(),
                                  nsHtml5Tokenizer::PROCESSING_INSTRUCTION,
                                  reconsume, pos);
            NS_HTML5_CONTINUE(stateloop);
          }
        }
      }
    }
  }
stateloop_end:;
  flushChars(buf, pos);
  stateSave = state;
  returnStateSave = returnState;
  return pos;
}

void nsHtml5Tokenizer::initDoctypeFields() {
  clearStrBufAfterUse();
  doctypeName = nullptr;
  if (systemIdentifier) {
    systemIdentifier.Release();
    systemIdentifier = nullptr;
  }
  if (publicIdentifier) {
    publicIdentifier.Release();
    publicIdentifier = nullptr;
  }
  forceQuirks = false;
}

void nsHtml5Tokenizer::emitCarriageReturn(char16_t* buf, int32_t pos) {
  silentCarriageReturn();
  flushChars(buf, pos);
  tokenHandler->characters(nsHtml5Tokenizer::LF, 0, 1);
  cstart = INT32_MAX;
}

void nsHtml5Tokenizer::emitReplacementCharacter(char16_t* buf, int32_t pos) {
  flushChars(buf, pos);
  tokenHandler->zeroOriginatingReplacementCharacter();
  cstart = pos + 1;
}

void nsHtml5Tokenizer::maybeEmitReplacementCharacter(char16_t* buf,
                                                     int32_t pos) {
  flushChars(buf, pos);
  tokenHandler->zeroOrReplacementCharacter();
  cstart = pos + 1;
}

void nsHtml5Tokenizer::emitPlaintextReplacementCharacter(char16_t* buf,
                                                         int32_t pos) {
  flushChars(buf, pos);
  tokenHandler->characters(REPLACEMENT_CHARACTER, 0, 1);
  cstart = pos + 1;
}

void nsHtml5Tokenizer::setAdditionalAndRememberAmpersandLocation(char16_t add) {
  additional = add;
}

void nsHtml5Tokenizer::bogusDoctype() {
  errBogusDoctype();
  forceQuirks = true;
}

void nsHtml5Tokenizer::bogusDoctypeWithoutQuirks() {
  errBogusDoctype();
  forceQuirks = false;
}

void nsHtml5Tokenizer::handleNcrValue(int32_t returnState) {
  if (value <= 0xFFFF) {
    if (value >= 0x80 && value <= 0x9f) {
      errNcrInC1Range();
      char16_t* val = nsHtml5NamedCharacters::WINDOWS_1252[value - 0x80];
      emitOrAppendOne(val, returnState);
    } else if (value == 0x0) {
      errNcrZero();
      emitOrAppendOne(nsHtml5Tokenizer::REPLACEMENT_CHARACTER, returnState);
    } else if ((value & 0xF800) == 0xD800) {
      errNcrSurrogate();
      emitOrAppendOne(nsHtml5Tokenizer::REPLACEMENT_CHARACTER, returnState);
    } else {
      char16_t ch = (char16_t)value;
      bmpChar[0] = ch;
      emitOrAppendOne(bmpChar, returnState);
    }
  } else if (value <= 0x10FFFF) {
    astralChar[0] = (char16_t)(nsHtml5Tokenizer::LEAD_OFFSET + (value >> 10));
    astralChar[1] = (char16_t)(0xDC00 + (value & 0x3FF));
    emitOrAppendTwo(astralChar, returnState);
  } else {
    errNcrOutOfRange();
    emitOrAppendOne(nsHtml5Tokenizer::REPLACEMENT_CHARACTER, returnState);
  }
}

void nsHtml5Tokenizer::eof() {
  int32_t state = stateSave;
  int32_t returnState = returnStateSave;
eofloop:
  for (;;) {
    switch (state) {
      case SCRIPT_DATA_LESS_THAN_SIGN:
      case SCRIPT_DATA_ESCAPED_LESS_THAN_SIGN: {
        tokenHandler->characters(nsHtml5Tokenizer::LT_GT, 0, 1);
        NS_HTML5_BREAK(eofloop);
      }
      case TAG_OPEN: {
        errEofAfterLt();
        tokenHandler->characters(nsHtml5Tokenizer::LT_GT, 0, 1);
        NS_HTML5_BREAK(eofloop);
      }
      case RAWTEXT_RCDATA_LESS_THAN_SIGN: {
        tokenHandler->characters(nsHtml5Tokenizer::LT_GT, 0, 1);
        NS_HTML5_BREAK(eofloop);
      }
      case NON_DATA_END_TAG_NAME: {
        tokenHandler->characters(nsHtml5Tokenizer::LT_SOLIDUS, 0, 2);
        emitStrBuf();
        NS_HTML5_BREAK(eofloop);
      }
      case CLOSE_TAG_OPEN: {
        errEofAfterLt();
        tokenHandler->characters(nsHtml5Tokenizer::LT_SOLIDUS, 0, 2);
        NS_HTML5_BREAK(eofloop);
      }
      case TAG_NAME: {
        errEofInTagName();
        NS_HTML5_BREAK(eofloop);
      }
      case BEFORE_ATTRIBUTE_NAME:
      case AFTER_ATTRIBUTE_VALUE_QUOTED:
      case SELF_CLOSING_START_TAG: {
        errEofWithoutGt();
        NS_HTML5_BREAK(eofloop);
      }
      case ATTRIBUTE_NAME: {
        errEofInAttributeName();
        NS_HTML5_BREAK(eofloop);
      }
      case AFTER_ATTRIBUTE_NAME:
      case BEFORE_ATTRIBUTE_VALUE: {
        errEofWithoutGt();
        NS_HTML5_BREAK(eofloop);
      }
      case ATTRIBUTE_VALUE_DOUBLE_QUOTED:
      case ATTRIBUTE_VALUE_SINGLE_QUOTED:
      case ATTRIBUTE_VALUE_UNQUOTED: {
        errEofInAttributeValue();
        NS_HTML5_BREAK(eofloop);
      }
      case BOGUS_COMMENT: {
        emitComment(0, 0);
        NS_HTML5_BREAK(eofloop);
      }
      case BOGUS_COMMENT_HYPHEN: {
        emitComment(0, 0);
        NS_HTML5_BREAK(eofloop);
      }
      case MARKUP_DECLARATION_OPEN: {
        errBogusComment();
        emitComment(0, 0);
        NS_HTML5_BREAK(eofloop);
      }
      case MARKUP_DECLARATION_HYPHEN: {
        errBogusComment();
        emitComment(0, 0);
        NS_HTML5_BREAK(eofloop);
      }
      case MARKUP_DECLARATION_OCTYPE: {
        if (index < 6) {
          errBogusComment();
          emitComment(0, 0);
        } else {
          errEofInDoctype();
          doctypeName = nullptr;
          if (systemIdentifier) {
            systemIdentifier.Release();
            systemIdentifier = nullptr;
          }
          if (publicIdentifier) {
            publicIdentifier.Release();
            publicIdentifier = nullptr;
          }
          forceQuirks = true;
          emitDoctypeToken(0);
          NS_HTML5_BREAK(eofloop);
        }
        NS_HTML5_BREAK(eofloop);
      }
      case COMMENT_START:
      case COMMENT:
      case COMMENT_LESSTHAN:
      case COMMENT_LESSTHAN_BANG: {
        errEofInComment();
        emitComment(0, 0);
        NS_HTML5_BREAK(eofloop);
      }
      case COMMENT_END:
      case COMMENT_LESSTHAN_BANG_DASH_DASH: {
        errEofInComment();
        emitComment(2, 0);
        NS_HTML5_BREAK(eofloop);
      }
      case COMMENT_END_DASH:
      case COMMENT_START_DASH:
      case COMMENT_LESSTHAN_BANG_DASH: {
        errEofInComment();
        emitComment(1, 0);
        NS_HTML5_BREAK(eofloop);
      }
      case COMMENT_END_BANG: {
        errEofInComment();
        emitComment(3, 0);
        NS_HTML5_BREAK(eofloop);
      }
      case DOCTYPE:
      case BEFORE_DOCTYPE_NAME: {
        errEofInDoctype();
        forceQuirks = true;
        emitDoctypeToken(0);
        NS_HTML5_BREAK(eofloop);
      }
      case DOCTYPE_NAME: {
        errEofInDoctype();
        strBufToDoctypeName();
        forceQuirks = true;
        emitDoctypeToken(0);
        NS_HTML5_BREAK(eofloop);
      }
      case DOCTYPE_UBLIC:
      case DOCTYPE_YSTEM:
      case AFTER_DOCTYPE_NAME:
      case AFTER_DOCTYPE_PUBLIC_KEYWORD:
      case AFTER_DOCTYPE_SYSTEM_KEYWORD:
      case BEFORE_DOCTYPE_PUBLIC_IDENTIFIER: {
        errEofInDoctype();
        forceQuirks = true;
        emitDoctypeToken(0);
        NS_HTML5_BREAK(eofloop);
      }
      case DOCTYPE_PUBLIC_IDENTIFIER_DOUBLE_QUOTED:
      case DOCTYPE_PUBLIC_IDENTIFIER_SINGLE_QUOTED: {
        errEofInPublicId();
        forceQuirks = true;
        publicIdentifier = strBufToString();
        emitDoctypeToken(0);
        NS_HTML5_BREAK(eofloop);
      }
      case AFTER_DOCTYPE_PUBLIC_IDENTIFIER:
      case BEFORE_DOCTYPE_SYSTEM_IDENTIFIER:
      case BETWEEN_DOCTYPE_PUBLIC_AND_SYSTEM_IDENTIFIERS: {
        errEofInDoctype();
        forceQuirks = true;
        emitDoctypeToken(0);
        NS_HTML5_BREAK(eofloop);
      }
      case DOCTYPE_SYSTEM_IDENTIFIER_DOUBLE_QUOTED:
      case DOCTYPE_SYSTEM_IDENTIFIER_SINGLE_QUOTED: {
        errEofInSystemId();
        forceQuirks = true;
        systemIdentifier = strBufToString();
        emitDoctypeToken(0);
        NS_HTML5_BREAK(eofloop);
      }
      case AFTER_DOCTYPE_SYSTEM_IDENTIFIER: {
        errEofInDoctype();
        forceQuirks = true;
        emitDoctypeToken(0);
        NS_HTML5_BREAK(eofloop);
      }
      case BOGUS_DOCTYPE: {
        emitDoctypeToken(0);
        NS_HTML5_BREAK(eofloop);
      }
      case CONSUME_CHARACTER_REFERENCE: {
        emitOrAppendCharRefBuf(returnState);
        state = returnState;
        continue;
      }
      case CHARACTER_REFERENCE_HILO_LOOKUP: {
        emitOrAppendCharRefBuf(returnState);
        state = returnState;
        continue;
      }
      case CHARACTER_REFERENCE_TAIL: {
        for (;;) {
          char16_t c = '\0';
          entCol++;
          for (;;) {
            if (hi == -1) {
              NS_HTML5_BREAK(hiloop);
            }
            if (entCol == nsHtml5NamedCharacters::NAMES[hi].length()) {
              NS_HTML5_BREAK(hiloop);
            }
            if (entCol > nsHtml5NamedCharacters::NAMES[hi].length()) {
              NS_HTML5_BREAK(outer);
            } else if (c < nsHtml5NamedCharacters::NAMES[hi].charAt(entCol)) {
              hi--;
            } else {
              NS_HTML5_BREAK(hiloop);
            }
          }
        hiloop_end:;
          for (;;) {
            if (hi < lo) {
              NS_HTML5_BREAK(outer);
            }
            if (entCol == nsHtml5NamedCharacters::NAMES[lo].length()) {
              candidate = lo;
              charRefBufMark = charRefBufLen;
              lo++;
            } else if (entCol > nsHtml5NamedCharacters::NAMES[lo].length()) {
              NS_HTML5_BREAK(outer);
            } else if (c > nsHtml5NamedCharacters::NAMES[lo].charAt(entCol)) {
              lo++;
            } else {
              NS_HTML5_BREAK(loloop);
            }
          }
        loloop_end:;
          if (hi < lo) {
            NS_HTML5_BREAK(outer);
          }
          continue;
        }
      outer_end:;
        if (candidate == -1) {
          emitOrAppendCharRefBuf(returnState);
          state = returnState;
          NS_HTML5_CONTINUE(eofloop);
        } else {
          const nsHtml5CharacterName& candidateName =
              nsHtml5NamedCharacters::NAMES[candidate];
          if (!candidateName.length() ||
              candidateName.charAt(candidateName.length() - 1) != ';') {
            if ((returnState & DATA_AND_RCDATA_MASK)) {
              char16_t ch;
              if (charRefBufMark == charRefBufLen) {
                ch = '\0';
              } else {
                ch = charRefBuf[charRefBufMark];
              }
              if ((ch >= '0' && ch <= '9') || (ch >= 'A' && ch <= 'Z') ||
                  (ch >= 'a' && ch <= 'z')) {
                appendCharRefBufToStrBuf();
                state = returnState;
                NS_HTML5_CONTINUE(eofloop);
              }
            }
            if ((returnState & DATA_AND_RCDATA_MASK)) {
              errUnescapedAmpersandInterpretedAsCharacterReference();
            } else {
              errNotSemicolonTerminated();
            }
          }
          const char16_t* val = nsHtml5NamedCharacters::VALUES[candidate];
          if (!val[1]) {
            emitOrAppendOne(val, returnState);
          } else {
            emitOrAppendTwo(val, returnState);
          }
          if (charRefBufMark < charRefBufLen) {
            if ((returnState & DATA_AND_RCDATA_MASK)) {
              appendStrBuf(charRefBuf, charRefBufMark,
                           charRefBufLen - charRefBufMark);
            } else {
              tokenHandler->characters(charRefBuf, charRefBufMark,
                                       charRefBufLen - charRefBufMark);
            }
          }
          charRefBufLen = 0;
          state = returnState;
          NS_HTML5_CONTINUE(eofloop);
        }
      }
      case CONSUME_NCR:
      case DECIMAL_NRC_LOOP:
      case HEX_NCR_LOOP: {
        if (!seenDigits) {
          errNoDigitsInNCR();
          emitOrAppendCharRefBuf(returnState);
          state = returnState;
          continue;
        } else {
          errCharRefLacksSemicolon();
        }
        handleNcrValue(returnState);
        state = returnState;
        continue;
      }
      case CDATA_RSQB: {
        tokenHandler->characters(nsHtml5Tokenizer::RSQB_RSQB, 0, 1);
        NS_HTML5_BREAK(eofloop);
      }
      case CDATA_RSQB_RSQB: {
        tokenHandler->characters(nsHtml5Tokenizer::RSQB_RSQB, 0, 2);
        NS_HTML5_BREAK(eofloop);
      }
      case DATA:
      default: {
        NS_HTML5_BREAK(eofloop);
      }
    }
  }
eofloop_end:;
  tokenHandler->eof();
  return;
}

void nsHtml5Tokenizer::emitDoctypeToken(int32_t pos) {
  cstart = pos + 1;
  tokenHandler->doctype(doctypeName, publicIdentifier, systemIdentifier,
                        forceQuirks);
  doctypeName = nullptr;
  publicIdentifier.Release();
  publicIdentifier = nullptr;
  systemIdentifier.Release();
  systemIdentifier = nullptr;
}

bool nsHtml5Tokenizer::internalEncodingDeclaration(
    nsHtml5String internalCharset) {
  if (encodingDeclarationHandler) {
    return encodingDeclarationHandler->internalEncodingDeclaration(
        internalCharset);
  }
  return false;
}

void nsHtml5Tokenizer::emitOrAppendTwo(const char16_t* val,
                                       int32_t returnState) {
  if ((returnState & DATA_AND_RCDATA_MASK)) {
    appendStrBuf(val[0]);
    appendStrBuf(val[1]);
  } else {
    tokenHandler->characters(val, 0, 2);
  }
}

void nsHtml5Tokenizer::emitOrAppendOne(const char16_t* val,
                                       int32_t returnState) {
  if ((returnState & DATA_AND_RCDATA_MASK)) {
    appendStrBuf(val[0]);
  } else {
    tokenHandler->characters(val, 0, 1);
  }
}

void nsHtml5Tokenizer::end() {
  strBuf = nullptr;
  doctypeName = nullptr;
  if (systemIdentifier) {
    systemIdentifier.Release();
    systemIdentifier = nullptr;
  }
  if (publicIdentifier) {
    publicIdentifier.Release();
    publicIdentifier = nullptr;
  }
  tagName = nullptr;
  nonInternedTagName->setNameForNonInterned(nullptr, false);
  attributeName = nullptr;
  nonInternedAttributeName->setNameForNonInterned(nullptr);
  tokenHandler->endTokenization();
  if (attributes) {
    attributes->clear(0);
  }
}

void nsHtml5Tokenizer::requestSuspension() { shouldSuspend = true; }

bool nsHtml5Tokenizer::isInDataState() { return (stateSave == DATA); }

void nsHtml5Tokenizer::resetToDataState() {
  clearStrBufAfterUse();
  charRefBufLen = 0;
  stateSave = nsHtml5Tokenizer::DATA;
  lastCR = false;
  index = 0;
  forceQuirks = false;
  additional = '\0';
  entCol = -1;
  firstCharKey = -1;
  lo = 0;
  hi = 0;
  candidate = -1;
  charRefBufMark = 0;
  value = 0;
  seenDigits = false;
  endTag = false;
  shouldSuspend = false;
  initDoctypeFields();
  containsHyphen = false;
  tagName = nullptr;
  attributeName = nullptr;
  if (newAttributesEachTime) {
    if (attributes) {
      delete attributes;
      attributes = nullptr;
    }
  }
}

void nsHtml5Tokenizer::loadState(nsHtml5Tokenizer* other) {
  strBufLen = other->strBufLen;
  if (strBufLen > strBuf.length) {
    strBuf = jArray<char16_t, int32_t>::newJArray(strBufLen);
  }
  nsHtml5ArrayCopy::arraycopy(other->strBuf, strBuf, strBufLen);
  charRefBufLen = other->charRefBufLen;
  nsHtml5ArrayCopy::arraycopy(other->charRefBuf, charRefBuf, charRefBufLen);
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
  charRefBufMark = other->charRefBufMark;
  value = other->value;
  seenDigits = other->seenDigits;
  endTag = other->endTag;
  shouldSuspend = false;
  doctypeName = other->doctypeName;
  systemIdentifier.Release();
  if (!other->systemIdentifier) {
    systemIdentifier = nullptr;
  } else {
    systemIdentifier =
        nsHtml5Portability::newStringFromString(other->systemIdentifier);
  }
  publicIdentifier.Release();
  if (!other->publicIdentifier) {
    publicIdentifier = nullptr;
  } else {
    publicIdentifier =
        nsHtml5Portability::newStringFromString(other->publicIdentifier);
  }
  containsHyphen = other->containsHyphen;
  if (!other->tagName) {
    tagName = nullptr;
  } else if (other->tagName->isInterned()) {
    tagName = other->tagName;
  } else {
    nonInternedTagName->setNameForNonInterned(other->tagName->getName(),
                                              other->tagName->isCustom());
    tagName = nonInternedTagName;
  }
  if (!other->attributeName) {
    attributeName = nullptr;
  } else if (other->attributeName->isInterned()) {
    attributeName = other->attributeName;
  } else {
    nonInternedAttributeName->setNameForNonInterned(
        other->attributeName->getLocal(nsHtml5AttributeName::HTML));
    attributeName = nonInternedAttributeName;
  }
  delete attributes;
  if (!other->attributes) {
    attributes = nullptr;
  } else {
    attributes = other->attributes->cloneAttributes();
  }
}

void nsHtml5Tokenizer::initializeWithoutStarting() {
  confident = false;
  strBuf = nullptr;
  line = 1;
  attributeLine = 1;
  resetToDataState();
}

void nsHtml5Tokenizer::setEncodingDeclarationHandler(
    nsHtml5StreamParser* encodingDeclarationHandler) {
  this->encodingDeclarationHandler = encodingDeclarationHandler;
}

nsHtml5Tokenizer::~nsHtml5Tokenizer() {
  MOZ_COUNT_DTOR(nsHtml5Tokenizer);
  delete nonInternedTagName;
  nonInternedTagName = nullptr;
  delete nonInternedAttributeName;
  nonInternedAttributeName = nullptr;
  delete attributes;
  attributes = nullptr;
}

void nsHtml5Tokenizer::initializeStatics() {}

void nsHtml5Tokenizer::releaseStatics() {}

#include "nsHtml5TokenizerCppSupplement.h"
