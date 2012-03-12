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
 * The Original Code is HTML Parser Gecko integration code.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2009
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

#ifndef nsHtml5TreeOpExecutor_h__
#define nsHtml5TreeOpExecutor_h__

#include "prtypes.h"
#include "nsIAtom.h"
#include "nsINameSpaceManager.h"
#include "nsIContent.h"
#include "nsIDocument.h"
#include "nsTraceRefcnt.h"
#include "nsHtml5TreeOperation.h"
#include "nsHtml5SpeculativeLoad.h"
#include "nsHtml5PendingNotification.h"
#include "nsTArray.h"
#include "nsContentSink.h"
#include "nsNodeInfoManager.h"
#include "nsHtml5DocumentMode.h"
#include "nsIScriptElement.h"
#include "nsIParser.h"
#include "nsCOMArray.h"
#include "nsAHtml5TreeOpSink.h"
#include "nsHtml5TreeOpStage.h"
#include "nsIURI.h"
#include "nsTHashtable.h"
#include "nsHashKeys.h"

class nsHtml5Parser;
class nsHtml5TreeBuilder;
class nsHtml5Tokenizer;
class nsHtml5StreamParser;

typedef nsIContent* nsIContentPtr;

enum eHtml5FlushState {
  eNotFlushing = 0,  // not flushing
  eInFlush = 1,      // the Flush() method is on the call stack
  eInDocUpdate = 2,  // inside an update batch on the document
  eNotifying = 3     // flushing pending append notifications
};

class nsHtml5TreeOpExecutor : public nsContentSink,
                              public nsIContentSink,
                              public nsAHtml5TreeOpSink
{
  friend class nsHtml5FlushLoopGuard;

  public:
    NS_DECL_AND_IMPL_ZEROING_OPERATOR_NEW
    NS_DECL_ISUPPORTS_INHERITED
    NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(nsHtml5TreeOpExecutor, nsContentSink)

  private:
#ifdef DEBUG_NS_HTML5_TREE_OP_EXECUTOR_FLUSH
    static PRUint32    sAppendBatchMaxSize;
    static PRUint32    sAppendBatchSlotsExamined;
    static PRUint32    sAppendBatchExaminations;
    static PRUint32    sLongestTimeOffTheEventLoop;
    static PRUint32    sTimesFlushLoopInterrupted;
#endif

    /**
     * Whether EOF needs to be suppressed
     */
    bool                                 mSuppressEOF;
    
    bool                                 mReadingFromStage;
    nsTArray<nsHtml5TreeOperation>       mOpQueue;
    nsTArray<nsIContentPtr>              mElementsSeenInThisAppendBatch;
    nsTArray<nsHtml5PendingNotification> mPendingNotifications;
    nsHtml5StreamParser*                 mStreamParser;
    nsCOMArray<nsIContent>               mOwnedElements;
    
    /**
     * URLs already preloaded/preloading.
     */
    nsTHashtable<nsCStringHashKey> mPreloadedURLs;

    nsCOMPtr<nsIURI> mSpeculationBaseURI;

    nsCOMPtr<nsIURI> mViewSourceBaseURI;

    /**
     * Whether the parser has started
     */
    bool                          mStarted;

    nsHtml5TreeOpStage            mStage;

    eHtml5FlushState              mFlushState;

    bool                          mRunFlushLoopOnStack;

    bool                          mCallContinueInterruptedParsingIfEnabled;

    /**
     * True if this parser should refuse to process any more input.
     * Currently, the only way a parser can break is if it drops some input
     * due to a memory allocation failure. In such a case, the whole parser
     * needs to be marked as broken, because some input has been lost and
     * parsing more input could lead to a DOM where pieces of HTML source
     * that weren't supposed to become scripts become scripts.
     */
    bool                          mBroken;

  public:
  
    nsHtml5TreeOpExecutor(bool aRunsToCompletion = false);
    virtual ~nsHtml5TreeOpExecutor();
  
    // nsIContentSink

    /**
     * Unimplemented. For interface compat only.
     */
    NS_IMETHOD WillParse();

    /**
     * 
     */
    NS_IMETHOD WillBuildModel(nsDTDMode aDTDMode) {
      NS_ASSERTION(!mDocShell || GetDocument()->GetScriptGlobalObject(),
                   "Script global object not ready");
      mDocument->AddObserver(this);
      WillBuildModelImpl();
      GetDocument()->BeginLoad();
      return NS_OK;
    }

    /**
     * Emits EOF.
     */
    NS_IMETHOD DidBuildModel(bool aTerminated);

    /**
     * Forwards to nsContentSink
     */
    NS_IMETHOD WillInterrupt();

    /**
     * Unimplemented. For interface compat only.
     */
    NS_IMETHOD WillResume();

    /**
     * Sets the parser.
     */
    NS_IMETHOD SetParser(nsParserBase* aParser);

    /**
     * No-op for backwards compat.
     */
    virtual void FlushPendingNotifications(mozFlushType aType);

    /**
     * Don't call. For interface compat only.
     */
    NS_IMETHOD SetDocumentCharset(nsACString& aCharset) {
    	NS_NOTREACHED("No one should call this.");
    	return NS_ERROR_NOT_IMPLEMENTED;
    }

    /**
     * Returns the document.
     */
    virtual nsISupports *GetTarget();
  
    // nsContentSink methods
    virtual void UpdateChildCounts();
    virtual nsresult FlushTags();
    virtual void ContinueInterruptedParsingAsync();
 
    /**
     * Sets up style sheet load / parse
     */
    void UpdateStyleSheet(nsIContent* aElement);

    // Getters and setters for fields from nsContentSink
    nsIDocument* GetDocument() {
      return mDocument;
    }
    nsNodeInfoManager* GetNodeInfoManager() {
      return mNodeInfoManager;
    }
    nsIDocShell* GetDocShell() {
      return mDocShell;
    }

    bool IsScriptExecuting() {
      return IsScriptExecutingImpl();
    }
    
    void SetNodeInfoManager(nsNodeInfoManager* aManager) {
      mNodeInfoManager = aManager;
    }
    
    // Not from interface

    void SetDocumentCharsetAndSource(nsACString& aCharset, PRInt32 aCharsetSource);

    void SetStreamParser(nsHtml5StreamParser* aStreamParser) {
      mStreamParser = aStreamParser;
    }
    
    void InitializeDocWriteParserState(nsAHtml5TreeBuilderState* aState, PRInt32 aLine);

    bool IsScriptEnabled();

    /**
     * Enables the fragment mode.
     *
     * @param aPreventScriptExecution if true, scripts are prevented from
     * executing; don't set to false when parsing a fragment directly into
     * a document--only when parsing to an actual DOM fragment
     */
    void EnableFragmentMode(bool aPreventScriptExecution) {
      mPreventScriptExecution = aPreventScriptExecution;
    }
    
    void PreventScriptExecution() {
      mPreventScriptExecution = true;
    }

    bool BelongsToStringParser() {
      return mRunsToCompletion;
    }

    /**
     * Marks this parser as broken and tells the stream parser (if any) to
     * terminate.
     */
    void MarkAsBroken();

    /**
     * Checks if this parser is broken.
     */
    inline bool IsBroken() {
      NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
      return mBroken;
    }

    inline void BeginDocUpdate() {
      NS_PRECONDITION(mFlushState == eInFlush, "Tried to double-open update.");
      NS_PRECONDITION(mParser, "Started update without parser.");
      mFlushState = eInDocUpdate;
      mDocument->BeginUpdate(UPDATE_CONTENT_MODEL);
    }

    inline void EndDocUpdate() {
      NS_PRECONDITION(mFlushState != eNotifying, "mFlushState out of sync");
      if (mFlushState == eInDocUpdate) {
        FlushPendingAppendNotifications();
        mFlushState = eInFlush;
        mDocument->EndUpdate(UPDATE_CONTENT_MODEL);
      }
    }

    void PostPendingAppendNotification(nsIContent* aParent, nsIContent* aChild) {
      bool newParent = true;
      const nsIContentPtr* first = mElementsSeenInThisAppendBatch.Elements();
      const nsIContentPtr* last = first + mElementsSeenInThisAppendBatch.Length() - 1;
      for (const nsIContentPtr* iter = last; iter >= first; --iter) {
#ifdef DEBUG_NS_HTML5_TREE_OP_EXECUTOR_FLUSH
        sAppendBatchSlotsExamined++;
#endif
        if (*iter == aParent) {
          newParent = false;
          break;
        }
      }
      if (aChild->IsElement()) {
        mElementsSeenInThisAppendBatch.AppendElement(aChild);
      }
      mElementsSeenInThisAppendBatch.AppendElement(aParent);
      if (newParent) {
        mPendingNotifications.AppendElement(aParent);
      }
#ifdef DEBUG_NS_HTML5_TREE_OP_EXECUTOR_FLUSH
      sAppendBatchExaminations++;
#endif
    }

    void FlushPendingAppendNotifications() {
      NS_PRECONDITION(mFlushState == eInDocUpdate, "Notifications flushed outside update");
      mFlushState = eNotifying;
      const nsHtml5PendingNotification* start = mPendingNotifications.Elements();
      const nsHtml5PendingNotification* end = start + mPendingNotifications.Length();
      for (nsHtml5PendingNotification* iter = (nsHtml5PendingNotification*)start; iter < end; ++iter) {
        iter->Fire();
      }
      mPendingNotifications.Clear();
#ifdef DEBUG_NS_HTML5_TREE_OP_EXECUTOR_FLUSH
      if (mElementsSeenInThisAppendBatch.Length() > sAppendBatchMaxSize) {
        sAppendBatchMaxSize = mElementsSeenInThisAppendBatch.Length();
      }
#endif
      mElementsSeenInThisAppendBatch.Clear();
      NS_ASSERTION(mFlushState == eNotifying, "mFlushState out of sync");
      mFlushState = eInDocUpdate;
    }
    
    inline bool HaveNotified(nsIContent* aNode) {
      NS_PRECONDITION(aNode, "HaveNotified called with null argument.");
      const nsHtml5PendingNotification* start = mPendingNotifications.Elements();
      const nsHtml5PendingNotification* end = start + mPendingNotifications.Length();
      for (;;) {
        nsIContent* parent = aNode->GetParent();
        if (!parent) {
          return true;
        }
        for (nsHtml5PendingNotification* iter = (nsHtml5PendingNotification*)start; iter < end; ++iter) {
          if (iter->Contains(parent)) {
            return iter->HaveNotifiedIndex(parent->IndexOf(aNode));
          }
        }
        aNode = parent;
      }
    }

    void StartLayout();
    
    void SetDocumentMode(nsHtml5DocumentMode m);

    nsresult Init(nsIDocument* aDoc, nsIURI* aURI,
                  nsISupports* aContainer, nsIChannel* aChannel);

    void FlushSpeculativeLoads();
                  
    void RunFlushLoop();

    void FlushDocumentWrite();

    void MaybeSuspend();

    void Start();

    void NeedsCharsetSwitchTo(const char* aEncoding, PRInt32 aSource);
    
    bool IsComplete() {
      return !mParser;
    }
    
    bool HasStarted() {
      return mStarted;
    }
    
    bool IsFlushing() {
      return mFlushState >= eInFlush;
    }

#ifdef DEBUG
    bool IsInFlushLoop() {
      return mRunFlushLoopOnStack;
    }
#endif
    
    void RunScript(nsIContent* aScriptElement);
    
    void Reset();
    
    inline void HoldElement(nsIContent* aContent) {
      mOwnedElements.AppendObject(aContent);
    }

    void DropHeldElements();

    /**
     * Flush the operations from the tree operations from the argument
     * queue unconditionally. (This is for the main thread case.)
     */
    virtual void MoveOpsFrom(nsTArray<nsHtml5TreeOperation>& aOpQueue);
    
    nsHtml5TreeOpStage* GetStage() {
      return &mStage;
    }
    
    void StartReadingFromStage() {
      mReadingFromStage = true;
    }

    void StreamEnded();
    
#ifdef DEBUG
    void AssertStageEmpty() {
      mStage.AssertEmpty();
    }
#endif

    nsIURI* GetViewSourceBaseURI();

    void PreloadScript(const nsAString& aURL,
                       const nsAString& aCharset,
                       const nsAString& aType,
                       const nsAString& aCrossOrigin);

    void PreloadStyle(const nsAString& aURL, const nsAString& aCharset);

    void PreloadImage(const nsAString& aURL, const nsAString& aCrossOrigin);

    void SetSpeculationBase(const nsAString& aURL);

  private:
    nsHtml5Parser* GetParser();

    /**
     * Get a nsIURI for an nsString if the URL hasn't been preloaded yet.
     */
    already_AddRefed<nsIURI> ConvertIfNotPreloadedYet(const nsAString& aURL);

};

#endif // nsHtml5TreeOpExecutor_h__
