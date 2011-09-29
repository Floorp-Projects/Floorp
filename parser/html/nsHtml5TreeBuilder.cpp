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

#include "prtypes.h"
#include "nsIAtom.h"
#include "nsHtml5AtomTable.h"
#include "nsITimer.h"
#include "nsString.h"
#include "nsINameSpaceManager.h"
#include "nsIContent.h"
#include "nsIDocument.h"
#include "nsTraceRefcnt.h"
#include "jArray.h"
#include "nsHtml5DocumentMode.h"
#include "nsHtml5ArrayCopy.h"
#include "nsHtml5Parser.h"
#include "nsHtml5Atoms.h"
#include "nsHtml5ByteReadable.h"
#include "nsHtml5TreeOperation.h"
#include "nsHtml5PendingNotification.h"
#include "nsHtml5StateSnapshot.h"
#include "nsHtml5StackNode.h"
#include "nsHtml5TreeOpExecutor.h"
#include "nsHtml5StreamParser.h"
#include "nsAHtml5TreeBuilderState.h"

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

PRUnichar nsHtml5TreeBuilder::REPLACEMENT_CHARACTER[] = { 0xfffd };
static const char* const QUIRKY_PUBLIC_IDS_DATA[] = { "+//silmaril//dtd html pro v0r11 19970101//", "-//advasoft ltd//dtd html 3.0 aswedit + extensions//", "-//as//dtd html 3.0 aswedit + extensions//", "-//ietf//dtd html 2.0 level 1//", "-//ietf//dtd html 2.0 level 2//", "-//ietf//dtd html 2.0 strict level 1//", "-//ietf//dtd html 2.0 strict level 2//", "-//ietf//dtd html 2.0 strict//", "-//ietf//dtd html 2.0//", "-//ietf//dtd html 2.1e//", "-//ietf//dtd html 3.0//", "-//ietf//dtd html 3.2 final//", "-//ietf//dtd html 3.2//", "-//ietf//dtd html 3//", "-//ietf//dtd html level 0//", "-//ietf//dtd html level 1//", "-//ietf//dtd html level 2//", "-//ietf//dtd html level 3//", "-//ietf//dtd html strict level 0//", "-//ietf//dtd html strict level 1//", "-//ietf//dtd html strict level 2//", "-//ietf//dtd html strict level 3//", "-//ietf//dtd html strict//", "-//ietf//dtd html//", "-//metrius//dtd metrius presentational//", "-//microsoft//dtd internet explorer 2.0 html strict//", "-//microsoft//dtd internet explorer 2.0 html//", "-//microsoft//dtd internet explorer 2.0 tables//", "-//microsoft//dtd internet explorer 3.0 html strict//", "-//microsoft//dtd internet explorer 3.0 html//", "-//microsoft//dtd internet explorer 3.0 tables//", "-//netscape comm. corp.//dtd html//", "-//netscape comm. corp.//dtd strict html//", "-//o'reilly and associates//dtd html 2.0//", "-//o'reilly and associates//dtd html extended 1.0//", "-//o'reilly and associates//dtd html extended relaxed 1.0//", "-//softquad software//dtd hotmetal pro 6.0::19990601::extensions to html 4.0//", "-//softquad//dtd hotmetal pro 4.0::19971010::extensions to html 4.0//", "-//spyglass//dtd html 2.0 extended//", "-//sq//dtd html 2.0 hotmetal + extensions//", "-//sun microsystems corp.//dtd hotjava html//", "-//sun microsystems corp.//dtd hotjava strict html//", "-//w3c//dtd html 3 1995-03-24//", "-//w3c//dtd html 3.2 draft//", "-//w3c//dtd html 3.2 final//", "-//w3c//dtd html 3.2//", "-//w3c//dtd html 3.2s draft//", "-//w3c//dtd html 4.0 frameset//", "-//w3c//dtd html 4.0 transitional//", "-//w3c//dtd html experimental 19960712//", "-//w3c//dtd html experimental 970421//", "-//w3c//dtd w3 html//", "-//w3o//dtd w3 html 3.0//", "-//webtechs//dtd mozilla html 2.0//", "-//webtechs//dtd mozilla html//" };
staticJArray<const char*,PRInt32> nsHtml5TreeBuilder::QUIRKY_PUBLIC_IDS = { QUIRKY_PUBLIC_IDS_DATA, NS_ARRAY_LENGTH(QUIRKY_PUBLIC_IDS_DATA) };
void 
nsHtml5TreeBuilder::startTokenization(nsHtml5Tokenizer* self)
{
  tokenizer = self;
  stack = jArray<nsHtml5StackNode*,PRInt32>::newJArray(64);
  listOfActiveFormattingElements = jArray<nsHtml5StackNode*,PRInt32>::newJArray(64);
  needToDropLF = PR_FALSE;
  originalMode = NS_HTML5TREE_BUILDER_INITIAL;
  currentPtr = -1;
  listPtr = -1;
  formPointer = nsnull;
  headPointer = nsnull;
  deepTreeSurrogateParent = nsnull;
  start(fragment);
  charBufferLen = 0;
  charBuffer = jArray<PRUnichar,PRInt32>::newJArray(1024);
  framesetOk = PR_TRUE;
  if (fragment) {
    nsIContent** elt;
    if (contextNode) {
      elt = contextNode;
    } else {
      elt = createHtmlElementSetAsRoot(tokenizer->emptyAttributes());
    }
    nsHtml5StackNode* node = new nsHtml5StackNode(nsHtml5ElementName::ELT_HTML, elt);
    currentPtr++;
    stack[currentPtr] = node;
    resetTheInsertionMode();
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
    contextName = nsnull;
    contextNode = nsnull;
  } else {
    mode = NS_HTML5TREE_BUILDER_INITIAL;
  }
}

void 
nsHtml5TreeBuilder::doctype(nsIAtom* name, nsString* publicIdentifier, nsString* systemIdentifier, bool forceQuirks)
{
  needToDropLF = PR_FALSE;
  if (!isInForeign()) {
    switch(mode) {
      case NS_HTML5TREE_BUILDER_INITIAL: {
        nsString* emptyString = nsHtml5Portability::newEmptyString();
        appendDoctypeToDocument(!name ? nsHtml5Atoms::emptystring : name, !publicIdentifier ? emptyString : publicIdentifier, !systemIdentifier ? emptyString : systemIdentifier);
        nsHtml5Portability::releaseString(emptyString);
        if (isQuirky(name, publicIdentifier, systemIdentifier, forceQuirks)) {

          documentModeInternal(QUIRKS_MODE, publicIdentifier, systemIdentifier, PR_FALSE);
        } else if (isAlmostStandards(publicIdentifier, systemIdentifier)) {

          documentModeInternal(ALMOST_STANDARDS_MODE, publicIdentifier, systemIdentifier, PR_FALSE);
        } else {
          documentModeInternal(STANDARDS_MODE, publicIdentifier, systemIdentifier, PR_FALSE);
        }
        mode = NS_HTML5TREE_BUILDER_BEFORE_HTML;
        return;
      }
      default: {
        break;
      }
    }
  }

  return;
}

void 
nsHtml5TreeBuilder::comment(PRUnichar* buf, PRInt32 start, PRInt32 length)
{
  needToDropLF = PR_FALSE;
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
nsHtml5TreeBuilder::characters(const PRUnichar* buf, PRInt32 start, PRInt32 length)
{
  if (needToDropLF) {
    needToDropLF = PR_FALSE;
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
      if (!isInForeignButNotHtmlIntegrationPoint()) {
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
      PRInt32 end = start + length;
      for (PRInt32 i = start; i < end; i++) {
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
              case NS_HTML5TREE_BUILDER_IN_BODY:
              case NS_HTML5TREE_BUILDER_IN_CELL:
              case NS_HTML5TREE_BUILDER_IN_CAPTION: {
                if (start < i) {
                  accumulateCharacters(buf, start, i - start);
                  start = i;
                }
                if (!isInForeignButNotHtmlIntegrationPoint()) {
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
                documentModeInternal(QUIRKS_MODE, nsnull, nsnull, PR_FALSE);
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
                framesetOk = PR_FALSE;
                mode = NS_HTML5TREE_BUILDER_IN_BODY;
                i--;
                continue;
              }
              case NS_HTML5TREE_BUILDER_IN_BODY:
              case NS_HTML5TREE_BUILDER_IN_CELL:
              case NS_HTML5TREE_BUILDER_IN_CAPTION: {
                if (start < i) {
                  accumulateCharacters(buf, start, i - start);
                  start = i;
                }
                if (!isInForeignButNotHtmlIntegrationPoint()) {
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
                if (!currentPtr) {

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


                mode = framesetOk ? NS_HTML5TREE_BUILDER_FRAMESET_OK : NS_HTML5TREE_BUILDER_IN_BODY;
                i--;
                continue;
              }
              case NS_HTML5TREE_BUILDER_IN_FRAMESET: {
                if (start < i) {
                  accumulateCharacters(buf, start, i - start);
                  start = i;
                }

                start = i + 1;
                continue;
              }
              case NS_HTML5TREE_BUILDER_AFTER_FRAMESET: {
                if (start < i) {
                  accumulateCharacters(buf, start, i - start);
                  start = i;
                }

                start = i + 1;
                continue;
              }
              case NS_HTML5TREE_BUILDER_AFTER_AFTER_BODY: {

                mode = framesetOk ? NS_HTML5TREE_BUILDER_FRAMESET_OK : NS_HTML5TREE_BUILDER_IN_BODY;
                i--;
                continue;
              }
              case NS_HTML5TREE_BUILDER_AFTER_AFTER_FRAMESET: {

                mode = NS_HTML5TREE_BUILDER_IN_FRAMESET;
                i--;
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
    nsHtml5StackNode* stackNode = stack[currentPtr];
    if (stackNode->ns == kNameSpaceID_XHTML) {
      return;
    }
    if (stackNode->isHtmlIntegrationPoint()) {
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
    if (isInForeign()) {

      NS_HTML5_BREAK(eofloop);
    }
    switch(mode) {
      case NS_HTML5TREE_BUILDER_INITIAL: {
        documentModeInternal(QUIRKS_MODE, nsnull, nsnull, PR_FALSE);
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
      case NS_HTML5TREE_BUILDER_IN_COLUMN_GROUP: {
        if (!currentPtr) {

          NS_HTML5_BREAK(eofloop);
        } else {
          popOnEof();
          mode = NS_HTML5TREE_BUILDER_IN_TABLE;
          continue;
        }
      }
      case NS_HTML5TREE_BUILDER_FRAMESET_OK:
      case NS_HTML5TREE_BUILDER_IN_CAPTION:
      case NS_HTML5TREE_BUILDER_IN_CELL:
      case NS_HTML5TREE_BUILDER_IN_BODY: {
        NS_HTML5_BREAK(eofloop);
      }
      case NS_HTML5TREE_BUILDER_TEXT: {

        if (originalMode == NS_HTML5TREE_BUILDER_AFTER_HEAD) {
          popOnEof();
        }
        popOnEof();
        mode = originalMode;
        continue;
      }
      case NS_HTML5TREE_BUILDER_IN_TABLE_BODY:
      case NS_HTML5TREE_BUILDER_IN_ROW:
      case NS_HTML5TREE_BUILDER_IN_TABLE:
      case NS_HTML5TREE_BUILDER_IN_SELECT:
      case NS_HTML5TREE_BUILDER_IN_SELECT_IN_TABLE:
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
  formPointer = nsnull;
  headPointer = nsnull;
  deepTreeSurrogateParent = nsnull;
  if (stack) {
    while (currentPtr > -1) {
      stack[currentPtr]->release();
      currentPtr--;
    }
    stack = nsnull;
  }
  if (listOfActiveFormattingElements) {
    while (listPtr > -1) {
      if (listOfActiveFormattingElements[listPtr]) {
        listOfActiveFormattingElements[listPtr]->release();
      }
      listPtr--;
    }
    listOfActiveFormattingElements = nsnull;
  }
  charBuffer = nsnull;
  end();
}

void 
nsHtml5TreeBuilder::startTag(nsHtml5ElementName* elementName, nsHtml5HtmlAttributes* attributes, bool selfClosing)
{
  flushCharacters();
  PRInt32 eltPos;
  needToDropLF = PR_FALSE;
  starttagloop: for (; ; ) {
    PRInt32 group = elementName->getGroup();
    nsIAtom* name = elementName->name;
    if (isInForeign()) {
      nsHtml5StackNode* currentNode = stack[currentPtr];
      PRInt32 currNs = currentNode->ns;
      if (!(currentNode->isHtmlIntegrationPoint() || (currNs == kNameSpaceID_MathML && ((currentNode->getGroup() == NS_HTML5TREE_BUILDER_MI_MO_MN_MS_MTEXT && group != NS_HTML5TREE_BUILDER_MGLYPH_OR_MALIGNMARK) || (currentNode->getGroup() == NS_HTML5TREE_BUILDER_ANNOTATION_XML && group == NS_HTML5TREE_BUILDER_SVG))))) {
        switch(group) {
          case NS_HTML5TREE_BUILDER_B_OR_BIG_OR_CODE_OR_EM_OR_I_OR_S_OR_SMALL_OR_STRIKE_OR_STRONG_OR_TT_OR_U:
          case NS_HTML5TREE_BUILDER_DIV_OR_BLOCKQUOTE_OR_CENTER_OR_MENU:
          case NS_HTML5TREE_BUILDER_BODY:
          case NS_HTML5TREE_BUILDER_BR:
          case NS_HTML5TREE_BUILDER_RUBY_OR_SPAN_OR_SUB_OR_SUP_OR_VAR:
          case NS_HTML5TREE_BUILDER_DD_OR_DT:
          case NS_HTML5TREE_BUILDER_UL_OR_OL_OR_DL:
          case NS_HTML5TREE_BUILDER_EMBED_OR_IMG:
          case NS_HTML5TREE_BUILDER_H1_OR_H2_OR_H3_OR_H4_OR_H5_OR_H6:
          case NS_HTML5TREE_BUILDER_HEAD:
          case NS_HTML5TREE_BUILDER_HR:
          case NS_HTML5TREE_BUILDER_LI:
          case NS_HTML5TREE_BUILDER_META:
          case NS_HTML5TREE_BUILDER_NOBR:
          case NS_HTML5TREE_BUILDER_P:
          case NS_HTML5TREE_BUILDER_PRE_OR_LISTING:
          case NS_HTML5TREE_BUILDER_TABLE: {

            while (!isSpecialParentInForeign(stack[currentPtr])) {
              pop();
            }
            NS_HTML5_CONTINUE(starttagloop);
          }
          case NS_HTML5TREE_BUILDER_FONT: {
            if (attributes->contains(nsHtml5AttributeName::ATTR_COLOR) || attributes->contains(nsHtml5AttributeName::ATTR_FACE) || attributes->contains(nsHtml5AttributeName::ATTR_SIZE)) {

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
                selfClosing = PR_FALSE;
              } else {
                appendToCurrentNodeAndPushElementMayFosterSVG(elementName, attributes);
              }
              attributes = nsnull;
              NS_HTML5_BREAK(starttagloop);
            } else {
              attributes->adjustForMath();
              if (selfClosing) {
                appendVoidElementToCurrentMayFosterMathML(elementName, attributes);
                selfClosing = PR_FALSE;
              } else {
                appendToCurrentNodeAndPushElementMayFosterMathML(elementName, attributes);
              }
              attributes = nsnull;
              NS_HTML5_BREAK(starttagloop);
            }
          }
        }
      }
    }
    switch(mode) {
      case NS_HTML5TREE_BUILDER_IN_TABLE_BODY: {
        switch(group) {
          case NS_HTML5TREE_BUILDER_TR: {
            clearStackBackTo(findLastInTableScopeOrRootTbodyTheadTfoot());
            appendToCurrentNodeAndPushElement(elementName, attributes);
            mode = NS_HTML5TREE_BUILDER_IN_ROW;
            attributes = nsnull;
            NS_HTML5_BREAK(starttagloop);
          }
          case NS_HTML5TREE_BUILDER_TD_OR_TH: {

            clearStackBackTo(findLastInTableScopeOrRootTbodyTheadTfoot());
            appendToCurrentNodeAndPushElement(nsHtml5ElementName::ELT_TR, nsHtml5HtmlAttributes::EMPTY_ATTRIBUTES);
            mode = NS_HTML5TREE_BUILDER_IN_ROW;
            continue;
          }
          case NS_HTML5TREE_BUILDER_CAPTION:
          case NS_HTML5TREE_BUILDER_COL:
          case NS_HTML5TREE_BUILDER_COLGROUP:
          case NS_HTML5TREE_BUILDER_TBODY_OR_THEAD_OR_TFOOT: {
            eltPos = findLastInTableScopeOrRootTbodyTheadTfoot();
            if (!eltPos) {

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
      case NS_HTML5TREE_BUILDER_IN_ROW: {
        switch(group) {
          case NS_HTML5TREE_BUILDER_TD_OR_TH: {
            clearStackBackTo(findLastOrRoot(NS_HTML5TREE_BUILDER_TR));
            appendToCurrentNodeAndPushElement(elementName, attributes);
            mode = NS_HTML5TREE_BUILDER_IN_CELL;
            insertMarker();
            attributes = nsnull;
            NS_HTML5_BREAK(starttagloop);
          }
          case NS_HTML5TREE_BUILDER_CAPTION:
          case NS_HTML5TREE_BUILDER_COL:
          case NS_HTML5TREE_BUILDER_COLGROUP:
          case NS_HTML5TREE_BUILDER_TBODY_OR_THEAD_OR_TFOOT:
          case NS_HTML5TREE_BUILDER_TR: {
            eltPos = findLastOrRoot(NS_HTML5TREE_BUILDER_TR);
            if (!eltPos) {


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
      case NS_HTML5TREE_BUILDER_IN_TABLE: {
        for (; ; ) {
          switch(group) {
            case NS_HTML5TREE_BUILDER_CAPTION: {
              clearStackBackTo(findLastOrRoot(NS_HTML5TREE_BUILDER_TABLE));
              insertMarker();
              appendToCurrentNodeAndPushElement(elementName, attributes);
              mode = NS_HTML5TREE_BUILDER_IN_CAPTION;
              attributes = nsnull;
              NS_HTML5_BREAK(starttagloop);
            }
            case NS_HTML5TREE_BUILDER_COLGROUP: {
              clearStackBackTo(findLastOrRoot(NS_HTML5TREE_BUILDER_TABLE));
              appendToCurrentNodeAndPushElement(elementName, attributes);
              mode = NS_HTML5TREE_BUILDER_IN_COLUMN_GROUP;
              attributes = nsnull;
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
              attributes = nsnull;
              NS_HTML5_BREAK(starttagloop);
            }
            case NS_HTML5TREE_BUILDER_TR:
            case NS_HTML5TREE_BUILDER_TD_OR_TH: {
              clearStackBackTo(findLastOrRoot(NS_HTML5TREE_BUILDER_TABLE));
              appendToCurrentNodeAndPushElement(nsHtml5ElementName::ELT_TBODY, nsHtml5HtmlAttributes::EMPTY_ATTRIBUTES);
              mode = NS_HTML5TREE_BUILDER_IN_TABLE_BODY;
              NS_HTML5_CONTINUE(starttagloop);
            }
            case NS_HTML5TREE_BUILDER_TABLE: {

              eltPos = findLastInTableScope(name);
              if (eltPos == NS_HTML5TREE_BUILDER_NOT_FOUND_ON_STACK) {

                NS_HTML5_BREAK(starttagloop);
              }
              generateImpliedEndTags();

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
              attributes = nsnull;
              NS_HTML5_BREAK(starttagloop);
            }
            case NS_HTML5TREE_BUILDER_STYLE: {
              appendToCurrentNodeAndPushElement(elementName, attributes);
              originalMode = mode;
              mode = NS_HTML5TREE_BUILDER_TEXT;
              tokenizer->setStateAndEndTagExpectation(NS_HTML5TOKENIZER_RAWTEXT, elementName);
              attributes = nsnull;
              NS_HTML5_BREAK(starttagloop);
            }
            case NS_HTML5TREE_BUILDER_INPUT: {
              if (!nsHtml5Portability::lowerCaseLiteralEqualsIgnoreAsciiCaseString("hidden", attributes->getValue(nsHtml5AttributeName::ATTR_TYPE))) {
                NS_HTML5_BREAK(intableloop);
              }
              appendVoidElementToCurrent(name, attributes, formPointer);
              selfClosing = PR_FALSE;
              attributes = nsnull;
              NS_HTML5_BREAK(starttagloop);
            }
            case NS_HTML5TREE_BUILDER_FORM: {
              if (formPointer) {

                NS_HTML5_BREAK(starttagloop);
              } else {

                appendVoidFormToCurrent(attributes);
                attributes = nsnull;
                NS_HTML5_BREAK(starttagloop);
              }
            }
            default: {

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

            eltPos = findLastInTableScope(nsHtml5Atoms::caption);
            if (eltPos == NS_HTML5TREE_BUILDER_NOT_FOUND_ON_STACK) {
              NS_HTML5_BREAK(starttagloop);
            }
            generateImpliedEndTags();

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


                NS_HTML5_BREAK(starttagloop);
              } else {

                detachFromParent(stack[1]->node);
                while (currentPtr > 0) {
                  pop();
                }
                appendToCurrentNodeAndPushElement(elementName, attributes);
                mode = NS_HTML5TREE_BUILDER_IN_FRAMESET;
                attributes = nsnull;
                NS_HTML5_BREAK(starttagloop);
              }
            } else {

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
          case NS_HTML5TREE_BUILDER_EMBED_OR_IMG:
          case NS_HTML5TREE_BUILDER_INPUT:
          case NS_HTML5TREE_BUILDER_KEYGEN:
          case NS_HTML5TREE_BUILDER_HR:
          case NS_HTML5TREE_BUILDER_TEXTAREA:
          case NS_HTML5TREE_BUILDER_XMP:
          case NS_HTML5TREE_BUILDER_IFRAME:
          case NS_HTML5TREE_BUILDER_SELECT: {
            if (mode == NS_HTML5TREE_BUILDER_FRAMESET_OK && !(group == NS_HTML5TREE_BUILDER_INPUT && nsHtml5Portability::lowerCaseLiteralEqualsIgnoreAsciiCaseString("hidden", attributes->getValue(nsHtml5AttributeName::ATTR_TYPE)))) {
              framesetOk = PR_FALSE;
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

              if (!fragment) {
                addAttributesToHtml(attributes);
                attributes = nsnull;
              }
              NS_HTML5_BREAK(starttagloop);
            }
            case NS_HTML5TREE_BUILDER_BASE:
            case NS_HTML5TREE_BUILDER_LINK_OR_BASEFONT_OR_BGSOUND:
            case NS_HTML5TREE_BUILDER_META:
            case NS_HTML5TREE_BUILDER_STYLE:
            case NS_HTML5TREE_BUILDER_SCRIPT:
            case NS_HTML5TREE_BUILDER_TITLE:
            case NS_HTML5TREE_BUILDER_COMMAND: {
              NS_HTML5_BREAK(inbodyloop);
            }
            case NS_HTML5TREE_BUILDER_BODY: {
              if (!currentPtr || stack[1]->getGroup() != NS_HTML5TREE_BUILDER_BODY) {


                NS_HTML5_BREAK(starttagloop);
              }

              framesetOk = PR_FALSE;
              if (mode == NS_HTML5TREE_BUILDER_FRAMESET_OK) {
                mode = NS_HTML5TREE_BUILDER_IN_BODY;
              }
              if (addAttributesToBody(attributes)) {
                attributes = nsnull;
              }
              NS_HTML5_BREAK(starttagloop);
            }
            case NS_HTML5TREE_BUILDER_P:
            case NS_HTML5TREE_BUILDER_DIV_OR_BLOCKQUOTE_OR_CENTER_OR_MENU:
            case NS_HTML5TREE_BUILDER_UL_OR_OL_OR_DL:
            case NS_HTML5TREE_BUILDER_ADDRESS_OR_ARTICLE_OR_ASIDE_OR_DETAILS_OR_DIR_OR_FIGCAPTION_OR_FIGURE_OR_FOOTER_OR_HEADER_OR_HGROUP_OR_NAV_OR_SECTION_OR_SUMMARY: {
              implicitlyCloseP();
              appendToCurrentNodeAndPushElementMayFoster(elementName, attributes);
              attributes = nsnull;
              NS_HTML5_BREAK(starttagloop);
            }
            case NS_HTML5TREE_BUILDER_H1_OR_H2_OR_H3_OR_H4_OR_H5_OR_H6: {
              implicitlyCloseP();
              if (stack[currentPtr]->getGroup() == NS_HTML5TREE_BUILDER_H1_OR_H2_OR_H3_OR_H4_OR_H5_OR_H6) {

                pop();
              }
              appendToCurrentNodeAndPushElementMayFoster(elementName, attributes);
              attributes = nsnull;
              NS_HTML5_BREAK(starttagloop);
            }
            case NS_HTML5TREE_BUILDER_FIELDSET: {
              implicitlyCloseP();
              appendToCurrentNodeAndPushElementMayFoster(elementName, attributes, formPointer);
              attributes = nsnull;
              NS_HTML5_BREAK(starttagloop);
            }
            case NS_HTML5TREE_BUILDER_PRE_OR_LISTING: {
              implicitlyCloseP();
              appendToCurrentNodeAndPushElementMayFoster(elementName, attributes);
              needToDropLF = PR_TRUE;
              attributes = nsnull;
              NS_HTML5_BREAK(starttagloop);
            }
            case NS_HTML5TREE_BUILDER_FORM: {
              if (formPointer) {

                NS_HTML5_BREAK(starttagloop);
              } else {
                implicitlyCloseP();
                appendToCurrentNodeAndPushFormElementMayFoster(attributes);
                attributes = nsnull;
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

                  while (currentPtr >= eltPos) {
                    pop();
                  }
                  break;
                } else if (node->isScoping() || (node->isSpecial() && node->name != nsHtml5Atoms::p && node->name != nsHtml5Atoms::address && node->name != nsHtml5Atoms::div)) {
                  break;
                }
                eltPos--;
              }
              implicitlyCloseP();
              appendToCurrentNodeAndPushElementMayFoster(elementName, attributes);
              attributes = nsnull;
              NS_HTML5_BREAK(starttagloop);
            }
            case NS_HTML5TREE_BUILDER_PLAINTEXT: {
              implicitlyCloseP();
              appendToCurrentNodeAndPushElementMayFoster(elementName, attributes);
              tokenizer->setStateAndEndTagExpectation(NS_HTML5TOKENIZER_PLAINTEXT, elementName);
              attributes = nsnull;
              NS_HTML5_BREAK(starttagloop);
            }
            case NS_HTML5TREE_BUILDER_A: {
              PRInt32 activeAPos = findInListOfActiveFormattingElementsContainsBetweenEndAndLastMarker(nsHtml5Atoms::a);
              if (activeAPos != -1) {

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
              attributes = nsnull;
              NS_HTML5_BREAK(starttagloop);
            }
            case NS_HTML5TREE_BUILDER_B_OR_BIG_OR_CODE_OR_EM_OR_I_OR_S_OR_SMALL_OR_STRIKE_OR_STRONG_OR_TT_OR_U:
            case NS_HTML5TREE_BUILDER_FONT: {
              reconstructTheActiveFormattingElements();
              maybeForgetEarlierDuplicateFormattingElement(elementName->name, attributes);
              appendToCurrentNodeAndPushFormattingElementMayFoster(elementName, attributes);
              attributes = nsnull;
              NS_HTML5_BREAK(starttagloop);
            }
            case NS_HTML5TREE_BUILDER_NOBR: {
              reconstructTheActiveFormattingElements();
              if (NS_HTML5TREE_BUILDER_NOT_FOUND_ON_STACK != findLastInScope(nsHtml5Atoms::nobr)) {

                adoptionAgencyEndTag(nsHtml5Atoms::nobr);
                reconstructTheActiveFormattingElements();
              }
              appendToCurrentNodeAndPushFormattingElementMayFoster(elementName, attributes);
              attributes = nsnull;
              NS_HTML5_BREAK(starttagloop);
            }
            case NS_HTML5TREE_BUILDER_BUTTON: {
              eltPos = findLastInScope(name);
              if (eltPos != NS_HTML5TREE_BUILDER_NOT_FOUND_ON_STACK) {

                generateImpliedEndTags();

                while (currentPtr >= eltPos) {
                  pop();
                }
                NS_HTML5_CONTINUE(starttagloop);
              } else {
                reconstructTheActiveFormattingElements();
                appendToCurrentNodeAndPushElementMayFoster(elementName, attributes, formPointer);
                attributes = nsnull;
                NS_HTML5_BREAK(starttagloop);
              }
            }
            case NS_HTML5TREE_BUILDER_OBJECT: {
              reconstructTheActiveFormattingElements();
              appendToCurrentNodeAndPushElementMayFoster(elementName, attributes, formPointer);
              insertMarker();
              attributes = nsnull;
              NS_HTML5_BREAK(starttagloop);
            }
            case NS_HTML5TREE_BUILDER_MARQUEE_OR_APPLET: {
              reconstructTheActiveFormattingElements();
              appendToCurrentNodeAndPushElementMayFoster(elementName, attributes);
              insertMarker();
              attributes = nsnull;
              NS_HTML5_BREAK(starttagloop);
            }
            case NS_HTML5TREE_BUILDER_TABLE: {
              if (!quirks) {
                implicitlyCloseP();
              }
              appendToCurrentNodeAndPushElementMayFoster(elementName, attributes);
              mode = NS_HTML5TREE_BUILDER_IN_TABLE;
              attributes = nsnull;
              NS_HTML5_BREAK(starttagloop);
            }
            case NS_HTML5TREE_BUILDER_BR:
            case NS_HTML5TREE_BUILDER_EMBED_OR_IMG:
            case NS_HTML5TREE_BUILDER_AREA_OR_WBR: {
              reconstructTheActiveFormattingElements();
            }
#ifdef ENABLE_VOID_MENUITEM
            case NS_HTML5TREE_BUILDER_MENUITEM:
#endif
            case NS_HTML5TREE_BUILDER_PARAM_OR_SOURCE_OR_TRACK: {
              appendVoidElementToCurrentMayFoster(elementName, attributes);
              selfClosing = PR_FALSE;
              attributes = nsnull;
              NS_HTML5_BREAK(starttagloop);
            }
            case NS_HTML5TREE_BUILDER_HR: {
              implicitlyCloseP();
              appendVoidElementToCurrentMayFoster(elementName, attributes);
              selfClosing = PR_FALSE;
              attributes = nsnull;
              NS_HTML5_BREAK(starttagloop);
            }
            case NS_HTML5TREE_BUILDER_IMAGE: {

              elementName = nsHtml5ElementName::ELT_IMG;
              NS_HTML5_CONTINUE(starttagloop);
            }
            case NS_HTML5TREE_BUILDER_KEYGEN:
            case NS_HTML5TREE_BUILDER_INPUT: {
              reconstructTheActiveFormattingElements();
              appendVoidElementToCurrentMayFoster(name, attributes, formPointer);
              selfClosing = PR_FALSE;
              attributes = nsnull;
              NS_HTML5_BREAK(starttagloop);
            }
            case NS_HTML5TREE_BUILDER_ISINDEX: {

              if (formPointer) {
                NS_HTML5_BREAK(starttagloop);
              }
              implicitlyCloseP();
              nsHtml5HtmlAttributes* formAttrs = new nsHtml5HtmlAttributes(0);
              PRInt32 actionIndex = attributes->getIndex(nsHtml5AttributeName::ATTR_ACTION);
              if (actionIndex > -1) {
                formAttrs->addAttribute(nsHtml5AttributeName::ATTR_ACTION, attributes->getValue(actionIndex));
              }
              appendToCurrentNodeAndPushFormElementMayFoster(formAttrs);
              appendVoidElementToCurrentMayFoster(nsHtml5ElementName::ELT_HR, nsHtml5HtmlAttributes::EMPTY_ATTRIBUTES);
              appendToCurrentNodeAndPushElementMayFoster(nsHtml5ElementName::ELT_LABEL, nsHtml5HtmlAttributes::EMPTY_ATTRIBUTES);
              PRInt32 promptIndex = attributes->getIndex(nsHtml5AttributeName::ATTR_PROMPT);
              if (promptIndex > -1) {
                autoJArray<PRUnichar,PRInt32> prompt = nsHtml5Portability::newCharArrayFromString(attributes->getValue(promptIndex));
                appendCharacters(stack[currentPtr]->node, prompt, 0, prompt.length);
              } else {
                appendIsindexPrompt(stack[currentPtr]->node);
              }
              nsHtml5HtmlAttributes* inputAttributes = new nsHtml5HtmlAttributes(0);
              inputAttributes->addAttribute(nsHtml5AttributeName::ATTR_NAME, nsHtml5Portability::newStringFromLiteral("isindex"));
              for (PRInt32 i = 0; i < attributes->getLength(); i++) {
                nsHtml5AttributeName* attributeQName = attributes->getAttributeName(i);
                if (nsHtml5AttributeName::ATTR_NAME == attributeQName || nsHtml5AttributeName::ATTR_PROMPT == attributeQName) {
                  attributes->releaseValue(i);
                } else if (nsHtml5AttributeName::ATTR_ACTION != attributeQName) {
                  inputAttributes->addAttribute(attributeQName, attributes->getValue(i));
                }
              }
              attributes->clearWithoutReleasingContents();
              appendVoidElementToCurrentMayFoster(nsHtml5Atoms::input, inputAttributes, formPointer);
              pop();
              appendVoidElementToCurrentMayFoster(nsHtml5ElementName::ELT_HR, nsHtml5HtmlAttributes::EMPTY_ATTRIBUTES);
              pop();
              selfClosing = PR_FALSE;
              NS_HTML5_BREAK(starttagloop);
            }
            case NS_HTML5TREE_BUILDER_TEXTAREA: {
              appendToCurrentNodeAndPushElementMayFoster(elementName, attributes, formPointer);
              tokenizer->setStateAndEndTagExpectation(NS_HTML5TOKENIZER_RCDATA, elementName);
              originalMode = mode;
              mode = NS_HTML5TREE_BUILDER_TEXT;
              needToDropLF = PR_TRUE;
              attributes = nsnull;
              NS_HTML5_BREAK(starttagloop);
            }
            case NS_HTML5TREE_BUILDER_XMP: {
              implicitlyCloseP();
              reconstructTheActiveFormattingElements();
              appendToCurrentNodeAndPushElementMayFoster(elementName, attributes);
              originalMode = mode;
              mode = NS_HTML5TREE_BUILDER_TEXT;
              tokenizer->setStateAndEndTagExpectation(NS_HTML5TOKENIZER_RAWTEXT, elementName);
              attributes = nsnull;
              NS_HTML5_BREAK(starttagloop);
            }
            case NS_HTML5TREE_BUILDER_NOSCRIPT: {
              if (!scriptingEnabled) {
                reconstructTheActiveFormattingElements();
                appendToCurrentNodeAndPushElementMayFoster(elementName, attributes);
                attributes = nsnull;
                NS_HTML5_BREAK(starttagloop);
              } else {
              }
            }
            case NS_HTML5TREE_BUILDER_NOFRAMES:
            case NS_HTML5TREE_BUILDER_IFRAME:
            case NS_HTML5TREE_BUILDER_NOEMBED: {
              appendToCurrentNodeAndPushElementMayFoster(elementName, attributes);
              originalMode = mode;
              mode = NS_HTML5TREE_BUILDER_TEXT;
              tokenizer->setStateAndEndTagExpectation(NS_HTML5TOKENIZER_RAWTEXT, elementName);
              attributes = nsnull;
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
              attributes = nsnull;
              NS_HTML5_BREAK(starttagloop);
            }
            case NS_HTML5TREE_BUILDER_OPTGROUP:
            case NS_HTML5TREE_BUILDER_OPTION: {
              if (isCurrent(nsHtml5Atoms::option)) {
                pop();
              }
              reconstructTheActiveFormattingElements();
              appendToCurrentNodeAndPushElementMayFoster(elementName, attributes);
              attributes = nsnull;
              NS_HTML5_BREAK(starttagloop);
            }
            case NS_HTML5TREE_BUILDER_RT_OR_RP: {
              eltPos = findLastInScope(nsHtml5Atoms::ruby);
              if (eltPos != NS_HTML5TREE_BUILDER_NOT_FOUND_ON_STACK) {
                generateImpliedEndTags();
              }
              if (eltPos != currentPtr) {

                while (currentPtr > eltPos) {
                  pop();
                }
              }
              appendToCurrentNodeAndPushElementMayFoster(elementName, attributes);
              attributes = nsnull;
              NS_HTML5_BREAK(starttagloop);
            }
            case NS_HTML5TREE_BUILDER_MATH: {
              reconstructTheActiveFormattingElements();
              attributes->adjustForMath();
              if (selfClosing) {
                appendVoidElementToCurrentMayFosterMathML(elementName, attributes);
                selfClosing = PR_FALSE;
              } else {
                appendToCurrentNodeAndPushElementMayFosterMathML(elementName, attributes);
              }
              attributes = nsnull;
              NS_HTML5_BREAK(starttagloop);
            }
            case NS_HTML5TREE_BUILDER_SVG: {
              reconstructTheActiveFormattingElements();
              attributes->adjustForSvg();
              if (selfClosing) {
                appendVoidElementToCurrentMayFosterSVG(elementName, attributes);
                selfClosing = PR_FALSE;
              } else {
                appendToCurrentNodeAndPushElementMayFosterSVG(elementName, attributes);
              }
              attributes = nsnull;
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

              NS_HTML5_BREAK(starttagloop);
            }
            case NS_HTML5TREE_BUILDER_OUTPUT_OR_LABEL: {
              reconstructTheActiveFormattingElements();
              appendToCurrentNodeAndPushElementMayFoster(elementName, attributes, formPointer);
              attributes = nsnull;
              NS_HTML5_BREAK(starttagloop);
            }
            default: {
              reconstructTheActiveFormattingElements();
              appendToCurrentNodeAndPushElementMayFoster(elementName, attributes);
              attributes = nsnull;
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

              if (!fragment) {
                addAttributesToHtml(attributes);
                attributes = nsnull;
              }
              NS_HTML5_BREAK(starttagloop);
            }
            case NS_HTML5TREE_BUILDER_BASE:
            case NS_HTML5TREE_BUILDER_COMMAND: {
              appendVoidElementToCurrentMayFoster(elementName, attributes);
              selfClosing = PR_FALSE;
              attributes = nsnull;
              NS_HTML5_BREAK(starttagloop);
            }
            case NS_HTML5TREE_BUILDER_META:
            case NS_HTML5TREE_BUILDER_LINK_OR_BASEFONT_OR_BGSOUND: {
              NS_HTML5_BREAK(inheadloop);
            }
            case NS_HTML5TREE_BUILDER_TITLE: {
              appendToCurrentNodeAndPushElementMayFoster(elementName, attributes);
              originalMode = mode;
              mode = NS_HTML5TREE_BUILDER_TEXT;
              tokenizer->setStateAndEndTagExpectation(NS_HTML5TOKENIZER_RCDATA, elementName);
              attributes = nsnull;
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
              attributes = nsnull;
              NS_HTML5_BREAK(starttagloop);
            }
            case NS_HTML5TREE_BUILDER_SCRIPT: {
              appendToCurrentNodeAndPushElementMayFoster(elementName, attributes);
              originalMode = mode;
              mode = NS_HTML5TREE_BUILDER_TEXT;
              tokenizer->setStateAndEndTagExpectation(NS_HTML5TOKENIZER_SCRIPT_DATA, elementName);
              attributes = nsnull;
              NS_HTML5_BREAK(starttagloop);
            }
            case NS_HTML5TREE_BUILDER_STYLE:
            case NS_HTML5TREE_BUILDER_NOFRAMES: {
              appendToCurrentNodeAndPushElementMayFoster(elementName, attributes);
              originalMode = mode;
              mode = NS_HTML5TREE_BUILDER_TEXT;
              tokenizer->setStateAndEndTagExpectation(NS_HTML5TOKENIZER_RAWTEXT, elementName);
              attributes = nsnull;
              NS_HTML5_BREAK(starttagloop);
            }
            case NS_HTML5TREE_BUILDER_HEAD: {

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

            if (!fragment) {
              addAttributesToHtml(attributes);
              attributes = nsnull;
            }
            NS_HTML5_BREAK(starttagloop);
          }
          case NS_HTML5TREE_BUILDER_LINK_OR_BASEFONT_OR_BGSOUND: {
            appendVoidElementToCurrentMayFoster(elementName, attributes);
            selfClosing = PR_FALSE;
            attributes = nsnull;
            NS_HTML5_BREAK(starttagloop);
          }
          case NS_HTML5TREE_BUILDER_META: {
            checkMetaCharset(attributes);
            appendVoidElementToCurrentMayFoster(elementName, attributes);
            selfClosing = PR_FALSE;
            attributes = nsnull;
            NS_HTML5_BREAK(starttagloop);
          }
          case NS_HTML5TREE_BUILDER_STYLE:
          case NS_HTML5TREE_BUILDER_NOFRAMES: {
            appendToCurrentNodeAndPushElement(elementName, attributes);
            originalMode = mode;
            mode = NS_HTML5TREE_BUILDER_TEXT;
            tokenizer->setStateAndEndTagExpectation(NS_HTML5TOKENIZER_RAWTEXT, elementName);
            attributes = nsnull;
            NS_HTML5_BREAK(starttagloop);
          }
          case NS_HTML5TREE_BUILDER_HEAD: {

            NS_HTML5_BREAK(starttagloop);
          }
          case NS_HTML5TREE_BUILDER_NOSCRIPT: {

            NS_HTML5_BREAK(starttagloop);
          }
          default: {

            pop();
            mode = NS_HTML5TREE_BUILDER_IN_HEAD;
            continue;
          }
        }
      }
      case NS_HTML5TREE_BUILDER_IN_COLUMN_GROUP: {
        switch(group) {
          case NS_HTML5TREE_BUILDER_HTML: {

            if (!fragment) {
              addAttributesToHtml(attributes);
              attributes = nsnull;
            }
            NS_HTML5_BREAK(starttagloop);
          }
          case NS_HTML5TREE_BUILDER_COL: {
            appendVoidElementToCurrentMayFoster(elementName, attributes);
            selfClosing = PR_FALSE;
            attributes = nsnull;
            NS_HTML5_BREAK(starttagloop);
          }
          default: {
            if (!currentPtr) {


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

            eltPos = findLastInTableScope(nsHtml5Atoms::select);
            if (eltPos == NS_HTML5TREE_BUILDER_NOT_FOUND_ON_STACK) {

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

            if (!fragment) {
              addAttributesToHtml(attributes);
              attributes = nsnull;
            }
            NS_HTML5_BREAK(starttagloop);
          }
          case NS_HTML5TREE_BUILDER_OPTION: {
            if (isCurrent(nsHtml5Atoms::option)) {
              pop();
            }
            appendToCurrentNodeAndPushElement(elementName, attributes);
            attributes = nsnull;
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
            attributes = nsnull;
            NS_HTML5_BREAK(starttagloop);
          }
          case NS_HTML5TREE_BUILDER_SELECT: {

            eltPos = findLastInTableScope(name);
            if (eltPos == NS_HTML5TREE_BUILDER_NOT_FOUND_ON_STACK) {


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

            eltPos = findLastInTableScope(nsHtml5Atoms::select);
            if (eltPos == NS_HTML5TREE_BUILDER_NOT_FOUND_ON_STACK) {

              NS_HTML5_BREAK(starttagloop);
            }
            while (currentPtr >= eltPos) {
              pop();
            }
            resetTheInsertionMode();
            continue;
          }
          case NS_HTML5TREE_BUILDER_SCRIPT: {
            appendToCurrentNodeAndPushElementMayFoster(elementName, attributes);
            originalMode = mode;
            mode = NS_HTML5TREE_BUILDER_TEXT;
            tokenizer->setStateAndEndTagExpectation(NS_HTML5TOKENIZER_SCRIPT_DATA, elementName);
            attributes = nsnull;
            NS_HTML5_BREAK(starttagloop);
          }
          default: {

            NS_HTML5_BREAK(starttagloop);
          }
        }
      }
      case NS_HTML5TREE_BUILDER_AFTER_BODY: {
        switch(group) {
          case NS_HTML5TREE_BUILDER_HTML: {

            if (!fragment) {
              addAttributesToHtml(attributes);
              attributes = nsnull;
            }
            NS_HTML5_BREAK(starttagloop);
          }
          default: {

            mode = framesetOk ? NS_HTML5TREE_BUILDER_FRAMESET_OK : NS_HTML5TREE_BUILDER_IN_BODY;
            continue;
          }
        }
      }
      case NS_HTML5TREE_BUILDER_IN_FRAMESET: {
        switch(group) {
          case NS_HTML5TREE_BUILDER_FRAMESET: {
            appendToCurrentNodeAndPushElement(elementName, attributes);
            attributes = nsnull;
            NS_HTML5_BREAK(starttagloop);
          }
          case NS_HTML5TREE_BUILDER_FRAME: {
            appendVoidElementToCurrentMayFoster(elementName, attributes);
            selfClosing = PR_FALSE;
            attributes = nsnull;
            NS_HTML5_BREAK(starttagloop);
          }
          default:
            ; // fall through
        }
      }
      case NS_HTML5TREE_BUILDER_AFTER_FRAMESET: {
        switch(group) {
          case NS_HTML5TREE_BUILDER_HTML: {

            if (!fragment) {
              addAttributesToHtml(attributes);
              attributes = nsnull;
            }
            NS_HTML5_BREAK(starttagloop);
          }
          case NS_HTML5TREE_BUILDER_NOFRAMES: {
            appendToCurrentNodeAndPushElement(elementName, attributes);
            originalMode = mode;
            mode = NS_HTML5TREE_BUILDER_TEXT;
            tokenizer->setStateAndEndTagExpectation(NS_HTML5TOKENIZER_RAWTEXT, elementName);
            attributes = nsnull;
            NS_HTML5_BREAK(starttagloop);
          }
          default: {

            NS_HTML5_BREAK(starttagloop);
          }
        }
      }
      case NS_HTML5TREE_BUILDER_INITIAL: {
        documentModeInternal(QUIRKS_MODE, nsnull, nsnull, PR_FALSE);
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
            attributes = nsnull;
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

            if (!fragment) {
              addAttributesToHtml(attributes);
              attributes = nsnull;
            }
            NS_HTML5_BREAK(starttagloop);
          }
          case NS_HTML5TREE_BUILDER_HEAD: {
            appendToCurrentNodeAndPushHeadElement(attributes);
            mode = NS_HTML5TREE_BUILDER_IN_HEAD;
            attributes = nsnull;
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

            if (!fragment) {
              addAttributesToHtml(attributes);
              attributes = nsnull;
            }
            NS_HTML5_BREAK(starttagloop);
          }
          case NS_HTML5TREE_BUILDER_BODY: {
            if (!attributes->getLength()) {
              appendToCurrentNodeAndPushBodyElement();
            } else {
              appendToCurrentNodeAndPushBodyElement(attributes);
            }
            framesetOk = PR_FALSE;
            mode = NS_HTML5TREE_BUILDER_IN_BODY;
            attributes = nsnull;
            NS_HTML5_BREAK(starttagloop);
          }
          case NS_HTML5TREE_BUILDER_FRAMESET: {
            appendToCurrentNodeAndPushElement(elementName, attributes);
            mode = NS_HTML5TREE_BUILDER_IN_FRAMESET;
            attributes = nsnull;
            NS_HTML5_BREAK(starttagloop);
          }
          case NS_HTML5TREE_BUILDER_BASE: {

            pushHeadPointerOntoStack();
            appendVoidElementToCurrentMayFoster(elementName, attributes);
            selfClosing = PR_FALSE;
            pop();
            attributes = nsnull;
            NS_HTML5_BREAK(starttagloop);
          }
          case NS_HTML5TREE_BUILDER_LINK_OR_BASEFONT_OR_BGSOUND: {

            pushHeadPointerOntoStack();
            appendVoidElementToCurrentMayFoster(elementName, attributes);
            selfClosing = PR_FALSE;
            pop();
            attributes = nsnull;
            NS_HTML5_BREAK(starttagloop);
          }
          case NS_HTML5TREE_BUILDER_META: {

            checkMetaCharset(attributes);
            pushHeadPointerOntoStack();
            appendVoidElementToCurrentMayFoster(elementName, attributes);
            selfClosing = PR_FALSE;
            pop();
            attributes = nsnull;
            NS_HTML5_BREAK(starttagloop);
          }
          case NS_HTML5TREE_BUILDER_SCRIPT: {

            pushHeadPointerOntoStack();
            appendToCurrentNodeAndPushElement(elementName, attributes);
            originalMode = mode;
            mode = NS_HTML5TREE_BUILDER_TEXT;
            tokenizer->setStateAndEndTagExpectation(NS_HTML5TOKENIZER_SCRIPT_DATA, elementName);
            attributes = nsnull;
            NS_HTML5_BREAK(starttagloop);
          }
          case NS_HTML5TREE_BUILDER_STYLE:
          case NS_HTML5TREE_BUILDER_NOFRAMES: {

            pushHeadPointerOntoStack();
            appendToCurrentNodeAndPushElement(elementName, attributes);
            originalMode = mode;
            mode = NS_HTML5TREE_BUILDER_TEXT;
            tokenizer->setStateAndEndTagExpectation(NS_HTML5TOKENIZER_RAWTEXT, elementName);
            attributes = nsnull;
            NS_HTML5_BREAK(starttagloop);
          }
          case NS_HTML5TREE_BUILDER_TITLE: {

            pushHeadPointerOntoStack();
            appendToCurrentNodeAndPushElement(elementName, attributes);
            originalMode = mode;
            mode = NS_HTML5TREE_BUILDER_TEXT;
            tokenizer->setStateAndEndTagExpectation(NS_HTML5TOKENIZER_RCDATA, elementName);
            attributes = nsnull;
            NS_HTML5_BREAK(starttagloop);
          }
          case NS_HTML5TREE_BUILDER_HEAD: {

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

            if (!fragment) {
              addAttributesToHtml(attributes);
              attributes = nsnull;
            }
            NS_HTML5_BREAK(starttagloop);
          }
          default: {


            mode = framesetOk ? NS_HTML5TREE_BUILDER_FRAMESET_OK : NS_HTML5TREE_BUILDER_IN_BODY;
            continue;
          }
        }
      }
      case NS_HTML5TREE_BUILDER_AFTER_AFTER_FRAMESET: {
        switch(group) {
          case NS_HTML5TREE_BUILDER_HTML: {

            if (!fragment) {
              addAttributesToHtml(attributes);
              attributes = nsnull;
            }
            NS_HTML5_BREAK(starttagloop);
          }
          case NS_HTML5TREE_BUILDER_NOFRAMES: {
            appendToCurrentNodeAndPushElementMayFoster(elementName, attributes);
            originalMode = mode;
            mode = NS_HTML5TREE_BUILDER_TEXT;
            tokenizer->setStateAndEndTagExpectation(NS_HTML5TOKENIZER_SCRIPT_DATA, elementName);
            attributes = nsnull;
            NS_HTML5_BREAK(starttagloop);
          }
          default: {

            NS_HTML5_BREAK(starttagloop);
          }
        }
      }
      case NS_HTML5TREE_BUILDER_TEXT: {

        NS_HTML5_BREAK(starttagloop);
      }
    }
  }
  starttagloop_end: ;

  if (attributes != nsHtml5HtmlAttributes::EMPTY_ATTRIBUTES) {
    delete attributes;
  }
}

bool 
nsHtml5TreeBuilder::isSpecialParentInForeign(nsHtml5StackNode* stackNode)
{
  PRInt32 ns = stackNode->ns;
  return (kNameSpaceID_XHTML == ns) || (stackNode->isHtmlIntegrationPoint()) || ((kNameSpaceID_MathML == ns) && (stackNode->getGroup() == NS_HTML5TREE_BUILDER_MI_MO_MN_MS_MTEXT));
}

nsString* 
nsHtml5TreeBuilder::extractCharsetFromContent(nsString* attributeValue)
{
  PRInt32 charsetState = NS_HTML5TREE_BUILDER_CHARSET_INITIAL;
  PRInt32 start = -1;
  PRInt32 end = -1;
  autoJArray<PRUnichar,PRInt32> buffer = nsHtml5Portability::newCharArrayFromString(attributeValue);
  for (PRInt32 i = 0; i < buffer.length; i++) {
    PRUnichar c = buffer[i];
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
            return nsnull;
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
  nsString* charset = nsnull;
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
  needToDropLF = PR_FALSE;
  PRInt32 eltPos;
  PRInt32 group = elementName->getGroup();
  nsIAtom* name = elementName->name;
  for (; ; ) {
    if (isInForeign()) {

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
      case NS_HTML5TREE_BUILDER_IN_ROW: {
        switch(group) {
          case NS_HTML5TREE_BUILDER_TR: {
            eltPos = findLastOrRoot(NS_HTML5TREE_BUILDER_TR);
            if (!eltPos) {


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


              NS_HTML5_BREAK(endtagloop);
            }
            clearStackBackTo(eltPos);
            pop();
            mode = NS_HTML5TREE_BUILDER_IN_TABLE_BODY;
            continue;
          }
          case NS_HTML5TREE_BUILDER_TBODY_OR_THEAD_OR_TFOOT: {
            if (findLastInTableScope(name) == NS_HTML5TREE_BUILDER_NOT_FOUND_ON_STACK) {

              NS_HTML5_BREAK(endtagloop);
            }
            eltPos = findLastOrRoot(NS_HTML5TREE_BUILDER_TR);
            if (!eltPos) {


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

              NS_HTML5_BREAK(endtagloop);
            }
            clearStackBackTo(eltPos);
            pop();
            mode = NS_HTML5TREE_BUILDER_IN_TABLE;
            NS_HTML5_BREAK(endtagloop);
          }
          case NS_HTML5TREE_BUILDER_TABLE: {
            eltPos = findLastInTableScopeOrRootTbodyTheadTfoot();
            if (!eltPos) {


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

            NS_HTML5_BREAK(endtagloop);
          }
          default:
            ; // fall through
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

            while (currentPtr >= eltPos) {
              pop();
            }
            clearTheListOfActiveFormattingElementsUpToTheLastMarker();
            mode = NS_HTML5TREE_BUILDER_IN_TABLE;
            NS_HTML5_BREAK(endtagloop);
          }
          case NS_HTML5TREE_BUILDER_TABLE: {

            eltPos = findLastInTableScope(nsHtml5Atoms::caption);
            if (eltPos == NS_HTML5TREE_BUILDER_NOT_FOUND_ON_STACK) {
              NS_HTML5_BREAK(endtagloop);
            }
            generateImpliedEndTags();

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

              NS_HTML5_BREAK(endtagloop);
            }
            generateImpliedEndTags();

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


              NS_HTML5_BREAK(endtagloop);
            }


            mode = NS_HTML5TREE_BUILDER_AFTER_BODY;
            NS_HTML5_BREAK(endtagloop);
          }
          case NS_HTML5TREE_BUILDER_HTML: {
            if (!isSecondOnStackBody()) {


              NS_HTML5_BREAK(endtagloop);
            }

            mode = NS_HTML5TREE_BUILDER_AFTER_BODY;
            continue;
          }
          case NS_HTML5TREE_BUILDER_DIV_OR_BLOCKQUOTE_OR_CENTER_OR_MENU:
          case NS_HTML5TREE_BUILDER_UL_OR_OL_OR_DL:
          case NS_HTML5TREE_BUILDER_PRE_OR_LISTING:
          case NS_HTML5TREE_BUILDER_FIELDSET:
          case NS_HTML5TREE_BUILDER_BUTTON:
          case NS_HTML5TREE_BUILDER_ADDRESS_OR_ARTICLE_OR_ASIDE_OR_DETAILS_OR_DIR_OR_FIGCAPTION_OR_FIGURE_OR_FOOTER_OR_HEADER_OR_HGROUP_OR_NAV_OR_SECTION_OR_SUMMARY: {
            eltPos = findLastInScope(name);
            if (eltPos != NS_HTML5TREE_BUILDER_NOT_FOUND_ON_STACK) {
              generateImpliedEndTags();

              while (currentPtr >= eltPos) {
                pop();
              }
            }
            NS_HTML5_BREAK(endtagloop);
          }
          case NS_HTML5TREE_BUILDER_FORM: {
            if (!formPointer) {

              NS_HTML5_BREAK(endtagloop);
            }
            formPointer = nsnull;
            eltPos = findLastInScope(name);
            if (eltPos == NS_HTML5TREE_BUILDER_NOT_FOUND_ON_STACK) {

              NS_HTML5_BREAK(endtagloop);
            }
            generateImpliedEndTags();

            removeFromStack(eltPos);
            NS_HTML5_BREAK(endtagloop);
          }
          case NS_HTML5TREE_BUILDER_P: {
            eltPos = findLastInButtonScope(nsHtml5Atoms::p);
            if (eltPos == NS_HTML5TREE_BUILDER_NOT_FOUND_ON_STACK) {

              if (isInForeign()) {

                while (stack[currentPtr]->ns != kNameSpaceID_XHTML) {
                  pop();
                }
              }
              appendVoidElementToCurrentMayFoster(elementName, nsHtml5HtmlAttributes::EMPTY_ATTRIBUTES);
              NS_HTML5_BREAK(endtagloop);
            }
            generateImpliedEndTagsExceptFor(nsHtml5Atoms::p);


            while (currentPtr >= eltPos) {
              pop();
            }
            NS_HTML5_BREAK(endtagloop);
          }
          case NS_HTML5TREE_BUILDER_LI: {
            eltPos = findLastInListScope(name);
            if (eltPos != NS_HTML5TREE_BUILDER_NOT_FOUND_ON_STACK) {
              generateImpliedEndTagsExceptFor(name);

              while (currentPtr >= eltPos) {
                pop();
              }
            }
            NS_HTML5_BREAK(endtagloop);
          }
          case NS_HTML5TREE_BUILDER_DD_OR_DT: {
            eltPos = findLastInScope(name);
            if (eltPos != NS_HTML5TREE_BUILDER_NOT_FOUND_ON_STACK) {
              generateImpliedEndTagsExceptFor(name);

              while (currentPtr >= eltPos) {
                pop();
              }
            }
            NS_HTML5_BREAK(endtagloop);
          }
          case NS_HTML5TREE_BUILDER_H1_OR_H2_OR_H3_OR_H4_OR_H5_OR_H6: {
            eltPos = findLastInScopeHn();
            if (eltPos != NS_HTML5TREE_BUILDER_NOT_FOUND_ON_STACK) {
              generateImpliedEndTags();

              while (currentPtr >= eltPos) {
                pop();
              }
            }
            NS_HTML5_BREAK(endtagloop);
          }
          case NS_HTML5TREE_BUILDER_OBJECT:
          case NS_HTML5TREE_BUILDER_MARQUEE_OR_APPLET: {
            eltPos = findLastInScope(name);
            if (eltPos != NS_HTML5TREE_BUILDER_NOT_FOUND_ON_STACK) {
              generateImpliedEndTags();

              while (currentPtr >= eltPos) {
                pop();
              }
              clearTheListOfActiveFormattingElementsUpToTheLastMarker();
            }
            NS_HTML5_BREAK(endtagloop);
          }
          case NS_HTML5TREE_BUILDER_BR: {

            if (isInForeign()) {

              while (stack[currentPtr]->ns != kNameSpaceID_XHTML) {
                pop();
              }
            }
            reconstructTheActiveFormattingElements();
            appendVoidElementToCurrentMayFoster(elementName, nsHtml5HtmlAttributes::EMPTY_ATTRIBUTES);
            NS_HTML5_BREAK(endtagloop);
          }
          case NS_HTML5TREE_BUILDER_AREA_OR_WBR:
#ifdef ENABLE_VOID_MENUITEM
          case NS_HTML5TREE_BUILDER_MENUITEM:
#endif
          case NS_HTML5TREE_BUILDER_PARAM_OR_SOURCE_OR_TRACK:
          case NS_HTML5TREE_BUILDER_EMBED_OR_IMG:
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

            NS_HTML5_BREAK(endtagloop);
          }
          case NS_HTML5TREE_BUILDER_NOSCRIPT: {
            if (scriptingEnabled) {

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
              if (node->name == name) {
                generateImpliedEndTags();

                while (currentPtr >= eltPos) {
                  pop();
                }
                NS_HTML5_BREAK(endtagloop);
              } else if (node->isSpecial()) {

                NS_HTML5_BREAK(endtagloop);
              }
              eltPos--;
            }
          }
        }
      }
      case NS_HTML5TREE_BUILDER_IN_COLUMN_GROUP: {
        switch(group) {
          case NS_HTML5TREE_BUILDER_COLGROUP: {
            if (!currentPtr) {


              NS_HTML5_BREAK(endtagloop);
            }
            pop();
            mode = NS_HTML5TREE_BUILDER_IN_TABLE;
            NS_HTML5_BREAK(endtagloop);
          }
          case NS_HTML5TREE_BUILDER_COL: {

            NS_HTML5_BREAK(endtagloop);
          }
          default: {
            if (!currentPtr) {


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

            if (findLastInTableScope(name) != NS_HTML5TREE_BUILDER_NOT_FOUND_ON_STACK) {
              eltPos = findLastInTableScope(nsHtml5Atoms::select);
              if (eltPos == NS_HTML5TREE_BUILDER_NOT_FOUND_ON_STACK) {

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

              NS_HTML5_BREAK(endtagloop);
            }
          }
          case NS_HTML5TREE_BUILDER_OPTGROUP: {
            if (isCurrent(nsHtml5Atoms::option) && nsHtml5Atoms::optgroup == stack[currentPtr - 1]->name) {
              pop();
            }
            if (isCurrent(nsHtml5Atoms::optgroup)) {
              pop();
            }
            NS_HTML5_BREAK(endtagloop);
          }
          case NS_HTML5TREE_BUILDER_SELECT: {
            eltPos = findLastInTableScope(nsHtml5Atoms::select);
            if (eltPos == NS_HTML5TREE_BUILDER_NOT_FOUND_ON_STACK) {


              NS_HTML5_BREAK(endtagloop);
            }
            while (currentPtr >= eltPos) {
              pop();
            }
            resetTheInsertionMode();
            NS_HTML5_BREAK(endtagloop);
          }
          default: {

            NS_HTML5_BREAK(endtagloop);
          }
        }
      }
      case NS_HTML5TREE_BUILDER_AFTER_BODY: {
        switch(group) {
          case NS_HTML5TREE_BUILDER_HTML: {
            if (fragment) {

              NS_HTML5_BREAK(endtagloop);
            } else {
              mode = NS_HTML5TREE_BUILDER_AFTER_AFTER_BODY;
              NS_HTML5_BREAK(endtagloop);
            }
          }
          default: {

            mode = framesetOk ? NS_HTML5TREE_BUILDER_FRAMESET_OK : NS_HTML5TREE_BUILDER_IN_BODY;
            continue;
          }
        }
      }
      case NS_HTML5TREE_BUILDER_IN_FRAMESET: {
        switch(group) {
          case NS_HTML5TREE_BUILDER_FRAMESET: {
            if (!currentPtr) {


              NS_HTML5_BREAK(endtagloop);
            }
            pop();
            if ((!fragment) && !isCurrent(nsHtml5Atoms::frameset)) {
              mode = NS_HTML5TREE_BUILDER_AFTER_FRAMESET;
            }
            NS_HTML5_BREAK(endtagloop);
          }
          default: {

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

            NS_HTML5_BREAK(endtagloop);
          }
        }
      }
      case NS_HTML5TREE_BUILDER_INITIAL: {
        documentModeInternal(QUIRKS_MODE, nsnull, nsnull, PR_FALSE);
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

            NS_HTML5_BREAK(endtagloop);
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
          default: {

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

            pop();
            mode = NS_HTML5TREE_BUILDER_IN_HEAD;
            continue;
          }
          default: {

            NS_HTML5_BREAK(endtagloop);
          }
        }
      }
      case NS_HTML5TREE_BUILDER_AFTER_HEAD: {
        switch(group) {
          case NS_HTML5TREE_BUILDER_HTML:
          case NS_HTML5TREE_BUILDER_BODY:
          case NS_HTML5TREE_BUILDER_BR: {
            appendToCurrentNodeAndPushBodyElement();
            mode = NS_HTML5TREE_BUILDER_FRAMESET_OK;
            continue;
          }
          default: {

            NS_HTML5_BREAK(endtagloop);
          }
        }
      }
      case NS_HTML5TREE_BUILDER_AFTER_AFTER_BODY: {

        mode = framesetOk ? NS_HTML5TREE_BUILDER_FRAMESET_OK : NS_HTML5TREE_BUILDER_IN_BODY;
        continue;
      }
      case NS_HTML5TREE_BUILDER_AFTER_AFTER_FRAMESET: {

        mode = NS_HTML5TREE_BUILDER_IN_FRAMESET;
        continue;
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

PRInt32 
nsHtml5TreeBuilder::findLastInTableScopeOrRootTbodyTheadTfoot()
{
  for (PRInt32 i = currentPtr; i > 0; i--) {
    if (stack[i]->getGroup() == NS_HTML5TREE_BUILDER_TBODY_OR_THEAD_OR_TFOOT) {
      return i;
    }
  }
  return 0;
}

PRInt32 
nsHtml5TreeBuilder::findLast(nsIAtom* name)
{
  for (PRInt32 i = currentPtr; i > 0; i--) {
    if (stack[i]->name == name) {
      return i;
    }
  }
  return NS_HTML5TREE_BUILDER_NOT_FOUND_ON_STACK;
}

PRInt32 
nsHtml5TreeBuilder::findLastInTableScope(nsIAtom* name)
{
  for (PRInt32 i = currentPtr; i > 0; i--) {
    if (stack[i]->name == name) {
      return i;
    } else if (stack[i]->name == nsHtml5Atoms::table) {
      return NS_HTML5TREE_BUILDER_NOT_FOUND_ON_STACK;
    }
  }
  return NS_HTML5TREE_BUILDER_NOT_FOUND_ON_STACK;
}

PRInt32 
nsHtml5TreeBuilder::findLastInButtonScope(nsIAtom* name)
{
  for (PRInt32 i = currentPtr; i > 0; i--) {
    if (stack[i]->name == name) {
      return i;
    } else if (stack[i]->isScoping() || stack[i]->name == nsHtml5Atoms::button) {
      return NS_HTML5TREE_BUILDER_NOT_FOUND_ON_STACK;
    }
  }
  return NS_HTML5TREE_BUILDER_NOT_FOUND_ON_STACK;
}

PRInt32 
nsHtml5TreeBuilder::findLastInScope(nsIAtom* name)
{
  for (PRInt32 i = currentPtr; i > 0; i--) {
    if (stack[i]->name == name) {
      return i;
    } else if (stack[i]->isScoping()) {
      return NS_HTML5TREE_BUILDER_NOT_FOUND_ON_STACK;
    }
  }
  return NS_HTML5TREE_BUILDER_NOT_FOUND_ON_STACK;
}

PRInt32 
nsHtml5TreeBuilder::findLastInListScope(nsIAtom* name)
{
  for (PRInt32 i = currentPtr; i > 0; i--) {
    if (stack[i]->name == name) {
      return i;
    } else if (stack[i]->isScoping() || stack[i]->name == nsHtml5Atoms::ul || stack[i]->name == nsHtml5Atoms::ol) {
      return NS_HTML5TREE_BUILDER_NOT_FOUND_ON_STACK;
    }
  }
  return NS_HTML5TREE_BUILDER_NOT_FOUND_ON_STACK;
}

PRInt32 
nsHtml5TreeBuilder::findLastInScopeHn()
{
  for (PRInt32 i = currentPtr; i > 0; i--) {
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
        if (node->name == name) {
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
  quirks = (m == QUIRKS_MODE);
  if (this) {
    this->documentMode(m);
  }
}

bool 
nsHtml5TreeBuilder::isAlmostStandards(nsString* publicIdentifier, nsString* systemIdentifier)
{
  if (nsHtml5Portability::lowerCaseLiteralEqualsIgnoreAsciiCaseString("-//w3c//dtd xhtml 1.0 transitional//en", publicIdentifier)) {
    return PR_TRUE;
  }
  if (nsHtml5Portability::lowerCaseLiteralEqualsIgnoreAsciiCaseString("-//w3c//dtd xhtml 1.0 frameset//en", publicIdentifier)) {
    return PR_TRUE;
  }
  if (systemIdentifier) {
    if (nsHtml5Portability::lowerCaseLiteralEqualsIgnoreAsciiCaseString("-//w3c//dtd html 4.01 transitional//en", publicIdentifier)) {
      return PR_TRUE;
    }
    if (nsHtml5Portability::lowerCaseLiteralEqualsIgnoreAsciiCaseString("-//w3c//dtd html 4.01 frameset//en", publicIdentifier)) {
      return PR_TRUE;
    }
  }
  return PR_FALSE;
}

bool 
nsHtml5TreeBuilder::isQuirky(nsIAtom* name, nsString* publicIdentifier, nsString* systemIdentifier, bool forceQuirks)
{
  if (forceQuirks) {
    return PR_TRUE;
  }
  if (name != nsHtml5Atoms::html) {
    return PR_TRUE;
  }
  if (publicIdentifier) {
    for (PRInt32 i = 0; i < nsHtml5TreeBuilder::QUIRKY_PUBLIC_IDS.length; i++) {
      if (nsHtml5Portability::lowerCaseLiteralIsPrefixOfIgnoreAsciiCaseString(nsHtml5TreeBuilder::QUIRKY_PUBLIC_IDS[i], publicIdentifier)) {
        return PR_TRUE;
      }
    }
    if (nsHtml5Portability::lowerCaseLiteralEqualsIgnoreAsciiCaseString("-//w3o//dtd w3 html strict 3.0//en//", publicIdentifier) || nsHtml5Portability::lowerCaseLiteralEqualsIgnoreAsciiCaseString("-/w3c/dtd html 4.0 transitional/en", publicIdentifier) || nsHtml5Portability::lowerCaseLiteralEqualsIgnoreAsciiCaseString("html", publicIdentifier)) {
      return PR_TRUE;
    }
  }
  if (!systemIdentifier) {
    if (nsHtml5Portability::lowerCaseLiteralEqualsIgnoreAsciiCaseString("-//w3c//dtd html 4.01 transitional//en", publicIdentifier)) {
      return PR_TRUE;
    } else if (nsHtml5Portability::lowerCaseLiteralEqualsIgnoreAsciiCaseString("-//w3c//dtd html 4.01 frameset//en", publicIdentifier)) {
      return PR_TRUE;
    }
  } else if (nsHtml5Portability::lowerCaseLiteralEqualsIgnoreAsciiCaseString("http://www.ibm.com/data/dtd/v11/ibmxhtml1-transitional.dtd", systemIdentifier)) {
    return PR_TRUE;
  }
  return PR_FALSE;
}

void 
nsHtml5TreeBuilder::closeTheCell(PRInt32 eltPos)
{
  generateImpliedEndTags();

  while (currentPtr >= eltPos) {
    pop();
  }
  clearTheListOfActiveFormattingElementsUpToTheLastMarker();
  mode = NS_HTML5TREE_BUILDER_IN_ROW;
  return;
}

PRInt32 
nsHtml5TreeBuilder::findLastInTableScopeTdTh()
{
  for (PRInt32 i = currentPtr; i > 0; i--) {
    nsIAtom* name = stack[i]->name;
    if (nsHtml5Atoms::td == name || nsHtml5Atoms::th == name) {
      return i;
    } else if (name == nsHtml5Atoms::table) {
      return NS_HTML5TREE_BUILDER_NOT_FOUND_ON_STACK;
    }
  }
  return NS_HTML5TREE_BUILDER_NOT_FOUND_ON_STACK;
}

void 
nsHtml5TreeBuilder::clearStackBackTo(PRInt32 eltPos)
{
  while (currentPtr > eltPos) {
    pop();
  }
}

void 
nsHtml5TreeBuilder::resetTheInsertionMode()
{
  nsHtml5StackNode* node;
  nsIAtom* name;
  PRInt32 ns;
  for (PRInt32 i = currentPtr; i >= 0; i--) {
    node = stack[i];
    name = node->name;
    ns = node->ns;
    if (!i) {
      if (!(contextNamespace == kNameSpaceID_XHTML && (contextName == nsHtml5Atoms::td || contextName == nsHtml5Atoms::th))) {
        name = contextName;
        ns = contextNamespace;
      } else {
        mode = framesetOk ? NS_HTML5TREE_BUILDER_FRAMESET_OK : NS_HTML5TREE_BUILDER_IN_BODY;
        return;
      }
    }
    if (nsHtml5Atoms::select == name) {
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
    } else if (nsHtml5Atoms::head == name) {
      mode = framesetOk ? NS_HTML5TREE_BUILDER_FRAMESET_OK : NS_HTML5TREE_BUILDER_IN_BODY;
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
  PRInt32 eltPos = findLastInButtonScope(nsHtml5Atoms::p);
  if (eltPos == NS_HTML5TREE_BUILDER_NOT_FOUND_ON_STACK) {
    return;
  }
  generateImpliedEndTagsExceptFor(nsHtml5Atoms::p);

  while (currentPtr >= eltPos) {
    pop();
  }
}

bool 
nsHtml5TreeBuilder::clearLastStackSlot()
{
  stack[currentPtr] = nsnull;
  return PR_TRUE;
}

bool 
nsHtml5TreeBuilder::clearLastListSlot()
{
  listOfActiveFormattingElements[listPtr] = nsnull;
  return PR_TRUE;
}

void 
nsHtml5TreeBuilder::push(nsHtml5StackNode* node)
{
  currentPtr++;
  if (currentPtr == stack.length) {
    jArray<nsHtml5StackNode*,PRInt32> newStack = jArray<nsHtml5StackNode*,PRInt32>::newJArray(stack.length + 64);
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
    jArray<nsHtml5StackNode*,PRInt32> newStack = jArray<nsHtml5StackNode*,PRInt32>::newJArray(stack.length + 64);
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
    jArray<nsHtml5StackNode*,PRInt32> newList = jArray<nsHtml5StackNode*,PRInt32>::newJArray(listOfActiveFormattingElements.length + 64);
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
nsHtml5TreeBuilder::removeFromStack(PRInt32 pos)
{
  if (currentPtr == pos) {
    pop();
  } else {

    stack[pos]->release();
    nsHtml5ArrayCopy::arraycopy(stack, pos + 1, pos, currentPtr - pos);

    currentPtr--;
  }
}

void 
nsHtml5TreeBuilder::removeFromStack(nsHtml5StackNode* node)
{
  if (stack[currentPtr] == node) {
    pop();
  } else {
    PRInt32 pos = currentPtr - 1;
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
nsHtml5TreeBuilder::removeFromListOfActiveFormattingElements(PRInt32 pos)
{

  listOfActiveFormattingElements[pos]->release();
  if (pos == listPtr) {

    listPtr--;
    return;
  }

  nsHtml5ArrayCopy::arraycopy(listOfActiveFormattingElements, pos + 1, pos, listPtr - pos);

  listPtr--;
}

bool 
nsHtml5TreeBuilder::adoptionAgencyEndTag(nsIAtom* name)
{
  for (PRInt32 i = 0; i < 8; ++i) {
    PRInt32 formattingEltListPos = listPtr;
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
      return PR_FALSE;
    }
    nsHtml5StackNode* formattingElt = listOfActiveFormattingElements[formattingEltListPos];
    PRInt32 formattingEltStackPos = currentPtr;
    bool inScope = true;
    while (formattingEltStackPos > -1) {
      nsHtml5StackNode* node = stack[formattingEltStackPos];
      if (node == formattingElt) {
        break;
      } else if (node->isScoping()) {
        inScope = PR_FALSE;
      }
      formattingEltStackPos--;
    }
    if (formattingEltStackPos == -1) {

      removeFromListOfActiveFormattingElements(formattingEltListPos);
      return PR_TRUE;
    }
    if (!inScope) {

      return PR_TRUE;
    }

    PRInt32 furthestBlockPos = formattingEltStackPos + 1;
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
      return PR_TRUE;
    }
    nsHtml5StackNode* commonAncestor = stack[formattingEltStackPos - 1];
    nsHtml5StackNode* furthestBlock = stack[furthestBlockPos];
    PRInt32 bookmark = formattingEltListPos;
    PRInt32 nodePos = furthestBlockPos;
    nsHtml5StackNode* lastNode = furthestBlock;
    for (PRInt32 j = 0; j < 3; ++j) {
      nodePos--;
      nsHtml5StackNode* node = stack[nodePos];
      PRInt32 nodeListPos = findInListOfActiveFormattingElements(node);
      if (nodeListPos == -1) {



        removeFromStack(nodePos);
        furthestBlockPos--;
        continue;
      }
      if (nodePos == formattingEltStackPos) {
        break;
      }
      if (nodePos == furthestBlockPos) {
        bookmark = nodeListPos + 1;
      }


      nsIContent** clone = createElement(kNameSpaceID_XHTML, node->name, node->attributes->cloneAttributes(nsnull));
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
    nsIContent** clone = createElement(kNameSpaceID_XHTML, formattingElt->name, formattingElt->attributes->cloneAttributes(nsnull));
    nsHtml5StackNode* formattingClone = new nsHtml5StackNode(formattingElt->getFlags(), formattingElt->ns, formattingElt->name, clone, formattingElt->popName, formattingElt->attributes);
    formattingElt->dropAttributes();
    appendChildrenToNewParent(furthestBlock->node, clone);
    appendElement(clone, furthestBlock->node);
    removeFromListOfActiveFormattingElements(formattingEltListPos);
    insertIntoListOfActiveFormattingElements(formattingClone, bookmark);

    removeFromStack(formattingEltStackPos);
    insertIntoStack(formattingClone, furthestBlockPos);
  }
  return PR_TRUE;
}

void 
nsHtml5TreeBuilder::insertIntoStack(nsHtml5StackNode* node, PRInt32 position)
{


  if (position == currentPtr + 1) {
    push(node);
  } else {
    nsHtml5ArrayCopy::arraycopy(stack, position, position + 1, (currentPtr - position) + 1);
    currentPtr++;
    stack[position] = node;
  }
}

void 
nsHtml5TreeBuilder::insertIntoListOfActiveFormattingElements(nsHtml5StackNode* formattingClone, PRInt32 bookmark)
{
  formattingClone->retain();

  if (bookmark <= listPtr) {
    nsHtml5ArrayCopy::arraycopy(listOfActiveFormattingElements, bookmark, bookmark + 1, (listPtr - bookmark) + 1);
  }
  listPtr++;
  listOfActiveFormattingElements[bookmark] = formattingClone;
}

PRInt32 
nsHtml5TreeBuilder::findInListOfActiveFormattingElements(nsHtml5StackNode* node)
{
  for (PRInt32 i = listPtr; i >= 0; i--) {
    if (node == listOfActiveFormattingElements[i]) {
      return i;
    }
  }
  return -1;
}

PRInt32 
nsHtml5TreeBuilder::findInListOfActiveFormattingElementsContainsBetweenEndAndLastMarker(nsIAtom* name)
{
  for (PRInt32 i = listPtr; i >= 0; i--) {
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
  PRInt32 candidate = -1;
  PRInt32 count = 0;
  for (PRInt32 i = listPtr; i >= 0; i--) {
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

PRInt32 
nsHtml5TreeBuilder::findLastOrRoot(nsIAtom* name)
{
  for (PRInt32 i = currentPtr; i > 0; i--) {
    if (stack[i]->name == name) {
      return i;
    }
  }
  return 0;
}

PRInt32 
nsHtml5TreeBuilder::findLastOrRoot(PRInt32 group)
{
  for (PRInt32 i = currentPtr; i > 0; i--) {
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
      return PR_TRUE;
    }
  }
  return PR_FALSE;
}

void 
nsHtml5TreeBuilder::addAttributesToHtml(nsHtml5HtmlAttributes* attributes)
{
  addAttributesToElement(stack[0]->node, attributes);
}

void 
nsHtml5TreeBuilder::pushHeadPointerOntoStack()
{




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
  PRInt32 entryPos = listPtr;
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
    nsIContent** clone = createElement(kNameSpaceID_XHTML, entry->name, entry->attributes->cloneAttributes(nsnull));
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
nsHtml5TreeBuilder::insertIntoFosterParent(nsIContent** child)
{
  PRInt32 eltPos = findLastOrRoot(NS_HTML5TREE_BUILDER_TABLE);
  nsHtml5StackNode* node = stack[eltPos];
  nsIContent** elt = node->node;
  if (!eltPos) {
    appendElement(child, elt);
    return;
  }
  insertFosterParentedChild(child, elt, stack[eltPos - 1]->node);
}

bool 
nsHtml5TreeBuilder::isInStack(nsHtml5StackNode* node)
{
  for (PRInt32 i = currentPtr; i >= 0; i--) {
    if (stack[i] == node) {
      return PR_TRUE;
    }
  }
  return PR_FALSE;
}

void 
nsHtml5TreeBuilder::pop()
{
  nsHtml5StackNode* node = stack[currentPtr];

  currentPtr--;
  elementPopped(node->ns, node->popName, node->node);
  node->release();
}

void 
nsHtml5TreeBuilder::silentPop()
{
  nsHtml5StackNode* node = stack[currentPtr];

  currentPtr--;
  node->release();
}

void 
nsHtml5TreeBuilder::popOnEof()
{
  nsHtml5StackNode* node = stack[currentPtr];

  currentPtr--;
  markMalformedIfScript(node->node);
  elementPopped(node->ns, node->popName, node->node);
  node->release();
}

void 
nsHtml5TreeBuilder::appendHtmlElementToDocumentAndPush(nsHtml5HtmlAttributes* attributes)
{
  nsIContent** elt = createHtmlElementSetAsRoot(attributes);
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
  nsIContent** elt = createElement(kNameSpaceID_XHTML, nsHtml5Atoms::head, attributes);
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
  nsIContent** elt = createElement(kNameSpaceID_XHTML, nsHtml5Atoms::form, attributes);
  formPointer = elt;
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
  nsIContent** elt = createElement(kNameSpaceID_XHTML, elementName->name, attributes);
  nsHtml5StackNode* current = stack[currentPtr];
  if (current->isFosterParenting()) {

    insertIntoFosterParent(elt);
  } else {
    appendElement(elt, current->node);
  }
  nsHtml5StackNode* node = new nsHtml5StackNode(elementName, elt, attributes->cloneAttributes(nsnull));
  push(node);
  append(node);
  node->retain();
}

void 
nsHtml5TreeBuilder::appendToCurrentNodeAndPushElement(nsHtml5ElementName* elementName, nsHtml5HtmlAttributes* attributes)
{
  nsIContent** elt = createElement(kNameSpaceID_XHTML, elementName->name, attributes);
  appendElement(elt, stack[currentPtr]->node);
  nsHtml5StackNode* node = new nsHtml5StackNode(elementName, elt);
  push(node);
}

void 
nsHtml5TreeBuilder::appendToCurrentNodeAndPushElementMayFoster(nsHtml5ElementName* elementName, nsHtml5HtmlAttributes* attributes)
{
  nsIAtom* popName = elementName->name;
  nsIContent** elt = createElement(kNameSpaceID_XHTML, popName, attributes);
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
  nsIContent** elt = createElement(kNameSpaceID_MathML, popName, attributes);
  nsHtml5StackNode* current = stack[currentPtr];
  if (current->isFosterParenting()) {

    insertIntoFosterParent(elt);
  } else {
    appendElement(elt, current->node);
  }
  bool markAsHtmlIntegrationPoint = false;
  if (nsHtml5ElementName::ELT_ANNOTATION_XML == elementName && annotationXmlEncodingPermitsHtml(attributes)) {
    markAsHtmlIntegrationPoint = PR_TRUE;
  }
  nsHtml5StackNode* node = new nsHtml5StackNode(elementName, elt, popName, markAsHtmlIntegrationPoint);
  push(node);
}

bool 
nsHtml5TreeBuilder::annotationXmlEncodingPermitsHtml(nsHtml5HtmlAttributes* attributes)
{
  nsString* encoding = attributes->getValue(nsHtml5AttributeName::ATTR_ENCODING);
  if (!encoding) {
    return PR_FALSE;
  }
  return nsHtml5Portability::lowerCaseLiteralEqualsIgnoreAsciiCaseString("application/xhtml+xml", encoding) || nsHtml5Portability::lowerCaseLiteralEqualsIgnoreAsciiCaseString("text/html", encoding);
}

void 
nsHtml5TreeBuilder::appendToCurrentNodeAndPushElementMayFosterSVG(nsHtml5ElementName* elementName, nsHtml5HtmlAttributes* attributes)
{
  nsIAtom* popName = elementName->camelCaseName;
  nsIContent** elt = createElement(kNameSpaceID_SVG, popName, attributes);
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
nsHtml5TreeBuilder::appendToCurrentNodeAndPushElementMayFoster(nsHtml5ElementName* elementName, nsHtml5HtmlAttributes* attributes, nsIContent** form)
{
  nsIContent** elt = createElement(kNameSpaceID_XHTML, elementName->name, attributes, fragment ? nsnull : form);
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
nsHtml5TreeBuilder::appendVoidElementToCurrentMayFoster(nsIAtom* name, nsHtml5HtmlAttributes* attributes, nsIContent** form)
{
  nsIContent** elt = createElement(kNameSpaceID_XHTML, name, attributes, fragment ? nsnull : form);
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
  nsIContent** elt = createElement(kNameSpaceID_XHTML, popName, attributes);
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
  nsIContent** elt = createElement(kNameSpaceID_SVG, popName, attributes);
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
  nsIContent** elt = createElement(kNameSpaceID_MathML, popName, attributes);
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
nsHtml5TreeBuilder::appendVoidElementToCurrent(nsIAtom* name, nsHtml5HtmlAttributes* attributes, nsIContent** form)
{
  nsIContent** elt = createElement(kNameSpaceID_XHTML, name, attributes, fragment ? nsnull : form);
  nsHtml5StackNode* current = stack[currentPtr];
  appendElement(elt, current->node);
  elementPushed(kNameSpaceID_XHTML, name, elt);
  elementPopped(kNameSpaceID_XHTML, name, elt);
}

void 
nsHtml5TreeBuilder::appendVoidFormToCurrent(nsHtml5HtmlAttributes* attributes)
{
  nsIContent** elt = createElement(kNameSpaceID_XHTML, nsHtml5Atoms::form, attributes);
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
nsHtml5TreeBuilder::isInForeignButNotHtmlIntegrationPoint()
{
  return currentPtr >= 0 && stack[currentPtr]->ns != kNameSpaceID_XHTML && !stack[currentPtr]->isHtmlIntegrationPoint();
}

void 
nsHtml5TreeBuilder::setFragmentContext(nsIAtom* context, PRInt32 ns, nsIContent** node, bool quirks)
{
  this->contextName = context;
  this->contextNamespace = ns;
  this->contextNode = node;
  this->fragment = (!!contextName);
  this->quirks = quirks;
}

nsIContent** 
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
nsHtml5TreeBuilder::flushCharacters()
{
  if (charBufferLen > 0) {
    if ((mode == NS_HTML5TREE_BUILDER_IN_TABLE || mode == NS_HTML5TREE_BUILDER_IN_TABLE_BODY || mode == NS_HTML5TREE_BUILDER_IN_ROW) && charBufferContainsNonWhitespace()) {

      reconstructTheActiveFormattingElements();
      if (!stack[currentPtr]->isFosterParenting()) {
        appendCharacters(currentNode(), charBuffer, 0, charBufferLen);
        charBufferLen = 0;
        return;
      }
      PRInt32 eltPos = findLastOrRoot(NS_HTML5TREE_BUILDER_TABLE);
      nsHtml5StackNode* node = stack[eltPos];
      nsIContent** elt = node->node;
      if (!eltPos) {
        appendCharacters(elt, charBuffer, 0, charBufferLen);
        charBufferLen = 0;
        return;
      }
      insertFosterParentedCharacters(charBuffer, 0, charBufferLen, elt, stack[eltPos - 1]->node);
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
  for (PRInt32 i = 0; i < charBufferLen; i++) {
    switch(charBuffer[i]) {
      case ' ':
      case '\t':
      case '\n':
      case '\r':
      case '\f': {
        continue;
      }
      default: {
        return PR_TRUE;
      }
    }
  }
  return PR_FALSE;
}

nsAHtml5TreeBuilderState* 
nsHtml5TreeBuilder::newSnapshot()
{
  jArray<nsHtml5StackNode*,PRInt32> listCopy = jArray<nsHtml5StackNode*,PRInt32>::newJArray(listPtr + 1);
  for (PRInt32 i = 0; i < listCopy.length; i++) {
    nsHtml5StackNode* node = listOfActiveFormattingElements[i];
    if (node) {
      nsHtml5StackNode* newNode = new nsHtml5StackNode(node->getFlags(), node->ns, node->name, node->node, node->popName, node->attributes->cloneAttributes(nsnull));
      listCopy[i] = newNode;
    } else {
      listCopy[i] = nsnull;
    }
  }
  jArray<nsHtml5StackNode*,PRInt32> stackCopy = jArray<nsHtml5StackNode*,PRInt32>::newJArray(currentPtr + 1);
  for (PRInt32 i = 0; i < stackCopy.length; i++) {
    nsHtml5StackNode* node = stack[i];
    PRInt32 listIndex = findInListOfActiveFormattingElements(node);
    if (listIndex == -1) {
      nsHtml5StackNode* newNode = new nsHtml5StackNode(node->getFlags(), node->ns, node->name, node->node, node->popName, nsnull);
      stackCopy[i] = newNode;
    } else {
      stackCopy[i] = listCopy[listIndex];
      stackCopy[i]->retain();
    }
  }
  return new nsHtml5StateSnapshot(stackCopy, listCopy, formPointer, headPointer, deepTreeSurrogateParent, mode, originalMode, framesetOk, needToDropLF, quirks);
}

bool 
nsHtml5TreeBuilder::snapshotMatches(nsAHtml5TreeBuilderState* snapshot)
{
  jArray<nsHtml5StackNode*,PRInt32> stackCopy = snapshot->getStack();
  PRInt32 stackLen = snapshot->getStackLength();
  jArray<nsHtml5StackNode*,PRInt32> listCopy = snapshot->getListOfActiveFormattingElements();
  PRInt32 listLen = snapshot->getListOfActiveFormattingElementsLength();
  if (stackLen != currentPtr + 1 || listLen != listPtr + 1 || formPointer != snapshot->getFormPointer() || headPointer != snapshot->getHeadPointer() || deepTreeSurrogateParent != snapshot->getDeepTreeSurrogateParent() || mode != snapshot->getMode() || originalMode != snapshot->getOriginalMode() || framesetOk != snapshot->isFramesetOk() || needToDropLF != snapshot->isNeedToDropLF() || quirks != snapshot->isQuirks()) {
    return PR_FALSE;
  }
  for (PRInt32 i = listLen - 1; i >= 0; i--) {
    if (!listCopy[i] && !listOfActiveFormattingElements[i]) {
      continue;
    } else if (!listCopy[i] || !listOfActiveFormattingElements[i]) {
      return PR_FALSE;
    }
    if (listCopy[i]->node != listOfActiveFormattingElements[i]->node) {
      return PR_FALSE;
    }
  }
  for (PRInt32 i = stackLen - 1; i >= 0; i--) {
    if (stackCopy[i]->node != stack[i]->node) {
      return PR_FALSE;
    }
  }
  return PR_TRUE;
}

void 
nsHtml5TreeBuilder::loadState(nsAHtml5TreeBuilderState* snapshot, nsHtml5AtomTable* interner)
{
  jArray<nsHtml5StackNode*,PRInt32> stackCopy = snapshot->getStack();
  PRInt32 stackLen = snapshot->getStackLength();
  jArray<nsHtml5StackNode*,PRInt32> listCopy = snapshot->getListOfActiveFormattingElements();
  PRInt32 listLen = snapshot->getListOfActiveFormattingElementsLength();
  for (PRInt32 i = 0; i <= listPtr; i++) {
    if (listOfActiveFormattingElements[i]) {
      listOfActiveFormattingElements[i]->release();
    }
  }
  if (listOfActiveFormattingElements.length < listLen) {
    listOfActiveFormattingElements = jArray<nsHtml5StackNode*,PRInt32>::newJArray(listLen);
  }
  listPtr = listLen - 1;
  for (PRInt32 i = 0; i <= currentPtr; i++) {
    stack[i]->release();
  }
  if (stack.length < stackLen) {
    stack = jArray<nsHtml5StackNode*,PRInt32>::newJArray(stackLen);
  }
  currentPtr = stackLen - 1;
  for (PRInt32 i = 0; i < listLen; i++) {
    nsHtml5StackNode* node = listCopy[i];
    if (node) {
      nsHtml5StackNode* newNode = new nsHtml5StackNode(node->getFlags(), node->ns, nsHtml5Portability::newLocalFromLocal(node->name, interner), node->node, nsHtml5Portability::newLocalFromLocal(node->popName, interner), node->attributes->cloneAttributes(nsnull));
      listOfActiveFormattingElements[i] = newNode;
    } else {
      listOfActiveFormattingElements[i] = nsnull;
    }
  }
  for (PRInt32 i = 0; i < stackLen; i++) {
    nsHtml5StackNode* node = stackCopy[i];
    PRInt32 listIndex = findInArray(node, listCopy);
    if (listIndex == -1) {
      nsHtml5StackNode* newNode = new nsHtml5StackNode(node->getFlags(), node->ns, nsHtml5Portability::newLocalFromLocal(node->name, interner), node->node, nsHtml5Portability::newLocalFromLocal(node->popName, interner), nsnull);
      stack[i] = newNode;
    } else {
      stack[i] = listOfActiveFormattingElements[listIndex];
      stack[i]->retain();
    }
  }
  formPointer = snapshot->getFormPointer();
  headPointer = snapshot->getHeadPointer();
  deepTreeSurrogateParent = snapshot->getDeepTreeSurrogateParent();
  mode = snapshot->getMode();
  originalMode = snapshot->getOriginalMode();
  framesetOk = snapshot->isFramesetOk();
  needToDropLF = snapshot->isNeedToDropLF();
  quirks = snapshot->isQuirks();
}

PRInt32 
nsHtml5TreeBuilder::findInArray(nsHtml5StackNode* node, jArray<nsHtml5StackNode*,PRInt32> arr)
{
  for (PRInt32 i = listPtr; i >= 0; i--) {
    if (node == arr[i]) {
      return i;
    }
  }
  return -1;
}

nsIContent** 
nsHtml5TreeBuilder::getFormPointer()
{
  return formPointer;
}

nsIContent** 
nsHtml5TreeBuilder::getHeadPointer()
{
  return headPointer;
}

nsIContent** 
nsHtml5TreeBuilder::getDeepTreeSurrogateParent()
{
  return deepTreeSurrogateParent;
}

jArray<nsHtml5StackNode*,PRInt32> 
nsHtml5TreeBuilder::getListOfActiveFormattingElements()
{
  return listOfActiveFormattingElements;
}

jArray<nsHtml5StackNode*,PRInt32> 
nsHtml5TreeBuilder::getStack()
{
  return stack;
}

PRInt32 
nsHtml5TreeBuilder::getMode()
{
  return mode;
}

PRInt32 
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

PRInt32 
nsHtml5TreeBuilder::getListOfActiveFormattingElementsLength()
{
  return listPtr + 1;
}

PRInt32 
nsHtml5TreeBuilder::getStackLength()
{
  return currentPtr + 1;
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

