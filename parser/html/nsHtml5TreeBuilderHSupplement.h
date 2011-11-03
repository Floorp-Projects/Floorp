/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is HTML Parser C++ Translator code.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2008-2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Henri Sivonen <hsivonen@iki.fi>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#define NS_HTML5_TREE_BUILDER_HANDLE_ARRAY_LENGTH 512

  private:
    nsHtml5Highlighter*                    mViewSource;
    nsTArray<nsHtml5TreeOperation>         mOpQueue;
    nsTArray<nsHtml5SpeculativeLoad>       mSpeculativeLoadQueue;
    nsAHtml5TreeOpSink*                    mOpSink;
    nsAutoArrayPtr<nsIContent*>            mHandles;
    PRInt32                                mHandlesUsed;
    nsTArray<nsAutoArrayPtr<nsIContent*> > mOldHandles;
    nsHtml5TreeOpStage*                    mSpeculativeLoadStage;
    nsIContent**                           mDeepTreeSurrogateParent;
    bool                                   mCurrentHtmlScriptIsAsyncOrDefer;
#ifdef DEBUG
    bool                                   mActive;
#endif

    // DocumentModeHandler
    /**
     * Tree builder uses this to report quirkiness of the document
     */
    void documentMode(nsHtml5DocumentMode m);

    /**
     * Using nsIContent** instead of nsIContent* is the parser deals with DOM
     * nodes in a way that works off the main thread. Non-main-thread code
     * can't refcount or otherwise touch nsIContent objects in any way.
     * Yet, the off-the-main-thread code needs to have a way to hold onto a
     * particular node and repeatedly operate on the same node.
     *
     * The way this works is that the off-the-main-thread code has an
     * nsIContent** for each DOM node and a given nsIContent** is only ever
     * actually dereferenced into an actual nsIContent* on the main thread.
     * When the off-the-main-thread code requests a new node, it gets an
     * nsIContent** immediately and a tree op is enqueued for later allocating
     * an actual nsIContent object and writing a pointer to it into the memory
     * location pointed to by the nsIContent**.
     *
     * Since tree ops are in a queue, the node creating tree op will always
     * run before tree ops that try to further operate on the node that the
     * nsIContent** is a handle to.
     *
     * On-the-main-thread parts of the parser use the same mechanism in order
     * to avoid having to have duplicate code paths for on-the-main-thread and
     * off-the-main-thread tree builder instances.)
     */
    nsIContent** AllocateContentHandle();
    
    void accumulateCharactersForced(const PRUnichar* aBuf, PRInt32 aStart, PRInt32 aLength)
    {
      accumulateCharacters(aBuf, aStart, aLength);
    }

  public:

    nsHtml5TreeBuilder(nsAHtml5TreeOpSink* aOpSink,
                       nsHtml5TreeOpStage* aStage);

    ~nsHtml5TreeBuilder();
    
    void StartPlainText();

    bool HasScript();
    
    void SetOpSink(nsAHtml5TreeOpSink* aOpSink) {
      mOpSink = aOpSink;
    }

    void ClearOps() {
      mOpQueue.Clear();
    }
    
    bool Flush(bool aDiscretionary = false);
    
    void FlushLoads();

    void SetDocumentCharset(nsACString& aCharset, PRInt32 aCharsetSource);

    void StreamEnded();

    void NeedsCharsetSwitchTo(const nsACString& aEncoding, PRInt32 aSource);

    void AddSnapshotToScript(nsAHtml5TreeBuilderState* aSnapshot, PRInt32 aLine);

    void DropHandles();

    void EnableViewSource(nsHtml5Highlighter* aHighlighter);

    void errStrayStartTag(nsIAtom* aName);

    void errStrayEndTag(nsIAtom* aName);

    void errUnclosedElements(PRInt32 aIndex, nsIAtom* aName);

    void errUnclosedElementsImplied(PRInt32 aIndex, nsIAtom* aName);

    void errUnclosedElementsCell(PRInt32 aIndex);

    void errStrayDoctype();

    void errAlmostStandardsDoctype();

    void errQuirkyDoctype();

    void errNonSpaceInTrailer();

    void errNonSpaceAfterFrameset();

    void errNonSpaceInFrameset();

    void errNonSpaceAfterBody();

    void errNonSpaceInColgroupInFragment();

    void errNonSpaceInNoscriptInHead();

    void errFooBetweenHeadAndBody(nsIAtom* aName);

    void errStartTagWithoutDoctype();

    void errNoSelectInTableScope();

    void errStartSelectWhereEndSelectExpected();

    void errStartTagWithSelectOpen(nsIAtom* aName);

    void errBadStartTagInHead(nsIAtom* aName);

    void errImage();

    void errIsindex();

    void errFooSeenWhenFooOpen(nsIAtom* aName);

    void errHeadingWhenHeadingOpen();

    void errFramesetStart();

    void errNoCellToClose();

    void errStartTagInTable(nsIAtom* aName);

    void errFormWhenFormOpen();

    void errTableSeenWhileTableOpen();

    void errStartTagInTableBody(nsIAtom* aName);

    void errEndTagSeenWithoutDoctype();

    void errEndTagAfterBody();

    void errEndTagSeenWithSelectOpen(nsIAtom* aName);

    void errGarbageInColgroup();

    void errEndTagBr();

    void errNoElementToCloseButEndTagSeen(nsIAtom* aName);

    void errHtmlStartTagInForeignContext(nsIAtom* aName);

    void errTableClosedWhileCaptionOpen();

    void errNoTableRowToClose();

    void errNonSpaceInTable();

    void errUnclosedChildrenInRuby();

    void errStartTagSeenWithoutRuby(nsIAtom* aName);

    void errSelfClosing();

    void errNoCheckUnclosedElementsOnStack();

    void errEndTagDidNotMatchCurrentOpenElement(nsIAtom* aName, nsIAtom* aOther);

    void errEndTagViolatesNestingRules(nsIAtom* aName);

    void errEndWithUnclosedElements(nsIAtom* aName);

    void MarkAsBroken();
