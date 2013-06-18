/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsHtml5TreeOpExecutor_h__
#define nsHtml5TreeOpExecutor_h__

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
#include "nsAHtml5TreeOpSink.h"
#include "nsHtml5TreeOpStage.h"
#include "nsIURI.h"
#include "nsTHashtable.h"
#include "nsHashKeys.h"
#include "mozilla/LinkedList.h"

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
                              public nsAHtml5TreeOpSink,
                              public mozilla::LinkedListElement<nsHtml5TreeOpExecutor>
{
  friend class nsHtml5FlushLoopGuard;

  public:
    NS_DECL_AND_IMPL_ZEROING_OPERATOR_NEW
    NS_DECL_ISUPPORTS_INHERITED
    NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(nsHtml5TreeOpExecutor, nsContentSink)

  private:
    static bool        sExternalViewSource;
#ifdef DEBUG_NS_HTML5_TREE_OP_EXECUTOR_FLUSH
    static uint32_t    sAppendBatchMaxSize;
    static uint32_t    sAppendBatchSlotsExamined;
    static uint32_t    sAppendBatchExaminations;
    static uint32_t    sLongestTimeOffTheEventLoop;
    static uint32_t    sTimesFlushLoopInterrupted;
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
    nsTArray<nsCOMPtr<nsIContent> >      mOwnedElements;
    
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
     * Non-NS_OK if this parser should refuse to process any more input.
     * For example, the parser needs to be marked as broken if it drops some
     * input due to a memory allocation failure. In such a case, the whole
     * parser needs to be marked as broken, because some input has been lost
     * and parsing more input could lead to a DOM where pieces of HTML source
     * that weren't supposed to become scripts become scripts.
     *
     * Since NS_OK is actually 0, zeroing operator new takes care of
     * initializing this.
     */
    nsresult                      mBroken;

    /**
     * Whether this executor has already complained about matters related
     * to character encoding declarations.
     */
    bool                          mAlreadyComplainedAboutCharset;

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
    NS_IMETHOD WillBuildModel(nsDTDMode aDTDMode);

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

    void SetDocumentCharsetAndSource(nsACString& aCharset, int32_t aCharsetSource);

    void SetStreamParser(nsHtml5StreamParser* aStreamParser) {
      mStreamParser = aStreamParser;
    }
    
    void InitializeDocWriteParserState(nsAHtml5TreeBuilderState* aState, int32_t aLine);

    bool IsScriptEnabled();

    bool BelongsToStringParser() {
      return mRunsToCompletion;
    }

    /**
     * Marks this parser as broken and tells the stream parser (if any) to
     * terminate.
     *
     * @return aReason for convenience
     */
    nsresult MarkAsBroken(nsresult aReason);

    /**
     * Checks if this parser is broken. Returns a non-NS_OK (i.e. non-0)
     * value if broken.
     */
    inline nsresult IsBroken() {
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

    void NeedsCharsetSwitchTo(const char* aEncoding,
                              int32_t aSource,
                              uint32_t aLineNumber);

    void MaybeComplainAboutCharset(const char* aMsgId,
                                   bool aError,
                                   uint32_t aLineNumber);

    void ComplainAboutBogusProtocolCharset(nsIDocument* aDoc);

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
      mOwnedElements.AppendElement(aContent);
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
                       const nsAString& aCrossOrigin,
                       bool aScriptFromHead);

    void PreloadStyle(const nsAString& aURL, const nsAString& aCharset,
		      const nsAString& aCrossOrigin);

    void PreloadImage(const nsAString& aURL, const nsAString& aCrossOrigin);

    void SetSpeculationBase(const nsAString& aURL);

    static void InitializeStatics();

  private:
    nsHtml5Parser* GetParser();

    bool IsExternalViewSource();

    /**
     * Get a nsIURI for an nsString if the URL hasn't been preloaded yet.
     */
    already_AddRefed<nsIURI> ConvertIfNotPreloadedYet(const nsAString& aURL);

};

#endif // nsHtml5TreeOpExecutor_h__
