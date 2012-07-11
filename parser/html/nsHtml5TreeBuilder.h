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

#ifndef nsHtml5TreeBuilder_h__
#define nsHtml5TreeBuilder_h__

#include "prtypes.h"
#include "nsIAtom.h"
#include "nsHtml5AtomTable.h"
#include "nsITimer.h"
#include "nsString.h"
#include "nsINameSpaceManager.h"
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
#include "nsHtml5ViewSourceUtils.h"

class nsHtml5StreamParser;

class nsHtml5Tokenizer;
class nsHtml5MetaScanner;
class nsHtml5AttributeName;
class nsHtml5ElementName;
class nsHtml5HtmlAttributes;
class nsHtml5UTF16Buffer;
class nsHtml5StateSnapshot;
class nsHtml5Portability;


class nsHtml5TreeBuilder : public nsAHtml5TreeBuilderState
{
  private:
    static PRUnichar REPLACEMENT_CHARACTER[];
    static staticJArray<const char*,PRInt32> QUIRKY_PUBLIC_IDS;
    PRInt32 mode;
    PRInt32 originalMode;
    bool framesetOk;
  protected:
    nsHtml5Tokenizer* tokenizer;
  private:
    bool scriptingEnabled;
    bool needToDropLF;
    bool fragment;
    nsIAtom* contextName;
    PRInt32 contextNamespace;
    nsIContent** contextNode;
    autoJArray<nsHtml5StackNode*,PRInt32> stack;
    PRInt32 currentPtr;
    autoJArray<nsHtml5StackNode*,PRInt32> listOfActiveFormattingElements;
    PRInt32 listPtr;
    nsIContent** formPointer;
    nsIContent** headPointer;
    nsIContent** deepTreeSurrogateParent;
  protected:
    autoJArray<PRUnichar,PRInt32> charBuffer;
    PRInt32 charBufferLen;
  private:
    bool quirks;
  public:
    void startTokenization(nsHtml5Tokenizer* self);
    void doctype(nsIAtom* name, nsString* publicIdentifier, nsString* systemIdentifier, bool forceQuirks);
    void comment(PRUnichar* buf, PRInt32 start, PRInt32 length);
    void characters(const PRUnichar* buf, PRInt32 start, PRInt32 length);
    void zeroOriginatingReplacementCharacter();
    void eof();
    void endTokenization();
    void startTag(nsHtml5ElementName* elementName, nsHtml5HtmlAttributes* attributes, bool selfClosing);
  private:
    bool isSpecialParentInForeign(nsHtml5StackNode* stackNode);
  public:
    static nsString* extractCharsetFromContent(nsString* attributeValue);
  private:
    void checkMetaCharset(nsHtml5HtmlAttributes* attributes);
  public:
    void endTag(nsHtml5ElementName* elementName);
  private:
    PRInt32 findLastInTableScopeOrRootTbodyTheadTfoot();
    PRInt32 findLast(nsIAtom* name);
    PRInt32 findLastInTableScope(nsIAtom* name);
    PRInt32 findLastInButtonScope(nsIAtom* name);
    PRInt32 findLastInScope(nsIAtom* name);
    PRInt32 findLastInListScope(nsIAtom* name);
    PRInt32 findLastInScopeHn();
    void generateImpliedEndTagsExceptFor(nsIAtom* name);
    void generateImpliedEndTags();
    bool isSecondOnStackBody();
    void documentModeInternal(nsHtml5DocumentMode m, nsString* publicIdentifier, nsString* systemIdentifier, bool html4SpecificAdditionalErrorChecks);
    bool isAlmostStandards(nsString* publicIdentifier, nsString* systemIdentifier);
    bool isQuirky(nsIAtom* name, nsString* publicIdentifier, nsString* systemIdentifier, bool forceQuirks);
    void closeTheCell(PRInt32 eltPos);
    PRInt32 findLastInTableScopeTdTh();
    void clearStackBackTo(PRInt32 eltPos);
    void resetTheInsertionMode();
    void implicitlyCloseP();
    bool clearLastStackSlot();
    bool clearLastListSlot();
    void push(nsHtml5StackNode* node);
    void silentPush(nsHtml5StackNode* node);
    void append(nsHtml5StackNode* node);
    inline void insertMarker()
    {
      append(nsnull);
    }

    void clearTheListOfActiveFormattingElementsUpToTheLastMarker();
    inline bool isCurrent(nsIAtom* name)
    {
      return name == stack[currentPtr]->name;
    }

    void removeFromStack(PRInt32 pos);
    void removeFromStack(nsHtml5StackNode* node);
    void removeFromListOfActiveFormattingElements(PRInt32 pos);
    bool adoptionAgencyEndTag(nsIAtom* name);
    void insertIntoStack(nsHtml5StackNode* node, PRInt32 position);
    void insertIntoListOfActiveFormattingElements(nsHtml5StackNode* formattingClone, PRInt32 bookmark);
    PRInt32 findInListOfActiveFormattingElements(nsHtml5StackNode* node);
    PRInt32 findInListOfActiveFormattingElementsContainsBetweenEndAndLastMarker(nsIAtom* name);
    void maybeForgetEarlierDuplicateFormattingElement(nsIAtom* name, nsHtml5HtmlAttributes* attributes);
    PRInt32 findLastOrRoot(nsIAtom* name);
    PRInt32 findLastOrRoot(PRInt32 group);
    bool addAttributesToBody(nsHtml5HtmlAttributes* attributes);
    void addAttributesToHtml(nsHtml5HtmlAttributes* attributes);
    void pushHeadPointerOntoStack();
    void reconstructTheActiveFormattingElements();
    void insertIntoFosterParent(nsIContent** child);
    bool isInStack(nsHtml5StackNode* node);
    void pop();
    void silentPop();
    void popOnEof();
    void appendHtmlElementToDocumentAndPush(nsHtml5HtmlAttributes* attributes);
    void appendHtmlElementToDocumentAndPush();
    void appendToCurrentNodeAndPushHeadElement(nsHtml5HtmlAttributes* attributes);
    void appendToCurrentNodeAndPushBodyElement(nsHtml5HtmlAttributes* attributes);
    void appendToCurrentNodeAndPushBodyElement();
    void appendToCurrentNodeAndPushFormElementMayFoster(nsHtml5HtmlAttributes* attributes);
    void appendToCurrentNodeAndPushFormattingElementMayFoster(nsHtml5ElementName* elementName, nsHtml5HtmlAttributes* attributes);
    void appendToCurrentNodeAndPushElement(nsHtml5ElementName* elementName, nsHtml5HtmlAttributes* attributes);
    void appendToCurrentNodeAndPushElementMayFoster(nsHtml5ElementName* elementName, nsHtml5HtmlAttributes* attributes);
    void appendToCurrentNodeAndPushElementMayFosterMathML(nsHtml5ElementName* elementName, nsHtml5HtmlAttributes* attributes);
    bool annotationXmlEncodingPermitsHtml(nsHtml5HtmlAttributes* attributes);
    void appendToCurrentNodeAndPushElementMayFosterSVG(nsHtml5ElementName* elementName, nsHtml5HtmlAttributes* attributes);
    void appendToCurrentNodeAndPushElementMayFoster(nsHtml5ElementName* elementName, nsHtml5HtmlAttributes* attributes, nsIContent** form);
    void appendVoidElementToCurrentMayFoster(nsIAtom* name, nsHtml5HtmlAttributes* attributes, nsIContent** form);
    void appendVoidElementToCurrentMayFoster(nsHtml5ElementName* elementName, nsHtml5HtmlAttributes* attributes);
    void appendVoidElementToCurrentMayFosterSVG(nsHtml5ElementName* elementName, nsHtml5HtmlAttributes* attributes);
    void appendVoidElementToCurrentMayFosterMathML(nsHtml5ElementName* elementName, nsHtml5HtmlAttributes* attributes);
    void appendVoidElementToCurrent(nsIAtom* name, nsHtml5HtmlAttributes* attributes, nsIContent** form);
    void appendVoidFormToCurrent(nsHtml5HtmlAttributes* attributes);
  protected:
    void accumulateCharacters(const PRUnichar* buf, PRInt32 start, PRInt32 length);
    void requestSuspension();
    nsIContent** createElement(PRInt32 ns, nsIAtom* name, nsHtml5HtmlAttributes* attributes);
    nsIContent** createElement(PRInt32 ns, nsIAtom* name, nsHtml5HtmlAttributes* attributes, nsIContent** form);
    nsIContent** createHtmlElementSetAsRoot(nsHtml5HtmlAttributes* attributes);
    void detachFromParent(nsIContent** element);
    bool hasChildren(nsIContent** element);
    void appendElement(nsIContent** child, nsIContent** newParent);
    void appendChildrenToNewParent(nsIContent** oldParent, nsIContent** newParent);
    void insertFosterParentedChild(nsIContent** child, nsIContent** table, nsIContent** stackParent);
    void insertFosterParentedCharacters(PRUnichar* buf, PRInt32 start, PRInt32 length, nsIContent** table, nsIContent** stackParent);
    void appendCharacters(nsIContent** parent, PRUnichar* buf, PRInt32 start, PRInt32 length);
    void appendIsindexPrompt(nsIContent** parent);
    void appendComment(nsIContent** parent, PRUnichar* buf, PRInt32 start, PRInt32 length);
    void appendCommentToDocument(PRUnichar* buf, PRInt32 start, PRInt32 length);
    void addAttributesToElement(nsIContent** element, nsHtml5HtmlAttributes* attributes);
    void markMalformedIfScript(nsIContent** elt);
    void start(bool fragmentMode);
    void end();
    void appendDoctypeToDocument(nsIAtom* name, nsString* publicIdentifier, nsString* systemIdentifier);
    void elementPushed(PRInt32 ns, nsIAtom* name, nsIContent** node);
    void elementPopped(PRInt32 ns, nsIAtom* name, nsIContent** node);
  public:
    inline bool cdataSectionAllowed()
    {
      return isInForeign();
    }

  private:
    bool isInForeign();
    bool isInForeignButNotHtmlOrMathTextIntegrationPoint();
  public:
    void setFragmentContext(nsIAtom* context, PRInt32 ns, nsIContent** node, bool quirks);
  protected:
    nsIContent** currentNode();
  public:
    bool isScriptingEnabled();
    void setScriptingEnabled(bool scriptingEnabled);
    void flushCharacters();
  private:
    bool charBufferContainsNonWhitespace();
  public:
    nsAHtml5TreeBuilderState* newSnapshot();
    bool snapshotMatches(nsAHtml5TreeBuilderState* snapshot);
    void loadState(nsAHtml5TreeBuilderState* snapshot, nsHtml5AtomTable* interner);
  private:
    PRInt32 findInArray(nsHtml5StackNode* node, jArray<nsHtml5StackNode*,PRInt32> arr);
  public:
    nsIContent** getFormPointer();
    nsIContent** getHeadPointer();
    nsIContent** getDeepTreeSurrogateParent();
    jArray<nsHtml5StackNode*,PRInt32> getListOfActiveFormattingElements();
    jArray<nsHtml5StackNode*,PRInt32> getStack();
    PRInt32 getMode();
    PRInt32 getOriginalMode();
    bool isFramesetOk();
    bool isNeedToDropLF();
    bool isQuirks();
    PRInt32 getListOfActiveFormattingElementsLength();
    PRInt32 getStackLength();
    static void initializeStatics();
    static void releaseStatics();

#include "nsHtml5TreeBuilderHSupplement.h"
};

#define NS_HTML5TREE_BUILDER_OTHER 0
#define NS_HTML5TREE_BUILDER_A 1
#define NS_HTML5TREE_BUILDER_BASE 2
#define NS_HTML5TREE_BUILDER_BODY 3
#define NS_HTML5TREE_BUILDER_BR 4
#define NS_HTML5TREE_BUILDER_BUTTON 5
#define NS_HTML5TREE_BUILDER_CAPTION 6
#define NS_HTML5TREE_BUILDER_COL 7
#define NS_HTML5TREE_BUILDER_COLGROUP 8
#define NS_HTML5TREE_BUILDER_FORM 9
#define NS_HTML5TREE_BUILDER_FRAME 10
#define NS_HTML5TREE_BUILDER_FRAMESET 11
#define NS_HTML5TREE_BUILDER_IMAGE 12
#define NS_HTML5TREE_BUILDER_INPUT 13
#define NS_HTML5TREE_BUILDER_ISINDEX 14
#define NS_HTML5TREE_BUILDER_LI 15
#define NS_HTML5TREE_BUILDER_LINK_OR_BASEFONT_OR_BGSOUND 16
#define NS_HTML5TREE_BUILDER_MATH 17
#define NS_HTML5TREE_BUILDER_META 18
#define NS_HTML5TREE_BUILDER_SVG 19
#define NS_HTML5TREE_BUILDER_HEAD 20
#define NS_HTML5TREE_BUILDER_HR 22
#define NS_HTML5TREE_BUILDER_HTML 23
#define NS_HTML5TREE_BUILDER_NOBR 24
#define NS_HTML5TREE_BUILDER_NOFRAMES 25
#define NS_HTML5TREE_BUILDER_NOSCRIPT 26
#define NS_HTML5TREE_BUILDER_OPTGROUP 27
#define NS_HTML5TREE_BUILDER_OPTION 28
#define NS_HTML5TREE_BUILDER_P 29
#define NS_HTML5TREE_BUILDER_PLAINTEXT 30
#define NS_HTML5TREE_BUILDER_SCRIPT 31
#define NS_HTML5TREE_BUILDER_SELECT 32
#define NS_HTML5TREE_BUILDER_STYLE 33
#define NS_HTML5TREE_BUILDER_TABLE 34
#define NS_HTML5TREE_BUILDER_TEXTAREA 35
#define NS_HTML5TREE_BUILDER_TITLE 36
#define NS_HTML5TREE_BUILDER_TR 37
#define NS_HTML5TREE_BUILDER_XMP 38
#define NS_HTML5TREE_BUILDER_TBODY_OR_THEAD_OR_TFOOT 39
#define NS_HTML5TREE_BUILDER_TD_OR_TH 40
#define NS_HTML5TREE_BUILDER_DD_OR_DT 41
#define NS_HTML5TREE_BUILDER_H1_OR_H2_OR_H3_OR_H4_OR_H5_OR_H6 42
#define NS_HTML5TREE_BUILDER_MARQUEE_OR_APPLET 43
#define NS_HTML5TREE_BUILDER_PRE_OR_LISTING 44
#define NS_HTML5TREE_BUILDER_B_OR_BIG_OR_CODE_OR_EM_OR_I_OR_S_OR_SMALL_OR_STRIKE_OR_STRONG_OR_TT_OR_U 45
#define NS_HTML5TREE_BUILDER_UL_OR_OL_OR_DL 46
#define NS_HTML5TREE_BUILDER_IFRAME 47
#define NS_HTML5TREE_BUILDER_EMBED_OR_IMG 48
#define NS_HTML5TREE_BUILDER_AREA_OR_WBR 49
#define NS_HTML5TREE_BUILDER_DIV_OR_BLOCKQUOTE_OR_CENTER_OR_MENU 50
#define NS_HTML5TREE_BUILDER_ADDRESS_OR_ARTICLE_OR_ASIDE_OR_DETAILS_OR_DIR_OR_FIGCAPTION_OR_FIGURE_OR_FOOTER_OR_HEADER_OR_HGROUP_OR_NAV_OR_SECTION_OR_SUMMARY 51
#define NS_HTML5TREE_BUILDER_RUBY_OR_SPAN_OR_SUB_OR_SUP_OR_VAR 52
#define NS_HTML5TREE_BUILDER_RT_OR_RP 53
#define NS_HTML5TREE_BUILDER_COMMAND 54
#define NS_HTML5TREE_BUILDER_PARAM_OR_SOURCE_OR_TRACK 55
#define NS_HTML5TREE_BUILDER_MGLYPH_OR_MALIGNMARK 56
#define NS_HTML5TREE_BUILDER_MI_MO_MN_MS_MTEXT 57
#define NS_HTML5TREE_BUILDER_ANNOTATION_XML 58
#define NS_HTML5TREE_BUILDER_FOREIGNOBJECT_OR_DESC 59
#define NS_HTML5TREE_BUILDER_NOEMBED 60
#define NS_HTML5TREE_BUILDER_FIELDSET 61
#define NS_HTML5TREE_BUILDER_OUTPUT_OR_LABEL 62
#define NS_HTML5TREE_BUILDER_OBJECT 63
#define NS_HTML5TREE_BUILDER_FONT 64
#define NS_HTML5TREE_BUILDER_KEYGEN 65
#define NS_HTML5TREE_BUILDER_MENUITEM 66
#define NS_HTML5TREE_BUILDER_INITIAL 0
#define NS_HTML5TREE_BUILDER_BEFORE_HTML 1
#define NS_HTML5TREE_BUILDER_BEFORE_HEAD 2
#define NS_HTML5TREE_BUILDER_IN_HEAD 3
#define NS_HTML5TREE_BUILDER_IN_HEAD_NOSCRIPT 4
#define NS_HTML5TREE_BUILDER_AFTER_HEAD 5
#define NS_HTML5TREE_BUILDER_IN_BODY 6
#define NS_HTML5TREE_BUILDER_IN_TABLE 7
#define NS_HTML5TREE_BUILDER_IN_CAPTION 8
#define NS_HTML5TREE_BUILDER_IN_COLUMN_GROUP 9
#define NS_HTML5TREE_BUILDER_IN_TABLE_BODY 10
#define NS_HTML5TREE_BUILDER_IN_ROW 11
#define NS_HTML5TREE_BUILDER_IN_CELL 12
#define NS_HTML5TREE_BUILDER_IN_SELECT 13
#define NS_HTML5TREE_BUILDER_IN_SELECT_IN_TABLE 14
#define NS_HTML5TREE_BUILDER_AFTER_BODY 15
#define NS_HTML5TREE_BUILDER_IN_FRAMESET 16
#define NS_HTML5TREE_BUILDER_AFTER_FRAMESET 17
#define NS_HTML5TREE_BUILDER_AFTER_AFTER_BODY 18
#define NS_HTML5TREE_BUILDER_AFTER_AFTER_FRAMESET 19
#define NS_HTML5TREE_BUILDER_TEXT 20
#define NS_HTML5TREE_BUILDER_FRAMESET_OK 21
#define NS_HTML5TREE_BUILDER_CHARSET_INITIAL 0
#define NS_HTML5TREE_BUILDER_CHARSET_C 1
#define NS_HTML5TREE_BUILDER_CHARSET_H 2
#define NS_HTML5TREE_BUILDER_CHARSET_A 3
#define NS_HTML5TREE_BUILDER_CHARSET_R 4
#define NS_HTML5TREE_BUILDER_CHARSET_S 5
#define NS_HTML5TREE_BUILDER_CHARSET_E 6
#define NS_HTML5TREE_BUILDER_CHARSET_T 7
#define NS_HTML5TREE_BUILDER_CHARSET_EQUALS 8
#define NS_HTML5TREE_BUILDER_CHARSET_SINGLE_QUOTED 9
#define NS_HTML5TREE_BUILDER_CHARSET_DOUBLE_QUOTED 10
#define NS_HTML5TREE_BUILDER_CHARSET_UNQUOTED 11
#define NS_HTML5TREE_BUILDER_NOT_FOUND_ON_STACK PR_INT32_MAX


#endif

