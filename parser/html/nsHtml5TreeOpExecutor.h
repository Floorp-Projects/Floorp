/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsHtml5TreeOpExecutor_h
#define nsHtml5TreeOpExecutor_h

#include "nsIAtom.h"
#include "nsTraceRefcnt.h"
#include "nsHtml5TreeOperation.h"
#include "nsHtml5SpeculativeLoad.h"
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
#include "nsHtml5DocumentBuilder.h"

class nsHtml5Parser;
class nsHtml5TreeBuilder;
class nsHtml5Tokenizer;
class nsHtml5StreamParser;
class nsIContent;
class nsIDocument;

class nsHtml5TreeOpExecutor MOZ_FINAL : public nsHtml5DocumentBuilder,
                                        public nsIContentSink,
                                        public nsAHtml5TreeOpSink,
                                        public mozilla::LinkedListElement<nsHtml5TreeOpExecutor>
{
  friend class nsHtml5FlushLoopGuard;

  public:
    NS_DECL_AND_IMPL_ZEROING_OPERATOR_NEW
    NS_DECL_ISUPPORTS_INHERITED

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
    nsHtml5StreamParser*                 mStreamParser;
    
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

    bool                          mRunFlushLoopOnStack;

    bool                          mCallContinueInterruptedParsingIfEnabled;

    /**
     * Whether this executor has already complained about matters related
     * to character encoding declarations.
     */
    bool                          mAlreadyComplainedAboutCharset;

  public:

    nsHtml5TreeOpExecutor();

  protected:

    virtual ~nsHtml5TreeOpExecutor();

  public:

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
  
    virtual void ContinueInterruptedParsingAsync();
 
    // XXX Does anyone need this?
    nsIDocShell* GetDocShell()
    {
      return mDocShell;
    }

    bool IsScriptExecuting()
    {
      return IsScriptExecutingImpl();
    }

    // Not from interface

    void SetStreamParser(nsHtml5StreamParser* aStreamParser)
    {
      mStreamParser = aStreamParser;
    }
    
    void InitializeDocWriteParserState(nsAHtml5TreeBuilderState* aState, int32_t aLine);

    bool IsScriptEnabled();

    virtual nsresult MarkAsBroken(nsresult aReason);

    void StartLayout();
    
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

    bool IsComplete()
    {
      return !mParser;
    }
    
    bool HasStarted()
    {
      return mStarted;
    }
    
    bool IsFlushing()
    {
      return mFlushState >= eInFlush;
    }

#ifdef DEBUG
    bool IsInFlushLoop()
    {
      return mRunFlushLoopOnStack;
    }
#endif
    
    void RunScript(nsIContent* aScriptElement);
    
    /**
     * Flush the operations from the tree operations from the argument
     * queue unconditionally. (This is for the main thread case.)
     */
    virtual void MoveOpsFrom(nsTArray<nsHtml5TreeOperation>& aOpQueue);
    
    nsHtml5TreeOpStage* GetStage()
    {
      return &mStage;
    }
    
    void StartReadingFromStage()
    {
      mReadingFromStage = true;
    }

    void StreamEnded();
    
#ifdef DEBUG
    void AssertStageEmpty()
    {
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

#endif // nsHtml5TreeOpExecutor_h
