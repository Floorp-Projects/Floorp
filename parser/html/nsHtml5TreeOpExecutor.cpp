/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/DebugOnly.h"
#include "mozilla/Likely.h"
#include "mozilla/dom/BrowsingContext.h"
#include "mozilla/dom/ScriptLoader.h"
#include "mozilla/dom/nsCSPContext.h"
#include "mozilla/dom/nsCSPService.h"

#include "GeckoProfiler.h"
#include "mozAutoDocUpdate.h"
#include "mozilla/IdleTaskRunner.h"
#include "mozilla/Preferences.h"
#include "mozilla/StaticPrefs_content.h"
#include "mozilla/StaticPrefs_security.h"
#include "mozilla/StaticPrefs_view_source.h"
#include "mozilla/css/Loader.h"
#include "nsContentUtils.h"
#include "nsDocShell.h"
#include "nsError.h"
#include "nsHtml5AutoPauseUpdate.h"
#include "nsHtml5Parser.h"
#include "nsHtml5StreamParser.h"
#include "nsHtml5Tokenizer.h"
#include "nsHtml5TreeBuilder.h"
#include "nsHtml5TreeOpExecutor.h"
#include "nsIContentSecurityPolicy.h"
#include "nsIDocShell.h"
#include "nsIDocShellTreeItem.h"
#include "nsINestedURI.h"
#include "nsIScriptContext.h"
#include "nsIScriptError.h"
#include "nsIScriptGlobalObject.h"
#include "nsIViewSourceChannel.h"
#include "nsNetUtil.h"
#include "xpcpublic.h"

using namespace mozilla;

NS_IMPL_ISUPPORTS_CYCLE_COLLECTION_INHERITED(nsHtml5TreeOpExecutor,
                                             nsHtml5DocumentBuilder,
                                             nsIContentSink)

class nsHtml5ExecutorReflusher : public Runnable {
 private:
  RefPtr<nsHtml5TreeOpExecutor> mExecutor;

 public:
  explicit nsHtml5ExecutorReflusher(nsHtml5TreeOpExecutor* aExecutor)
      : mozilla::Runnable("nsHtml5ExecutorReflusher"), mExecutor(aExecutor) {}
  NS_IMETHOD Run() override {
    Document* doc = mExecutor->GetDocument();
    if (XRE_IsContentProcess() &&
        nsContentUtils::
            HighPriorityEventPendingForTopLevelDocumentBeforeContentfulPaint(
                doc)) {
      // Possible early paint pending, reuse the runnable and try to
      // call RunFlushLoop later.
      nsCOMPtr<nsIRunnable> flusher = this;
      if (NS_SUCCEEDED(
              doc->Dispatch(TaskCategory::Network, flusher.forget()))) {
        PROFILER_ADD_MARKER("HighPrio blocking parser flushing(2)", DOM);
        return NS_OK;
      }
    }
    mExecutor->RunFlushLoop();
    return NS_OK;
  }
};

class MOZ_RAII nsHtml5AutoFlush final {
 private:
  RefPtr<nsHtml5TreeOpExecutor> mExecutor;
  size_t mOpsToRemove;

 public:
  explicit nsHtml5AutoFlush(nsHtml5TreeOpExecutor* aExecutor)
      : mExecutor(aExecutor), mOpsToRemove(aExecutor->OpQueueLength()) {
    mExecutor->BeginFlush();
    mExecutor->BeginDocUpdate();
  }
  ~nsHtml5AutoFlush() {
    if (mExecutor->IsInDocUpdate()) {
      mExecutor->EndDocUpdate();
    } else {
      // We aren't in an update if nsHtml5AutoPauseUpdate
      // caused something to terminate the parser.
      MOZ_RELEASE_ASSERT(
          mExecutor->IsComplete(),
          "How do we have mParser but the doc update isn't open?");
    }
    mExecutor->EndFlush();
    mExecutor->RemoveFromStartOfOpQueue(mOpsToRemove);
  }
  void SetNumberOfOpsToRemove(size_t aOpsToRemove) {
    MOZ_ASSERT(aOpsToRemove < mOpsToRemove,
               "Requested partial clearing of op queue but the number to clear "
               "wasn't less than the length of the queue.");
    mOpsToRemove = aOpsToRemove;
  }
};

static mozilla::LinkedList<nsHtml5TreeOpExecutor>* gBackgroundFlushList =
    nullptr;
StaticRefPtr<IdleTaskRunner> gBackgroundFlushRunner;

nsHtml5TreeOpExecutor::nsHtml5TreeOpExecutor()
    : nsHtml5DocumentBuilder(false),
      mSuppressEOF(false),
      mReadingFromStage(false),
      mStreamParser(nullptr),
      mPreloadedURLs(23),  // Mean # of preloadable resources per page on dmoz
      mStarted(false),
      mRunFlushLoopOnStack(false),
      mCallContinueInterruptedParsingIfEnabled(false),
      mAlreadyComplainedAboutCharset(false),
      mAlreadyComplainedAboutDeepTree(false) {}

nsHtml5TreeOpExecutor::~nsHtml5TreeOpExecutor() {
  if (gBackgroundFlushList && isInList()) {
    ClearOpQueue();
    removeFrom(*gBackgroundFlushList);
    if (gBackgroundFlushList->isEmpty()) {
      delete gBackgroundFlushList;
      gBackgroundFlushList = nullptr;
      if (gBackgroundFlushRunner) {
        gBackgroundFlushRunner->Cancel();
        gBackgroundFlushRunner = nullptr;
      }
    }
  }
  NS_ASSERTION(mOpQueue.IsEmpty(), "Somehow there's stuff in the op queue.");
}

// nsIContentSink
NS_IMETHODIMP
nsHtml5TreeOpExecutor::WillParse() {
  MOZ_ASSERT_UNREACHABLE("No one should call this");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsHtml5TreeOpExecutor::WillBuildModel(nsDTDMode aDTDMode) {
  mDocument->AddObserver(this);
  WillBuildModelImpl();
  GetDocument()->BeginLoad();
  if (mDocShell && !GetDocument()->GetWindow() && !IsExternalViewSource()) {
    // Not loading as data but script global object not ready
    return MarkAsBroken(NS_ERROR_DOM_INVALID_STATE_ERR);
  }
  return NS_OK;
}

// This is called when the tree construction has ended
NS_IMETHODIMP
nsHtml5TreeOpExecutor::DidBuildModel(bool aTerminated) {
  if (mRunsToCompletion) {
    return NS_OK;
  }

  MOZ_RELEASE_ASSERT(!IsInDocUpdate(),
                     "DidBuildModel from inside a doc update.");

  // This comes from nsXMLContentSink and nsHTMLContentSink
  // If this parser has been marked as broken, treat the end of parse as
  // forced termination.
  DidBuildModelImpl(aTerminated || NS_FAILED(IsBroken()));

  bool destroying = true;
  if (mDocShell) {
    mDocShell->IsBeingDestroyed(&destroying);
  }

  if (!destroying) {
    mDocument->TriggerInitialDocumentTranslation();

    if (!mLayoutStarted) {
      // We never saw the body, and layout never got started. Force
      // layout *now*, to get an initial reflow.

      // NOTE: only force the layout if we are NOT destroying the
      // docshell. If we are destroying it, then starting layout will
      // likely cause us to crash, or at best waste a lot of time as we
      // are just going to tear it down anyway.
      nsContentSink::StartLayout(false);
    }
  }

  ScrollToRef();
  mDocument->RemoveObserver(this);
  if (!mParser) {
    // DidBuildModelImpl may cause mParser to be nulled out
    // Return early to avoid unblocking the onload event too many times.
    return NS_OK;
  }

  // We may not have called BeginLoad() if loading is terminated before
  // OnStartRequest call.
  if (mStarted) {
    mDocument->EndLoad();
  }

  // Dropping the stream parser changes the parser's apparent
  // script-createdness, which is why the stream parser must not be dropped
  // before this executor's nsHtml5Parser has been made unreachable from its
  // nsHTMLDocument. (mDocument->EndLoad() above drops the parser from the
  // document.)
  GetParser()->DropStreamParser();
  DropParserAndPerfHint();
#ifdef GATHER_DOCWRITE_STATISTICS
  printf("UNSAFE SCRIPTS: %d\n", sUnsafeDocWrites);
  printf("TOKENIZER-SAFE SCRIPTS: %d\n", sTokenSafeDocWrites);
  printf("TREEBUILDER-SAFE SCRIPTS: %d\n", sTreeSafeDocWrites);
#endif
#ifdef DEBUG_NS_HTML5_TREE_OP_EXECUTOR_FLUSH
  printf("MAX NOTIFICATION BATCH LEN: %d\n", sAppendBatchMaxSize);
  if (sAppendBatchExaminations != 0) {
    printf("AVERAGE SLOTS EXAMINED: %d\n",
           sAppendBatchSlotsExamined / sAppendBatchExaminations);
  }
#endif
  return NS_OK;
}

NS_IMETHODIMP
nsHtml5TreeOpExecutor::WillInterrupt() {
  MOZ_ASSERT_UNREACHABLE("Don't call. For interface compat only.");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsHtml5TreeOpExecutor::WillResume() {
  MOZ_ASSERT_UNREACHABLE("Don't call. For interface compat only.");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsHtml5TreeOpExecutor::SetParser(nsParserBase* aParser) {
  mParser = aParser;
  return NS_OK;
}

void nsHtml5TreeOpExecutor::InitialDocumentTranslationCompleted() {
  nsContentSink::StartLayout(false);
}

void nsHtml5TreeOpExecutor::FlushPendingNotifications(FlushType aType) {
  if (aType >= FlushType::EnsurePresShellInitAndFrames) {
    // Bug 577508 / 253951
    nsContentSink::StartLayout(true);
  }
}

nsISupports* nsHtml5TreeOpExecutor::GetTarget() {
  return ToSupports(mDocument);
}

nsresult nsHtml5TreeOpExecutor::MarkAsBroken(nsresult aReason) {
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  mBroken = aReason;
  if (mStreamParser) {
    mStreamParser->Terminate();
  }
  // We are under memory pressure, but let's hope the following allocation
  // works out so that we get to terminate and clean up the parser from
  // a safer point.
  if (mParser && mDocument) {  // can mParser ever be null here?
    nsCOMPtr<nsIRunnable> terminator = NewRunnableMethod(
        "nsHtml5Parser::Terminate", GetParser(), &nsHtml5Parser::Terminate);
    if (NS_FAILED(
            mDocument->Dispatch(TaskCategory::Network, terminator.forget()))) {
      NS_WARNING("failed to dispatch executor flush event");
    }
  }
  return aReason;
}

static bool BackgroundFlushCallback(TimeStamp /*aDeadline*/) {
  RefPtr<nsHtml5TreeOpExecutor> ex = gBackgroundFlushList->popFirst();
  if (ex) {
    ex->RunFlushLoop();
  }
  if (gBackgroundFlushList && gBackgroundFlushList->isEmpty()) {
    delete gBackgroundFlushList;
    gBackgroundFlushList = nullptr;
    gBackgroundFlushRunner->Cancel();
    gBackgroundFlushRunner = nullptr;
    return true;
  }
  return true;
}

void nsHtml5TreeOpExecutor::ContinueInterruptedParsingAsync() {
  if (!mDocument || !mDocument->IsInBackgroundWindow()) {
    nsCOMPtr<nsIRunnable> flusher = new nsHtml5ExecutorReflusher(this);
    if (NS_FAILED(
            mDocument->Dispatch(TaskCategory::Network, flusher.forget()))) {
      NS_WARNING("failed to dispatch executor flush event");
    }
  } else {
    if (!gBackgroundFlushList) {
      gBackgroundFlushList = new mozilla::LinkedList<nsHtml5TreeOpExecutor>();
    }
    if (!isInList()) {
      gBackgroundFlushList->insertBack(this);
    }
    if (gBackgroundFlushRunner) {
      return;
    }
    // Now we set up a repetitive idle scheduler for flushing background list.
    gBackgroundFlushRunner = IdleTaskRunner::Create(
        &BackgroundFlushCallback,
        "nsHtml5TreeOpExecutor::BackgroundFlushCallback",
        250,  // The hard deadline: 250ms.
        StaticPrefs::content_sink_interactive_parse_time() /
            1000,               // Required budget.
        true,                   // repeating
        [] { return false; });  // MayStopProcessing
  }
}

void nsHtml5TreeOpExecutor::FlushSpeculativeLoads() {
  nsTArray<nsHtml5SpeculativeLoad> speculativeLoadQueue;
  mStage.MoveSpeculativeLoadsTo(speculativeLoadQueue);
  nsHtml5SpeculativeLoad* start = speculativeLoadQueue.Elements();
  nsHtml5SpeculativeLoad* end = start + speculativeLoadQueue.Length();
  for (nsHtml5SpeculativeLoad* iter = start; iter < end; ++iter) {
    if (MOZ_UNLIKELY(!mParser)) {
      // An extension terminated the parser from a HTTP observer.
      return;
    }
    iter->Perform(this);
  }
}

class nsHtml5FlushLoopGuard {
 private:
  RefPtr<nsHtml5TreeOpExecutor> mExecutor;
#ifdef DEBUG_NS_HTML5_TREE_OP_EXECUTOR_FLUSH
  uint32_t mStartTime;
#endif
 public:
  explicit nsHtml5FlushLoopGuard(nsHtml5TreeOpExecutor* aExecutor)
      : mExecutor(aExecutor)
#ifdef DEBUG_NS_HTML5_TREE_OP_EXECUTOR_FLUSH
        ,
        mStartTime(PR_IntervalToMilliseconds(PR_IntervalNow()))
#endif
  {
    mExecutor->mRunFlushLoopOnStack = true;
  }
  ~nsHtml5FlushLoopGuard() {
#ifdef DEBUG_NS_HTML5_TREE_OP_EXECUTOR_FLUSH
    uint32_t timeOffTheEventLoop =
        PR_IntervalToMilliseconds(PR_IntervalNow()) - mStartTime;
    if (timeOffTheEventLoop >
        nsHtml5TreeOpExecutor::sLongestTimeOffTheEventLoop) {
      nsHtml5TreeOpExecutor::sLongestTimeOffTheEventLoop = timeOffTheEventLoop;
    }
    printf("Longest time off the event loop: %d\n",
           nsHtml5TreeOpExecutor::sLongestTimeOffTheEventLoop);
#endif

    mExecutor->mRunFlushLoopOnStack = false;
  }
};

/**
 * The purpose of the loop here is to avoid returning to the main event loop
 */
void nsHtml5TreeOpExecutor::RunFlushLoop() {
  AUTO_PROFILER_LABEL("nsHtml5TreeOpExecutor::RunFlushLoop", OTHER);

  if (mRunFlushLoopOnStack) {
    // There's already a RunFlushLoop() on the call stack.
    return;
  }

  nsHtml5FlushLoopGuard guard(this);  // this is also the self-kungfu!

  RefPtr<nsParserBase> parserKungFuDeathGrip(mParser);
  RefPtr<nsHtml5StreamParser> streamParserGrip;
  if (mParser) {
    streamParserGrip = GetParser()->GetStreamParser();
  }
  mozilla::Unused
      << streamParserGrip;  // Intentionally not used within function

  // Remember the entry time
  (void)nsContentSink::WillParseImpl();

  for (;;) {
    if (!mParser) {
      // Parse has terminated.
      ClearOpQueue();  // clear in order to be able to assert in destructor
      return;
    }

    if (NS_FAILED(IsBroken())) {
      return;
    }

    if (!parserKungFuDeathGrip->IsParserEnabled()) {
      // The parser is blocked.
      return;
    }

    if (mFlushState != eNotFlushing) {
      // XXX Can this happen? In case it can, let's avoid crashing.
      return;
    }

    // If there are scripts executing, then the content sink is jumping the gun
    // (probably due to a synchronous XMLHttpRequest) and will re-enable us
    // later, see bug 460706.
    if (IsScriptExecuting()) {
      return;
    }

    if (mReadingFromStage) {
      nsTArray<nsHtml5SpeculativeLoad> speculativeLoadQueue;
      MOZ_RELEASE_ASSERT(mFlushState == eNotFlushing,
                         "mOpQueue modified during flush.");
      mStage.MoveOpsAndSpeculativeLoadsTo(mOpQueue, speculativeLoadQueue);
      // Make sure speculative loads never start after the corresponding
      // normal loads for the same URLs.
      nsHtml5SpeculativeLoad* start = speculativeLoadQueue.Elements();
      nsHtml5SpeculativeLoad* end = start + speculativeLoadQueue.Length();
      for (nsHtml5SpeculativeLoad* iter = start; iter < end; ++iter) {
        iter->Perform(this);
        if (MOZ_UNLIKELY(!mParser)) {
          // An extension terminated the parser from a HTTP observer.
          ClearOpQueue();  // clear in order to be able to assert in destructor
          return;
        }
      }
    } else {
      FlushSpeculativeLoads();  // Make sure speculative loads never start after
                                // the corresponding normal loads for the same
                                // URLs.
      if (MOZ_UNLIKELY(!mParser)) {
        // An extension terminated the parser from a HTTP observer.
        ClearOpQueue();  // clear in order to be able to assert in destructor
        return;
      }
      // Now parse content left in the document.write() buffer queue if any.
      // This may generate tree ops on its own or dequeue a speculation.
      nsresult rv = GetParser()->ParseUntilBlocked();

      // ParseUntilBlocked flushes operations from the stage to the OpQueue.
      // Those operations may have accompanying speculative operations.
      // If so, we have to flush those speculative loads so that we maintain
      // the invariant that no speculative load starts after the corresponding
      // normal load for the same URL. See
      // https://bugzilla.mozilla.org/show_bug.cgi?id=1513292#c80
      // for a more detailed explanation of why this is necessary.
      FlushSpeculativeLoads();

      if (NS_FAILED(rv)) {
        MarkAsBroken(rv);
        return;
      }
    }

    if (mOpQueue.IsEmpty()) {
      // Avoid bothering the rest of the engine with a doc update if there's
      // nothing to do.
      return;
    }

    nsIContent* scriptElement = nullptr;
    bool interrupted = false;
    bool streamEnded = false;

    {
      // autoFlush clears mOpQueue in its destructor unless
      // SetNumberOfOpsToRemove is called first, in which case only
      // some ops from the start of the queue are cleared.
      nsHtml5AutoFlush autoFlush(this);

      nsHtml5TreeOperation* first = mOpQueue.Elements();
      nsHtml5TreeOperation* last = first + mOpQueue.Length() - 1;
      for (nsHtml5TreeOperation* iter = first;; ++iter) {
        if (MOZ_UNLIKELY(!mParser)) {
          // The previous tree op caused a call to nsIParser::Terminate().
          return;
        }
        MOZ_ASSERT(IsInDocUpdate(),
                   "Tried to perform tree op outside update batch.");
        nsresult rv =
            iter->Perform(this, &scriptElement, &interrupted, &streamEnded);
        if (NS_FAILED(rv)) {
          MarkAsBroken(rv);
          break;
        }

        // Be sure not to check the deadline if the last op was just performed.
        if (MOZ_UNLIKELY(iter == last)) {
          break;
        } else if (MOZ_UNLIKELY(interrupted) ||
                   MOZ_UNLIKELY(nsContentSink::DidProcessATokenImpl() ==
                                NS_ERROR_HTMLPARSER_INTERRUPTED)) {
          autoFlush.SetNumberOfOpsToRemove((iter - first) + 1);

          nsHtml5TreeOpExecutor::ContinueInterruptedParsingAsync();
          return;
        }
      }

      if (MOZ_UNLIKELY(!mParser)) {
        // The parse ended during an update pause.
        return;
      }
      if (streamEnded) {
        GetParser()->PermanentlyUndefineInsertionPoint();
      }
    }  // end autoFlush

    if (MOZ_UNLIKELY(!mParser)) {
      // Ending the doc update caused a call to nsIParser::Terminate().
      return;
    }

    if (streamEnded) {
      DidBuildModel(false);
#ifdef DEBUG
      if (scriptElement) {
        nsCOMPtr<nsIScriptElement> sele = do_QueryInterface(scriptElement);
        if (!sele) {
          MOZ_ASSERT(nsNameSpaceManager::GetInstance()->mSVGDisabled,
                     "Node didn't QI to script, but SVG wasn't disabled.");
        }
        MOZ_ASSERT(sele->IsMalformed(), "Script wasn't marked as malformed.");
      }
#endif
    } else if (scriptElement) {
      // must be tail call when mFlushState is eNotFlushing
      RunScript(scriptElement);

      // Always check the clock in nsContentSink right after a script
      StopDeflecting();
      if (nsContentSink::DidProcessATokenImpl() ==
          NS_ERROR_HTMLPARSER_INTERRUPTED) {
#ifdef DEBUG_NS_HTML5_TREE_OP_EXECUTOR_FLUSH
        printf("REFLUSH SCHEDULED (after script): %d\n",
               ++sTimesFlushLoopInterrupted);
#endif
        nsHtml5TreeOpExecutor::ContinueInterruptedParsingAsync();
        return;
      }
    }
  }
}

nsresult nsHtml5TreeOpExecutor::FlushDocumentWrite() {
  nsresult rv = IsBroken();
  NS_ENSURE_SUCCESS(rv, rv);

  FlushSpeculativeLoads();  // Make sure speculative loads never start after the
                            // corresponding normal loads for the same URLs.

  if (MOZ_UNLIKELY(!mParser)) {
    // The parse has ended.
    ClearOpQueue();  // clear in order to be able to assert in destructor
    return rv;
  }

  if (mFlushState != eNotFlushing) {
    // XXX Can this happen? In case it can, let's avoid crashing.
    return rv;
  }

  // avoid crashing near EOF
  RefPtr<nsHtml5TreeOpExecutor> kungFuDeathGrip(this);
  RefPtr<nsParserBase> parserKungFuDeathGrip(mParser);
  mozilla::Unused
      << parserKungFuDeathGrip;  // Intentionally not used within function
  RefPtr<nsHtml5StreamParser> streamParserGrip;
  if (mParser) {
    streamParserGrip = GetParser()->GetStreamParser();
  }
  mozilla::Unused
      << streamParserGrip;  // Intentionally not used within function

  MOZ_RELEASE_ASSERT(!mReadingFromStage,
                     "Got doc write flush when reading from stage");

#ifdef DEBUG
  mStage.AssertEmpty();
#endif

  nsIContent* scriptElement = nullptr;
  bool interrupted = false;
  bool streamEnded = false;

  {
    // autoFlush clears mOpQueue in its destructor.
    nsHtml5AutoFlush autoFlush(this);

    nsHtml5TreeOperation* start = mOpQueue.Elements();
    nsHtml5TreeOperation* end = start + mOpQueue.Length();
    for (nsHtml5TreeOperation* iter = start; iter < end; ++iter) {
      if (MOZ_UNLIKELY(!mParser)) {
        // The previous tree op caused a call to nsIParser::Terminate().
        return rv;
      }
      NS_ASSERTION(IsInDocUpdate(),
                   "Tried to perform tree op outside update batch.");
      rv = iter->Perform(this, &scriptElement, &interrupted, &streamEnded);
      if (NS_FAILED(rv)) {
        MarkAsBroken(rv);
        break;
      }
    }

    if (MOZ_UNLIKELY(!mParser)) {
      // The parse ended during an update pause.
      return rv;
    }
    if (streamEnded) {
      // This should be redundant but let's do it just in case.
      GetParser()->PermanentlyUndefineInsertionPoint();
    }
  }  // autoFlush

  if (MOZ_UNLIKELY(!mParser)) {
    // Ending the doc update caused a call to nsIParser::Terminate().
    return rv;
  }

  if (streamEnded) {
    DidBuildModel(false);
#ifdef DEBUG
    if (scriptElement) {
      nsCOMPtr<nsIScriptElement> sele = do_QueryInterface(scriptElement);
      if (!sele) {
        MOZ_ASSERT(nsNameSpaceManager::GetInstance()->mSVGDisabled,
                   "Node didn't QI to script, but SVG wasn't disabled.");
      }
      MOZ_ASSERT(sele->IsMalformed(), "Script wasn't marked as malformed.");
    }
#endif
  } else if (scriptElement) {
    // must be tail call when mFlushState is eNotFlushing
    RunScript(scriptElement);
  }
  return rv;
}

// copied from HTML content sink
bool nsHtml5TreeOpExecutor::IsScriptEnabled() {
  // Note that if we have no document or no docshell or no global or whatnot we
  // want to claim script _is_ enabled, so we don't parse the contents of
  // <noscript> tags!
  if (!mDocument || !mDocShell) {
    return true;
  }

  return mDocument->IsScriptEnabled();
}

void nsHtml5TreeOpExecutor::StartLayout(bool* aInterrupted) {
  if (mLayoutStarted || !mDocument) {
    return;
  }

  nsHtml5AutoPauseUpdate autoPause(this);

  if (MOZ_UNLIKELY(!mParser)) {
    // got terminate
    return;
  }

  nsContentSink::StartLayout(false);

  if (mParser) {
    *aInterrupted = !GetParser()->IsParserEnabled();
  }
}

void nsHtml5TreeOpExecutor::PauseDocUpdate(bool* aInterrupted) {
  // Pausing the document update allows JS to run, and potentially block
  // further parsing.
  nsHtml5AutoPauseUpdate autoPause(this);

  if (MOZ_LIKELY(mParser)) {
    *aInterrupted = !GetParser()->IsParserEnabled();
  }
}

/**
 * The reason why this code is here and not in the tree builder even in the
 * main-thread case is to allow the control to return from the tokenizer
 * before scripts run. This way, the tokenizer is not invoked re-entrantly
 * although the parser is.
 *
 * The reason why this is called as a tail call when mFlushState is set to
 * eNotFlushing is to allow re-entry to Flush() but only after the current
 * Flush() has cleared the op queue and is otherwise done cleaning up after
 * itself.
 */
void nsHtml5TreeOpExecutor::RunScript(nsIContent* aScriptElement) {
  if (mRunsToCompletion) {
    // We are in createContextualFragment() or in the upcoming document.parse().
    // Do nothing. Let's not even mark scripts malformed here, because that
    // could cause serialization weirdness later.
    return;
  }

  MOZ_ASSERT(mParser, "Trying to run script with a terminated parser.");
  MOZ_ASSERT(aScriptElement, "No script to run");
  nsCOMPtr<nsIScriptElement> sele = do_QueryInterface(aScriptElement);
  if (!sele) {
    MOZ_ASSERT(nsNameSpaceManager::GetInstance()->mSVGDisabled,
               "Node didn't QI to script, but SVG wasn't disabled.");
    return;
  }

  if (sele->GetScriptDeferred() || sele->GetScriptAsync()) {
    DebugOnly<bool> block = sele->AttemptToExecute();
    NS_ASSERTION(!block, "Defer or async script tried to block.");
    return;
  }

  MOZ_RELEASE_ASSERT(mFlushState == eNotFlushing,
                     "Tried to run script while flushing.");

  mReadingFromStage = false;

  sele->SetCreatorParser(GetParser());

  // Copied from nsXMLContentSink
  // Now tell the script that it's ready to go. This may execute the script
  // or return true, or neither if the script doesn't need executing.
  bool block = sele->AttemptToExecute();

  // If the act of insertion evaluated the script, we're fine.
  // Else, block the parser till the script has loaded.
  if (block) {
    if (mParser) {
      GetParser()->BlockParser();
    }
  } else {
    // mParser may have been nulled out by now, but the flusher deals

    // If this event isn't needed, it doesn't do anything. It is sometimes
    // necessary for the parse to continue after complex situations.
    nsHtml5TreeOpExecutor::ContinueInterruptedParsingAsync();
  }
}

void nsHtml5TreeOpExecutor::Start() {
  MOZ_ASSERT(!mStarted, "Tried to start when already started.");
  mStarted = true;
}

void nsHtml5TreeOpExecutor::NeedsCharsetSwitchTo(
    NotNull<const Encoding*> aEncoding, int32_t aSource, uint32_t aLineNumber) {
  nsHtml5AutoPauseUpdate autoPause(this);
  if (MOZ_UNLIKELY(!mParser)) {
    // got terminate
    return;
  }

  if (!mDocShell) {
    return;
  }

  nsDocShell* docShell = static_cast<nsDocShell*>(mDocShell.get());

  if (NS_SUCCEEDED(docShell->CharsetChangeStopDocumentLoad())) {
    nsAutoCString charset;
    aEncoding->Name(charset);
    docShell->CharsetChangeReloadDocument(charset.get(), aSource);
  }
  // if the charset switch was accepted, mDocShell has called Terminate() on the
  // parser by now

  if (!mParser) {
    // success
    if (aSource == kCharsetFromMetaTag) {
      MaybeComplainAboutCharset("EncLateMetaReload", false, aLineNumber);
    }
    return;
  }

  if (aSource == kCharsetFromMetaTag) {
    MaybeComplainAboutCharset("EncLateMetaTooLate", true, aLineNumber);
  }

  GetParser()->ContinueAfterFailedCharsetSwitch();
}

void nsHtml5TreeOpExecutor::MaybeComplainAboutCharset(const char* aMsgId,
                                                      bool aError,
                                                      uint32_t aLineNumber) {
  if (mAlreadyComplainedAboutCharset) {
    return;
  }
  // The EncNoDeclaration case for advertising iframes is so common that it
  // would result is way too many errors. The iframe case doesn't matter
  // when the ad is an image or a Flash animation anyway. When the ad is
  // textual, a misrendered ad probably isn't a huge loss for users.
  // Let's suppress the message in this case.
  // This means that errors about other different-origin iframes in mashups
  // are lost as well, but generally, the site author isn't in control of
  // the embedded different-origin pages anyway and can't fix problems even
  // if alerted about them.
  if (!strcmp(aMsgId, "EncNoDeclaration") && mDocShell) {
    BrowsingContext* const bc = mDocShell->GetBrowsingContext();
    if (bc && bc->GetParent()) {
      return;
    }
  }
  mAlreadyComplainedAboutCharset = true;
  nsContentUtils::ReportToConsole(
      aError ? nsIScriptError::errorFlag : nsIScriptError::warningFlag,
      NS_LITERAL_CSTRING("HTML parser"), mDocument,
      nsContentUtils::eHTMLPARSER_PROPERTIES, aMsgId, nsTArray<nsString>(),
      nullptr, EmptyString(), aLineNumber);
}

void nsHtml5TreeOpExecutor::ComplainAboutBogusProtocolCharset(Document* aDoc) {
  NS_ASSERTION(!mAlreadyComplainedAboutCharset,
               "How come we already managed to complain?");
  mAlreadyComplainedAboutCharset = true;
  nsContentUtils::ReportToConsole(
      nsIScriptError::errorFlag, NS_LITERAL_CSTRING("HTML parser"), aDoc,
      nsContentUtils::eHTMLPARSER_PROPERTIES, "EncProtocolUnsupported");
}

void nsHtml5TreeOpExecutor::MaybeComplainAboutDeepTree(uint32_t aLineNumber) {
  if (mAlreadyComplainedAboutDeepTree) {
    return;
  }
  mAlreadyComplainedAboutDeepTree = true;
  nsContentUtils::ReportToConsole(
      nsIScriptError::errorFlag, NS_LITERAL_CSTRING("HTML parser"), mDocument,
      nsContentUtils::eHTMLPARSER_PROPERTIES, "errDeepTree",
      nsTArray<nsString>(), nullptr, EmptyString(), aLineNumber);
}

nsHtml5Parser* nsHtml5TreeOpExecutor::GetParser() {
  MOZ_ASSERT(!mRunsToCompletion);
  return static_cast<nsHtml5Parser*>(mParser.get());
}

void nsHtml5TreeOpExecutor::MoveOpsFrom(
    nsTArray<nsHtml5TreeOperation>& aOpQueue) {
  MOZ_RELEASE_ASSERT(mFlushState == eNotFlushing,
                     "Ops added to mOpQueue during tree op execution.");
  mOpQueue.AppendElements(std::move(aOpQueue));
}

void nsHtml5TreeOpExecutor::ClearOpQueue() {
  MOZ_RELEASE_ASSERT(mFlushState == eNotFlushing,
                     "mOpQueue cleared during tree op execution.");
  mOpQueue.Clear();
}

void nsHtml5TreeOpExecutor::RemoveFromStartOfOpQueue(
    size_t aNumberOfOpsToRemove) {
  MOZ_RELEASE_ASSERT(mFlushState == eNotFlushing,
                     "Ops removed from mOpQueue during tree op execution.");
  mOpQueue.RemoveElementsAt(0, aNumberOfOpsToRemove);
}

void nsHtml5TreeOpExecutor::InitializeDocWriteParserState(
    nsAHtml5TreeBuilderState* aState, int32_t aLine) {
  GetParser()->InitializeDocWriteParserState(aState, aLine);
}

nsIURI* nsHtml5TreeOpExecutor::GetViewSourceBaseURI() {
  if (!mViewSourceBaseURI) {
    // We query the channel for the baseURI because in certain situations it
    // cannot otherwise be determined. If this process fails, fall back to the
    // standard method.
    nsCOMPtr<nsIViewSourceChannel> vsc =
        do_QueryInterface(mDocument->GetChannel());
    if (vsc) {
      nsresult rv = vsc->GetBaseURI(getter_AddRefs(mViewSourceBaseURI));
      if (NS_SUCCEEDED(rv) && mViewSourceBaseURI) {
        return mViewSourceBaseURI;
      }
    }

    nsCOMPtr<nsIURI> orig = mDocument->GetOriginalURI();
    if (orig->SchemeIs("view-source")) {
      nsCOMPtr<nsINestedURI> nested = do_QueryInterface(orig);
      NS_ASSERTION(nested, "URI with scheme view-source didn't QI to nested!");
      nested->GetInnerURI(getter_AddRefs(mViewSourceBaseURI));
    } else {
      // Fail gracefully if the base URL isn't a view-source: URL.
      // Not sure if this can ever happen.
      mViewSourceBaseURI = orig;
    }
  }
  return mViewSourceBaseURI;
}

bool nsHtml5TreeOpExecutor::IsExternalViewSource() {
  if (!StaticPrefs::view_source_editor_external()) {
    return false;
  }
  if (mDocumentURI) {
    return mDocumentURI->SchemeIs("view-source");
  }
  return false;
}

// Speculative loading

nsIURI* nsHtml5TreeOpExecutor::BaseURIForPreload() {
  // The URL of the document without <base>
  nsIURI* documentURI = mDocument->GetDocumentURI();
  // The URL of the document with non-speculative <base>
  nsIURI* documentBaseURI = mDocument->GetDocBaseURI();

  // If the two above are different, use documentBaseURI. If they are the same,
  // the document object isn't aware of a <base>, so attempt to use the
  // mSpeculationBaseURI or, failing, that, documentURI.
  return (documentURI == documentBaseURI)
             ? (mSpeculationBaseURI ? mSpeculationBaseURI.get() : documentURI)
             : documentBaseURI;
}

already_AddRefed<nsIURI> nsHtml5TreeOpExecutor::ConvertIfNotPreloadedYet(
    const nsAString& aURL) {
  if (aURL.IsEmpty()) {
    return nullptr;
  }

  nsIURI* base = BaseURIForPreload();
  auto encoding = mDocument->GetDocumentCharacterSet();
  nsCOMPtr<nsIURI> uri;
  nsresult rv = NS_NewURI(getter_AddRefs(uri), aURL, encoding, base);
  if (NS_FAILED(rv)) {
    NS_WARNING("Failed to create a URI");
    return nullptr;
  }

  if (ShouldPreloadURI(uri)) {
    return uri.forget();
  }

  return nullptr;
}

bool nsHtml5TreeOpExecutor::ShouldPreloadURI(nsIURI* aURI) {
  nsAutoCString spec;
  nsresult rv = aURI->GetSpec(spec);
  NS_ENSURE_SUCCESS(rv, false);
  return mPreloadedURLs.EnsureInserted(spec);
}

dom::ReferrerPolicy nsHtml5TreeOpExecutor::GetPreloadReferrerPolicy(
    const nsAString& aReferrerPolicy) {
  dom::ReferrerPolicy referrerPolicy =
      dom::ReferrerInfo::ReferrerPolicyAttributeFromString(aReferrerPolicy);
  return GetPreloadReferrerPolicy(referrerPolicy);
}

dom::ReferrerPolicy nsHtml5TreeOpExecutor::GetPreloadReferrerPolicy(
    ReferrerPolicy aReferrerPolicy) {
  if (aReferrerPolicy != dom::ReferrerPolicy::_empty) {
    return aReferrerPolicy;
  }

  return mDocument->GetPreloadReferrerInfo()->ReferrerPolicy();
}

void nsHtml5TreeOpExecutor::PreloadScript(
    const nsAString& aURL, const nsAString& aCharset, const nsAString& aType,
    const nsAString& aCrossOrigin, const nsAString& aIntegrity,
    dom::ReferrerPolicy aReferrerPolicy, bool aScriptFromHead, bool aAsync,
    bool aDefer, bool aNoModule, bool aLinkPreload) {
  nsCOMPtr<nsIURI> uri = ConvertIfNotPreloadedYet(aURL);
  if (!uri) {
    return;
  }
  mDocument->ScriptLoader()->PreloadURI(
      uri, aCharset, aType, aCrossOrigin, aIntegrity, aScriptFromHead, aAsync,
      aDefer, aNoModule, aLinkPreload,
      GetPreloadReferrerPolicy(aReferrerPolicy));
}

void nsHtml5TreeOpExecutor::PreloadStyle(const nsAString& aURL,
                                         const nsAString& aCharset,
                                         const nsAString& aCrossOrigin,
                                         const nsAString& aReferrerPolicy,
                                         const nsAString& aIntegrity,
                                         bool aLinkPreload) {
  nsCOMPtr<nsIURI> uri = ConvertIfNotPreloadedYet(aURL);
  if (!uri) {
    return;
  }

  mDocument->PreloadStyle(uri, Encoding::ForLabel(aCharset), aCrossOrigin,
                          GetPreloadReferrerPolicy(aReferrerPolicy), aIntegrity,
                          aLinkPreload);
}

void nsHtml5TreeOpExecutor::PreloadImage(
    const nsAString& aURL, const nsAString& aCrossOrigin,
    const nsAString& aSrcset, const nsAString& aSizes,
    const nsAString& aImageReferrerPolicy) {
  nsCOMPtr<nsIURI> baseURI = BaseURIForPreload();
  bool isImgSet = false;
  nsCOMPtr<nsIURI> uri =
      mDocument->ResolvePreloadImage(baseURI, aURL, aSrcset, aSizes, &isImgSet);
  if (uri && ShouldPreloadURI(uri)) {
    // use document wide referrer policy
    mDocument->MaybePreLoadImage(uri, aCrossOrigin,
                                 GetPreloadReferrerPolicy(aImageReferrerPolicy),
                                 isImgSet);
  }
}

// These calls inform the document of picture state and seen sources, such that
// it can use them to inform ResolvePreLoadImage as necessary
void nsHtml5TreeOpExecutor::PreloadPictureSource(const nsAString& aSrcset,
                                                 const nsAString& aSizes,
                                                 const nsAString& aType,
                                                 const nsAString& aMedia) {
  mDocument->PreloadPictureImageSource(aSrcset, aSizes, aType, aMedia);
}

void nsHtml5TreeOpExecutor::PreloadOpenPicture() {
  mDocument->PreloadPictureOpened();
}

void nsHtml5TreeOpExecutor::PreloadEndPicture() {
  mDocument->PreloadPictureClosed();
}

void nsHtml5TreeOpExecutor::AddBase(const nsAString& aURL) {
  auto encoding = mDocument->GetDocumentCharacterSet();
  nsresult rv = NS_NewURI(getter_AddRefs(mViewSourceBaseURI), aURL, encoding,
                          GetViewSourceBaseURI());
  if (NS_FAILED(rv)) {
    mViewSourceBaseURI = nullptr;
  }
}
void nsHtml5TreeOpExecutor::SetSpeculationBase(const nsAString& aURL) {
  if (mSpeculationBaseURI) {
    // the first one wins
    return;
  }
  auto encoding = mDocument->GetDocumentCharacterSet();
  DebugOnly<nsresult> rv = NS_NewURI(getter_AddRefs(mSpeculationBaseURI), aURL,
                                     encoding, mDocument->GetDocumentURI());
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "Failed to create a URI");
}

void nsHtml5TreeOpExecutor::UpdateReferrerInfoFromMeta(
    const nsAString& aMetaReferrer) {
  mDocument->UpdateReferrerInfoFromMeta(aMetaReferrer, true);
}

void nsHtml5TreeOpExecutor::AddSpeculationCSP(const nsAString& aCSP) {
  if (!StaticPrefs::security_csp_enable()) {
    return;
  }

  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  nsresult rv = NS_OK;
  nsCOMPtr<nsIContentSecurityPolicy> preloadCsp = mDocument->GetPreloadCsp();
  if (!preloadCsp) {
    preloadCsp = new nsCSPContext();
    rv = preloadCsp->SetRequestContextWithDocument(mDocument);
    NS_ENSURE_SUCCESS_VOID(rv);
  }

  // please note that meta CSPs and CSPs delivered through a header need
  // to be joined together.
  rv = preloadCsp->AppendPolicy(
      aCSP,
      false,  // csp via meta tag can not be report only
      true);  // delivered through the meta tag
  NS_ENSURE_SUCCESS_VOID(rv);

  nsPIDOMWindowInner* inner = mDocument->GetInnerWindow();
  if (inner) {
    inner->SetPreloadCsp(preloadCsp);
  }
  mDocument->ApplySettingsFromCSP(true);
}

#ifdef DEBUG_NS_HTML5_TREE_OP_EXECUTOR_FLUSH
uint32_t nsHtml5TreeOpExecutor::sAppendBatchMaxSize = 0;
uint32_t nsHtml5TreeOpExecutor::sAppendBatchSlotsExamined = 0;
uint32_t nsHtml5TreeOpExecutor::sAppendBatchExaminations = 0;
uint32_t nsHtml5TreeOpExecutor::sLongestTimeOffTheEventLoop = 0;
uint32_t nsHtml5TreeOpExecutor::sTimesFlushLoopInterrupted = 0;
#endif
