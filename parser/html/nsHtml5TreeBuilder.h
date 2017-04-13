/*
 * Copyright (c) 2007 Henri Sivonen
 * Copyright (c) 2007-2015 Mozilla Foundation
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

#ifndef nsHtml5TreeBuilder_h
#define nsHtml5TreeBuilder_h

#include "nsContentUtils.h"
#include "nsIAtom.h"
#include "nsHtml5AtomTable.h"
#include "nsITimer.h"
#include "nsHtml5String.h"
#include "nsNameSpaceManager.h"
#include "nsIContent.h"
#include "nsTraceRefcnt.h"
#include "jArray.h"
#include "nsHtml5DocumentMode.h"
#include "nsHtml5ArrayCopy.h"
#include "nsHtml5Parser.h"
#include "nsHtml5Atoms.h"
#include "nsHtml5TreeOperation.h"
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

class nsHtml5StreamParser;

class nsHtml5Tokenizer;
class nsHtml5MetaScanner;
class nsHtml5AttributeName;
class nsHtml5ElementName;
class nsHtml5UTF16Buffer;
class nsHtml5StateSnapshot;
class nsHtml5Portability;


class nsHtml5TreeBuilder : public nsAHtml5TreeBuilderState
{
  private:
    static char16_t REPLACEMENT_CHARACTER[];
    static staticJArray<const char*,int32_t> QUIRKY_PUBLIC_IDS;
    int32_t mode;
    int32_t originalMode;
    bool framesetOk;
  protected:
    nsHtml5Tokenizer* tokenizer;
  private:
    bool scriptingEnabled;
    bool needToDropLF;
    bool fragment;
    nsIAtom* contextName;
    int32_t contextNamespace;
    nsIContentHandle* contextNode;
    autoJArray<int32_t,int32_t> templateModeStack;
    int32_t templateModePtr;
    autoJArray<nsHtml5StackNode*,int32_t> stack;
    int32_t currentPtr;
    autoJArray<nsHtml5StackNode*,int32_t> listOfActiveFormattingElements;
    int32_t listPtr;
    nsIContentHandle* formPointer;
    nsIContentHandle* headPointer;
    nsIContentHandle* deepTreeSurrogateParent;
  protected:
    autoJArray<char16_t,int32_t> charBuffer;
    int32_t charBufferLen;
  private:
    bool quirks;
    bool isSrcdocDocument;
  public:
    void startTokenization(nsHtml5Tokenizer* self);
    void doctype(nsIAtom* name,
                 nsHtml5String publicIdentifier,
                 nsHtml5String systemIdentifier,
                 bool forceQuirks);
    void comment(char16_t* buf, int32_t start, int32_t length);
    void characters(const char16_t* buf, int32_t start, int32_t length);
    void zeroOriginatingReplacementCharacter();
    void eof();
    void endTokenization();
    void startTag(nsHtml5ElementName* elementName, nsHtml5HtmlAttributes* attributes, bool selfClosing);
  private:
    void startTagTitleInHead(nsHtml5ElementName* elementName, nsHtml5HtmlAttributes* attributes);
    void startTagGenericRawText(nsHtml5ElementName* elementName, nsHtml5HtmlAttributes* attributes);
    void startTagScriptInHead(nsHtml5ElementName* elementName, nsHtml5HtmlAttributes* attributes);
    void startTagTemplateInHead(nsHtml5ElementName* elementName, nsHtml5HtmlAttributes* attributes);
    bool isTemplateContents();
    bool isTemplateModeStackEmpty();
    bool isSpecialParentInForeign(nsHtml5StackNode* stackNode);
  public:
    static nsHtml5String extractCharsetFromContent(nsHtml5String attributeValue,
                                                   nsHtml5TreeBuilder* tb);

  private:
    void checkMetaCharset(nsHtml5HtmlAttributes* attributes);
  public:
    void endTag(nsHtml5ElementName* elementName);
  private:
    void endTagTemplateInHead();
    int32_t findLastInTableScopeOrRootTemplateTbodyTheadTfoot();
    int32_t findLast(nsIAtom* name);
    int32_t findLastInTableScope(nsIAtom* name);
    int32_t findLastInButtonScope(nsIAtom* name);
    int32_t findLastInScope(nsIAtom* name);
    int32_t findLastInListScope(nsIAtom* name);
    int32_t findLastInScopeHn();
    void generateImpliedEndTagsExceptFor(nsIAtom* name);
    void generateImpliedEndTags();
    bool isSecondOnStackBody();
    void documentModeInternal(nsHtml5DocumentMode m,
                              nsHtml5String publicIdentifier,
                              nsHtml5String systemIdentifier,
                              bool html4SpecificAdditionalErrorChecks);
    bool isAlmostStandards(nsHtml5String publicIdentifier,
                           nsHtml5String systemIdentifier);
    bool isQuirky(nsIAtom* name,
                  nsHtml5String publicIdentifier,
                  nsHtml5String systemIdentifier,
                  bool forceQuirks);
    void closeTheCell(int32_t eltPos);
    int32_t findLastInTableScopeTdTh();
    void clearStackBackTo(int32_t eltPos);
    void resetTheInsertionMode();
    void implicitlyCloseP();
    bool debugOnlyClearLastStackSlot();
    bool debugOnlyClearLastListSlot();
    void pushTemplateMode(int32_t mode);
    void push(nsHtml5StackNode* node);
    void silentPush(nsHtml5StackNode* node);
    void append(nsHtml5StackNode* node);
    inline void insertMarker()
    {
      append(nullptr);
    }

    void clearTheListOfActiveFormattingElementsUpToTheLastMarker();
    inline bool isCurrent(nsIAtom* name)
    {
      return stack[currentPtr]->ns == kNameSpaceID_XHTML && name == stack[currentPtr]->name;
    }

    void removeFromStack(int32_t pos);
    void removeFromStack(nsHtml5StackNode* node);
    void removeFromListOfActiveFormattingElements(int32_t pos);
    bool adoptionAgencyEndTag(nsIAtom* name);
    void insertIntoStack(nsHtml5StackNode* node, int32_t position);
    void insertIntoListOfActiveFormattingElements(nsHtml5StackNode* formattingClone, int32_t bookmark);
    int32_t findInListOfActiveFormattingElements(nsHtml5StackNode* node);
    int32_t findInListOfActiveFormattingElementsContainsBetweenEndAndLastMarker(nsIAtom* name);
    void maybeForgetEarlierDuplicateFormattingElement(nsIAtom* name, nsHtml5HtmlAttributes* attributes);
    int32_t findLastOrRoot(nsIAtom* name);
    int32_t findLastOrRoot(int32_t group);
    bool addAttributesToBody(nsHtml5HtmlAttributes* attributes);
    void addAttributesToHtml(nsHtml5HtmlAttributes* attributes);
    void pushHeadPointerOntoStack();
    void reconstructTheActiveFormattingElements();
    void insertIntoFosterParent(nsIContentHandle* child);
    nsIContentHandle* createAndInsertFosterParentedElement(int32_t ns, nsIAtom* name, nsHtml5HtmlAttributes* attributes);
    nsIContentHandle* createAndInsertFosterParentedElement(int32_t ns, nsIAtom* name, nsHtml5HtmlAttributes* attributes, nsIContentHandle* form);
    bool isInStack(nsHtml5StackNode* node);
    void popTemplateMode();
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
    void appendToCurrentNodeAndPushElementMayFoster(nsHtml5ElementName* elementName, nsHtml5HtmlAttributes* attributes, nsIContentHandle* form);
    void appendVoidElementToCurrentMayFoster(nsIAtom* name, nsHtml5HtmlAttributes* attributes, nsIContentHandle* form);
    void appendVoidElementToCurrentMayFoster(nsHtml5ElementName* elementName, nsHtml5HtmlAttributes* attributes);
    void appendVoidElementToCurrentMayFosterSVG(nsHtml5ElementName* elementName, nsHtml5HtmlAttributes* attributes);
    void appendVoidElementToCurrentMayFosterMathML(nsHtml5ElementName* elementName, nsHtml5HtmlAttributes* attributes);
    void appendVoidElementToCurrent(nsIAtom* name, nsHtml5HtmlAttributes* attributes, nsIContentHandle* form);
    void appendVoidFormToCurrent(nsHtml5HtmlAttributes* attributes);
  protected:
    void accumulateCharacters(const char16_t* buf, int32_t start, int32_t length);
    void requestSuspension();
    nsIContentHandle* createElement(int32_t ns, nsIAtom* name, nsHtml5HtmlAttributes* attributes, nsIContentHandle* intendedParent);
    nsIContentHandle* createElement(int32_t ns, nsIAtom* name, nsHtml5HtmlAttributes* attributes, nsIContentHandle* form, nsIContentHandle* intendedParent);
    nsIContentHandle* createHtmlElementSetAsRoot(nsHtml5HtmlAttributes* attributes);
    void detachFromParent(nsIContentHandle* element);
    bool hasChildren(nsIContentHandle* element);
    void appendElement(nsIContentHandle* child, nsIContentHandle* newParent);
    void appendChildrenToNewParent(nsIContentHandle* oldParent, nsIContentHandle* newParent);
    void insertFosterParentedChild(nsIContentHandle* child, nsIContentHandle* table, nsIContentHandle* stackParent);
    nsIContentHandle* createAndInsertFosterParentedElement(int32_t ns, nsIAtom* name, nsHtml5HtmlAttributes* attributes, nsIContentHandle* form, nsIContentHandle* table, nsIContentHandle* stackParent);
    ;void insertFosterParentedCharacters(char16_t* buf, int32_t start, int32_t length, nsIContentHandle* table, nsIContentHandle* stackParent);
    void appendCharacters(nsIContentHandle* parent, char16_t* buf, int32_t start, int32_t length);
    void appendIsindexPrompt(nsIContentHandle* parent);
    void appendComment(nsIContentHandle* parent, char16_t* buf, int32_t start, int32_t length);
    void appendCommentToDocument(char16_t* buf, int32_t start, int32_t length);
    void addAttributesToElement(nsIContentHandle* element, nsHtml5HtmlAttributes* attributes);
    void markMalformedIfScript(nsIContentHandle* elt);
    void start(bool fragmentMode);
    void end();
    void appendDoctypeToDocument(nsIAtom* name,
                                 nsHtml5String publicIdentifier,
                                 nsHtml5String systemIdentifier);
    void elementPushed(int32_t ns, nsIAtom* name, nsIContentHandle* node);
    void elementPopped(int32_t ns, nsIAtom* name, nsIContentHandle* node);
  public:
    inline bool cdataSectionAllowed()
    {
      return isInForeign();
    }

  private:
    bool isInForeign();
    bool isInForeignButNotHtmlOrMathTextIntegrationPoint();
  public:
    void setFragmentContext(nsIAtom* context, int32_t ns, nsIContentHandle* node, bool quirks);
  protected:
    nsIContentHandle* currentNode();
  public:
    bool isScriptingEnabled();
    void setScriptingEnabled(bool scriptingEnabled);
    void setIsSrcdocDocument(bool isSrcdocDocument);
    void flushCharacters();
  private:
    bool charBufferContainsNonWhitespace();
  public:
    nsAHtml5TreeBuilderState* newSnapshot();
    bool snapshotMatches(nsAHtml5TreeBuilderState* snapshot);
    void loadState(nsAHtml5TreeBuilderState* snapshot, nsHtml5AtomTable* interner);
  private:
    int32_t findInArray(nsHtml5StackNode* node, jArray<nsHtml5StackNode*,int32_t> arr);
  public:
    nsIContentHandle* getFormPointer();
    nsIContentHandle* getHeadPointer();
    nsIContentHandle* getDeepTreeSurrogateParent();
    jArray<nsHtml5StackNode*,int32_t> getListOfActiveFormattingElements();
    jArray<nsHtml5StackNode*,int32_t> getStack();
    jArray<int32_t,int32_t> getTemplateModeStack();
    int32_t getMode();
    int32_t getOriginalMode();
    bool isFramesetOk();
    bool isNeedToDropLF();
    bool isQuirks();
    int32_t getListOfActiveFormattingElementsLength();
    int32_t getStackLength();
    int32_t getTemplateModeStackLength();
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
#define NS_HTML5TREE_BUILDER_EMBED 48
#define NS_HTML5TREE_BUILDER_AREA_OR_WBR 49
#define NS_HTML5TREE_BUILDER_DIV_OR_BLOCKQUOTE_OR_CENTER_OR_MENU 50
#define NS_HTML5TREE_BUILDER_ADDRESS_OR_ARTICLE_OR_ASIDE_OR_DETAILS_OR_DIALOG_OR_DIR_OR_FIGCAPTION_OR_FIGURE_OR_FOOTER_OR_HEADER_OR_HGROUP_OR_MAIN_OR_NAV_OR_SECTION_OR_SUMMARY 51
#define NS_HTML5TREE_BUILDER_RUBY_OR_SPAN_OR_SUB_OR_SUP_OR_VAR 52
#define NS_HTML5TREE_BUILDER_RB_OR_RTC 53
#define NS_HTML5TREE_BUILDER_PARAM_OR_SOURCE_OR_TRACK 55
#define NS_HTML5TREE_BUILDER_MGLYPH_OR_MALIGNMARK 56
#define NS_HTML5TREE_BUILDER_MI_MO_MN_MS_MTEXT 57
#define NS_HTML5TREE_BUILDER_ANNOTATION_XML 58
#define NS_HTML5TREE_BUILDER_FOREIGNOBJECT_OR_DESC 59
#define NS_HTML5TREE_BUILDER_NOEMBED 60
#define NS_HTML5TREE_BUILDER_FIELDSET 61
#define NS_HTML5TREE_BUILDER_OUTPUT 62
#define NS_HTML5TREE_BUILDER_OBJECT 63
#define NS_HTML5TREE_BUILDER_FONT 64
#define NS_HTML5TREE_BUILDER_KEYGEN 65
#define NS_HTML5TREE_BUILDER_MENUITEM 66
#define NS_HTML5TREE_BUILDER_TEMPLATE 67
#define NS_HTML5TREE_BUILDER_IMG 68
#define NS_HTML5TREE_BUILDER_RT_OR_RP 69
#define NS_HTML5TREE_BUILDER_IN_ROW 0
#define NS_HTML5TREE_BUILDER_IN_TABLE_BODY 1
#define NS_HTML5TREE_BUILDER_IN_TABLE 2
#define NS_HTML5TREE_BUILDER_IN_CAPTION 3
#define NS_HTML5TREE_BUILDER_IN_CELL 4
#define NS_HTML5TREE_BUILDER_FRAMESET_OK 5
#define NS_HTML5TREE_BUILDER_IN_BODY 6
#define NS_HTML5TREE_BUILDER_IN_HEAD 7
#define NS_HTML5TREE_BUILDER_IN_HEAD_NOSCRIPT 8
#define NS_HTML5TREE_BUILDER_IN_COLUMN_GROUP 9
#define NS_HTML5TREE_BUILDER_IN_SELECT_IN_TABLE 10
#define NS_HTML5TREE_BUILDER_IN_SELECT 11
#define NS_HTML5TREE_BUILDER_AFTER_BODY 12
#define NS_HTML5TREE_BUILDER_IN_FRAMESET 13
#define NS_HTML5TREE_BUILDER_AFTER_FRAMESET 14
#define NS_HTML5TREE_BUILDER_INITIAL 15
#define NS_HTML5TREE_BUILDER_BEFORE_HTML 16
#define NS_HTML5TREE_BUILDER_BEFORE_HEAD 17
#define NS_HTML5TREE_BUILDER_AFTER_HEAD 18
#define NS_HTML5TREE_BUILDER_AFTER_AFTER_BODY 19
#define NS_HTML5TREE_BUILDER_AFTER_AFTER_FRAMESET 20
#define NS_HTML5TREE_BUILDER_TEXT 21
#define NS_HTML5TREE_BUILDER_IN_TEMPLATE 22
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
#define NS_HTML5TREE_BUILDER_NOT_FOUND_ON_STACK INT32_MAX


#endif

