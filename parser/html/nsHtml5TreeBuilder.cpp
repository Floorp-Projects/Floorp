/*
 * Copyright (c) 2007 Henri Sivonen
 * Copyright (c) 2007-2011 Mozilla Foundation
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
 * Please edit TreeBuilder.java instead and regenerate.
 */

#define nsHtml5TreeBuilder_cpp__

#include "nsContentUtils.h"
#include "nsIAtom.h"
#include "nsHtml5AtomTable.h"
#include "nsITimer.h"
#include "nsString.h"
#include "nsNameSpaceManager.h"
#include "nsIContent.h"
#include "nsTraceRefcnt.h"
#include "jArray.h"
#include "nsHtml5DocumentMode.h"
#include "nsHtml5ArrayCopy.h"
#include "nsHtml5Parser.h"
#include "nsHtml5Atoms.h"
#include "nsHtml5TreeOperation.h"
#include "nsHtml5PendingNotification.h"
#include "nsHtml5StateSnapshot.h"
#include "nsHtml5StackNode.h"
#include "nsHtml5TreeOpExecutor.h"
#include "nsHtml5StreamParser.h"
#include "nsAHtml5TreeBuilderState.h"
#include "nsHtml5Highlighter.h"
#include "nsHtml5PlainTextUtils.h"
#include "nsHtml5ViewSourceUtils.h"
#include "mozilla/Likely.h"
#include "nsIContentHandle.h"
#include "nsHtml5OplessBuilder.h"

#include "nsHtml5Tokenizer.h"
#include "nsHtml5MetaScanner.h"
#include "nsHtml5AttributeName.h"
#include "nsHtml5ElementName.h"
#include "nsHtml5HtmlAttributes.h"
#include "nsHtml5StackNode.h"
#include "nsHtml5UTF16Buffer.h"
#include "nsHtml5StateSnapshot.h"
#include "nsHtml5Portability.h"

#include "nsHtml5TreeBuilder.h"

char16_t nsHtml5TreeBuilder::REPLACEMENT_CHARACTER[] = { 0xfffd };
static const char* const QUIRKY_PUBLIC_IDS_DATA[] = { "+//silmaril//dtd html pro v0r11 19970101//", "-//advasoft ltd//dtd html 3.0 aswedit + extensions//", "-//as//dtd html 3.0 aswedit + extensions//", "-//ietf//dtd html 2.0 level 1//", "-//ietf//dtd html 2.0 level 2//", "-//ietf//dtd html 2.0 strict level 1//", "-//ietf//dtd html 2.0 strict level 2//", "-//ietf//dtd html 2.0 strict//", "-//ietf//dtd html 2.0//", "-//ietf//dtd html 2.1e//", "-//ietf//dtd html 3.0//", "-//ietf//dtd html 3.2 final//", "-//ietf//dtd html 3.2//", "-//ietf//dtd html 3//", "-//ietf//dtd html level 0//", "-//ietf//dtd html level 1//", "-//ietf//dtd html level 2//", "-//ietf//dtd html level 3//", "-//ietf//dtd html strict level 0//", "-//ietf//dtd html strict level 1//", "-//ietf//dtd html strict level 2//", "-//ietf//dtd html strict level 3//", "-//ietf//dtd html strict//", "-//ietf//dtd html//", "-//metrius//dtd metrius presentational//", "-//microsoft//dtd internet explorer 2.0 html strict//", "-//microsoft//dtd internet explorer 2.0 html//", "-//microsoft//dtd internet explorer 2.0 tables//", "-//microsoft//dtd internet explorer 3.0 html strict//", "-//microsoft//dtd internet explorer 3.0 html//", "-//microsoft//dtd internet explorer 3.0 tables//", "-//netscape comm. corp.//dtd html//", "-//netscape comm. corp.//dtd strict html//", "-//o'reilly and associates//dtd html 2.0//", "-//o'reilly and associates//dtd html extended 1.0//", "-//o'reilly and associates//dtd html extended relaxed 1.0//", "-//softquad software//dtd hotmetal pro 6.0::19990601::extensions to html 4.0//", "-//softquad//dtd hotmetal pro 4.0::19971010::extensions to html 4.0//", "-//spyglass//dtd html 2.0 extended//", "-//sq//dtd html 2.0 hotmetal + extensions//", "-//sun microsystems corp.//dtd hotjava html//", "-//sun microsystems corp.//dtd hotjava strict html//", "-//w3c//dtd html 3 1995-03-24//", "-//w3c//dtd html 3.2 draft//", "-//w3c//dtd html 3.2 final//", "-//w3c//dtd html 3.2//", "-//w3c//dtd html 3.2s draft//", "-//w3c//dtd html 4.0 frameset//", "-//w3c//dtd html 4.0 transitional//", "-//w3c//dtd html experimental 19960712//", "-//w3c//dtd html experimental 970421//", "-//w3c//dtd w3 html//", "-//w3o//dtd w3 html 3.0//", "-//webtechs//dtd mozilla html 2.0//", "-//webtechs//dtd mozilla html//" };
staticJArray<const char*,int32_t> nsHtml5TreeBuilder::QUIRKY_PUBLIC_IDS = { QUIRKY_PUBLIC_IDS_DATA, MOZ_ARRAY_LENGTH(QUIRKY_PUBLIC_IDS_DATA) };
void 
nsHtml5TreeBuilder::startTokenization(nsHtml5Tokenizer* self)
{
  tokenizer = self;
  stack = jArray<nsHtml5StackNode*,int32_t>::newJArray(64);
  templateModeStack = jArray<int32_t,int32_t>::newJArray(64);
  listOfActiveFormattingElements = jArray<nsHtml5StackNode*,int32_t>::newJArray(64);
  needToDropLF = false;
  originalMode = NS_HTML5TREE_BUILDER_INITIAL;
  templateModePtr = -1;
  currentPtr = -1;
  listPtr = -1;
  formPointer = nullptr;
  headPointer = nullptr;
  deepTreeSurrogateParent = nullptr;
  start(fragment);
  charBufferLen = 0;
  charBuffer = jArray<char16_t,int32_t>::newJArray(1024);
  framesetOk = true;
  if (fragment) {
    nsIContentHandle* elt;
    if (contextNode) {
      elt = contextNode;
    } else {
      elt = createHtmlElementSetAsRoot(tokenizer->emptyAttributes());
    }
    nsHtml5StackNode* node = new nsHtml5StackNode(nsHtml5ElementName::ELT_HTML, elt);
    currentPtr++;
    stack[currentPtr] = node;
    if (nsHtml5Atoms::template_ == contextName) {
      pushTemplateMode(NS_HTML5TREE_BUILDER_IN_TEMPLATE);
    }
    resetTheInsertionMode();
    formPointer = getFormPointerForContext(contextNode);
    if (nsHtml5Atoms::title == contextName || nsHtml5Atoms::textarea == contextName) {
      tokenizer->setStateAndEndTagExpectation(NS_HTML5TOKENIZER_RCDATA, contextName);
    } else if (nsHtml5Atoms::style == contextName || nsHtml5Atoms::xmp == contextName || nsHtml5Atoms::iframe == contextName || nsHtml5Atoms::noembed == contextName || nsHtml5Atoms::noframes == contextName || (scriptingEnabled && nsHtml5Atoms::noscript == contextName)) {
      tokenizer->setStateAndEndTagExpectation(NS_HTML5TOKENIZER_RAWTEXT, contextName);
    } else if (nsHtml5Atoms::plaintext == contextName) {
      tokenizer->setStateAndEndTagExpectation(NS_HTML5TOKENIZER_PLAINTEXT, contextName);
    } else if (nsHtml5Atoms::script == contextName) {
      tokenizer->setStateAndEndTagExpectation(NS_HTML5TOKENIZER_SCRIPT_DATA, contextName);
    } else {
      tokenizer->setStateAndEndTagExpectation(NS_HTML5TOKENIZER_DATA, contextName);
    }
    contextName = nullptr;
    contextNode = nullptr;
  } else {
    mode = NS_HTML5TREE_BUILDER_INITIAL;
    if (tokenizer->isViewingXmlSource()) {
      nsIContentHandle* elt = createElement(kNameSpaceID_SVG, nsHtml5Atoms::svg, tokenizer->emptyAttributes());
      nsHtml5StackNode* node = new nsHtml5StackNode(nsHtml5ElementName::ELT_SVG, nsHtml5Atoms::svg, elt);
      currentPtr++;
      stack[currentPtr] = node;
    }
  }
}

void 
nsHtml5TreeBuilder::doctype(nsIAtom* name, nsString* publicIdentifier, nsString* systemIdentifier, bool forceQuirks)
{
  needToDropLF = false;
  if (!isInForeign() && mode == NS_HTML5TREE_BUILDER_INITIAL) {
    nsString* emptyString = nsHtml5Portability::newEmptyString();
    appendDoctypeToDocument(!name ? nsHtml5Atoms::emptystring : name, !publicIdentifier ? emptyString : publicIdentifier, !systemIdentifier ? emptyString : systemIdentifier);
    nsHtml5Portability::releaseString(emptyString);
    if (isQuirky(name, publicIdentifier, systemIdentifier, forceQuirks)) {
      errQuirkyDoctype();
      documentModeInternal(QUIRKS_MODE, publicIdentifier, systemIdentifier, false);
    } else if (isAlmostStandards(publicIdentifier, systemIdentifier)) {
      errAlmostStandardsDoctype();
      documentModeInternal(ALMOST_STANDARDS_MODE, publicIdentifier, systemIdentifier, false);
    } else {
      documentModeInternal(STANDARDS_MODE, publicIdentifier, systemIdentifier, false);
    }
    mode = NS_HTML5TREE_BUILDER_BEFORE_HTML;
    return;
  }
  errStrayDoctype();
  return;
}

void 
nsHtml5TreeBuilder::comment(char16_t* buf, int32_t start, int32_t length)
{
  needToDropLF = false;
  if (!isInForeign()) {
    switch(mode) {
      case NS_HTML5TREE_BUILDER_INITIAL:
      case NS_HTML5TREE_BUILDER_BEFORE_HTML:
      case NS_HTML5TREE_BUILDER_AFTER_AFTER_BODY:
      case NS_HTML5TREE_BUILDER_AFTER_AFTER_FRAMESET: {
        appendCommentToDocument(buf, start, length);
        return;
      }
      case NS_HTML5TREE_BUILDER_AFTER_BODY: {
        flushCharacters();
        appendComment(stack[0]->node, buf, start, length);
        return;
      }
      default: {
        break;
      }
    }
  }
  flushCharacters();
  appendComment(stack[currentPtr]->node, buf, start, length);
  return;
}

void 
nsHtml5TreeBuilder::characters(const char16_t* buf, int32_t start, int32_t length)
{
  if (tokenizer->isViewingXmlSource()) {
    return;
  }
  if (needToDropLF) {
    needToDropLF = false;
    if (buf[start] == '\n') {
      start++;
      length--;
      if (!length) {
        return;
      }
    }
  }
  switch(mode) {
    case NS_HTML5TREE_BUILDER_IN_BODY:
    case NS_HTML5TREE_BUILDER_IN_CELL:
    case NS_HTML5TREE_BUILDER_IN_CAPTION: {
      if (!isInForeignButNotHtmlOrMathTextIntegrationPoint()) {
        reconstructTheActiveFormattingElements();
      }
    }
    case NS_HTML5TREE_BUILDER_TEXT: {
      accumulateCharacters(buf, start, length);
      return;
    }
    case NS_HTML5TREE_BUILDER_IN_TABLE:
    case NS_HTML5TREE_BUILDER_IN_TABLE_BODY:
    case NS_HTML5TREE_BUILDER_IN_ROW: {
      accumulateCharactersForced(buf, start, length);
      return;
    }
    default: {
      int32_t end = start + length;
      for (int32_t i = start; i < end; i++) {
        switch(buf[i]) {
          case ' ':
          case '\t':
          case '\n':
          case '\r':
          case '\f': {
            switch(mode) {
              case NS_HTML5TREE_BUILDER_INITIAL:
              case NS_HTML5TREE_BUILDER_BEFORE_HTML:
              case NS_HTML5TREE_BUILDER_BEFORE_HEAD: {
                start = i + 1;
                continue;
              }
              case NS_HTML5TREE_BUILDER_IN_HEAD:
              case NS_HTML5TREE_BUILDER_IN_HEAD_NOSCRIPT:
              case NS_HTML5TREE_BUILDER_AFTER_HEAD:
              case NS_HTML5TREE_BUILDER_IN_COLUMN_GROUP:
              case NS_HTML5TREE_BUILDER_IN_FRAMESET:
              case NS_HTML5TREE_BUILDER_AFTER_FRAMESET: {
                continue;
              }
              case NS_HTML5TREE_BUILDER_FRAMESET_OK:
              case NS_HTML5TREE_BUILDER_IN_TEMPLATE:
              case NS_HTML5TREE_BUILDER_IN_BODY:
              case NS_HTML5TREE_BUILDER_IN_CELL:
              case NS_HTML5TREE_BUILDER_IN_CAPTION: {
                if (start < i) {
                  accumulateCharacters(buf, start, i - start);
                  start = i;
                }
                if (!isInForeignButNotHtmlOrMathTextIntegrationPoint()) {
                  flushCharacters();
                  reconstructTheActiveFormattingElements();
                }
                NS_HTML5_BREAK(charactersloop);
              }
              case NS_HTML5TREE_BUILDER_IN_SELECT:
              case NS_HTML5TREE_BUILDER_IN_SELECT_IN_TABLE: {
                NS_HTML5_BREAK(charactersloop);
              }
              case NS_HTML5TREE_BUILDER_IN_TABLE:
              case NS_HTML5TREE_BUILDER_IN_TABLE_BODY:
              case NS_HTML5TREE_BUILDER_IN_ROW: {
                accumulateCharactersForced(buf, i, 1);
                start = i + 1;
                continue;
              }
              case NS_HTML5TREE_BUILDER_AFTER_BODY:
              case NS_HTML5TREE_BUILDER_AFTER_AFTER_BODY:
              case NS_HTML5TREE_BUILDER_AFTER_AFTER_FRAMESET: {
                if (start < i) {
                  accumulateCharacters(buf, start, i - start);
                  start = i;
                }
                flushCharacters();
                reconstructTheActiveFormattingElements();
                continue;
              }
            }
          }
          default: {
            switch(mode) {
              case NS_HTML5TREE_BUILDER_INITIAL: {
                documentModeInternal(QUIRKS_MODE, nullptr, nullptr, false);
                mode = NS_HTML5TREE_BUILDER_BEFORE_HTML;
                i--;
                continue;
              }
              case NS_HTML5TREE_BUILDER_BEFORE_HTML: {
                appendHtmlElementToDocumentAndPush();
                mode = NS_HTML5TREE_BUILDER_BEFORE_HEAD;
                i--;
                continue;
              }
              case NS_HTML5TREE_BUILDER_BEFORE_HEAD: {
                if (start < i) {
                  accumulateCharacters(buf, start, i - start);
                  start = i;
                }
                flushCharacters();
                appendToCurrentNodeAndPushHeadElement(nsHtml5HtmlAttributes::EMPTY_ATTRIBUTES);
                mode = NS_HTML5TREE_BUILDER_IN_HEAD;
                i--;
                continue;
              }
              case NS_HTML5TREE_BUILDER_IN_HEAD: {
                if (start < i) {
                  accumulateCharacters(buf, start, i - start);
                  start = i;
                }
                flushCharacters();
                pop();
                mode = NS_HTML5TREE_BUILDER_AFTER_HEAD;
                i--;
                continue;
              }
              case NS_HTML5TREE_BUILDER_IN_HEAD_NOSCRIPT: {
                if (start < i) {
                  accumulateCharacters(buf, start, i - start);
                  start = i;
                }
                errNonSpaceInNoscriptInHead();
                flushCharacters();
                pop();
                mode = NS_HTML5TREE_BUILDER_IN_HEAD;
                i--;
                continue;
              }
              case NS_HTML5TREE_BUILDER_AFTER_HEAD: {
                if (start < i) {
                  accumulateCharacters(buf, start, i - start);
                  start = i;
                }
                flushCharacters();
                appendToCurrentNodeAndPushBodyElement();
                mode = NS_HTML5TREE_BUILDER_FRAMESET_OK;
                i--;
                continue;
              }
              case NS_HTML5TREE_BUILDER_FRAMESET_OK: {
                framesetOk = false;
                mode = NS_HTML5TREE_BUILDER_IN_BODY;
                i--;
                continue;
              }
              case NS_HTML5TREE_BUILDER_IN_TEMPLATE:
              case NS_HTML5TREE_BUILDER_IN_BODY:
              case NS_HTML5TREE_BUILDER_IN_CELL:
              case NS_HTML5TREE_BUILDER_IN_CAPTION: {
                if (start < i) {
                  accumulateCharacters(buf, start, i - start);
                  start = i;
                }
                if (!isInForeignButNotHtmlOrMathTextIntegrationPoint()) {
                  flushCharacters();
                  reconstructTheActiveFormattingElements();
                }
                NS_HTML5_BREAK(charactersloop);
              }
              case NS_HTML5TREE_BUILDER_IN_TABLE:
              case NS_HTML5TREE_BUILDER_IN_TABLE_BODY:
              case NS_HTML5TREE_BUILDER_IN_ROW: {
                accumulateCharactersForced(buf, i, 1);
                start = i + 1;
                continue;
              }
              case NS_HTML5TREE_BUILDER_IN_COLUMN_GROUP: {
                if (start < i) {
                  accumulateCharacters(buf, start, i - start);
                  start = i;
                }
                if (!currentPtr || stack[currentPtr]->getGroup() == NS_HTML5TREE_BUILDER_TEMPLATE) {
                  errNonSpaceInColgroupInFragment();
                  start = i + 1;
                  continue;
                }
                flushCharacters();
                pop();
                mode = NS_HTML5TREE_BUILDER_IN_TABLE;
                i--;
                continue;
              }
              case NS_HTML5TREE_BUILDER_IN_SELECT:
              case NS_HTML5TREE_BUILDER_IN_SELECT_IN_TABLE: {
                NS_HTML5_BREAK(charactersloop);
              }
              case NS_HTML5TREE_BUILDER_AFTER_BODY: {
                errNonSpaceAfterBody();

                mode = framesetOk ? NS_HTML5TREE_BUILDER_FRAMESET_OK : NS_HTML5TREE_BUILDER_IN_BODY;
                i--;
                continue;
              }
              case NS_HTML5TREE_BUILDER_IN_FRAMESET: {
                if (start < i) {
                  accumulateCharacters(buf, start, i - start);
                }
                errNonSpaceInFrameset();
                start = i + 1;
                continue;
              }
              case NS_HTML5TREE_BUILDER_AFTER_FRAMESET: {
                if (start < i) {
                  accumulateCharacters(buf, start, i - start);
                }
                errNonSpaceAfterFrameset();
                start = i + 1;
                continue;
              }
              case NS_HTML5TREE_BUILDER_AFTER_AFTER_BODY: {
                errNonSpaceInTrailer();
                mode = framesetOk ? NS_HTML5TREE_BUILDER_FRAMESET_OK : NS_HTML5TREE_BUILDER_IN_BODY;
                i--;
                continue;
              }
              case NS_HTML5TREE_BUILDER_AFTER_AFTER_FRAMESET: {
                if (start < i) {
                  accumulateCharacters(buf, start, i - start);
                }
                errNonSpaceInTrailer();
                start = i + 1;
                continue;
              }
            }
          }
        }
      }
      charactersloop_end: ;
      if (start < end) {
        accumulateCharacters(buf, start, end - start);
      }
    }
  }
}

void 
nsHtml5TreeBuilder::zeroOriginatingReplacementCharacter()
{
  if (mode == NS_HTML5TREE_BUILDER_TEXT) {
    accumulateCharacters(REPLACEMENT_CHARACTER, 0, 1);
    return;
  }
  if (currentPtr >= 0) {
    if (isSpecialParentInForeign(stack[currentPtr])) {
      return;
    }
    accumulateCharacters(REPLACEMENT_CHARACTER, 0, 1);
  }
}

void 
nsHtml5TreeBuilder::eof()
{
  flushCharacters();
  for (; ; ) {
    switch(mode) {
      case NS_HTML5TREE_BUILDER_INITIAL: {
        documentModeInternal(QUIRKS_MODE, nullptr, nullptr, false);
        mode = NS_HTML5TREE_BUILDER_BEFORE_HTML;
        continue;
      }
      case NS_HTML5TREE_BUILDER_BEFORE_HTML: {
        appendHtmlElementToDocumentAndPush();
        mode = NS_HTML5TREE_BUILDER_BEFORE_HEAD;
        continue;
      }
      case NS_HTML5TREE_BUILDER_BEFORE_HEAD: {
        appendToCurrentNodeAndPushHeadElement(nsHtml5HtmlAttributes::EMPTY_ATTRIBUTES);
        mode = NS_HTML5TREE_BUILDER_IN_HEAD;
        continue;
      }
      case NS_HTML5TREE_BUILDER_IN_HEAD: {
        while (currentPtr > 0) {
          popOnEof();
        }
        mode = NS_HTML5TREE_BUILDER_AFTER_HEAD;
        continue;
      }
      case NS_HTML5TREE_BUILDER_IN_HEAD_NOSCRIPT: {
        while (currentPtr > 1) {
          popOnEof();
        }
        mode = NS_HTML5TREE_BUILDER_IN_HEAD;
        continue;
      }
      case NS_HTML5TREE_BUILDER_AFTER_HEAD: {
        appendToCurrentNodeAndPushBodyElement();
        mode = NS_HTML5TREE_BUILDER_IN_BODY;
        continue;
      }
      case NS_HTML5TREE_BUILDER_IN_TABLE_BODY:
      case NS_HTML5TREE_BUILDER_IN_ROW:
      case NS_HTML5TREE_BUILDER_IN_TABLE:
      case NS_HTML5TREE_BUILDER_IN_SELECT_IN_TABLE:
      case NS_HTML5TREE_BUILDER_IN_SELECT:
      case NS_HTML5TREE_BUILDER_IN_COLUMN_GROUP:
      case NS_HTML5TREE_BUILDER_FRAMESET_OK:
      case NS_HTML5TREE_BUILDER_IN_CAPTION:
      case NS_HTML5TREE_BUILDER_IN_CELL:
      case NS_HTML5TREE_BUILDER_IN_BODY: {
        if (isTemplateModeStackEmpty()) {
          NS_HTML5_BREAK(eofloop);
        }
      }
      case NS_HTML5TREE_BUILDER_IN_TEMPLATE: {
        int32_t eltPos = findLast(nsHtml5Atoms::template_);
        if (eltPos == NS_HTML5TREE_BUILDER_NOT_FOUND_ON_STACK) {
          MOZ_ASSERT(fragment);
          NS_HTML5_BREAK(eofloop);
        }
        if (MOZ_UNLIKELY(mViewSource)) {
          errUnclosedElements(eltPos, nsHtml5Atoms::template_);
        }
        while (currentPtr >= eltPos) {
          pop();
        }
        clearTheListOfActiveFormattingElementsUpToTheLastMarker();
        popTemplateMode();
        resetTheInsertionMode();
        continue;
      }
      case NS_HTML5TREE_BUILDER_TEXT: {
        if (originalMode == NS_HTML5TREE_BUILDER_AFTER_HEAD) {
          popOnEof();
        }
        popOnEof();
        mode = originalMode;
        continue;
      }
      case NS_HTML5TREE_BUILDER_IN_FRAMESET: {
        NS_HTML5_BREAK(eofloop);
      }
      case NS_HTML5TREE_BUILDER_AFTER_BODY:
      case NS_HTML5TREE_BUILDER_AFTER_FRAMESET:
      case NS_HTML5TREE_BUILDER_AFTER_AFTER_BODY:
      case NS_HTML5TREE_BUILDER_AFTER_AFTER_FRAMESET:
      default: {
        NS_HTML5_BREAK(eofloop);
      }
    }
  }
  eofloop_end: ;
  while (currentPtr > 0) {
    popOnEof();
  }
  if (!fragment) {
    popOnEof();
  }
}

void 
nsHtml5TreeBuilder::endTokenization()
{
  formPointer = nullptr;
  headPointer = nullptr;
  deepTreeSurrogateParent = nullptr;
  templateModeStack = nullptr;
  if (stack) {
    while (currentPtr > -1) {
      stack[currentPtr]->release();
      currentPtr--;
    }
    stack = nullptr;
  }
  if (listOfActiveFormattingElements) {
    while (listPtr > -1) {
      if (listOfActiveFormattingElements[listPtr]) {
        listOfActiveFormattingElements[listPtr]->release();
      }
      listPtr--;
    }
    listOfActiveFormattingElements = nullptr;
  }
  charBuffer = nullptr;
  end();
}

void 
nsHtml5TreeBuilder::startTag(nsHtml5ElementName* elementName, nsHtml5HtmlAttributes* attributes, bool selfClosing)
{
  flushCharacters();
  int32_t eltPos;
  needToDropLF = false;
  starttagloop: for (; ; ) {
    int32_t group = elementName->getGroup();
    nsIAtom* name = elementName->name;
    if (isInForeign()) {
      nsHtml5StackNode* currentNode = stack[currentPtr];
      int32_t currNs = currentNode->ns;
      if (!(currentNode->isHtmlIntegrationPoint() || (currNs == kNameSpaceID_MathML && ((currentNode->getGroup() == NS_HTML5TREE_BUILDER_MI_MO_MN_MS_MTEXT && group != NS_HTML5TREE_BUILDER_MGLYPH_OR_MALIGNMARK) || (currentNode->getGroup() == NS_HTML5TREE_BUILDER_ANNOTATION_XML && group == NS_HTML5TREE_BUILDER_SVG))))) {
        switch(group) {
          case NS_HTML5TREE_BUILDER_B_OR_BIG_OR_CODE_OR_EM_OR_I_OR_S_OR_SMALL_OR_STRIKE_OR_STRONG_OR_TT_OR_U:
          case NS_HTML5TREE_BUILDER_DIV_OR_BLOCKQUOTE_OR_CENTER_OR_MENU:
          case NS_HTML5TREE_BUILDER_BODY:
          case NS_HTML5TREE_BUILDER_BR:
          case NS_HTML5TREE_BUILDER_RUBY_OR_SPAN_OR_SUB_OR_SUP_OR_VAR:
          case NS_HTML5TREE_BUILDER_DD_OR_DT:
          case NS_HTML5TREE_BUILDER_UL_OR_OL_OR_DL:
          case NS_HTML5TREE_BUILDER_EMBED:
          case NS_HTML5TREE_BUILDER_IMG:
          case NS_HTML5TREE_BUILDER_H1_OR_H2_OR_H3_OR_H4_OR_H5_OR_H6:
          case NS_HTML5TREE_BUILDER_HEAD:
          case NS_HTML5TREE_BUILDER_HR:
          case NS_HTML5TREE_BUILDER_LI:
          case NS_HTML5TREE_BUILDER_META:
          case NS_HTML5TREE_BUILDER_NOBR:
          case NS_HTML5TREE_BUILDER_P:
          case NS_HTML5TREE_BUILDER_PRE_OR_LISTING:
          case NS_HTML5TREE_BUILDER_TABLE: {
            errHtmlStartTagInForeignContext(name);
            while (!isSpecialParentInForeign(stack[currentPtr])) {
              pop();
            }
            NS_HTML5_CONTINUE(starttagloop);
          }
          case NS_HTML5TREE_BUILDER_FONT: {
            if (attributes->contains(nsHtml5AttributeName::ATTR_COLOR) || attributes->contains(nsHtml5AttributeName::ATTR_FACE) || attributes->contains(nsHtml5AttributeName::ATTR_SIZE)) {
              errHtmlStartTagInForeignContext(name);
              while (!isSpecialParentInForeign(stack[currentPtr])) {
                pop();
              }
              NS_HTML5_CONTINUE(starttagloop);
            }
          }
          default: {
            if (kNameSpaceID_SVG == currNs) {
              attributes->adjustForSvg();
              if (selfClosing) {
                appendVoidElementToCurrentMayFosterSVG(elementName, attributes);
                selfClosing = false;
              } else {
                appendToCurrentNodeAndPushElementMayFosterSVG(elementName, attributes);
              }
              attributes = nullptr;
              NS_HTML5_BREAK(starttagloop);
            } else {
              attributes->adjustForMath();
              if (selfClosing) {
                appendVoidElementToCurrentMayFosterMathML(elementName, attributes);
                selfClosing = false;
              } else {
                appendToCurrentNodeAndPushElementMayFosterMathML(elementName, attributes);
              }
              attributes = nullptr;
              NS_HTML5_BREAK(starttagloop);
            }
          }
        }
      }
    }
    switch(mode) {
      case NS_HTML5TREE_BUILDER_IN_TEMPLATE: {
        switch(group) {
          case NS_HTML5TREE_BUILDER_COL: {
            popTemplateMode();
            pushTemplateMode(NS_HTML5TREE_BUILDER_IN_COLUMN_GROUP);
            mode = NS_HTML5TREE_BUILDER_IN_COLUMN_GROUP;
            continue;
          }
          case NS_HTML5TREE_BUILDER_CAPTION:
          case NS_HTML5TREE_BUILDER_COLGROUP:
          case NS_HTML5TREE_BUILDER_TBODY_OR_THEAD_OR_TFOOT: {
            popTemplateMode();
            pushTemplateMode(NS_HTML5TREE_BUILDER_IN_TABLE);
            mode = NS_HTML5TREE_BUILDER_IN_TABLE;
            continue;
          }
          case NS_HTML5TREE_BUILDER_TR: {
            popTemplateMode();
            pushTemplateMode(NS_HTML5TREE_BUILDER_IN_TABLE_BODY);
            mode = NS_HTML5TREE_BUILDER_IN_TABLE_BODY;
            continue;
          }
          case NS_HTML5TREE_BUILDER_TD_OR_TH: {
            popTemplateMode();
            pushTemplateMode(NS_HTML5TREE_BUILDER_IN_ROW);
            mode = NS_HTML5TREE_BUILDER_IN_ROW;
            continue;
          }
          case NS_HTML5TREE_BUILDER_META: {
            checkMetaCharset(attributes);
            appendVoidElementToCurrentMayFoster(elementName, attributes);
            selfClosing = false;
            attributes = nullptr;
            NS_HTML5_BREAK(starttagloop);
          }
          case NS_HTML5TREE_BUILDER_TITLE: {
            startTagTitleInHead(elementName, attributes);
            attributes = nullptr;
            NS_HTML5_BREAK(starttagloop);
          }
          case NS_HTML5TREE_BUILDER_BASE:
          case NS_HTML5TREE_BUILDER_LINK_OR_BASEFONT_OR_BGSOUND: {
            appendVoidElementToCurrentMayFoster(elementName, attributes);
            selfClosing = false;
            attributes = nullptr;
            NS_HTML5_BREAK(starttagloop);
          }
          case NS_HTML5TREE_BUILDER_SCRIPT: {
            startTagScriptInHead(elementName, attributes);
            attributes = nullptr;
            NS_HTML5_BREAK(starttagloop);
          }
          case NS_HTML5TREE_BUILDER_NOFRAMES:
          case NS_HTML5TREE_BUILDER_STYLE: {
            startTagGenericRawText(elementName, attributes);
            attributes = nullptr;
            NS_HTML5_BREAK(starttagloop);
          }
          case NS_HTML5TREE_BUILDER_TEMPLATE: {
            startTagTemplateInHead(elementName, attributes);
            attributes = nullptr;
            NS_HTML5_BREAK(starttagloop);
          }
          default: {
            popTemplateMode();
            pushTemplateMode(NS_HTML5TREE_BUILDER_IN_BODY);
            mode = NS_HTML5TREE_BUILDER_IN_BODY;
            continue;
          }
        }
      }
      case NS_HTML5TREE_BUILDER_IN_ROW: {
        switch(group) {
          case NS_HTML5TREE_BUILDER_TD_OR_TH: {
            clearStackBackTo(findLastOrRoot(NS_HTML5TREE_BUILDER_TR));
            appendToCurrentNodeAndPushElement(elementName, attributes);
            mode = NS_HTML5TREE_BUILDER_IN_CELL;
            insertMarker();
            attributes = nullptr;
            NS_HTML5_BREAK(starttagloop);
          }
          case NS_HTML5TREE_BUILDER_CAPTION:
          case NS_HTML5TREE_BUILDER_COL:
          case NS_HTML5TREE_BUILDER_COLGROUP:
          case NS_HTML5TREE_BUILDER_TBODY_OR_THEAD_OR_TFOOT:
          case NS_HTML5TREE_BUILDER_TR: {
            eltPos = findLastOrRoot(NS_HTML5TREE_BUILDER_TR);
            if (!eltPos) {
              MOZ_ASSERT(fragment || isTemplateContents());
              errNoTableRowToClose();
              NS_HTML5_BREAK(starttagloop);
            }
            clearStackBackTo(eltPos);
            pop();
            mode = NS_HTML5TREE_BUILDER_IN_TABLE_BODY;
            continue;
          }
          default:
            ; // fall through
        }
      }
      case NS_HTML5TREE_BUILDER_IN_TABLE_BODY: {
        switch(group) {
          case NS_HTML5TREE_BUILDER_TR: {
            clearStackBackTo(findLastInTableScopeOrRootTemplateTbodyTheadTfoot());
            appendToCurrentNodeAndPushElement(elementName, attributes);
            mode = NS_HTML5TREE_BUILDER_IN_ROW;
            attributes = nullptr;
            NS_HTML5_BREAK(starttagloop);
          }
          case NS_HTML5TREE_BUILDER_TD_OR_TH: {
            errStartTagInTableBody(name);
            clearStackBackTo(findLastInTableScopeOrRootTemplateTbodyTheadTfoot());
            appendToCurrentNodeAndPushElement(nsHtml5ElementName::ELT_TR, nsHtml5HtmlAttributes::EMPTY_ATTRIBUTES);
            mode = NS_HTML5TREE_BUILDER_IN_ROW;
            continue;
          }
          case NS_HTML5TREE_BUILDER_CAPTION:
          case NS_HTML5TREE_BUILDER_COL:
          case NS_HTML5TREE_BUILDER_COLGROUP:
          case NS_HTML5TREE_BUILDER_TBODY_OR_THEAD_OR_TFOOT: {
            eltPos = findLastInTableScopeOrRootTemplateTbodyTheadTfoot();
            if (!eltPos || stack[eltPos]->getGroup() == NS_HTML5TREE_BUILDER_TEMPLATE) {
              MOZ_ASSERT(fragment || isTemplateContents());
              errStrayStartTag(name);
              NS_HTML5_BREAK(starttagloop);
            } else {
              clearStackBackTo(eltPos);
              pop();
              mode = NS_HTML5TREE_BUILDER_IN_TABLE;
              continue;
            }
          }
          default:
            ; // fall through
        }
      }
      case NS_HTML5TREE_BUILDER_IN_TABLE: {
        for (; ; ) {
          switch(group) {
            case NS_HTML5TREE_BUILDER_CAPTION: {
              clearStackBackTo(findLastOrRoot(NS_HTML5TREE_BUILDER_TABLE));
              insertMarker();
              appendToCurrentNodeAndPushElement(elementName, attributes);
              mode = NS_HTML5TREE_BUILDER_IN_CAPTION;
              attributes = nullptr;
              NS_HTML5_BREAK(starttagloop);
            }
            case NS_HTML5TREE_BUILDER_COLGROUP: {
              clearStackBackTo(findLastOrRoot(NS_HTML5TREE_BUILDER_TABLE));
              appendToCurrentNodeAndPushElement(elementName, attributes);
              mode = NS_HTML5TREE_BUILDER_IN_COLUMN_GROUP;
              attributes = nullptr;
              NS_HTML5_BREAK(starttagloop);
            }
            case NS_HTML5TREE_BUILDER_COL: {
              clearStackBackTo(findLastOrRoot(NS_HTML5TREE_BUILDER_TABLE));
              appendToCurrentNodeAndPushElement(nsHtml5ElementName::ELT_COLGROUP, nsHtml5HtmlAttributes::EMPTY_ATTRIBUTES);
              mode = NS_HTML5TREE_BUILDER_IN_COLUMN_GROUP;
              NS_HTML5_CONTINUE(starttagloop);
            }
            case NS_HTML5TREE_BUILDER_TBODY_OR_THEAD_OR_TFOOT: {
              clearStackBackTo(findLastOrRoot(NS_HTML5TREE_BUILDER_TABLE));
              appendToCurrentNodeAndPushElement(elementName, attributes);
              mode = NS_HTML5TREE_BUILDER_IN_TABLE_BODY;
              attributes = nullptr;
              NS_HTML5_BREAK(starttagloop);
            }
            case NS_HTML5TREE_BUILDER_TR:
            case NS_HTML5TREE_BUILDER_TD_OR_TH: {
              clearStackBackTo(findLastOrRoot(NS_HTML5TREE_BUILDER_TABLE));
              appendToCurrentNodeAndPushElement(nsHtml5ElementName::ELT_TBODY, nsHtml5HtmlAttributes::EMPTY_ATTRIBUTES);
              mode = NS_HTML5TREE_BUILDER_IN_TABLE_BODY;
              NS_HTML5_CONTINUE(starttagloop);
            }
            case NS_HTML5TREE_BUILDER_TEMPLATE: {
              NS_HTML5_BREAK(intableloop);
            }
            case NS_HTML5TREE_BUILDER_TABLE: {
              errTableSeenWhileTableOpen();
              eltPos = findLastInTableScope(name);
              if (eltPos == NS_HTML5TREE_BUILDER_NOT_FOUND_ON_STACK) {
                MOZ_ASSERT(fragment || isTemplateContents());
                NS_HTML5_BREAK(starttagloop);
              }
              generateImpliedEndTags();
              if (!!MOZ_UNLIKELY(mViewSource) && !isCurrent(nsHtml5Atoms::table)) {
                errNoCheckUnclosedElementsOnStack();
              }
              while (currentPtr >= eltPos) {
                pop();
              }
              resetTheInsertionMode();
              NS_HTML5_CONTINUE(starttagloop);
            }
            case NS_HTML5TREE_BUILDER_SCRIPT: {
              appendToCurrentNodeAndPushElement(elementName, attributes);
              originalMode = mode;
              mode = NS_HTML5TREE_BUILDER_TEXT;
              tokenizer->setStateAndEndTagExpectation(NS_HTML5TOKENIZER_SCRIPT_DATA, elementName);
              attributes = nullptr;
              NS_HTML5_BREAK(starttagloop);
            }
            case NS_HTML5TREE_BUILDER_STYLE: {
              appendToCurrentNodeAndPushElement(elementName, attributes);
              originalMode = mode;
              mode = NS_HTML5TREE_BUILDER_TEXT;
              tokenizer->setStateAndEndTagExpectation(NS_HTML5TOKENIZER_RAWTEXT, elementName);
              attributes = nullptr;
              NS_HTML5_BREAK(starttagloop);
            }
            case NS_HTML5TREE_BUILDER_INPUT: {
              errStartTagInTable(name);
              if (!nsHtml5Portability::lowerCaseLiteralEqualsIgnoreAsciiCaseString("hidden", attributes->getValue(nsHtml5AttributeName::ATTR_TYPE))) {
                NS_HTML5_BREAK(intableloop);
              }
              appendVoidElementToCurrent(name, attributes, formPointer);
              selfClosing = false;
              attributes = nullptr;
              NS_HTML5_BREAK(starttagloop);
            }
            case NS_HTML5TREE_BUILDER_FORM: {
              if (!!formPointer || isTemplateContents()) {
                errFormWhenFormOpen();
                NS_HTML5_BREAK(starttagloop);
              } else {
                errStartTagInTable(name);
                appendVoidFormToCurrent(attributes);
                attributes = nullptr;
                NS_HTML5_BREAK(starttagloop);
              }
            }
            default: {
              errStartTagInTable(name);
              NS_HTML5_BREAK(intableloop);
            }
          }
        }
        intableloop_end: ;
      }
      case NS_HTML5TREE_BUILDER_IN_CAPTION: {
        switch(group) {
          case NS_HTML5TREE_BUILDER_CAPTION:
          case NS_HTML5TREE_BUILDER_COL:
          case NS_HTML5TREE_BUILDER_COLGROUP:
          case NS_HTML5TREE_BUILDER_TBODY_OR_THEAD_OR_TFOOT:
          case NS_HTML5TREE_BUILDER_TR:
          case NS_HTML5TREE_BUILDER_TD_OR_TH: {
            errStrayStartTag(name);
            eltPos = findLastInTableScope(nsHtml5Atoms::caption);
            if (eltPos == NS_HTML5TREE_BUILDER_NOT_FOUND_ON_STACK) {
              NS_HTML5_BREAK(starttagloop);
            }
            generateImpliedEndTags();
            if (!!MOZ_UNLIKELY(mViewSource) && currentPtr != eltPos) {
              errNoCheckUnclosedElementsOnStack();
            }
            while (currentPtr >= eltPos) {
              pop();
            }
            clearTheListOfActiveFormattingElementsUpToTheLastMarker();
            mode = NS_HTML5TREE_BUILDER_IN_TABLE;
            continue;
          }
          default:
            ; // fall through
        }
      }
      case NS_HTML5TREE_BUILDER_IN_CELL: {
        switch(group) {
          case NS_HTML5TREE_BUILDER_CAPTION:
          case NS_HTML5TREE_BUILDER_COL:
          case NS_HTML5TREE_BUILDER_COLGROUP:
          case NS_HTML5TREE_BUILDER_TBODY_OR_THEAD_OR_TFOOT:
          case NS_HTML5TREE_BUILDER_TR:
          case NS_HTML5TREE_BUILDER_TD_OR_TH: {
            eltPos = findLastInTableScopeTdTh();
            if (eltPos == NS_HTML5TREE_BUILDER_NOT_FOUND_ON_STACK) {
              errNoCellToClose();
              NS_HTML5_BREAK(starttagloop);
            } else {
              closeTheCell(eltPos);
              continue;
            }
          }
          default:
            ; // fall through
        }
      }
      case NS_HTML5TREE_BUILDER_FRAMESET_OK: {
        switch(group) {
          case NS_HTML5TREE_BUILDER_FRAMESET: {
            if (mode == NS_HTML5TREE_BUILDER_FRAMESET_OK) {
              if (!currentPtr || stack[1]->getGroup() != NS_HTML5TREE_BUILDER_BODY) {
                MOZ_ASSERT(fragment || isTemplateContents());
                errStrayStartTag(name);
                NS_HTML5_BREAK(starttagloop);
              } else {
                errFramesetStart();
                detachFromParent(stack[1]->node);
                while (currentPtr > 0) {
                  pop();
                }
                appendToCurrentNodeAndPushElement(elementName, attributes);
                mode = NS_HTML5TREE_BUILDER_IN_FRAMESET;
                attributes = nullptr;
                NS_HTML5_BREAK(starttagloop);
              }
            } else {
              errStrayStartTag(name);
              NS_HTML5_BREAK(starttagloop);
            }
          }
          case NS_HTML5TREE_BUILDER_PRE_OR_LISTING:
          case NS_HTML5TREE_BUILDER_LI:
          case NS_HTML5TREE_BUILDER_DD_OR_DT:
          case NS_HTML5TREE_BUILDER_BUTTON:
          case NS_HTML5TREE_BUILDER_MARQUEE_OR_APPLET:
          case NS_HTML5TREE_BUILDER_OBJECT:
          case NS_HTML5TREE_BUILDER_TABLE:
          case NS_HTML5TREE_BUILDER_AREA_OR_WBR:
          case NS_HTML5TREE_BUILDER_BR:
          case NS_HTML5TREE_BUILDER_EMBED:
          case NS_HTML5TREE_BUILDER_IMG:
          case NS_HTML5TREE_BUILDER_INPUT:
          case NS_HTML5TREE_BUILDER_KEYGEN:
          case NS_HTML5TREE_BUILDER_HR:
          case NS_HTML5TREE_BUILDER_TEXTAREA:
          case NS_HTML5TREE_BUILDER_XMP:
          case NS_HTML5TREE_BUILDER_IFRAME:
          case NS_HTML5TREE_BUILDER_SELECT: {
            if (mode == NS_HTML5TREE_BUILDER_FRAMESET_OK && !(group == NS_HTML5TREE_BUILDER_INPUT && nsHtml5Portability::lowerCaseLiteralEqualsIgnoreAsciiCaseString("hidden", attributes->getValue(nsHtml5AttributeName::ATTR_TYPE)))) {
              framesetOk = false;
              mode = NS_HTML5TREE_BUILDER_IN_BODY;
            }
          }
          default:
            ; // fall through
        }
      }
      case NS_HTML5TREE_BUILDER_IN_BODY: {
        for (; ; ) {
          switch(group) {
            case NS_HTML5TREE_BUILDER_HTML: {
              errStrayStartTag(name);
              if (!fragment && !isTemplateContents()) {
                addAttributesToHtml(attributes);
                attributes = nullptr;
              }
              NS_HTML5_BREAK(starttagloop);
            }
            case NS_HTML5TREE_BUILDER_BASE:
            case NS_HTML5TREE_BUILDER_LINK_OR_BASEFONT_OR_BGSOUND:
            case NS_HTML5TREE_BUILDER_META:
            case NS_HTML5TREE_BUILDER_STYLE:
            case NS_HTML5TREE_BUILDER_SCRIPT:
            case NS_HTML5TREE_BUILDER_TITLE:
            case NS_HTML5TREE_BUILDER_TEMPLATE: {
              NS_HTML5_BREAK(inbodyloop);
            }
            case NS_HTML5TREE_BUILDER_BODY: {
              if (!currentPtr || stack[1]->getGroup() != NS_HTML5TREE_BUILDER_BODY || isTemplateContents()) {
                MOZ_ASSERT(fragment || isTemplateContents());
                errStrayStartTag(name);
                NS_HTML5_BREAK(starttagloop);
              }
              errFooSeenWhenFooOpen(name);
              framesetOk = false;
              if (mode == NS_HTML5TREE_BUILDER_FRAMESET_OK) {
                mode = NS_HTML5TREE_BUILDER_IN_BODY;
              }
              if (addAttributesToBody(attributes)) {
                attributes = nullptr;
              }
              NS_HTML5_BREAK(starttagloop);
            }
            case NS_HTML5TREE_BUILDER_P:
            case NS_HTML5TREE_BUILDER_DIV_OR_BLOCKQUOTE_OR_CENTER_OR_MENU:
            case NS_HTML5TREE_BUILDER_UL_OR_OL_OR_DL:
            case NS_HTML5TREE_BUILDER_ADDRESS_OR_ARTICLE_OR_ASIDE_OR_DETAILS_OR_DIR_OR_FIGCAPTION_OR_FIGURE_OR_FOOTER_OR_HEADER_OR_HGROUP_OR_MAIN_OR_NAV_OR_SECTION_OR_SUMMARY: {
              implicitlyCloseP();
              appendToCurrentNodeAndPushElementMayFoster(elementName, attributes);
              attributes = nullptr;
              NS_HTML5_BREAK(starttagloop);
            }
            case NS_HTML5TREE_BUILDER_H1_OR_H2_OR_H3_OR_H4_OR_H5_OR_H6: {
              implicitlyCloseP();
              if (stack[currentPtr]->getGroup() == NS_HTML5TREE_BUILDER_H1_OR_H2_OR_H3_OR_H4_OR_H5_OR_H6) {
                errHeadingWhenHeadingOpen();
                pop();
              }
              appendToCurrentNodeAndPushElementMayFoster(elementName, attributes);
              attributes = nullptr;
              NS_HTML5_BREAK(starttagloop);
            }
            case NS_HTML5TREE_BUILDER_FIELDSET: {
              implicitlyCloseP();
              appendToCurrentNodeAndPushElementMayFoster(elementName, attributes, formPointer);
              attributes = nullptr;
              NS_HTML5_BREAK(starttagloop);
            }
            case NS_HTML5TREE_BUILDER_PRE_OR_LISTING: {
              implicitlyCloseP();
              appendToCurrentNodeAndPushElementMayFoster(elementName, attributes);
              needToDropLF = true;
              attributes = nullptr;
              NS_HTML5_BREAK(starttagloop);
            }
            case NS_HTML5TREE_BUILDER_FORM: {
              if (!!formPointer && !isTemplateContents()) {
                errFormWhenFormOpen();
                NS_HTML5_BREAK(starttagloop);
              } else {
                implicitlyCloseP();
                appendToCurrentNodeAndPushFormElementMayFoster(attributes);
                attributes = nullptr;
                NS_HTML5_BREAK(starttagloop);
              }
            }
            case NS_HTML5TREE_BUILDER_LI:
            case NS_HTML5TREE_BUILDER_DD_OR_DT: {
              eltPos = currentPtr;
              for (; ; ) {
                nsHtml5StackNode* node = stack[eltPos];
                if (node->getGroup() == group) {
                  generateImpliedEndTagsExceptFor(node->name);
                  if (!!MOZ_UNLIKELY(mViewSource) && eltPos != currentPtr) {
                    errUnclosedElementsImplied(eltPos, name);
                  }
                  while (currentPtr >= eltPos) {
                    pop();
                  }
                  break;
                } else if (node->isSpecial() && (node->ns != kNameSpaceID_XHTML || (node->name != nsHtml5Atoms::p && node->name != nsHtml5Atoms::address && node->name != nsHtml5Atoms::div))) {
                  break;
                }
                eltPos--;
              }
              implicitlyCloseP();
              appendToCurrentNodeAndPushElementMayFoster(elementName, attributes);
              attributes = nullptr;
              NS_HTML5_BREAK(starttagloop);
            }
            case NS_HTML5TREE_BUILDER_PLAINTEXT: {
              implicitlyCloseP();
              appendToCurrentNodeAndPushElementMayFoster(elementName, attributes);
              tokenizer->setStateAndEndTagExpectation(NS_HTML5TOKENIZER_PLAINTEXT, elementName);
              attributes = nullptr;
              NS_HTML5_BREAK(starttagloop);
            }
            case NS_HTML5TREE_BUILDER_A: {
              int32_t activeAPos = findInListOfActiveFormattingElementsContainsBetweenEndAndLastMarker(nsHtml5Atoms::a);
              if (activeAPos != -1) {
                errFooSeenWhenFooOpen(name);
                nsHtml5StackNode* activeA = listOfActiveFormattingElements[activeAPos];
                activeA->retain();
                adoptionAgencyEndTag(nsHtml5Atoms::a);
                removeFromStack(activeA);
                activeAPos = findInListOfActiveFormattingElements(activeA);
                if (activeAPos != -1) {
                  removeFromListOfActiveFormattingElements(activeAPos);
                }
                activeA->release();
              }
              reconstructTheActiveFormattingElements();
              appendToCurrentNodeAndPushFormattingElementMayFoster(elementName, attributes);
              attributes = nullptr;
              NS_HTML5_BREAK(starttagloop);
            }
            case NS_HTML5TREE_BUILDER_B_OR_BIG_OR_CODE_OR_EM_OR_I_OR_S_OR_SMALL_OR_STRIKE_OR_STRONG_OR_TT_OR_U:
            case NS_HTML5TREE_BUILDER_FONT: {
              reconstructTheActiveFormattingElements();
              maybeForgetEarlierDuplicateFormattingElement(elementName->name, attributes);
              appendToCurrentNodeAndPushFormattingElementMayFoster(elementName, attributes);
              attributes = nullptr;
              NS_HTML5_BREAK(starttagloop);
            }
            case NS_HTML5TREE_BUILDER_NOBR: {
              reconstructTheActiveFormattingElements();
              if (NS_HTML5TREE_BUILDER_NOT_FOUND_ON_STACK != findLastInScope(nsHtml5Atoms::nobr)) {
                errFooSeenWhenFooOpen(name);
                adoptionAgencyEndTag(nsHtml5Atoms::nobr);
                reconstructTheActiveFormattingElements();
              }
              appendToCurrentNodeAndPushFormattingElementMayFoster(elementName, attributes);
              attributes = nullptr;
              NS_HTML5_BREAK(starttagloop);
            }
            case NS_HTML5TREE_BUILDER_BUTTON: {
              eltPos = findLastInScope(name);
              if (eltPos != NS_HTML5TREE_BUILDER_NOT_FOUND_ON_STACK) {
                errFooSeenWhenFooOpen(name);
                generateImpliedEndTags();
                if (!!MOZ_UNLIKELY(mViewSource) && !isCurrent(name)) {
                  errUnclosedElementsImplied(eltPos, name);
                }
                while (currentPtr >= eltPos) {
                  pop();
                }
                NS_HTML5_CONTINUE(starttagloop);
              } else {
                reconstructTheActiveFormattingElements();
                appendToCurrentNodeAndPushElementMayFoster(elementName, attributes, formPointer);
                attributes = nullptr;
                NS_HTML5_BREAK(starttagloop);
              }
            }
            case NS_HTML5TREE_BUILDER_OBJECT: {
              reconstructTheActiveFormattingElements();
              appendToCurrentNodeAndPushElementMayFoster(elementName, attributes, formPointer);
              insertMarker();
              attributes = nullptr;
              NS_HTML5_BREAK(starttagloop);
            }
            case NS_HTML5TREE_BUILDER_MARQUEE_OR_APPLET: {
              reconstructTheActiveFormattingElements();
              appendToCurrentNodeAndPushElementMayFoster(elementName, attributes);
              insertMarker();
              attributes = nullptr;
              NS_HTML5_BREAK(starttagloop);
            }
            case NS_HTML5TREE_BUILDER_TABLE: {
              if (!quirks) {
                implicitlyCloseP();
              }
              appendToCurrentNodeAndPushElementMayFoster(elementName, attributes);
              mode = NS_HTML5TREE_BUILDER_IN_TABLE;
              attributes = nullptr;
              NS_HTML5_BREAK(starttagloop);
            }
            case NS_HTML5TREE_BUILDER_BR:
            case NS_HTML5TREE_BUILDER_EMBED:
            case NS_HTML5TREE_BUILDER_AREA_OR_WBR: {
              reconstructTheActiveFormattingElements();
            }
#ifdef ENABLE_VOID_MENUITEM
            case NS_HTML5TREE_BUILDER_MENUITEM:
#endif
            case NS_HTML5TREE_BUILDER_PARAM_OR_SOURCE_OR_TRACK: {
              appendVoidElementToCurrentMayFoster(elementName, attributes);
              selfClosing = false;
              attributes = nullptr;
              NS_HTML5_BREAK(starttagloop);
            }
            case NS_HTML5TREE_BUILDER_HR: {
              implicitlyCloseP();
              appendVoidElementToCurrentMayFoster(elementName, attributes);
              selfClosing = false;
              attributes = nullptr;
              NS_HTML5_BREAK(starttagloop);
            }
            case NS_HTML5TREE_BUILDER_IMAGE: {
              errImage();
              elementName = nsHtml5ElementName::ELT_IMG;
              NS_HTML5_CONTINUE(starttagloop);
            }
            case NS_HTML5TREE_BUILDER_IMG:
            case NS_HTML5TREE_BUILDER_KEYGEN:
            case NS_HTML5TREE_BUILDER_INPUT: {
              reconstructTheActiveFormattingElements();
              appendVoidElementToCurrentMayFoster(name, attributes, formPointer);
              selfClosing = false;
              attributes = nullptr;
              NS_HTML5_BREAK(starttagloop);
            }
            case NS_HTML5TREE_BUILDER_ISINDEX: {
              errIsindex();
              if (!!formPointer && !isTemplateContents()) {
                NS_HTML5_BREAK(starttagloop);
              }
              implicitlyCloseP();
              nsHtml5HtmlAttributes* formAttrs = new nsHtml5HtmlAttributes(0);
              int32_t actionIndex = attributes->getIndex(nsHtml5AttributeName::ATTR_ACTION);
              if (actionIndex > -1) {
                formAttrs->addAttribute(nsHtml5AttributeName::ATTR_ACTION, attributes->getValueNoBoundsCheck(actionIndex));
              }
              appendToCurrentNodeAndPushFormElementMayFoster(formAttrs);
              appendVoidElementToCurrentMayFoster(nsHtml5ElementName::ELT_HR, nsHtml5HtmlAttributes::EMPTY_ATTRIBUTES);
              appendToCurrentNodeAndPushElementMayFoster(nsHtml5ElementName::ELT_LABEL, nsHtml5HtmlAttributes::EMPTY_ATTRIBUTES);
              int32_t promptIndex = attributes->getIndex(nsHtml5AttributeName::ATTR_PROMPT);
              if (promptIndex > -1) {
                autoJArray<char16_t,int32_t> prompt = nsHtml5Portability::newCharArrayFromString(attributes->getValueNoBoundsCheck(promptIndex));
                appendCharacters(stack[currentPtr]->node, prompt, 0, prompt.length);
              } else {
                appendIsindexPrompt(stack[currentPtr]->node);
              }
              nsHtml5HtmlAttributes* inputAttributes = new nsHtml5HtmlAttributes(0);
              inputAttributes->addAttribute(nsHtml5AttributeName::ATTR_NAME, nsHtml5Portability::newStringFromLiteral("isindex"));
              for (int32_t i = 0; i < attributes->getLength(); i++) {
                nsHtml5AttributeName* attributeQName = attributes->getAttributeNameNoBoundsCheck(i);
                if (nsHtml5AttributeName::ATTR_NAME == attributeQName || nsHtml5AttributeName::ATTR_PROMPT == attributeQName) {
                  attributes->releaseValue(i);
                } else if (nsHtml5AttributeName::ATTR_ACTION != attributeQName) {
                  inputAttributes->addAttribute(attributeQName, attributes->getValueNoBoundsCheck(i));
                }
              }
              attributes->clearWithoutReleasingContents();
              appendVoidElementToCurrentMayFoster(nsHtml5Atoms::input, inputAttributes, formPointer);
              pop();
              appendVoidElementToCurrentMayFoster(nsHtml5ElementName::ELT_HR, nsHtml5HtmlAttributes::EMPTY_ATTRIBUTES);
              pop();
              if (!isTemplateContents()) {
                formPointer = nullptr;
              }
              selfClosing = false;
              NS_HTML5_BREAK(starttagloop);
            }
            case NS_HTML5TREE_BUILDER_TEXTAREA: {
              appendToCurrentNodeAndPushElementMayFoster(elementName, attributes, formPointer);
              tokenizer->setStateAndEndTagExpectation(NS_HTML5TOKENIZER_RCDATA, elementName);
              originalMode = mode;
              mode = NS_HTML5TREE_BUILDER_TEXT;
              needToDropLF = true;
              attributes = nullptr;
              NS_HTML5_BREAK(starttagloop);
            }
            case NS_HTML5TREE_BUILDER_XMP: {
              implicitlyCloseP();
              reconstructTheActiveFormattingElements();
              appendToCurrentNodeAndPushElementMayFoster(elementName, attributes);
              originalMode = mode;
              mode = NS_HTML5TREE_BUILDER_TEXT;
              tokenizer->setStateAndEndTagExpectation(NS_HTML5TOKENIZER_RAWTEXT, elementName);
              attributes = nullptr;
              NS_HTML5_BREAK(starttagloop);
            }
            case NS_HTML5TREE_BUILDER_NOSCRIPT: {
              if (!scriptingEnabled) {
                reconstructTheActiveFormattingElements();
                appendToCurrentNodeAndPushElementMayFoster(elementName, attributes);
                attributes = nullptr;
                NS_HTML5_BREAK(starttagloop);
              } else {
              }
            }
            case NS_HTML5TREE_BUILDER_NOFRAMES:
            case NS_HTML5TREE_BUILDER_IFRAME:
            case NS_HTML5TREE_BUILDER_NOEMBED: {
              startTagGenericRawText(elementName, attributes);
              attributes = nullptr;
              NS_HTML5_BREAK(starttagloop);
            }
            case NS_HTML5TREE_BUILDER_SELECT: {
              reconstructTheActiveFormattingElements();
              appendToCurrentNodeAndPushElementMayFoster(elementName, attributes, formPointer);
              switch(mode) {
                case NS_HTML5TREE_BUILDER_IN_TABLE:
                case NS_HTML5TREE_BUILDER_IN_CAPTION:
                case NS_HTML5TREE_BUILDER_IN_COLUMN_GROUP:
                case NS_HTML5TREE_BUILDER_IN_TABLE_BODY:
                case NS_HTML5TREE_BUILDER_IN_ROW:
                case NS_HTML5TREE_BUILDER_IN_CELL: {
                  mode = NS_HTML5TREE_BUILDER_IN_SELECT_IN_TABLE;
                  break;
                }
                default: {
                  mode = NS_HTML5TREE_BUILDER_IN_SELECT;
                  break;
                }
              }
              attributes = nullptr;
              NS_HTML5_BREAK(starttagloop);
            }
            case NS_HTML5TREE_BUILDER_OPTGROUP:
            case NS_HTML5TREE_BUILDER_OPTION: {
              if (isCurrent(nsHtml5Atoms::option)) {
                pop();
              }
              reconstructTheActiveFormattingElements();
              appendToCurrentNodeAndPushElementMayFoster(elementName, attributes);
              attributes = nullptr;
              NS_HTML5_BREAK(starttagloop);
            }
            case NS_HTML5TREE_BUILDER_RT_OR_RP: {
              eltPos = findLastInScope(nsHtml5Atoms::ruby);
              if (eltPos != NS_HTML5TREE_BUILDER_NOT_FOUND_ON_STACK) {
                generateImpliedEndTags();
              }
              if (eltPos != currentPtr) {
                if (eltPos != NS_HTML5TREE_BUILDER_NOT_FOUND_ON_STACK) {
                  errStartTagSeenWithoutRuby(name);
                } else {
                  errUnclosedChildrenInRuby();
                }
              }
              appendToCurrentNodeAndPushElementMayFoster(elementName, attributes);
              attributes = nullptr;
              NS_HTML5_BREAK(starttagloop);
            }
            case NS_HTML5TREE_BUILDER_MATH: {
              reconstructTheActiveFormattingElements();
              attributes->adjustForMath();
              if (selfClosing) {
                appendVoidElementToCurrentMayFosterMathML(elementName, attributes);
                selfClosing = false;
              } else {
                appendToCurrentNodeAndPushElementMayFosterMathML(elementName, attributes);
              }
              attributes = nullptr;
              NS_HTML5_BREAK(starttagloop);
            }
            case NS_HTML5TREE_BUILDER_SVG: {
              reconstructTheActiveFormattingElements();
              attributes->adjustForSvg();
              if (selfClosing) {
                appendVoidElementToCurrentMayFosterSVG(elementName, attributes);
                selfClosing = false;
              } else {
                appendToCurrentNodeAndPushElementMayFosterSVG(elementName, attributes);
              }
              attributes = nullptr;
              NS_HTML5_BREAK(starttagloop);
            }
            case NS_HTML5TREE_BUILDER_CAPTION:
            case NS_HTML5TREE_BUILDER_COL:
            case NS_HTML5TREE_BUILDER_COLGROUP:
            case NS_HTML5TREE_BUILDER_TBODY_OR_THEAD_OR_TFOOT:
            case NS_HTML5TREE_BUILDER_TR:
            case NS_HTML5TREE_BUILDER_TD_OR_TH:
            case NS_HTML5TREE_BUILDER_FRAME:
            case NS_HTML5TREE_BUILDER_FRAMESET:
            case NS_HTML5TREE_BUILDER_HEAD: {
              errStrayStartTag(name);
              NS_HTML5_BREAK(starttagloop);
            }
            case NS_HTML5TREE_BUILDER_OUTPUT_OR_LABEL: {
              reconstructTheActiveFormattingElements();
              appendToCurrentNodeAndPushElementMayFoster(elementName, attributes, formPointer);
              attributes = nullptr;
              NS_HTML5_BREAK(starttagloop);
            }
            default: {
              reconstructTheActiveFormattingElements();
              appendToCurrentNodeAndPushElementMayFoster(elementName, attributes);
              attributes = nullptr;
              NS_HTML5_BREAK(starttagloop);
            }
          }
        }
        inbodyloop_end: ;
      }
      case NS_HTML5TREE_BUILDER_IN_HEAD: {
        for (; ; ) {
          switch(group) {
            case NS_HTML5TREE_BUILDER_HTML: {
              errStrayStartTag(name);
              if (!fragment && !isTemplateContents()) {
                addAttributesToHtml(attributes);
                attributes = nullptr;
              }
              NS_HTML5_BREAK(starttagloop);
            }
            case NS_HTML5TREE_BUILDER_BASE:
            case NS_HTML5TREE_BUILDER_LINK_OR_BASEFONT_OR_BGSOUND: {
              appendVoidElementToCurrentMayFoster(elementName, attributes);
              selfClosing = false;
              attributes = nullptr;
              NS_HTML5_BREAK(starttagloop);
            }
            case NS_HTML5TREE_BUILDER_META: {
              NS_HTML5_BREAK(inheadloop);
            }
            case NS_HTML5TREE_BUILDER_TITLE: {
              startTagTitleInHead(elementName, attributes);
              attributes = nullptr;
              NS_HTML5_BREAK(starttagloop);
            }
            case NS_HTML5TREE_BUILDER_NOSCRIPT: {
              if (scriptingEnabled) {
                appendToCurrentNodeAndPushElement(elementName, attributes);
                originalMode = mode;
                mode = NS_HTML5TREE_BUILDER_TEXT;
                tokenizer->setStateAndEndTagExpectation(NS_HTML5TOKENIZER_RAWTEXT, elementName);
              } else {
                appendToCurrentNodeAndPushElementMayFoster(elementName, attributes);
                mode = NS_HTML5TREE_BUILDER_IN_HEAD_NOSCRIPT;
              }
              attributes = nullptr;
              NS_HTML5_BREAK(starttagloop);
            }
            case NS_HTML5TREE_BUILDER_SCRIPT: {
              startTagScriptInHead(elementName, attributes);
              attributes = nullptr;
              NS_HTML5_BREAK(starttagloop);
            }
            case NS_HTML5TREE_BUILDER_STYLE:
            case NS_HTML5TREE_BUILDER_NOFRAMES: {
              startTagGenericRawText(elementName, attributes);
              attributes = nullptr;
              NS_HTML5_BREAK(starttagloop);
            }
            case NS_HTML5TREE_BUILDER_HEAD: {
              errFooSeenWhenFooOpen(name);
              NS_HTML5_BREAK(starttagloop);
            }
            case NS_HTML5TREE_BUILDER_TEMPLATE: {
              startTagTemplateInHead(elementName, attributes);
              attributes = nullptr;
              NS_HTML5_BREAK(starttagloop);
            }
            default: {
              pop();
              mode = NS_HTML5TREE_BUILDER_AFTER_HEAD;
              NS_HTML5_CONTINUE(starttagloop);
            }
          }
        }
        inheadloop_end: ;
      }
      case NS_HTML5TREE_BUILDER_IN_HEAD_NOSCRIPT: {
        switch(group) {
          case NS_HTML5TREE_BUILDER_HTML: {
            errStrayStartTag(name);
            if (!fragment && !isTemplateContents()) {
              addAttributesToHtml(attributes);
              attributes = nullptr;
            }
            NS_HTML5_BREAK(starttagloop);
          }
          case NS_HTML5TREE_BUILDER_LINK_OR_BASEFONT_OR_BGSOUND: {
            appendVoidElementToCurrentMayFoster(elementName, attributes);
            selfClosing = false;
            attributes = nullptr;
            NS_HTML5_BREAK(starttagloop);
          }
          case NS_HTML5TREE_BUILDER_META: {
            checkMetaCharset(attributes);
            appendVoidElementToCurrentMayFoster(elementName, attributes);
            selfClosing = false;
            attributes = nullptr;
            NS_HTML5_BREAK(starttagloop);
          }
          case NS_HTML5TREE_BUILDER_STYLE:
          case NS_HTML5TREE_BUILDER_NOFRAMES: {
            appendToCurrentNodeAndPushElement(elementName, attributes);
            originalMode = mode;
            mode = NS_HTML5TREE_BUILDER_TEXT;
            tokenizer->setStateAndEndTagExpectation(NS_HTML5TOKENIZER_RAWTEXT, elementName);
            attributes = nullptr;
            NS_HTML5_BREAK(starttagloop);
          }
          case NS_HTML5TREE_BUILDER_HEAD: {
            errFooSeenWhenFooOpen(name);
            NS_HTML5_BREAK(starttagloop);
          }
          case NS_HTML5TREE_BUILDER_NOSCRIPT: {
            errFooSeenWhenFooOpen(name);
            NS_HTML5_BREAK(starttagloop);
          }
          default: {
            errBadStartTagInHead(name);
            pop();
            mode = NS_HTML5TREE_BUILDER_IN_HEAD;
            continue;
          }
        }
      }
      case NS_HTML5TREE_BUILDER_IN_COLUMN_GROUP: {
        switch(group) {
          case NS_HTML5TREE_BUILDER_HTML: {
            errStrayStartTag(name);
            if (!fragment && !isTemplateContents()) {
              addAttributesToHtml(attributes);
              attributes = nullptr;
            }
            NS_HTML5_BREAK(starttagloop);
          }
          case NS_HTML5TREE_BUILDER_COL: {
            appendVoidElementToCurrentMayFoster(elementName, attributes);
            selfClosing = false;
            attributes = nullptr;
            NS_HTML5_BREAK(starttagloop);
          }
          case NS_HTML5TREE_BUILDER_TEMPLATE: {
            startTagTemplateInHead(elementName, attributes);
            attributes = nullptr;
            NS_HTML5_BREAK(starttagloop);
          }
          default: {
            if (!currentPtr || stack[currentPtr]->getGroup() == NS_HTML5TREE_BUILDER_TEMPLATE) {
              MOZ_ASSERT(fragment || isTemplateContents());
              errGarbageInColgroup();
              NS_HTML5_BREAK(starttagloop);
            }
            pop();
            mode = NS_HTML5TREE_BUILDER_IN_TABLE;
            continue;
          }
        }
      }
      case NS_HTML5TREE_BUILDER_IN_SELECT_IN_TABLE: {
        switch(group) {
          case NS_HTML5TREE_BUILDER_CAPTION:
          case NS_HTML5TREE_BUILDER_TBODY_OR_THEAD_OR_TFOOT:
          case NS_HTML5TREE_BUILDER_TR:
          case NS_HTML5TREE_BUILDER_TD_OR_TH:
          case NS_HTML5TREE_BUILDER_TABLE: {
            errStartTagWithSelectOpen(name);
            eltPos = findLastInTableScope(nsHtml5Atoms::select);
            if (eltPos == NS_HTML5TREE_BUILDER_NOT_FOUND_ON_STACK) {
              MOZ_ASSERT(fragment);
              NS_HTML5_BREAK(starttagloop);
            }
            while (currentPtr >= eltPos) {
              pop();
            }
            resetTheInsertionMode();
            continue;
          }
          default:
            ; // fall through
        }
      }
      case NS_HTML5TREE_BUILDER_IN_SELECT: {
        switch(group) {
          case NS_HTML5TREE_BUILDER_HTML: {
            errStrayStartTag(name);
            if (!fragment) {
              addAttributesToHtml(attributes);
              attributes = nullptr;
            }
            NS_HTML5_BREAK(starttagloop);
          }
          case NS_HTML5TREE_BUILDER_OPTION: {
            if (isCurrent(nsHtml5Atoms::option)) {
              pop();
            }
            appendToCurrentNodeAndPushElement(elementName, attributes);
            attributes = nullptr;
            NS_HTML5_BREAK(starttagloop);
          }
          case NS_HTML5TREE_BUILDER_OPTGROUP: {
            if (isCurrent(nsHtml5Atoms::option)) {
              pop();
            }
            if (isCurrent(nsHtml5Atoms::optgroup)) {
              pop();
            }
            appendToCurrentNodeAndPushElement(elementName, attributes);
            attributes = nullptr;
            NS_HTML5_BREAK(starttagloop);
          }
          case NS_HTML5TREE_BUILDER_SELECT: {
            errStartSelectWhereEndSelectExpected();
            eltPos = findLastInTableScope(name);
            if (eltPos == NS_HTML5TREE_BUILDER_NOT_FOUND_ON_STACK) {
              MOZ_ASSERT(fragment);
              errNoSelectInTableScope();
              NS_HTML5_BREAK(starttagloop);
            } else {
              while (currentPtr >= eltPos) {
                pop();
              }
              resetTheInsertionMode();
              NS_HTML5_BREAK(starttagloop);
            }
          }
          case NS_HTML5TREE_BUILDER_INPUT:
          case NS_HTML5TREE_BUILDER_TEXTAREA:
          case NS_HTML5TREE_BUILDER_KEYGEN: {
            errStartTagWithSelectOpen(name);
            eltPos = findLastInTableScope(nsHtml5Atoms::select);
            if (eltPos == NS_HTML5TREE_BUILDER_NOT_FOUND_ON_STACK) {
              MOZ_ASSERT(fragment);
              NS_HTML5_BREAK(starttagloop);
            }
            while (currentPtr >= eltPos) {
              pop();
            }
            resetTheInsertionMode();
            continue;
          }
          case NS_HTML5TREE_BUILDER_SCRIPT: {
            startTagScriptInHead(elementName, attributes);
            attributes = nullptr;
            NS_HTML5_BREAK(starttagloop);
          }
          case NS_HTML5TREE_BUILDER_TEMPLATE: {
            startTagTemplateInHead(elementName, attributes);
            attributes = nullptr;
            NS_HTML5_BREAK(starttagloop);
          }
          default: {
            errStrayStartTag(name);
            NS_HTML5_BREAK(starttagloop);
          }
        }
      }
      case NS_HTML5TREE_BUILDER_AFTER_BODY: {
        switch(group) {
          case NS_HTML5TREE_BUILDER_HTML: {
            errStrayStartTag(name);
            if (!fragment && !isTemplateContents()) {
              addAttributesToHtml(attributes);
              attributes = nullptr;
            }
            NS_HTML5_BREAK(starttagloop);
          }
          default: {
            errStrayStartTag(name);
            mode = framesetOk ? NS_HTML5TREE_BUILDER_FRAMESET_OK : NS_HTML5TREE_BUILDER_IN_BODY;
            continue;
          }
        }
      }
      case NS_HTML5TREE_BUILDER_IN_FRAMESET: {
        switch(group) {
          case NS_HTML5TREE_BUILDER_FRAMESET: {
            appendToCurrentNodeAndPushElement(elementName, attributes);
            attributes = nullptr;
            NS_HTML5_BREAK(starttagloop);
          }
          case NS_HTML5TREE_BUILDER_FRAME: {
            appendVoidElementToCurrentMayFoster(elementName, attributes);
            selfClosing = false;
            attributes = nullptr;
            NS_HTML5_BREAK(starttagloop);
          }
          default:
            ; // fall through
        }
      }
      case NS_HTML5TREE_BUILDER_AFTER_FRAMESET: {
        switch(group) {
          case NS_HTML5TREE_BUILDER_HTML: {
            errStrayStartTag(name);
            if (!fragment && !isTemplateContents()) {
              addAttributesToHtml(attributes);
              attributes = nullptr;
            }
            NS_HTML5_BREAK(starttagloop);
          }
          case NS_HTML5TREE_BUILDER_NOFRAMES: {
            appendToCurrentNodeAndPushElement(elementName, attributes);
            originalMode = mode;
            mode = NS_HTML5TREE_BUILDER_TEXT;
            tokenizer->setStateAndEndTagExpectation(NS_HTML5TOKENIZER_RAWTEXT, elementName);
            attributes = nullptr;
            NS_HTML5_BREAK(starttagloop);
          }
          default: {
            errStrayStartTag(name);
            NS_HTML5_BREAK(starttagloop);
          }
        }
      }
      case NS_HTML5TREE_BUILDER_INITIAL: {
        errStartTagWithoutDoctype();
        documentModeInternal(QUIRKS_MODE, nullptr, nullptr, false);
        mode = NS_HTML5TREE_BUILDER_BEFORE_HTML;
        continue;
      }
      case NS_HTML5TREE_BUILDER_BEFORE_HTML: {
        switch(group) {
          case NS_HTML5TREE_BUILDER_HTML: {
            if (attributes == nsHtml5HtmlAttributes::EMPTY_ATTRIBUTES) {
              appendHtmlElementToDocumentAndPush();
            } else {
              appendHtmlElementToDocumentAndPush(attributes);
            }
            mode = NS_HTML5TREE_BUILDER_BEFORE_HEAD;
            attributes = nullptr;
            NS_HTML5_BREAK(starttagloop);
          }
          default: {
            appendHtmlElementToDocumentAndPush();
            mode = NS_HTML5TREE_BUILDER_BEFORE_HEAD;
            continue;
          }
        }
      }
      case NS_HTML5TREE_BUILDER_BEFORE_HEAD: {
        switch(group) {
          case NS_HTML5TREE_BUILDER_HTML: {
            errStrayStartTag(name);
            if (!fragment && !isTemplateContents()) {
              addAttributesToHtml(attributes);
              attributes = nullptr;
            }
            NS_HTML5_BREAK(starttagloop);
          }
          case NS_HTML5TREE_BUILDER_HEAD: {
            appendToCurrentNodeAndPushHeadElement(attributes);
            mode = NS_HTML5TREE_BUILDER_IN_HEAD;
            attributes = nullptr;
            NS_HTML5_BREAK(starttagloop);
          }
          default: {
            appendToCurrentNodeAndPushHeadElement(nsHtml5HtmlAttributes::EMPTY_ATTRIBUTES);
            mode = NS_HTML5TREE_BUILDER_IN_HEAD;
            continue;
          }
        }
      }
      case NS_HTML5TREE_BUILDER_AFTER_HEAD: {
        switch(group) {
          case NS_HTML5TREE_BUILDER_HTML: {
            errStrayStartTag(name);
            if (!fragment && !isTemplateContents()) {
              addAttributesToHtml(attributes);
              attributes = nullptr;
            }
            NS_HTML5_BREAK(starttagloop);
          }
          case NS_HTML5TREE_BUILDER_BODY: {
            if (!attributes->getLength()) {
              appendToCurrentNodeAndPushBodyElement();
            } else {
              appendToCurrentNodeAndPushBodyElement(attributes);
            }
            framesetOk = false;
            mode = NS_HTML5TREE_BUILDER_IN_BODY;
            attributes = nullptr;
            NS_HTML5_BREAK(starttagloop);
          }
          case NS_HTML5TREE_BUILDER_FRAMESET: {
            appendToCurrentNodeAndPushElement(elementName, attributes);
            mode = NS_HTML5TREE_BUILDER_IN_FRAMESET;
            attributes = nullptr;
            NS_HTML5_BREAK(starttagloop);
          }
          case NS_HTML5TREE_BUILDER_TEMPLATE: {
            errFooBetweenHeadAndBody(name);
            pushHeadPointerOntoStack();
            nsHtml5StackNode* headOnStack = stack[currentPtr];
            startTagTemplateInHead(elementName, attributes);
            removeFromStack(headOnStack);
            attributes = nullptr;
            NS_HTML5_BREAK(starttagloop);
          }
          case NS_HTML5TREE_BUILDER_BASE:
          case NS_HTML5TREE_BUILDER_LINK_OR_BASEFONT_OR_BGSOUND: {
            errFooBetweenHeadAndBody(name);
            pushHeadPointerOntoStack();
            appendVoidElementToCurrentMayFoster(elementName, attributes);
            selfClosing = false;
            pop();
            attributes = nullptr;
            NS_HTML5_BREAK(starttagloop);
          }
          case NS_HTML5TREE_BUILDER_META: {
            errFooBetweenHeadAndBody(name);
            checkMetaCharset(attributes);
            pushHeadPointerOntoStack();
            appendVoidElementToCurrentMayFoster(elementName, attributes);
            selfClosing = false;
            pop();
            attributes = nullptr;
            NS_HTML5_BREAK(starttagloop);
          }
          case NS_HTML5TREE_BUILDER_SCRIPT: {
            errFooBetweenHeadAndBody(name);
            pushHeadPointerOntoStack();
            appendToCurrentNodeAndPushElement(elementName, attributes);
            originalMode = mode;
            mode = NS_HTML5TREE_BUILDER_TEXT;
            tokenizer->setStateAndEndTagExpectation(NS_HTML5TOKENIZER_SCRIPT_DATA, elementName);
            attributes = nullptr;
            NS_HTML5_BREAK(starttagloop);
          }
          case NS_HTML5TREE_BUILDER_STYLE:
          case NS_HTML5TREE_BUILDER_NOFRAMES: {
            errFooBetweenHeadAndBody(name);
            pushHeadPointerOntoStack();
            appendToCurrentNodeAndPushElement(elementName, attributes);
            originalMode = mode;
            mode = NS_HTML5TREE_BUILDER_TEXT;
            tokenizer->setStateAndEndTagExpectation(NS_HTML5TOKENIZER_RAWTEXT, elementName);
            attributes = nullptr;
            NS_HTML5_BREAK(starttagloop);
          }
          case NS_HTML5TREE_BUILDER_TITLE: {
            errFooBetweenHeadAndBody(name);
            pushHeadPointerOntoStack();
            appendToCurrentNodeAndPushElement(elementName, attributes);
            originalMode = mode;
            mode = NS_HTML5TREE_BUILDER_TEXT;
            tokenizer->setStateAndEndTagExpectation(NS_HTML5TOKENIZER_RCDATA, elementName);
            attributes = nullptr;
            NS_HTML5_BREAK(starttagloop);
          }
          case NS_HTML5TREE_BUILDER_HEAD: {
            errStrayStartTag(name);
            NS_HTML5_BREAK(starttagloop);
          }
          default: {
            appendToCurrentNodeAndPushBodyElement();
            mode = NS_HTML5TREE_BUILDER_FRAMESET_OK;
            continue;
          }
        }
      }
      case NS_HTML5TREE_BUILDER_AFTER_AFTER_BODY: {
        switch(group) {
          case NS_HTML5TREE_BUILDER_HTML: {
            errStrayStartTag(name);
            if (!fragment && !isTemplateContents()) {
              addAttributesToHtml(attributes);
              attributes = nullptr;
            }
            NS_HTML5_BREAK(starttagloop);
          }
          default: {
            errStrayStartTag(name);

            mode = framesetOk ? NS_HTML5TREE_BUILDER_FRAMESET_OK : NS_HTML5TREE_BUILDER_IN_BODY;
            continue;
          }
        }
      }
      case NS_HTML5TREE_BUILDER_AFTER_AFTER_FRAMESET: {
        switch(group) {
          case NS_HTML5TREE_BUILDER_HTML: {
            errStrayStartTag(name);
            if (!fragment && !isTemplateContents()) {
              addAttributesToHtml(attributes);
              attributes = nullptr;
            }
            NS_HTML5_BREAK(starttagloop);
          }
          case NS_HTML5TREE_BUILDER_NOFRAMES: {
            startTagGenericRawText(elementName, attributes);
            attributes = nullptr;
            NS_HTML5_BREAK(starttagloop);
          }
          default: {
            errStrayStartTag(name);
            NS_HTML5_BREAK(starttagloop);
          }
        }
      }
      case NS_HTML5TREE_BUILDER_TEXT: {
        MOZ_ASSERT(false);
        NS_HTML5_BREAK(starttagloop);
      }
    }
  }
  starttagloop_end: ;
  if (selfClosing) {
    errSelfClosing();
  }
  if (!mBuilder && attributes != nsHtml5HtmlAttributes::EMPTY_ATTRIBUTES) {
    delete attributes;
  }
}

void 
nsHtml5TreeBuilder::startTagTitleInHead(nsHtml5ElementName* elementName, nsHtml5HtmlAttributes* attributes)
{
  appendToCurrentNodeAndPushElementMayFoster(elementName, attributes);
  originalMode = mode;
  mode = NS_HTML5TREE_BUILDER_TEXT;
  tokenizer->setStateAndEndTagExpectation(NS_HTML5TOKENIZER_RCDATA, elementName);
}

void 
nsHtml5TreeBuilder::startTagGenericRawText(nsHtml5ElementName* elementName, nsHtml5HtmlAttributes* attributes)
{
  appendToCurrentNodeAndPushElementMayFoster(elementName, attributes);
  originalMode = mode;
  mode = NS_HTML5TREE_BUILDER_TEXT;
  tokenizer->setStateAndEndTagExpectation(NS_HTML5TOKENIZER_RAWTEXT, elementName);
}

void 
nsHtml5TreeBuilder::startTagScriptInHead(nsHtml5ElementName* elementName, nsHtml5HtmlAttributes* attributes)
{
  appendToCurrentNodeAndPushElementMayFoster(elementName, attributes);
  originalMode = mode;
  mode = NS_HTML5TREE_BUILDER_TEXT;
  tokenizer->setStateAndEndTagExpectation(NS_HTML5TOKENIZER_SCRIPT_DATA, elementName);
}

void 
nsHtml5TreeBuilder::startTagTemplateInHead(nsHtml5ElementName* elementName, nsHtml5HtmlAttributes* attributes)
{
  appendToCurrentNodeAndPushElement(elementName, attributes);
  insertMarker();
  framesetOk = false;
  originalMode = mode;
  mode = NS_HTML5TREE_BUILDER_IN_TEMPLATE;
  pushTemplateMode(NS_HTML5TREE_BUILDER_IN_TEMPLATE);
}

bool 
nsHtml5TreeBuilder::isTemplateContents()
{
  return NS_HTML5TREE_BUILDER_NOT_FOUND_ON_STACK != findLast(nsHtml5Atoms::template_);
}

bool 
nsHtml5TreeBuilder::isTemplateModeStackEmpty()
{
  return templateModePtr == -1;
}

bool 
nsHtml5TreeBuilder::isSpecialParentInForeign(nsHtml5StackNode* stackNode)
{
  int32_t ns = stackNode->ns;
  return (kNameSpaceID_XHTML == ns) || (stackNode->isHtmlIntegrationPoint()) || ((kNameSpaceID_MathML == ns) && (stackNode->getGroup() == NS_HTML5TREE_BUILDER_MI_MO_MN_MS_MTEXT));
}

nsString* 
nsHtml5TreeBuilder::extractCharsetFromContent(nsString* attributeValue)
{
  int32_t charsetState = NS_HTML5TREE_BUILDER_CHARSET_INITIAL;
  int32_t start = -1;
  int32_t end = -1;
  autoJArray<char16_t,int32_t> buffer = nsHtml5Portability::newCharArrayFromString(attributeValue);
  for (int32_t i = 0; i < buffer.length; i++) {
    char16_t c = buffer[i];
    switch(charsetState) {
      case NS_HTML5TREE_BUILDER_CHARSET_INITIAL: {
        switch(c) {
          case 'c':
          case 'C': {
            charsetState = NS_HTML5TREE_BUILDER_CHARSET_C;
            continue;
          }
          default: {
            continue;
          }
        }
      }
      case NS_HTML5TREE_BUILDER_CHARSET_C: {
        switch(c) {
          case 'h':
          case 'H': {
            charsetState = NS_HTML5TREE_BUILDER_CHARSET_H;
            continue;
          }
          default: {
            charsetState = NS_HTML5TREE_BUILDER_CHARSET_INITIAL;
            continue;
          }
        }
      }
      case NS_HTML5TREE_BUILDER_CHARSET_H: {
        switch(c) {
          case 'a':
          case 'A': {
            charsetState = NS_HTML5TREE_BUILDER_CHARSET_A;
            continue;
          }
          default: {
            charsetState = NS_HTML5TREE_BUILDER_CHARSET_INITIAL;
            continue;
          }
        }
      }
      case NS_HTML5TREE_BUILDER_CHARSET_A: {
        switch(c) {
          case 'r':
          case 'R': {
            charsetState = NS_HTML5TREE_BUILDER_CHARSET_R;
            continue;
          }
          default: {
            charsetState = NS_HTML5TREE_BUILDER_CHARSET_INITIAL;
            continue;
          }
        }
      }
      case NS_HTML5TREE_BUILDER_CHARSET_R: {
        switch(c) {
          case 's':
          case 'S': {
            charsetState = NS_HTML5TREE_BUILDER_CHARSET_S;
            continue;
          }
          default: {
            charsetState = NS_HTML5TREE_BUILDER_CHARSET_INITIAL;
            continue;
          }
        }
      }
      case NS_HTML5TREE_BUILDER_CHARSET_S: {
        switch(c) {
          case 'e':
          case 'E': {
            charsetState = NS_HTML5TREE_BUILDER_CHARSET_E;
            continue;
          }
          default: {
            charsetState = NS_HTML5TREE_BUILDER_CHARSET_INITIAL;
            continue;
          }
        }
      }
      case NS_HTML5TREE_BUILDER_CHARSET_E: {
        switch(c) {
          case 't':
          case 'T': {
            charsetState = NS_HTML5TREE_BUILDER_CHARSET_T;
            continue;
          }
          default: {
            charsetState = NS_HTML5TREE_BUILDER_CHARSET_INITIAL;
            continue;
          }
        }
      }
      case NS_HTML5TREE_BUILDER_CHARSET_T: {
        switch(c) {
          case '\t':
          case '\n':
          case '\f':
          case '\r':
          case ' ': {
            continue;
          }
          case '=': {
            charsetState = NS_HTML5TREE_BUILDER_CHARSET_EQUALS;
            continue;
          }
          default: {
            return nullptr;
          }
        }
      }
      case NS_HTML5TREE_BUILDER_CHARSET_EQUALS: {
        switch(c) {
          case '\t':
          case '\n':
          case '\f':
          case '\r':
          case ' ': {
            continue;
          }
          case '\'': {
            start = i + 1;
            charsetState = NS_HTML5TREE_BUILDER_CHARSET_SINGLE_QUOTED;
            continue;
          }
          case '\"': {
            start = i + 1;
            charsetState = NS_HTML5TREE_BUILDER_CHARSET_DOUBLE_QUOTED;
            continue;
          }
          default: {
            start = i;
            charsetState = NS_HTML5TREE_BUILDER_CHARSET_UNQUOTED;
            continue;
          }
        }
      }
      case NS_HTML5TREE_BUILDER_CHARSET_SINGLE_QUOTED: {
        switch(c) {
          case '\'': {
            end = i;
            NS_HTML5_BREAK(charsetloop);
          }
          default: {
            continue;
          }
        }
      }
      case NS_HTML5TREE_BUILDER_CHARSET_DOUBLE_QUOTED: {
        switch(c) {
          case '\"': {
            end = i;
            NS_HTML5_BREAK(charsetloop);
          }
          default: {
            continue;
          }
        }
      }
      case NS_HTML5TREE_BUILDER_CHARSET_UNQUOTED: {
        switch(c) {
          case '\t':
          case '\n':
          case '\f':
          case '\r':
          case ' ':
          case ';': {
            end = i;
            NS_HTML5_BREAK(charsetloop);
          }
          default: {
            continue;
          }
        }
      }
    }
  }
  charsetloop_end: ;
  nsString* charset = nullptr;
  if (start != -1) {
    if (end == -1) {
      end = buffer.length;
    }
    charset = nsHtml5Portability::newStringFromBuffer(buffer, start, end - start);
  }
  return charset;
}

void 
nsHtml5TreeBuilder::checkMetaCharset(nsHtml5HtmlAttributes* attributes)
{
  nsString* charset = attributes->getValue(nsHtml5AttributeName::ATTR_CHARSET);
  if (charset) {
    if (tokenizer->internalEncodingDeclaration(charset)) {
      requestSuspension();
      return;
    }
    return;
  }
  if (!nsHtml5Portability::lowerCaseLiteralEqualsIgnoreAsciiCaseString("content-type", attributes->getValue(nsHtml5AttributeName::ATTR_HTTP_EQUIV))) {
    return;
  }
  nsString* content = attributes->getValue(nsHtml5AttributeName::ATTR_CONTENT);
  if (content) {
    nsString* extract = nsHtml5TreeBuilder::extractCharsetFromContent(content);
    if (extract) {
      if (tokenizer->internalEncodingDeclaration(extract)) {
        requestSuspension();
      }
    }
    nsHtml5Portability::releaseString(extract);
  }
}

void 
nsHtml5TreeBuilder::endTag(nsHtml5ElementName* elementName)
{
  flushCharacters();
  needToDropLF = false;
  int32_t eltPos;
  int32_t group = elementName->getGroup();
  nsIAtom* name = elementName->name;
  for (; ; ) {
    if (isInForeign()) {
      if (stack[currentPtr]->name != name) {
        errEndTagDidNotMatchCurrentOpenElement(name, stack[currentPtr]->popName);
      }
      eltPos = currentPtr;
      for (; ; ) {
        if (stack[eltPos]->name == name) {
          while (currentPtr >= eltPos) {
            pop();
          }
          NS_HTML5_BREAK(endtagloop);
        }
        if (stack[--eltPos]->ns == kNameSpaceID_XHTML) {
          break;
        }
      }
    }
    switch(mode) {
      case NS_HTML5TREE_BUILDER_IN_TEMPLATE: {
        switch(group) {
          case NS_HTML5TREE_BUILDER_TEMPLATE: {
            break;
          }
          default: {
            errStrayEndTag(name);
            NS_HTML5_BREAK(endtagloop);
          }
        }
      }
      case NS_HTML5TREE_BUILDER_IN_ROW: {
        switch(group) {
          case NS_HTML5TREE_BUILDER_TR: {
            eltPos = findLastOrRoot(NS_HTML5TREE_BUILDER_TR);
            if (!eltPos) {
              MOZ_ASSERT(fragment || isTemplateContents());
              errNoTableRowToClose();
              NS_HTML5_BREAK(endtagloop);
            }
            clearStackBackTo(eltPos);
            pop();
            mode = NS_HTML5TREE_BUILDER_IN_TABLE_BODY;
            NS_HTML5_BREAK(endtagloop);
          }
          case NS_HTML5TREE_BUILDER_TABLE: {
            eltPos = findLastOrRoot(NS_HTML5TREE_BUILDER_TR);
            if (!eltPos) {
              MOZ_ASSERT(fragment || isTemplateContents());
              errNoTableRowToClose();
              NS_HTML5_BREAK(endtagloop);
            }
            clearStackBackTo(eltPos);
            pop();
            mode = NS_HTML5TREE_BUILDER_IN_TABLE_BODY;
            continue;
          }
          case NS_HTML5TREE_BUILDER_TBODY_OR_THEAD_OR_TFOOT: {
            if (findLastInTableScope(name) == NS_HTML5TREE_BUILDER_NOT_FOUND_ON_STACK) {
              errStrayEndTag(name);
              NS_HTML5_BREAK(endtagloop);
            }
            eltPos = findLastOrRoot(NS_HTML5TREE_BUILDER_TR);
            if (!eltPos) {
              MOZ_ASSERT(fragment || isTemplateContents());
              errNoTableRowToClose();
              NS_HTML5_BREAK(endtagloop);
            }
            clearStackBackTo(eltPos);
            pop();
            mode = NS_HTML5TREE_BUILDER_IN_TABLE_BODY;
            continue;
          }
          case NS_HTML5TREE_BUILDER_BODY:
          case NS_HTML5TREE_BUILDER_CAPTION:
          case NS_HTML5TREE_BUILDER_COL:
          case NS_HTML5TREE_BUILDER_COLGROUP:
          case NS_HTML5TREE_BUILDER_HTML:
          case NS_HTML5TREE_BUILDER_TD_OR_TH: {
            errStrayEndTag(name);
            NS_HTML5_BREAK(endtagloop);
          }
          default:
            ; // fall through
        }
      }
      case NS_HTML5TREE_BUILDER_IN_TABLE_BODY: {
        switch(group) {
          case NS_HTML5TREE_BUILDER_TBODY_OR_THEAD_OR_TFOOT: {
            eltPos = findLastOrRoot(name);
            if (!eltPos) {
              errStrayEndTag(name);
              NS_HTML5_BREAK(endtagloop);
            }
            clearStackBackTo(eltPos);
            pop();
            mode = NS_HTML5TREE_BUILDER_IN_TABLE;
            NS_HTML5_BREAK(endtagloop);
          }
          case NS_HTML5TREE_BUILDER_TABLE: {
            eltPos = findLastInTableScopeOrRootTemplateTbodyTheadTfoot();
            if (!eltPos || stack[eltPos]->getGroup() == NS_HTML5TREE_BUILDER_TEMPLATE) {
              MOZ_ASSERT(fragment || isTemplateContents());
              errStrayEndTag(name);
              NS_HTML5_BREAK(endtagloop);
            }
            clearStackBackTo(eltPos);
            pop();
            mode = NS_HTML5TREE_BUILDER_IN_TABLE;
            continue;
          }
          case NS_HTML5TREE_BUILDER_BODY:
          case NS_HTML5TREE_BUILDER_CAPTION:
          case NS_HTML5TREE_BUILDER_COL:
          case NS_HTML5TREE_BUILDER_COLGROUP:
          case NS_HTML5TREE_BUILDER_HTML:
          case NS_HTML5TREE_BUILDER_TD_OR_TH:
          case NS_HTML5TREE_BUILDER_TR: {
            errStrayEndTag(name);
            NS_HTML5_BREAK(endtagloop);
          }
          default:
            ; // fall through
        }
      }
      case NS_HTML5TREE_BUILDER_IN_TABLE: {
        switch(group) {
          case NS_HTML5TREE_BUILDER_TABLE: {
            eltPos = findLast(nsHtml5Atoms::table);
            if (eltPos == NS_HTML5TREE_BUILDER_NOT_FOUND_ON_STACK) {
              MOZ_ASSERT(fragment || isTemplateContents());
              errStrayEndTag(name);
              NS_HTML5_BREAK(endtagloop);
            }
            while (currentPtr >= eltPos) {
              pop();
            }
            resetTheInsertionMode();
            NS_HTML5_BREAK(endtagloop);
          }
          case NS_HTML5TREE_BUILDER_BODY:
          case NS_HTML5TREE_BUILDER_CAPTION:
          case NS_HTML5TREE_BUILDER_COL:
          case NS_HTML5TREE_BUILDER_COLGROUP:
          case NS_HTML5TREE_BUILDER_HTML:
          case NS_HTML5TREE_BUILDER_TBODY_OR_THEAD_OR_TFOOT:
          case NS_HTML5TREE_BUILDER_TD_OR_TH:
          case NS_HTML5TREE_BUILDER_TR: {
            errStrayEndTag(name);
            NS_HTML5_BREAK(endtagloop);
          }
          case NS_HTML5TREE_BUILDER_TEMPLATE: {
            break;
          }
          default: {
            errStrayEndTag(name);
          }
        }
      }
      case NS_HTML5TREE_BUILDER_IN_CAPTION: {
        switch(group) {
          case NS_HTML5TREE_BUILDER_CAPTION: {
            eltPos = findLastInTableScope(nsHtml5Atoms::caption);
            if (eltPos == NS_HTML5TREE_BUILDER_NOT_FOUND_ON_STACK) {
              NS_HTML5_BREAK(endtagloop);
            }
            generateImpliedEndTags();
            if (!!MOZ_UNLIKELY(mViewSource) && currentPtr != eltPos) {
              errUnclosedElements(eltPos, name);
            }
            while (currentPtr >= eltPos) {
              pop();
            }
            clearTheListOfActiveFormattingElementsUpToTheLastMarker();
            mode = NS_HTML5TREE_BUILDER_IN_TABLE;
            NS_HTML5_BREAK(endtagloop);
          }
          case NS_HTML5TREE_BUILDER_TABLE: {
            errTableClosedWhileCaptionOpen();
            eltPos = findLastInTableScope(nsHtml5Atoms::caption);
            if (eltPos == NS_HTML5TREE_BUILDER_NOT_FOUND_ON_STACK) {
              NS_HTML5_BREAK(endtagloop);
            }
            generateImpliedEndTags();
            if (!!MOZ_UNLIKELY(mViewSource) && currentPtr != eltPos) {
              errUnclosedElements(eltPos, name);
            }
            while (currentPtr >= eltPos) {
              pop();
            }
            clearTheListOfActiveFormattingElementsUpToTheLastMarker();
            mode = NS_HTML5TREE_BUILDER_IN_TABLE;
            continue;
          }
          case NS_HTML5TREE_BUILDER_BODY:
          case NS_HTML5TREE_BUILDER_COL:
          case NS_HTML5TREE_BUILDER_COLGROUP:
          case NS_HTML5TREE_BUILDER_HTML:
          case NS_HTML5TREE_BUILDER_TBODY_OR_THEAD_OR_TFOOT:
          case NS_HTML5TREE_BUILDER_TD_OR_TH:
          case NS_HTML5TREE_BUILDER_TR: {
            errStrayEndTag(name);
            NS_HTML5_BREAK(endtagloop);
          }
          default:
            ; // fall through
        }
      }
      case NS_HTML5TREE_BUILDER_IN_CELL: {
        switch(group) {
          case NS_HTML5TREE_BUILDER_TD_OR_TH: {
            eltPos = findLastInTableScope(name);
            if (eltPos == NS_HTML5TREE_BUILDER_NOT_FOUND_ON_STACK) {
              errStrayEndTag(name);
              NS_HTML5_BREAK(endtagloop);
            }
            generateImpliedEndTags();
            if (!!MOZ_UNLIKELY(mViewSource) && !isCurrent(name)) {
              errUnclosedElements(eltPos, name);
            }
            while (currentPtr >= eltPos) {
              pop();
            }
            clearTheListOfActiveFormattingElementsUpToTheLastMarker();
            mode = NS_HTML5TREE_BUILDER_IN_ROW;
            NS_HTML5_BREAK(endtagloop);
          }
          case NS_HTML5TREE_BUILDER_TABLE:
          case NS_HTML5TREE_BUILDER_TBODY_OR_THEAD_OR_TFOOT:
          case NS_HTML5TREE_BUILDER_TR: {
            if (findLastInTableScope(name) == NS_HTML5TREE_BUILDER_NOT_FOUND_ON_STACK) {
              MOZ_ASSERT(name == nsHtml5Atoms::tbody || name == nsHtml5Atoms::tfoot || name == nsHtml5Atoms::thead || fragment || isTemplateContents());
              errStrayEndTag(name);
              NS_HTML5_BREAK(endtagloop);
            }
            closeTheCell(findLastInTableScopeTdTh());
            continue;
          }
          case NS_HTML5TREE_BUILDER_BODY:
          case NS_HTML5TREE_BUILDER_CAPTION:
          case NS_HTML5TREE_BUILDER_COL:
          case NS_HTML5TREE_BUILDER_COLGROUP:
          case NS_HTML5TREE_BUILDER_HTML: {
            errStrayEndTag(name);
            NS_HTML5_BREAK(endtagloop);
          }
          default:
            ; // fall through
        }
      }
      case NS_HTML5TREE_BUILDER_FRAMESET_OK:
      case NS_HTML5TREE_BUILDER_IN_BODY: {
        switch(group) {
          case NS_HTML5TREE_BUILDER_BODY: {
            if (!isSecondOnStackBody()) {
              MOZ_ASSERT(fragment || isTemplateContents());
              errStrayEndTag(name);
              NS_HTML5_BREAK(endtagloop);
            }
            MOZ_ASSERT(currentPtr >= 1);
            if (MOZ_UNLIKELY(mViewSource)) {
              for (int32_t i = 2; i <= currentPtr; i++) {
                switch(stack[i]->getGroup()) {
                  case NS_HTML5TREE_BUILDER_DD_OR_DT:
                  case NS_HTML5TREE_BUILDER_LI:
                  case NS_HTML5TREE_BUILDER_OPTGROUP:
                  case NS_HTML5TREE_BUILDER_OPTION:
                  case NS_HTML5TREE_BUILDER_P:
                  case NS_HTML5TREE_BUILDER_RT_OR_RP:
                  case NS_HTML5TREE_BUILDER_TD_OR_TH:
                  case NS_HTML5TREE_BUILDER_TBODY_OR_THEAD_OR_TFOOT: {
                    break;
                  }
                  default: {
                    errEndWithUnclosedElements(name);
                    NS_HTML5_BREAK(uncloseloop1);
                  }
                }
              }
              uncloseloop1_end: ;
            }
            mode = NS_HTML5TREE_BUILDER_AFTER_BODY;
            NS_HTML5_BREAK(endtagloop);
          }
          case NS_HTML5TREE_BUILDER_HTML: {
            if (!isSecondOnStackBody()) {
              MOZ_ASSERT(fragment || isTemplateContents());
              errStrayEndTag(name);
              NS_HTML5_BREAK(endtagloop);
            }
            if (MOZ_UNLIKELY(mViewSource)) {
              for (int32_t i = 0; i <= currentPtr; i++) {
                switch(stack[i]->getGroup()) {
                  case NS_HTML5TREE_BUILDER_DD_OR_DT:
                  case NS_HTML5TREE_BUILDER_LI:
                  case NS_HTML5TREE_BUILDER_P:
                  case NS_HTML5TREE_BUILDER_TBODY_OR_THEAD_OR_TFOOT:
                  case NS_HTML5TREE_BUILDER_TD_OR_TH:
                  case NS_HTML5TREE_BUILDER_BODY:
                  case NS_HTML5TREE_BUILDER_HTML: {
                    break;
                  }
                  default: {
                    errEndWithUnclosedElements(name);
                    NS_HTML5_BREAK(uncloseloop2);
                  }
                }
              }
              uncloseloop2_end: ;
            }
            mode = NS_HTML5TREE_BUILDER_AFTER_BODY;
            continue;
          }
          case NS_HTML5TREE_BUILDER_DIV_OR_BLOCKQUOTE_OR_CENTER_OR_MENU:
          case NS_HTML5TREE_BUILDER_UL_OR_OL_OR_DL:
          case NS_HTML5TREE_BUILDER_PRE_OR_LISTING:
          case NS_HTML5TREE_BUILDER_FIELDSET:
          case NS_HTML5TREE_BUILDER_BUTTON:
          case NS_HTML5TREE_BUILDER_ADDRESS_OR_ARTICLE_OR_ASIDE_OR_DETAILS_OR_DIR_OR_FIGCAPTION_OR_FIGURE_OR_FOOTER_OR_HEADER_OR_HGROUP_OR_MAIN_OR_NAV_OR_SECTION_OR_SUMMARY: {
            eltPos = findLastInScope(name);
            if (eltPos == NS_HTML5TREE_BUILDER_NOT_FOUND_ON_STACK) {
              errStrayEndTag(name);
            } else {
              generateImpliedEndTags();
              if (!!MOZ_UNLIKELY(mViewSource) && !isCurrent(name)) {
                errUnclosedElements(eltPos, name);
              }
              while (currentPtr >= eltPos) {
                pop();
              }
            }
            NS_HTML5_BREAK(endtagloop);
          }
          case NS_HTML5TREE_BUILDER_FORM: {
            if (!isTemplateContents()) {
              if (!formPointer) {
                errStrayEndTag(name);
                NS_HTML5_BREAK(endtagloop);
              }
              formPointer = nullptr;
              eltPos = findLastInScope(name);
              if (eltPos == NS_HTML5TREE_BUILDER_NOT_FOUND_ON_STACK) {
                errStrayEndTag(name);
                NS_HTML5_BREAK(endtagloop);
              }
              generateImpliedEndTags();
              if (!!MOZ_UNLIKELY(mViewSource) && !isCurrent(name)) {
                errUnclosedElements(eltPos, name);
              }
              removeFromStack(eltPos);
              NS_HTML5_BREAK(endtagloop);
            } else {
              eltPos = findLastInScope(name);
              if (eltPos == NS_HTML5TREE_BUILDER_NOT_FOUND_ON_STACK) {
                errStrayEndTag(name);
                NS_HTML5_BREAK(endtagloop);
              }
              generateImpliedEndTags();
              if (!!MOZ_UNLIKELY(mViewSource) && !isCurrent(name)) {
                errUnclosedElements(eltPos, name);
              }
              while (currentPtr >= eltPos) {
                pop();
              }
              NS_HTML5_BREAK(endtagloop);
            }
          }
          case NS_HTML5TREE_BUILDER_P: {
            eltPos = findLastInButtonScope(nsHtml5Atoms::p);
            if (eltPos == NS_HTML5TREE_BUILDER_NOT_FOUND_ON_STACK) {
              errNoElementToCloseButEndTagSeen(nsHtml5Atoms::p);
              if (isInForeign()) {
                errHtmlStartTagInForeignContext(name);
                while (stack[currentPtr]->ns != kNameSpaceID_XHTML) {
                  pop();
                }
              }
              appendVoidElementToCurrentMayFoster(elementName, nsHtml5HtmlAttributes::EMPTY_ATTRIBUTES);
              NS_HTML5_BREAK(endtagloop);
            }
            generateImpliedEndTagsExceptFor(nsHtml5Atoms::p);
            MOZ_ASSERT(eltPos != NS_HTML5TREE_BUILDER_NOT_FOUND_ON_STACK);
            if (!!MOZ_UNLIKELY(mViewSource) && eltPos != currentPtr) {
              errUnclosedElements(eltPos, name);
            }
            while (currentPtr >= eltPos) {
              pop();
            }
            NS_HTML5_BREAK(endtagloop);
          }
          case NS_HTML5TREE_BUILDER_LI: {
            eltPos = findLastInListScope(name);
            if (eltPos == NS_HTML5TREE_BUILDER_NOT_FOUND_ON_STACK) {
              errNoElementToCloseButEndTagSeen(name);
            } else {
              generateImpliedEndTagsExceptFor(name);
              if (!!MOZ_UNLIKELY(mViewSource) && eltPos != currentPtr) {
                errUnclosedElements(eltPos, name);
              }
              while (currentPtr >= eltPos) {
                pop();
              }
            }
            NS_HTML5_BREAK(endtagloop);
          }
          case NS_HTML5TREE_BUILDER_DD_OR_DT: {
            eltPos = findLastInScope(name);
            if (eltPos == NS_HTML5TREE_BUILDER_NOT_FOUND_ON_STACK) {
              errNoElementToCloseButEndTagSeen(name);
            } else {
              generateImpliedEndTagsExceptFor(name);
              if (!!MOZ_UNLIKELY(mViewSource) && eltPos != currentPtr) {
                errUnclosedElements(eltPos, name);
              }
              while (currentPtr >= eltPos) {
                pop();
              }
            }
            NS_HTML5_BREAK(endtagloop);
          }
          case NS_HTML5TREE_BUILDER_H1_OR_H2_OR_H3_OR_H4_OR_H5_OR_H6: {
            eltPos = findLastInScopeHn();
            if (eltPos == NS_HTML5TREE_BUILDER_NOT_FOUND_ON_STACK) {
              errStrayEndTag(name);
            } else {
              generateImpliedEndTags();
              if (!!MOZ_UNLIKELY(mViewSource) && !isCurrent(name)) {
                errUnclosedElements(eltPos, name);
              }
              while (currentPtr >= eltPos) {
                pop();
              }
            }
            NS_HTML5_BREAK(endtagloop);
          }
          case NS_HTML5TREE_BUILDER_OBJECT:
          case NS_HTML5TREE_BUILDER_MARQUEE_OR_APPLET: {
            eltPos = findLastInScope(name);
            if (eltPos == NS_HTML5TREE_BUILDER_NOT_FOUND_ON_STACK) {
              errStrayEndTag(name);
            } else {
              generateImpliedEndTags();
              if (!!MOZ_UNLIKELY(mViewSource) && !isCurrent(name)) {
                errUnclosedElements(eltPos, name);
              }
              while (currentPtr >= eltPos) {
                pop();
              }
              clearTheListOfActiveFormattingElementsUpToTheLastMarker();
            }
            NS_HTML5_BREAK(endtagloop);
          }
          case NS_HTML5TREE_BUILDER_BR: {
            errEndTagBr();
            if (isInForeign()) {
              errHtmlStartTagInForeignContext(name);
              while (stack[currentPtr]->ns != kNameSpaceID_XHTML) {
                pop();
              }
            }
            reconstructTheActiveFormattingElements();
            appendVoidElementToCurrentMayFoster(elementName, nsHtml5HtmlAttributes::EMPTY_ATTRIBUTES);
            NS_HTML5_BREAK(endtagloop);
          }
          case NS_HTML5TREE_BUILDER_TEMPLATE: {
            break;
          }
          case NS_HTML5TREE_BUILDER_AREA_OR_WBR:
#ifdef ENABLE_VOID_MENUITEM
          case NS_HTML5TREE_BUILDER_MENUITEM:
#endif
          case NS_HTML5TREE_BUILDER_PARAM_OR_SOURCE_OR_TRACK:
          case NS_HTML5TREE_BUILDER_EMBED:
          case NS_HTML5TREE_BUILDER_IMG:
          case NS_HTML5TREE_BUILDER_IMAGE:
          case NS_HTML5TREE_BUILDER_INPUT:
          case NS_HTML5TREE_BUILDER_KEYGEN:
          case NS_HTML5TREE_BUILDER_HR:
          case NS_HTML5TREE_BUILDER_ISINDEX:
          case NS_HTML5TREE_BUILDER_IFRAME:
          case NS_HTML5TREE_BUILDER_NOEMBED:
          case NS_HTML5TREE_BUILDER_NOFRAMES:
          case NS_HTML5TREE_BUILDER_SELECT:
          case NS_HTML5TREE_BUILDER_TABLE:
          case NS_HTML5TREE_BUILDER_TEXTAREA: {
            errStrayEndTag(name);
            NS_HTML5_BREAK(endtagloop);
          }
          case NS_HTML5TREE_BUILDER_NOSCRIPT: {
            if (scriptingEnabled) {
              errStrayEndTag(name);
              NS_HTML5_BREAK(endtagloop);
            } else {
            }
          }
          case NS_HTML5TREE_BUILDER_A:
          case NS_HTML5TREE_BUILDER_B_OR_BIG_OR_CODE_OR_EM_OR_I_OR_S_OR_SMALL_OR_STRIKE_OR_STRONG_OR_TT_OR_U:
          case NS_HTML5TREE_BUILDER_FONT:
          case NS_HTML5TREE_BUILDER_NOBR: {
            if (adoptionAgencyEndTag(name)) {
              NS_HTML5_BREAK(endtagloop);
            }
          }
          default: {
            if (isCurrent(name)) {
              pop();
              NS_HTML5_BREAK(endtagloop);
            }
            eltPos = currentPtr;
            for (; ; ) {
              nsHtml5StackNode* node = stack[eltPos];
              if (node->ns == kNameSpaceID_XHTML && node->name == name) {
                generateImpliedEndTags();
                if (!!MOZ_UNLIKELY(mViewSource) && !isCurrent(name)) {
                  errUnclosedElements(eltPos, name);
                }
                while (currentPtr >= eltPos) {
                  pop();
                }
                NS_HTML5_BREAK(endtagloop);
              } else if (node->isSpecial()) {
                errStrayEndTag(name);
                NS_HTML5_BREAK(endtagloop);
              }
              eltPos--;
            }
          }
        }
      }
      case NS_HTML5TREE_BUILDER_IN_HEAD: {
        switch(group) {
          case NS_HTML5TREE_BUILDER_HEAD: {
            pop();
            mode = NS_HTML5TREE_BUILDER_AFTER_HEAD;
            NS_HTML5_BREAK(endtagloop);
          }
          case NS_HTML5TREE_BUILDER_BR:
          case NS_HTML5TREE_BUILDER_HTML:
          case NS_HTML5TREE_BUILDER_BODY: {
            pop();
            mode = NS_HTML5TREE_BUILDER_AFTER_HEAD;
            continue;
          }
          case NS_HTML5TREE_BUILDER_TEMPLATE: {
            endTagTemplateInHead();
            NS_HTML5_BREAK(endtagloop);
          }
          default: {
            errStrayEndTag(name);
            NS_HTML5_BREAK(endtagloop);
          }
        }
      }
      case NS_HTML5TREE_BUILDER_IN_HEAD_NOSCRIPT: {
        switch(group) {
          case NS_HTML5TREE_BUILDER_NOSCRIPT: {
            pop();
            mode = NS_HTML5TREE_BUILDER_IN_HEAD;
            NS_HTML5_BREAK(endtagloop);
          }
          case NS_HTML5TREE_BUILDER_BR: {
            errStrayEndTag(name);
            pop();
            mode = NS_HTML5TREE_BUILDER_IN_HEAD;
            continue;
          }
          default: {
            errStrayEndTag(name);
            NS_HTML5_BREAK(endtagloop);
          }
        }
      }
      case NS_HTML5TREE_BUILDER_IN_COLUMN_GROUP: {
        switch(group) {
          case NS_HTML5TREE_BUILDER_COLGROUP: {
            if (!currentPtr || stack[currentPtr]->getGroup() == NS_HTML5TREE_BUILDER_TEMPLATE) {
              MOZ_ASSERT(fragment || isTemplateContents());
              errGarbageInColgroup();
              NS_HTML5_BREAK(endtagloop);
            }
            pop();
            mode = NS_HTML5TREE_BUILDER_IN_TABLE;
            NS_HTML5_BREAK(endtagloop);
          }
          case NS_HTML5TREE_BUILDER_COL: {
            errStrayEndTag(name);
            NS_HTML5_BREAK(endtagloop);
          }
          case NS_HTML5TREE_BUILDER_TEMPLATE: {
            endTagTemplateInHead();
            NS_HTML5_BREAK(endtagloop);
          }
          default: {
            if (!currentPtr || stack[currentPtr]->getGroup() == NS_HTML5TREE_BUILDER_TEMPLATE) {
              MOZ_ASSERT(fragment || isTemplateContents());
              errGarbageInColgroup();
              NS_HTML5_BREAK(endtagloop);
            }
            pop();
            mode = NS_HTML5TREE_BUILDER_IN_TABLE;
            continue;
          }
        }
      }
      case NS_HTML5TREE_BUILDER_IN_SELECT_IN_TABLE: {
        switch(group) {
          case NS_HTML5TREE_BUILDER_CAPTION:
          case NS_HTML5TREE_BUILDER_TABLE:
          case NS_HTML5TREE_BUILDER_TBODY_OR_THEAD_OR_TFOOT:
          case NS_HTML5TREE_BUILDER_TR:
          case NS_HTML5TREE_BUILDER_TD_OR_TH: {
            errEndTagSeenWithSelectOpen(name);
            if (findLastInTableScope(name) != NS_HTML5TREE_BUILDER_NOT_FOUND_ON_STACK) {
              eltPos = findLastInTableScope(nsHtml5Atoms::select);
              if (eltPos == NS_HTML5TREE_BUILDER_NOT_FOUND_ON_STACK) {
                MOZ_ASSERT(fragment);
                NS_HTML5_BREAK(endtagloop);
              }
              while (currentPtr >= eltPos) {
                pop();
              }
              resetTheInsertionMode();
              continue;
            } else {
              NS_HTML5_BREAK(endtagloop);
            }
          }
          default:
            ; // fall through
        }
      }
      case NS_HTML5TREE_BUILDER_IN_SELECT: {
        switch(group) {
          case NS_HTML5TREE_BUILDER_OPTION: {
            if (isCurrent(nsHtml5Atoms::option)) {
              pop();
              NS_HTML5_BREAK(endtagloop);
            } else {
              errStrayEndTag(name);
              NS_HTML5_BREAK(endtagloop);
            }
          }
          case NS_HTML5TREE_BUILDER_OPTGROUP: {
            if (isCurrent(nsHtml5Atoms::option) && nsHtml5Atoms::optgroup == stack[currentPtr - 1]->name) {
              pop();
            }
            if (isCurrent(nsHtml5Atoms::optgroup)) {
              pop();
            } else {
              errStrayEndTag(name);
            }
            NS_HTML5_BREAK(endtagloop);
          }
          case NS_HTML5TREE_BUILDER_SELECT: {
            eltPos = findLastInTableScope(nsHtml5Atoms::select);
            if (eltPos == NS_HTML5TREE_BUILDER_NOT_FOUND_ON_STACK) {
              MOZ_ASSERT(fragment);
              errStrayEndTag(name);
              NS_HTML5_BREAK(endtagloop);
            }
            while (currentPtr >= eltPos) {
              pop();
            }
            resetTheInsertionMode();
            NS_HTML5_BREAK(endtagloop);
          }
          case NS_HTML5TREE_BUILDER_TEMPLATE: {
            endTagTemplateInHead();
            NS_HTML5_BREAK(endtagloop);
          }
          default: {
            errStrayEndTag(name);
            NS_HTML5_BREAK(endtagloop);
          }
        }
      }
      case NS_HTML5TREE_BUILDER_AFTER_BODY: {
        switch(group) {
          case NS_HTML5TREE_BUILDER_HTML: {
            if (fragment) {
              errStrayEndTag(name);
              NS_HTML5_BREAK(endtagloop);
            } else {
              mode = NS_HTML5TREE_BUILDER_AFTER_AFTER_BODY;
              NS_HTML5_BREAK(endtagloop);
            }
          }
          default: {
            errEndTagAfterBody();
            mode = framesetOk ? NS_HTML5TREE_BUILDER_FRAMESET_OK : NS_HTML5TREE_BUILDER_IN_BODY;
            continue;
          }
        }
      }
      case NS_HTML5TREE_BUILDER_IN_FRAMESET: {
        switch(group) {
          case NS_HTML5TREE_BUILDER_FRAMESET: {
            if (!currentPtr) {
              MOZ_ASSERT(fragment);
              errStrayEndTag(name);
              NS_HTML5_BREAK(endtagloop);
            }
            pop();
            if ((!fragment) && !isCurrent(nsHtml5Atoms::frameset)) {
              mode = NS_HTML5TREE_BUILDER_AFTER_FRAMESET;
            }
            NS_HTML5_BREAK(endtagloop);
          }
          default: {
            errStrayEndTag(name);
            NS_HTML5_BREAK(endtagloop);
          }
        }
      }
      case NS_HTML5TREE_BUILDER_AFTER_FRAMESET: {
        switch(group) {
          case NS_HTML5TREE_BUILDER_HTML: {
            mode = NS_HTML5TREE_BUILDER_AFTER_AFTER_FRAMESET;
            NS_HTML5_BREAK(endtagloop);
          }
          default: {
            errStrayEndTag(name);
            NS_HTML5_BREAK(endtagloop);
          }
        }
      }
      case NS_HTML5TREE_BUILDER_INITIAL: {
        errEndTagSeenWithoutDoctype();
        documentModeInternal(QUIRKS_MODE, nullptr, nullptr, false);
        mode = NS_HTML5TREE_BUILDER_BEFORE_HTML;
        continue;
      }
      case NS_HTML5TREE_BUILDER_BEFORE_HTML: {
        switch(group) {
          case NS_HTML5TREE_BUILDER_HEAD:
          case NS_HTML5TREE_BUILDER_BR:
          case NS_HTML5TREE_BUILDER_HTML:
          case NS_HTML5TREE_BUILDER_BODY: {
            appendHtmlElementToDocumentAndPush();
            mode = NS_HTML5TREE_BUILDER_BEFORE_HEAD;
            continue;
          }
          default: {
            errStrayEndTag(name);
            NS_HTML5_BREAK(endtagloop);
          }
        }
      }
      case NS_HTML5TREE_BUILDER_BEFORE_HEAD: {
        switch(group) {
          case NS_HTML5TREE_BUILDER_HEAD:
          case NS_HTML5TREE_BUILDER_BR:
          case NS_HTML5TREE_BUILDER_HTML:
          case NS_HTML5TREE_BUILDER_BODY: {
            appendToCurrentNodeAndPushHeadElement(nsHtml5HtmlAttributes::EMPTY_ATTRIBUTES);
            mode = NS_HTML5TREE_BUILDER_IN_HEAD;
            continue;
          }
          default: {
            errStrayEndTag(name);
            NS_HTML5_BREAK(endtagloop);
          }
        }
      }
      case NS_HTML5TREE_BUILDER_AFTER_HEAD: {
        switch(group) {
          case NS_HTML5TREE_BUILDER_TEMPLATE: {
            endTagTemplateInHead();
            NS_HTML5_BREAK(endtagloop);
          }
          case NS_HTML5TREE_BUILDER_HTML:
          case NS_HTML5TREE_BUILDER_BODY:
          case NS_HTML5TREE_BUILDER_BR: {
            appendToCurrentNodeAndPushBodyElement();
            mode = NS_HTML5TREE_BUILDER_FRAMESET_OK;
            continue;
          }
          default: {
            errStrayEndTag(name);
            NS_HTML5_BREAK(endtagloop);
          }
        }
      }
      case NS_HTML5TREE_BUILDER_AFTER_AFTER_BODY: {
        errStrayEndTag(name);
        mode = framesetOk ? NS_HTML5TREE_BUILDER_FRAMESET_OK : NS_HTML5TREE_BUILDER_IN_BODY;
        continue;
      }
      case NS_HTML5TREE_BUILDER_AFTER_AFTER_FRAMESET: {
        errStrayEndTag(name);
        NS_HTML5_BREAK(endtagloop);
      }
      case NS_HTML5TREE_BUILDER_TEXT: {
        pop();
        if (originalMode == NS_HTML5TREE_BUILDER_AFTER_HEAD) {
          silentPop();
        }
        mode = originalMode;
        NS_HTML5_BREAK(endtagloop);
      }
    }
  }
  endtagloop_end: ;
}

void 
nsHtml5TreeBuilder::endTagTemplateInHead()
{
  int32_t eltPos = findLast(nsHtml5Atoms::template_);
  if (eltPos == NS_HTML5TREE_BUILDER_NOT_FOUND_ON_STACK) {
    errStrayEndTag(nsHtml5Atoms::template_);
    return;
  }
  generateImpliedEndTags();
  if (!!MOZ_UNLIKELY(mViewSource) && !isCurrent(nsHtml5Atoms::template_)) {
    errUnclosedElements(eltPos, nsHtml5Atoms::template_);
  }
  while (currentPtr >= eltPos) {
    pop();
  }
  clearTheListOfActiveFormattingElementsUpToTheLastMarker();
  popTemplateMode();
  resetTheInsertionMode();
}

int32_t 
nsHtml5TreeBuilder::findLastInTableScopeOrRootTemplateTbodyTheadTfoot()
{
  for (int32_t i = currentPtr; i > 0; i--) {
    if (stack[i]->getGroup() == NS_HTML5TREE_BUILDER_TBODY_OR_THEAD_OR_TFOOT || stack[i]->getGroup() == NS_HTML5TREE_BUILDER_TEMPLATE) {
      return i;
    }
  }
  return 0;
}

int32_t 
nsHtml5TreeBuilder::findLast(nsIAtom* name)
{
  for (int32_t i = currentPtr; i > 0; i--) {
    if (stack[i]->ns == kNameSpaceID_XHTML && stack[i]->name == name) {
      return i;
    }
  }
  return NS_HTML5TREE_BUILDER_NOT_FOUND_ON_STACK;
}

int32_t 
nsHtml5TreeBuilder::findLastInTableScope(nsIAtom* name)
{
  for (int32_t i = currentPtr; i > 0; i--) {
    if (stack[i]->ns == kNameSpaceID_XHTML) {
      if (stack[i]->name == name) {
        return i;
      } else if (stack[i]->name == nsHtml5Atoms::table || stack[i]->name == nsHtml5Atoms::template_) {
        return NS_HTML5TREE_BUILDER_NOT_FOUND_ON_STACK;
      }
    }
  }
  return NS_HTML5TREE_BUILDER_NOT_FOUND_ON_STACK;
}

int32_t 
nsHtml5TreeBuilder::findLastInButtonScope(nsIAtom* name)
{
  for (int32_t i = currentPtr; i > 0; i--) {
    if (stack[i]->ns == kNameSpaceID_XHTML) {
      if (stack[i]->name == name) {
        return i;
      } else if (stack[i]->name == nsHtml5Atoms::button) {
        return NS_HTML5TREE_BUILDER_NOT_FOUND_ON_STACK;
      }
    }
    if (stack[i]->isScoping()) {
      return NS_HTML5TREE_BUILDER_NOT_FOUND_ON_STACK;
    }
  }
  return NS_HTML5TREE_BUILDER_NOT_FOUND_ON_STACK;
}

int32_t 
nsHtml5TreeBuilder::findLastInScope(nsIAtom* name)
{
  for (int32_t i = currentPtr; i > 0; i--) {
    if (stack[i]->ns == kNameSpaceID_XHTML && stack[i]->name == name) {
      return i;
    } else if (stack[i]->isScoping()) {
      return NS_HTML5TREE_BUILDER_NOT_FOUND_ON_STACK;
    }
  }
  return NS_HTML5TREE_BUILDER_NOT_FOUND_ON_STACK;
}

int32_t 
nsHtml5TreeBuilder::findLastInListScope(nsIAtom* name)
{
  for (int32_t i = currentPtr; i > 0; i--) {
    if (stack[i]->ns == kNameSpaceID_XHTML) {
      if (stack[i]->name == name) {
        return i;
      } else if (stack[i]->name == nsHtml5Atoms::ul || stack[i]->name == nsHtml5Atoms::ol) {
        return NS_HTML5TREE_BUILDER_NOT_FOUND_ON_STACK;
      }
    }
    if (stack[i]->isScoping()) {
      return NS_HTML5TREE_BUILDER_NOT_FOUND_ON_STACK;
    }
  }
  return NS_HTML5TREE_BUILDER_NOT_FOUND_ON_STACK;
}

int32_t 
nsHtml5TreeBuilder::findLastInScopeHn()
{
  for (int32_t i = currentPtr; i > 0; i--) {
    if (stack[i]->getGroup() == NS_HTML5TREE_BUILDER_H1_OR_H2_OR_H3_OR_H4_OR_H5_OR_H6) {
      return i;
    } else if (stack[i]->isScoping()) {
      return NS_HTML5TREE_BUILDER_NOT_FOUND_ON_STACK;
    }
  }
  return NS_HTML5TREE_BUILDER_NOT_FOUND_ON_STACK;
}

void 
nsHtml5TreeBuilder::generateImpliedEndTagsExceptFor(nsIAtom* name)
{
  for (; ; ) {
    nsHtml5StackNode* node = stack[currentPtr];
    switch(node->getGroup()) {
      case NS_HTML5TREE_BUILDER_P:
      case NS_HTML5TREE_BUILDER_LI:
      case NS_HTML5TREE_BUILDER_DD_OR_DT:
      case NS_HTML5TREE_BUILDER_OPTION:
      case NS_HTML5TREE_BUILDER_OPTGROUP:
      case NS_HTML5TREE_BUILDER_RT_OR_RP: {
        if (node->ns == kNameSpaceID_XHTML && node->name == name) {
          return;
        }
        pop();
        continue;
      }
      default: {
        return;
      }
    }
  }
}

void 
nsHtml5TreeBuilder::generateImpliedEndTags()
{
  for (; ; ) {
    switch(stack[currentPtr]->getGroup()) {
      case NS_HTML5TREE_BUILDER_P:
      case NS_HTML5TREE_BUILDER_LI:
      case NS_HTML5TREE_BUILDER_DD_OR_DT:
      case NS_HTML5TREE_BUILDER_OPTION:
      case NS_HTML5TREE_BUILDER_OPTGROUP:
      case NS_HTML5TREE_BUILDER_RT_OR_RP: {
        pop();
        continue;
      }
      default: {
        return;
      }
    }
  }
}

bool 
nsHtml5TreeBuilder::isSecondOnStackBody()
{
  return currentPtr >= 1 && stack[1]->getGroup() == NS_HTML5TREE_BUILDER_BODY;
}

void 
nsHtml5TreeBuilder::documentModeInternal(nsHtml5DocumentMode m, nsString* publicIdentifier, nsString* systemIdentifier, bool html4SpecificAdditionalErrorChecks)
{
  if (isSrcdocDocument) {
    quirks = false;
    if (this) {
      this->documentMode(STANDARDS_MODE);
    }
    return;
  }
  quirks = (m == QUIRKS_MODE);
  if (this) {
    this->documentMode(m);
  }
}

bool 
nsHtml5TreeBuilder::isAlmostStandards(nsString* publicIdentifier, nsString* systemIdentifier)
{
  if (nsHtml5Portability::lowerCaseLiteralEqualsIgnoreAsciiCaseString("-//w3c//dtd xhtml 1.0 transitional//en", publicIdentifier)) {
    return true;
  }
  if (nsHtml5Portability::lowerCaseLiteralEqualsIgnoreAsciiCaseString("-//w3c//dtd xhtml 1.0 frameset//en", publicIdentifier)) {
    return true;
  }
  if (systemIdentifier) {
    if (nsHtml5Portability::lowerCaseLiteralEqualsIgnoreAsciiCaseString("-//w3c//dtd html 4.01 transitional//en", publicIdentifier)) {
      return true;
    }
    if (nsHtml5Portability::lowerCaseLiteralEqualsIgnoreAsciiCaseString("-//w3c//dtd html 4.01 frameset//en", publicIdentifier)) {
      return true;
    }
  }
  return false;
}

bool 
nsHtml5TreeBuilder::isQuirky(nsIAtom* name, nsString* publicIdentifier, nsString* systemIdentifier, bool forceQuirks)
{
  if (forceQuirks) {
    return true;
  }
  if (name != nsHtml5Atoms::html) {
    return true;
  }
  if (publicIdentifier) {
    for (int32_t i = 0; i < nsHtml5TreeBuilder::QUIRKY_PUBLIC_IDS.length; i++) {
      if (nsHtml5Portability::lowerCaseLiteralIsPrefixOfIgnoreAsciiCaseString(nsHtml5TreeBuilder::QUIRKY_PUBLIC_IDS[i], publicIdentifier)) {
        return true;
      }
    }
    if (nsHtml5Portability::lowerCaseLiteralEqualsIgnoreAsciiCaseString("-//w3o//dtd w3 html strict 3.0//en//", publicIdentifier) || nsHtml5Portability::lowerCaseLiteralEqualsIgnoreAsciiCaseString("-/w3c/dtd html 4.0 transitional/en", publicIdentifier) || nsHtml5Portability::lowerCaseLiteralEqualsIgnoreAsciiCaseString("html", publicIdentifier)) {
      return true;
    }
  }
  if (!systemIdentifier) {
    if (nsHtml5Portability::lowerCaseLiteralEqualsIgnoreAsciiCaseString("-//w3c//dtd html 4.01 transitional//en", publicIdentifier)) {
      return true;
    } else if (nsHtml5Portability::lowerCaseLiteralEqualsIgnoreAsciiCaseString("-//w3c//dtd html 4.01 frameset//en", publicIdentifier)) {
      return true;
    }
  } else if (nsHtml5Portability::lowerCaseLiteralEqualsIgnoreAsciiCaseString("http://www.ibm.com/data/dtd/v11/ibmxhtml1-transitional.dtd", systemIdentifier)) {
    return true;
  }
  return false;
}

void 
nsHtml5TreeBuilder::closeTheCell(int32_t eltPos)
{
  generateImpliedEndTags();
  if (!!MOZ_UNLIKELY(mViewSource) && eltPos != currentPtr) {
    errUnclosedElementsCell(eltPos);
  }
  while (currentPtr >= eltPos) {
    pop();
  }
  clearTheListOfActiveFormattingElementsUpToTheLastMarker();
  mode = NS_HTML5TREE_BUILDER_IN_ROW;
  return;
}

int32_t 
nsHtml5TreeBuilder::findLastInTableScopeTdTh()
{
  for (int32_t i = currentPtr; i > 0; i--) {
    nsIAtom* name = stack[i]->name;
    if (stack[i]->ns == kNameSpaceID_XHTML) {
      if (nsHtml5Atoms::td == name || nsHtml5Atoms::th == name) {
        return i;
      } else if (name == nsHtml5Atoms::table || name == nsHtml5Atoms::template_) {
        return NS_HTML5TREE_BUILDER_NOT_FOUND_ON_STACK;
      }
    }
  }
  return NS_HTML5TREE_BUILDER_NOT_FOUND_ON_STACK;
}

void 
nsHtml5TreeBuilder::clearStackBackTo(int32_t eltPos)
{
  int32_t eltGroup = stack[eltPos]->getGroup();
  while (currentPtr > eltPos) {
    if (stack[currentPtr]->ns == kNameSpaceID_XHTML && stack[currentPtr]->getGroup() == NS_HTML5TREE_BUILDER_TEMPLATE && (eltGroup == NS_HTML5TREE_BUILDER_TABLE || eltGroup == NS_HTML5TREE_BUILDER_TBODY_OR_THEAD_OR_TFOOT || eltGroup == NS_HTML5TREE_BUILDER_TR || eltGroup == NS_HTML5TREE_BUILDER_HTML)) {
      return;
    }
    pop();
  }
}

void 
nsHtml5TreeBuilder::resetTheInsertionMode()
{
  nsHtml5StackNode* node;
  nsIAtom* name;
  int32_t ns;
  for (int32_t i = currentPtr; i >= 0; i--) {
    node = stack[i];
    name = node->name;
    ns = node->ns;
    if (!i) {
      if (!(contextNamespace == kNameSpaceID_XHTML && (contextName == nsHtml5Atoms::td || contextName == nsHtml5Atoms::th))) {
        if (fragment) {
          name = contextName;
          ns = contextNamespace;
        }
      } else {
        mode = framesetOk ? NS_HTML5TREE_BUILDER_FRAMESET_OK : NS_HTML5TREE_BUILDER_IN_BODY;
        return;
      }
    }
    if (nsHtml5Atoms::select == name) {
      int32_t ancestorIndex = i;
      while (ancestorIndex > 0) {
        nsHtml5StackNode* ancestor = stack[ancestorIndex--];
        if (kNameSpaceID_XHTML == ancestor->ns) {
          if (nsHtml5Atoms::template_ == ancestor->name) {
            break;
          }
          if (nsHtml5Atoms::table == ancestor->name) {
            mode = NS_HTML5TREE_BUILDER_IN_SELECT_IN_TABLE;
            return;
          }
        }
      }
      mode = NS_HTML5TREE_BUILDER_IN_SELECT;
      return;
    } else if (nsHtml5Atoms::td == name || nsHtml5Atoms::th == name) {
      mode = NS_HTML5TREE_BUILDER_IN_CELL;
      return;
    } else if (nsHtml5Atoms::tr == name) {
      mode = NS_HTML5TREE_BUILDER_IN_ROW;
      return;
    } else if (nsHtml5Atoms::tbody == name || nsHtml5Atoms::thead == name || nsHtml5Atoms::tfoot == name) {
      mode = NS_HTML5TREE_BUILDER_IN_TABLE_BODY;
      return;
    } else if (nsHtml5Atoms::caption == name) {
      mode = NS_HTML5TREE_BUILDER_IN_CAPTION;
      return;
    } else if (nsHtml5Atoms::colgroup == name) {
      mode = NS_HTML5TREE_BUILDER_IN_COLUMN_GROUP;
      return;
    } else if (nsHtml5Atoms::table == name) {
      mode = NS_HTML5TREE_BUILDER_IN_TABLE;
      return;
    } else if (kNameSpaceID_XHTML != ns) {
      mode = framesetOk ? NS_HTML5TREE_BUILDER_FRAMESET_OK : NS_HTML5TREE_BUILDER_IN_BODY;
      return;
    } else if (nsHtml5Atoms::template_ == name) {
      MOZ_ASSERT(templateModePtr >= 0);
      mode = templateModeStack[templateModePtr];
      return;
    } else if (nsHtml5Atoms::head == name) {
      if (name == contextName) {
        mode = framesetOk ? NS_HTML5TREE_BUILDER_FRAMESET_OK : NS_HTML5TREE_BUILDER_IN_BODY;
      } else {
        mode = NS_HTML5TREE_BUILDER_IN_HEAD;
      }
      return;
    } else if (nsHtml5Atoms::body == name) {
      mode = framesetOk ? NS_HTML5TREE_BUILDER_FRAMESET_OK : NS_HTML5TREE_BUILDER_IN_BODY;
      return;
    } else if (nsHtml5Atoms::frameset == name) {
      mode = NS_HTML5TREE_BUILDER_IN_FRAMESET;
      return;
    } else if (nsHtml5Atoms::html == name) {
      if (!headPointer) {
        mode = NS_HTML5TREE_BUILDER_BEFORE_HEAD;
      } else {
        mode = NS_HTML5TREE_BUILDER_AFTER_HEAD;
      }
      return;
    } else if (!i) {
      mode = framesetOk ? NS_HTML5TREE_BUILDER_FRAMESET_OK : NS_HTML5TREE_BUILDER_IN_BODY;
      return;
    }
  }
}

void 
nsHtml5TreeBuilder::implicitlyCloseP()
{
  int32_t eltPos = findLastInButtonScope(nsHtml5Atoms::p);
  if (eltPos == NS_HTML5TREE_BUILDER_NOT_FOUND_ON_STACK) {
    return;
  }
  generateImpliedEndTagsExceptFor(nsHtml5Atoms::p);
  if (!!MOZ_UNLIKELY(mViewSource) && eltPos != currentPtr) {
    errUnclosedElementsImplied(eltPos, nsHtml5Atoms::p);
  }
  while (currentPtr >= eltPos) {
    pop();
  }
}

bool 
nsHtml5TreeBuilder::debugOnlyClearLastStackSlot()
{
  stack[currentPtr] = nullptr;
  return true;
}

bool 
nsHtml5TreeBuilder::debugOnlyClearLastListSlot()
{
  listOfActiveFormattingElements[listPtr] = nullptr;
  return true;
}

void 
nsHtml5TreeBuilder::pushTemplateMode(int32_t mode)
{
  templateModePtr++;
  if (templateModePtr == templateModeStack.length) {
    jArray<int32_t,int32_t> newStack = jArray<int32_t,int32_t>::newJArray(templateModeStack.length + 64);
    nsHtml5ArrayCopy::arraycopy(templateModeStack, newStack, templateModeStack.length);
    templateModeStack = newStack;
  }
  templateModeStack[templateModePtr] = mode;
}

void 
nsHtml5TreeBuilder::push(nsHtml5StackNode* node)
{
  currentPtr++;
  if (currentPtr == stack.length) {
    jArray<nsHtml5StackNode*,int32_t> newStack = jArray<nsHtml5StackNode*,int32_t>::newJArray(stack.length + 64);
    nsHtml5ArrayCopy::arraycopy(stack, newStack, stack.length);
    stack = newStack;
  }
  stack[currentPtr] = node;
  elementPushed(node->ns, node->popName, node->node);
}

void 
nsHtml5TreeBuilder::silentPush(nsHtml5StackNode* node)
{
  currentPtr++;
  if (currentPtr == stack.length) {
    jArray<nsHtml5StackNode*,int32_t> newStack = jArray<nsHtml5StackNode*,int32_t>::newJArray(stack.length + 64);
    nsHtml5ArrayCopy::arraycopy(stack, newStack, stack.length);
    stack = newStack;
  }
  stack[currentPtr] = node;
}

void 
nsHtml5TreeBuilder::append(nsHtml5StackNode* node)
{
  listPtr++;
  if (listPtr == listOfActiveFormattingElements.length) {
    jArray<nsHtml5StackNode*,int32_t> newList = jArray<nsHtml5StackNode*,int32_t>::newJArray(listOfActiveFormattingElements.length + 64);
    nsHtml5ArrayCopy::arraycopy(listOfActiveFormattingElements, newList, listOfActiveFormattingElements.length);
    listOfActiveFormattingElements = newList;
  }
  listOfActiveFormattingElements[listPtr] = node;
}

void 
nsHtml5TreeBuilder::clearTheListOfActiveFormattingElementsUpToTheLastMarker()
{
  while (listPtr > -1) {
    if (!listOfActiveFormattingElements[listPtr]) {
      --listPtr;
      return;
    }
    listOfActiveFormattingElements[listPtr]->release();
    --listPtr;
  }
}

void 
nsHtml5TreeBuilder::removeFromStack(int32_t pos)
{
  if (currentPtr == pos) {
    pop();
  } else {

    stack[pos]->release();
    nsHtml5ArrayCopy::arraycopy(stack, pos + 1, pos, currentPtr - pos);
    MOZ_ASSERT(debugOnlyClearLastStackSlot());
    currentPtr--;
  }
}

void 
nsHtml5TreeBuilder::removeFromStack(nsHtml5StackNode* node)
{
  if (stack[currentPtr] == node) {
    pop();
  } else {
    int32_t pos = currentPtr - 1;
    while (pos >= 0 && stack[pos] != node) {
      pos--;
    }
    if (pos == -1) {
      return;
    }

    node->release();
    nsHtml5ArrayCopy::arraycopy(stack, pos + 1, pos, currentPtr - pos);
    currentPtr--;
  }
}

void 
nsHtml5TreeBuilder::removeFromListOfActiveFormattingElements(int32_t pos)
{
  MOZ_ASSERT(!!listOfActiveFormattingElements[pos]);
  listOfActiveFormattingElements[pos]->release();
  if (pos == listPtr) {
    MOZ_ASSERT(debugOnlyClearLastListSlot());
    listPtr--;
    return;
  }
  MOZ_ASSERT(pos < listPtr);
  nsHtml5ArrayCopy::arraycopy(listOfActiveFormattingElements, pos + 1, pos, listPtr - pos);
  MOZ_ASSERT(debugOnlyClearLastListSlot());
  listPtr--;
}

bool 
nsHtml5TreeBuilder::adoptionAgencyEndTag(nsIAtom* name)
{
  if (stack[currentPtr]->ns == kNameSpaceID_XHTML && stack[currentPtr]->name == name && findInListOfActiveFormattingElements(stack[currentPtr]) == -1) {
    pop();
    return true;
  }
  for (int32_t i = 0; i < 8; ++i) {
    int32_t formattingEltListPos = listPtr;
    while (formattingEltListPos > -1) {
      nsHtml5StackNode* listNode = listOfActiveFormattingElements[formattingEltListPos];
      if (!listNode) {
        formattingEltListPos = -1;
        break;
      } else if (listNode->name == name) {
        break;
      }
      formattingEltListPos--;
    }
    if (formattingEltListPos == -1) {
      return false;
    }
    nsHtml5StackNode* formattingElt = listOfActiveFormattingElements[formattingEltListPos];
    int32_t formattingEltStackPos = currentPtr;
    bool inScope = true;
    while (formattingEltStackPos > -1) {
      nsHtml5StackNode* node = stack[formattingEltStackPos];
      if (node == formattingElt) {
        break;
      } else if (node->isScoping()) {
        inScope = false;
      }
      formattingEltStackPos--;
    }
    if (formattingEltStackPos == -1) {
      errNoElementToCloseButEndTagSeen(name);
      removeFromListOfActiveFormattingElements(formattingEltListPos);
      return true;
    }
    if (!inScope) {
      errNoElementToCloseButEndTagSeen(name);
      return true;
    }
    if (formattingEltStackPos != currentPtr) {
      errEndTagViolatesNestingRules(name);
    }
    int32_t furthestBlockPos = formattingEltStackPos + 1;
    while (furthestBlockPos <= currentPtr) {
      nsHtml5StackNode* node = stack[furthestBlockPos];
      if (node->isSpecial()) {
        break;
      }
      furthestBlockPos++;
    }
    if (furthestBlockPos > currentPtr) {
      while (currentPtr >= formattingEltStackPos) {
        pop();
      }
      removeFromListOfActiveFormattingElements(formattingEltListPos);
      return true;
    }
    nsHtml5StackNode* commonAncestor = stack[formattingEltStackPos - 1];
    nsHtml5StackNode* furthestBlock = stack[furthestBlockPos];
    int32_t bookmark = formattingEltListPos;
    int32_t nodePos = furthestBlockPos;
    nsHtml5StackNode* lastNode = furthestBlock;
    int32_t j = 0;
    for (; ; ) {
      ++j;
      nodePos--;
      if (nodePos == formattingEltStackPos) {
        break;
      }
      nsHtml5StackNode* node = stack[nodePos];
      int32_t nodeListPos = findInListOfActiveFormattingElements(node);
      if (j > 3 && nodeListPos != -1) {
        removeFromListOfActiveFormattingElements(nodeListPos);
        if (nodeListPos <= formattingEltListPos) {
          formattingEltListPos--;
        }
        if (nodeListPos <= bookmark) {
          bookmark--;
        }
        nodeListPos = -1;
      }
      if (nodeListPos == -1) {
        MOZ_ASSERT(formattingEltStackPos < nodePos);
        MOZ_ASSERT(bookmark < nodePos);
        MOZ_ASSERT(furthestBlockPos > nodePos);
        removeFromStack(nodePos);
        furthestBlockPos--;
        continue;
      }
      if (nodePos == furthestBlockPos) {
        bookmark = nodeListPos + 1;
      }
      MOZ_ASSERT(node == listOfActiveFormattingElements[nodeListPos]);
      MOZ_ASSERT(node == stack[nodePos]);
      nsIContentHandle* clone = createElement(kNameSpaceID_XHTML, node->name, node->attributes->cloneAttributes(nullptr));
      nsHtml5StackNode* newNode = new nsHtml5StackNode(node->getFlags(), node->ns, node->name, clone, node->popName, node->attributes);
      node->dropAttributes();
      stack[nodePos] = newNode;
      newNode->retain();
      listOfActiveFormattingElements[nodeListPos] = newNode;
      node->release();
      node->release();
      node = newNode;
      detachFromParent(lastNode->node);
      appendElement(lastNode->node, node->node);
      lastNode = node;
    }
    if (commonAncestor->isFosterParenting()) {

      detachFromParent(lastNode->node);
      insertIntoFosterParent(lastNode->node);
    } else {
      detachFromParent(lastNode->node);
      appendElement(lastNode->node, commonAncestor->node);
    }
    nsIContentHandle* clone = createElement(kNameSpaceID_XHTML, formattingElt->name, formattingElt->attributes->cloneAttributes(nullptr));
    nsHtml5StackNode* formattingClone = new nsHtml5StackNode(formattingElt->getFlags(), formattingElt->ns, formattingElt->name, clone, formattingElt->popName, formattingElt->attributes);
    formattingElt->dropAttributes();
    appendChildrenToNewParent(furthestBlock->node, clone);
    appendElement(clone, furthestBlock->node);
    removeFromListOfActiveFormattingElements(formattingEltListPos);
    insertIntoListOfActiveFormattingElements(formattingClone, bookmark);
    MOZ_ASSERT(formattingEltStackPos < furthestBlockPos);
    removeFromStack(formattingEltStackPos);
    insertIntoStack(formattingClone, furthestBlockPos);
  }
  return true;
}

void 
nsHtml5TreeBuilder::insertIntoStack(nsHtml5StackNode* node, int32_t position)
{
  MOZ_ASSERT(currentPtr + 1 < stack.length);
  MOZ_ASSERT(position <= currentPtr + 1);
  if (position == currentPtr + 1) {
    push(node);
  } else {
    nsHtml5ArrayCopy::arraycopy(stack, position, position + 1, (currentPtr - position) + 1);
    currentPtr++;
    stack[position] = node;
  }
}

void 
nsHtml5TreeBuilder::insertIntoListOfActiveFormattingElements(nsHtml5StackNode* formattingClone, int32_t bookmark)
{
  formattingClone->retain();
  MOZ_ASSERT(listPtr + 1 < listOfActiveFormattingElements.length);
  if (bookmark <= listPtr) {
    nsHtml5ArrayCopy::arraycopy(listOfActiveFormattingElements, bookmark, bookmark + 1, (listPtr - bookmark) + 1);
  }
  listPtr++;
  listOfActiveFormattingElements[bookmark] = formattingClone;
}

int32_t 
nsHtml5TreeBuilder::findInListOfActiveFormattingElements(nsHtml5StackNode* node)
{
  for (int32_t i = listPtr; i >= 0; i--) {
    if (node == listOfActiveFormattingElements[i]) {
      return i;
    }
  }
  return -1;
}

int32_t 
nsHtml5TreeBuilder::findInListOfActiveFormattingElementsContainsBetweenEndAndLastMarker(nsIAtom* name)
{
  for (int32_t i = listPtr; i >= 0; i--) {
    nsHtml5StackNode* node = listOfActiveFormattingElements[i];
    if (!node) {
      return -1;
    } else if (node->name == name) {
      return i;
    }
  }
  return -1;
}

void 
nsHtml5TreeBuilder::maybeForgetEarlierDuplicateFormattingElement(nsIAtom* name, nsHtml5HtmlAttributes* attributes)
{
  int32_t candidate = -1;
  int32_t count = 0;
  for (int32_t i = listPtr; i >= 0; i--) {
    nsHtml5StackNode* node = listOfActiveFormattingElements[i];
    if (!node) {
      break;
    }
    if (node->name == name && node->attributes->equalsAnother(attributes)) {
      candidate = i;
      ++count;
    }
  }
  if (count >= 3) {
    removeFromListOfActiveFormattingElements(candidate);
  }
}

int32_t 
nsHtml5TreeBuilder::findLastOrRoot(nsIAtom* name)
{
  for (int32_t i = currentPtr; i > 0; i--) {
    if (stack[i]->ns == kNameSpaceID_XHTML && stack[i]->name == name) {
      return i;
    }
  }
  return 0;
}

int32_t 
nsHtml5TreeBuilder::findLastOrRoot(int32_t group)
{
  for (int32_t i = currentPtr; i > 0; i--) {
    if (stack[i]->getGroup() == group) {
      return i;
    }
  }
  return 0;
}

bool 
nsHtml5TreeBuilder::addAttributesToBody(nsHtml5HtmlAttributes* attributes)
{
  if (currentPtr >= 1) {
    nsHtml5StackNode* body = stack[1];
    if (body->getGroup() == NS_HTML5TREE_BUILDER_BODY) {
      addAttributesToElement(body->node, attributes);
      return true;
    }
  }
  return false;
}

void 
nsHtml5TreeBuilder::addAttributesToHtml(nsHtml5HtmlAttributes* attributes)
{
  addAttributesToElement(stack[0]->node, attributes);
}

void 
nsHtml5TreeBuilder::pushHeadPointerOntoStack()
{
  MOZ_ASSERT(!!headPointer);
  MOZ_ASSERT(mode == NS_HTML5TREE_BUILDER_AFTER_HEAD);

  silentPush(new nsHtml5StackNode(nsHtml5ElementName::ELT_HEAD, headPointer));
}

void 
nsHtml5TreeBuilder::reconstructTheActiveFormattingElements()
{
  if (listPtr == -1) {
    return;
  }
  nsHtml5StackNode* mostRecent = listOfActiveFormattingElements[listPtr];
  if (!mostRecent || isInStack(mostRecent)) {
    return;
  }
  int32_t entryPos = listPtr;
  for (; ; ) {
    entryPos--;
    if (entryPos == -1) {
      break;
    }
    if (!listOfActiveFormattingElements[entryPos]) {
      break;
    }
    if (isInStack(listOfActiveFormattingElements[entryPos])) {
      break;
    }
  }
  while (entryPos < listPtr) {
    entryPos++;
    nsHtml5StackNode* entry = listOfActiveFormattingElements[entryPos];
    nsIContentHandle* clone = createElement(kNameSpaceID_XHTML, entry->name, entry->attributes->cloneAttributes(nullptr));
    nsHtml5StackNode* entryClone = new nsHtml5StackNode(entry->getFlags(), entry->ns, entry->name, clone, entry->popName, entry->attributes);
    entry->dropAttributes();
    nsHtml5StackNode* currentNode = stack[currentPtr];
    if (currentNode->isFosterParenting()) {
      insertIntoFosterParent(clone);
    } else {
      appendElement(clone, currentNode->node);
    }
    push(entryClone);
    listOfActiveFormattingElements[entryPos] = entryClone;
    entry->release();
    entryClone->retain();
  }
}

void 
nsHtml5TreeBuilder::insertIntoFosterParent(nsIContentHandle* child)
{
  int32_t tablePos = findLastOrRoot(NS_HTML5TREE_BUILDER_TABLE);
  int32_t templatePos = findLastOrRoot(NS_HTML5TREE_BUILDER_TEMPLATE);
  if (templatePos >= tablePos) {
    appendElement(child, stack[templatePos]->node);
    return;
  }
  nsHtml5StackNode* node = stack[tablePos];
  insertFosterParentedChild(child, node->node, stack[tablePos - 1]->node);
}

bool 
nsHtml5TreeBuilder::isInStack(nsHtml5StackNode* node)
{
  for (int32_t i = currentPtr; i >= 0; i--) {
    if (stack[i] == node) {
      return true;
    }
  }
  return false;
}

void 
nsHtml5TreeBuilder::popTemplateMode()
{
  templateModePtr--;
}

void 
nsHtml5TreeBuilder::pop()
{
  nsHtml5StackNode* node = stack[currentPtr];
  MOZ_ASSERT(debugOnlyClearLastStackSlot());
  currentPtr--;
  elementPopped(node->ns, node->popName, node->node);
  node->release();
}

void 
nsHtml5TreeBuilder::silentPop()
{
  nsHtml5StackNode* node = stack[currentPtr];
  MOZ_ASSERT(debugOnlyClearLastStackSlot());
  currentPtr--;
  node->release();
}

void 
nsHtml5TreeBuilder::popOnEof()
{
  nsHtml5StackNode* node = stack[currentPtr];
  MOZ_ASSERT(debugOnlyClearLastStackSlot());
  currentPtr--;
  markMalformedIfScript(node->node);
  elementPopped(node->ns, node->popName, node->node);
  node->release();
}

void 
nsHtml5TreeBuilder::appendHtmlElementToDocumentAndPush(nsHtml5HtmlAttributes* attributes)
{
  nsIContentHandle* elt = createHtmlElementSetAsRoot(attributes);
  nsHtml5StackNode* node = new nsHtml5StackNode(nsHtml5ElementName::ELT_HTML, elt);
  push(node);
}

void 
nsHtml5TreeBuilder::appendHtmlElementToDocumentAndPush()
{
  appendHtmlElementToDocumentAndPush(tokenizer->emptyAttributes());
}

void 
nsHtml5TreeBuilder::appendToCurrentNodeAndPushHeadElement(nsHtml5HtmlAttributes* attributes)
{
  nsIContentHandle* elt = createElement(kNameSpaceID_XHTML, nsHtml5Atoms::head, attributes);
  appendElement(elt, stack[currentPtr]->node);
  headPointer = elt;
  nsHtml5StackNode* node = new nsHtml5StackNode(nsHtml5ElementName::ELT_HEAD, elt);
  push(node);
}

void 
nsHtml5TreeBuilder::appendToCurrentNodeAndPushBodyElement(nsHtml5HtmlAttributes* attributes)
{
  appendToCurrentNodeAndPushElement(nsHtml5ElementName::ELT_BODY, attributes);
}

void 
nsHtml5TreeBuilder::appendToCurrentNodeAndPushBodyElement()
{
  appendToCurrentNodeAndPushBodyElement(tokenizer->emptyAttributes());
}

void 
nsHtml5TreeBuilder::appendToCurrentNodeAndPushFormElementMayFoster(nsHtml5HtmlAttributes* attributes)
{
  nsIContentHandle* elt = createElement(kNameSpaceID_XHTML, nsHtml5Atoms::form, attributes);
  if (!isTemplateContents()) {
    formPointer = elt;
  }
  nsHtml5StackNode* current = stack[currentPtr];
  if (current->isFosterParenting()) {

    insertIntoFosterParent(elt);
  } else {
    appendElement(elt, current->node);
  }
  nsHtml5StackNode* node = new nsHtml5StackNode(nsHtml5ElementName::ELT_FORM, elt);
  push(node);
}

void 
nsHtml5TreeBuilder::appendToCurrentNodeAndPushFormattingElementMayFoster(nsHtml5ElementName* elementName, nsHtml5HtmlAttributes* attributes)
{
  nsHtml5HtmlAttributes* clone = attributes->cloneAttributes(nullptr);
  nsIContentHandle* elt = createElement(kNameSpaceID_XHTML, elementName->name, attributes);
  nsHtml5StackNode* current = stack[currentPtr];
  if (current->isFosterParenting()) {

    insertIntoFosterParent(elt);
  } else {
    appendElement(elt, current->node);
  }
  nsHtml5StackNode* node = new nsHtml5StackNode(elementName, elt, clone);
  push(node);
  append(node);
  node->retain();
}

void 
nsHtml5TreeBuilder::appendToCurrentNodeAndPushElement(nsHtml5ElementName* elementName, nsHtml5HtmlAttributes* attributes)
{
  nsIContentHandle* elt = createElement(kNameSpaceID_XHTML, elementName->name, attributes);
  appendElement(elt, stack[currentPtr]->node);
  if (nsHtml5ElementName::ELT_TEMPLATE == elementName) {
    elt = getDocumentFragmentForTemplate(elt);
  }
  nsHtml5StackNode* node = new nsHtml5StackNode(elementName, elt);
  push(node);
}

void 
nsHtml5TreeBuilder::appendToCurrentNodeAndPushElementMayFoster(nsHtml5ElementName* elementName, nsHtml5HtmlAttributes* attributes)
{
  nsIAtom* popName = elementName->name;
  nsIContentHandle* elt = createElement(kNameSpaceID_XHTML, popName, attributes);
  nsHtml5StackNode* current = stack[currentPtr];
  if (current->isFosterParenting()) {

    insertIntoFosterParent(elt);
  } else {
    appendElement(elt, current->node);
  }
  nsHtml5StackNode* node = new nsHtml5StackNode(elementName, elt, popName);
  push(node);
}

void 
nsHtml5TreeBuilder::appendToCurrentNodeAndPushElementMayFosterMathML(nsHtml5ElementName* elementName, nsHtml5HtmlAttributes* attributes)
{
  nsIAtom* popName = elementName->name;
  bool markAsHtmlIntegrationPoint = false;
  if (nsHtml5ElementName::ELT_ANNOTATION_XML == elementName && annotationXmlEncodingPermitsHtml(attributes)) {
    markAsHtmlIntegrationPoint = true;
  }
  nsIContentHandle* elt = createElement(kNameSpaceID_MathML, popName, attributes);
  nsHtml5StackNode* current = stack[currentPtr];
  if (current->isFosterParenting()) {

    insertIntoFosterParent(elt);
  } else {
    appendElement(elt, current->node);
  }
  nsHtml5StackNode* node = new nsHtml5StackNode(elementName, elt, popName, markAsHtmlIntegrationPoint);
  push(node);
}

bool 
nsHtml5TreeBuilder::annotationXmlEncodingPermitsHtml(nsHtml5HtmlAttributes* attributes)
{
  nsString* encoding = attributes->getValue(nsHtml5AttributeName::ATTR_ENCODING);
  if (!encoding) {
    return false;
  }
  return nsHtml5Portability::lowerCaseLiteralEqualsIgnoreAsciiCaseString("application/xhtml+xml", encoding) || nsHtml5Portability::lowerCaseLiteralEqualsIgnoreAsciiCaseString("text/html", encoding);
}

void 
nsHtml5TreeBuilder::appendToCurrentNodeAndPushElementMayFosterSVG(nsHtml5ElementName* elementName, nsHtml5HtmlAttributes* attributes)
{
  nsIAtom* popName = elementName->camelCaseName;
  nsIContentHandle* elt = createElement(kNameSpaceID_SVG, popName, attributes);
  nsHtml5StackNode* current = stack[currentPtr];
  if (current->isFosterParenting()) {

    insertIntoFosterParent(elt);
  } else {
    appendElement(elt, current->node);
  }
  nsHtml5StackNode* node = new nsHtml5StackNode(elementName, popName, elt);
  push(node);
}

void 
nsHtml5TreeBuilder::appendToCurrentNodeAndPushElementMayFoster(nsHtml5ElementName* elementName, nsHtml5HtmlAttributes* attributes, nsIContentHandle* form)
{
  nsIContentHandle* elt = createElement(kNameSpaceID_XHTML, elementName->name, attributes, !form || fragment || isTemplateContents() ? nullptr : form);
  nsHtml5StackNode* current = stack[currentPtr];
  if (current->isFosterParenting()) {

    insertIntoFosterParent(elt);
  } else {
    appendElement(elt, current->node);
  }
  nsHtml5StackNode* node = new nsHtml5StackNode(elementName, elt);
  push(node);
}

void 
nsHtml5TreeBuilder::appendVoidElementToCurrentMayFoster(nsIAtom* name, nsHtml5HtmlAttributes* attributes, nsIContentHandle* form)
{
  nsIContentHandle* elt = createElement(kNameSpaceID_XHTML, name, attributes, !form || fragment || isTemplateContents() ? nullptr : form);
  nsHtml5StackNode* current = stack[currentPtr];
  if (current->isFosterParenting()) {

    insertIntoFosterParent(elt);
  } else {
    appendElement(elt, current->node);
  }
  elementPushed(kNameSpaceID_XHTML, name, elt);
  elementPopped(kNameSpaceID_XHTML, name, elt);
}

void 
nsHtml5TreeBuilder::appendVoidElementToCurrentMayFoster(nsHtml5ElementName* elementName, nsHtml5HtmlAttributes* attributes)
{
  nsIAtom* popName = elementName->name;
  nsIContentHandle* elt = createElement(kNameSpaceID_XHTML, popName, attributes);
  nsHtml5StackNode* current = stack[currentPtr];
  if (current->isFosterParenting()) {

    insertIntoFosterParent(elt);
  } else {
    appendElement(elt, current->node);
  }
  elementPushed(kNameSpaceID_XHTML, popName, elt);
  elementPopped(kNameSpaceID_XHTML, popName, elt);
}

void 
nsHtml5TreeBuilder::appendVoidElementToCurrentMayFosterSVG(nsHtml5ElementName* elementName, nsHtml5HtmlAttributes* attributes)
{
  nsIAtom* popName = elementName->camelCaseName;
  nsIContentHandle* elt = createElement(kNameSpaceID_SVG, popName, attributes);
  nsHtml5StackNode* current = stack[currentPtr];
  if (current->isFosterParenting()) {

    insertIntoFosterParent(elt);
  } else {
    appendElement(elt, current->node);
  }
  elementPushed(kNameSpaceID_SVG, popName, elt);
  elementPopped(kNameSpaceID_SVG, popName, elt);
}

void 
nsHtml5TreeBuilder::appendVoidElementToCurrentMayFosterMathML(nsHtml5ElementName* elementName, nsHtml5HtmlAttributes* attributes)
{
  nsIAtom* popName = elementName->name;
  nsIContentHandle* elt = createElement(kNameSpaceID_MathML, popName, attributes);
  nsHtml5StackNode* current = stack[currentPtr];
  if (current->isFosterParenting()) {

    insertIntoFosterParent(elt);
  } else {
    appendElement(elt, current->node);
  }
  elementPushed(kNameSpaceID_MathML, popName, elt);
  elementPopped(kNameSpaceID_MathML, popName, elt);
}

void 
nsHtml5TreeBuilder::appendVoidElementToCurrent(nsIAtom* name, nsHtml5HtmlAttributes* attributes, nsIContentHandle* form)
{
  nsIContentHandle* elt = createElement(kNameSpaceID_XHTML, name, attributes, !form || fragment || isTemplateContents() ? nullptr : form);
  nsHtml5StackNode* current = stack[currentPtr];
  appendElement(elt, current->node);
  elementPushed(kNameSpaceID_XHTML, name, elt);
  elementPopped(kNameSpaceID_XHTML, name, elt);
}

void 
nsHtml5TreeBuilder::appendVoidFormToCurrent(nsHtml5HtmlAttributes* attributes)
{
  nsIContentHandle* elt = createElement(kNameSpaceID_XHTML, nsHtml5Atoms::form, attributes);
  formPointer = elt;
  nsHtml5StackNode* current = stack[currentPtr];
  appendElement(elt, current->node);
  elementPushed(kNameSpaceID_XHTML, nsHtml5Atoms::form, elt);
  elementPopped(kNameSpaceID_XHTML, nsHtml5Atoms::form, elt);
}

void 
nsHtml5TreeBuilder::requestSuspension()
{
  tokenizer->requestSuspension();
}

bool 
nsHtml5TreeBuilder::isInForeign()
{
  return currentPtr >= 0 && stack[currentPtr]->ns != kNameSpaceID_XHTML;
}

bool 
nsHtml5TreeBuilder::isInForeignButNotHtmlOrMathTextIntegrationPoint()
{
  if (currentPtr < 0) {
    return false;
  }
  return !isSpecialParentInForeign(stack[currentPtr]);
}

void 
nsHtml5TreeBuilder::setFragmentContext(nsIAtom* context, int32_t ns, nsIContentHandle* node, bool quirks)
{
  this->contextName = context;
  this->contextNamespace = ns;
  this->contextNode = node;
  this->fragment = (!!contextName);
  this->quirks = quirks;
}

nsIContentHandle* 
nsHtml5TreeBuilder::currentNode()
{
  return stack[currentPtr]->node;
}

bool 
nsHtml5TreeBuilder::isScriptingEnabled()
{
  return scriptingEnabled;
}

void 
nsHtml5TreeBuilder::setScriptingEnabled(bool scriptingEnabled)
{
  this->scriptingEnabled = scriptingEnabled;
}

void 
nsHtml5TreeBuilder::setIsSrcdocDocument(bool isSrcdocDocument)
{
  this->isSrcdocDocument = isSrcdocDocument;
}

void 
nsHtml5TreeBuilder::flushCharacters()
{
  if (charBufferLen > 0) {
    if ((mode == NS_HTML5TREE_BUILDER_IN_TABLE || mode == NS_HTML5TREE_BUILDER_IN_TABLE_BODY || mode == NS_HTML5TREE_BUILDER_IN_ROW) && charBufferContainsNonWhitespace()) {
      errNonSpaceInTable();
      reconstructTheActiveFormattingElements();
      if (!stack[currentPtr]->isFosterParenting()) {
        appendCharacters(currentNode(), charBuffer, 0, charBufferLen);
        charBufferLen = 0;
        return;
      }
      int32_t tablePos = findLastOrRoot(NS_HTML5TREE_BUILDER_TABLE);
      int32_t templatePos = findLastOrRoot(NS_HTML5TREE_BUILDER_TEMPLATE);
      if (templatePos >= tablePos) {
        appendCharacters(stack[templatePos]->node, charBuffer, 0, charBufferLen);
        charBufferLen = 0;
        return;
      }
      nsHtml5StackNode* tableElt = stack[tablePos];
      insertFosterParentedCharacters(charBuffer, 0, charBufferLen, tableElt->node, stack[tablePos - 1]->node);
      charBufferLen = 0;
      return;
    }
    appendCharacters(currentNode(), charBuffer, 0, charBufferLen);
    charBufferLen = 0;
  }
}

bool 
nsHtml5TreeBuilder::charBufferContainsNonWhitespace()
{
  for (int32_t i = 0; i < charBufferLen; i++) {
    switch(charBuffer[i]) {
      case ' ':
      case '\t':
      case '\n':
      case '\r':
      case '\f': {
        continue;
      }
      default: {
        return true;
      }
    }
  }
  return false;
}

nsAHtml5TreeBuilderState* 
nsHtml5TreeBuilder::newSnapshot()
{
  jArray<nsHtml5StackNode*,int32_t> listCopy = jArray<nsHtml5StackNode*,int32_t>::newJArray(listPtr + 1);
  for (int32_t i = 0; i < listCopy.length; i++) {
    nsHtml5StackNode* node = listOfActiveFormattingElements[i];
    if (node) {
      nsHtml5StackNode* newNode = new nsHtml5StackNode(node->getFlags(), node->ns, node->name, node->node, node->popName, node->attributes->cloneAttributes(nullptr));
      listCopy[i] = newNode;
    } else {
      listCopy[i] = nullptr;
    }
  }
  jArray<nsHtml5StackNode*,int32_t> stackCopy = jArray<nsHtml5StackNode*,int32_t>::newJArray(currentPtr + 1);
  for (int32_t i = 0; i < stackCopy.length; i++) {
    nsHtml5StackNode* node = stack[i];
    int32_t listIndex = findInListOfActiveFormattingElements(node);
    if (listIndex == -1) {
      nsHtml5StackNode* newNode = new nsHtml5StackNode(node->getFlags(), node->ns, node->name, node->node, node->popName, nullptr);
      stackCopy[i] = newNode;
    } else {
      stackCopy[i] = listCopy[listIndex];
      stackCopy[i]->retain();
    }
  }
  jArray<int32_t,int32_t> templateModeStackCopy = jArray<int32_t,int32_t>::newJArray(templateModePtr + 1);
  nsHtml5ArrayCopy::arraycopy(templateModeStack, templateModeStackCopy, templateModeStackCopy.length);
  return new nsHtml5StateSnapshot(stackCopy, listCopy, templateModeStackCopy, formPointer, headPointer, deepTreeSurrogateParent, mode, originalMode, framesetOk, needToDropLF, quirks);
}

bool 
nsHtml5TreeBuilder::snapshotMatches(nsAHtml5TreeBuilderState* snapshot)
{
  jArray<nsHtml5StackNode*,int32_t> stackCopy = snapshot->getStack();
  int32_t stackLen = snapshot->getStackLength();
  jArray<nsHtml5StackNode*,int32_t> listCopy = snapshot->getListOfActiveFormattingElements();
  int32_t listLen = snapshot->getListOfActiveFormattingElementsLength();
  jArray<int32_t,int32_t> templateModeStackCopy = snapshot->getTemplateModeStack();
  int32_t templateModeStackLen = snapshot->getTemplateModeStackLength();
  if (stackLen != currentPtr + 1 || listLen != listPtr + 1 || templateModeStackLen != templateModePtr + 1 || formPointer != snapshot->getFormPointer() || headPointer != snapshot->getHeadPointer() || deepTreeSurrogateParent != snapshot->getDeepTreeSurrogateParent() || mode != snapshot->getMode() || originalMode != snapshot->getOriginalMode() || framesetOk != snapshot->isFramesetOk() || needToDropLF != snapshot->isNeedToDropLF() || quirks != snapshot->isQuirks()) {
    return false;
  }
  for (int32_t i = listLen - 1; i >= 0; i--) {
    if (!listCopy[i] && !listOfActiveFormattingElements[i]) {
      continue;
    } else if (!listCopy[i] || !listOfActiveFormattingElements[i]) {
      return false;
    }
    if (listCopy[i]->node != listOfActiveFormattingElements[i]->node) {
      return false;
    }
  }
  for (int32_t i = stackLen - 1; i >= 0; i--) {
    if (stackCopy[i]->node != stack[i]->node) {
      return false;
    }
  }
  for (int32_t i = templateModeStackLen - 1; i >= 0; i--) {
    if (templateModeStackCopy[i] != templateModeStack[i]) {
      return false;
    }
  }
  return true;
}

void 
nsHtml5TreeBuilder::loadState(nsAHtml5TreeBuilderState* snapshot, nsHtml5AtomTable* interner)
{
  jArray<nsHtml5StackNode*,int32_t> stackCopy = snapshot->getStack();
  int32_t stackLen = snapshot->getStackLength();
  jArray<nsHtml5StackNode*,int32_t> listCopy = snapshot->getListOfActiveFormattingElements();
  int32_t listLen = snapshot->getListOfActiveFormattingElementsLength();
  jArray<int32_t,int32_t> templateModeStackCopy = snapshot->getTemplateModeStack();
  int32_t templateModeStackLen = snapshot->getTemplateModeStackLength();
  for (int32_t i = 0; i <= listPtr; i++) {
    if (listOfActiveFormattingElements[i]) {
      listOfActiveFormattingElements[i]->release();
    }
  }
  if (listOfActiveFormattingElements.length < listLen) {
    listOfActiveFormattingElements = jArray<nsHtml5StackNode*,int32_t>::newJArray(listLen);
  }
  listPtr = listLen - 1;
  for (int32_t i = 0; i <= currentPtr; i++) {
    stack[i]->release();
  }
  if (stack.length < stackLen) {
    stack = jArray<nsHtml5StackNode*,int32_t>::newJArray(stackLen);
  }
  currentPtr = stackLen - 1;
  if (templateModeStack.length < templateModeStackLen) {
    templateModeStack = jArray<int32_t,int32_t>::newJArray(templateModeStackLen);
  }
  templateModePtr = templateModeStackLen - 1;
  for (int32_t i = 0; i < listLen; i++) {
    nsHtml5StackNode* node = listCopy[i];
    if (node) {
      nsHtml5StackNode* newNode = new nsHtml5StackNode(node->getFlags(), node->ns, nsHtml5Portability::newLocalFromLocal(node->name, interner), node->node, nsHtml5Portability::newLocalFromLocal(node->popName, interner), node->attributes->cloneAttributes(nullptr));
      listOfActiveFormattingElements[i] = newNode;
    } else {
      listOfActiveFormattingElements[i] = nullptr;
    }
  }
  for (int32_t i = 0; i < stackLen; i++) {
    nsHtml5StackNode* node = stackCopy[i];
    int32_t listIndex = findInArray(node, listCopy);
    if (listIndex == -1) {
      nsHtml5StackNode* newNode = new nsHtml5StackNode(node->getFlags(), node->ns, nsHtml5Portability::newLocalFromLocal(node->name, interner), node->node, nsHtml5Portability::newLocalFromLocal(node->popName, interner), nullptr);
      stack[i] = newNode;
    } else {
      stack[i] = listOfActiveFormattingElements[listIndex];
      stack[i]->retain();
    }
  }
  nsHtml5ArrayCopy::arraycopy(templateModeStackCopy, templateModeStack, templateModeStackLen);
  formPointer = snapshot->getFormPointer();
  headPointer = snapshot->getHeadPointer();
  deepTreeSurrogateParent = snapshot->getDeepTreeSurrogateParent();
  mode = snapshot->getMode();
  originalMode = snapshot->getOriginalMode();
  framesetOk = snapshot->isFramesetOk();
  needToDropLF = snapshot->isNeedToDropLF();
  quirks = snapshot->isQuirks();
}

int32_t 
nsHtml5TreeBuilder::findInArray(nsHtml5StackNode* node, jArray<nsHtml5StackNode*,int32_t> arr)
{
  for (int32_t i = listPtr; i >= 0; i--) {
    if (node == arr[i]) {
      return i;
    }
  }
  return -1;
}

nsIContentHandle* 
nsHtml5TreeBuilder::getFormPointer()
{
  return formPointer;
}

nsIContentHandle* 
nsHtml5TreeBuilder::getHeadPointer()
{
  return headPointer;
}

nsIContentHandle* 
nsHtml5TreeBuilder::getDeepTreeSurrogateParent()
{
  return deepTreeSurrogateParent;
}

jArray<nsHtml5StackNode*,int32_t> 
nsHtml5TreeBuilder::getListOfActiveFormattingElements()
{
  return listOfActiveFormattingElements;
}

jArray<nsHtml5StackNode*,int32_t> 
nsHtml5TreeBuilder::getStack()
{
  return stack;
}

jArray<int32_t,int32_t> 
nsHtml5TreeBuilder::getTemplateModeStack()
{
  return templateModeStack;
}

int32_t 
nsHtml5TreeBuilder::getMode()
{
  return mode;
}

int32_t 
nsHtml5TreeBuilder::getOriginalMode()
{
  return originalMode;
}

bool 
nsHtml5TreeBuilder::isFramesetOk()
{
  return framesetOk;
}

bool 
nsHtml5TreeBuilder::isNeedToDropLF()
{
  return needToDropLF;
}

bool 
nsHtml5TreeBuilder::isQuirks()
{
  return quirks;
}

int32_t 
nsHtml5TreeBuilder::getListOfActiveFormattingElementsLength()
{
  return listPtr + 1;
}

int32_t 
nsHtml5TreeBuilder::getStackLength()
{
  return currentPtr + 1;
}

int32_t 
nsHtml5TreeBuilder::getTemplateModeStackLength()
{
  return templateModePtr + 1;
}

void
nsHtml5TreeBuilder::initializeStatics()
{
}

void
nsHtml5TreeBuilder::releaseStatics()
{
}


#include "nsHtml5TreeBuilderCppSupplement.h"

