/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsAtom.h"
#include "nsParser.h"
#include "nsString.h"
#include "nsCRT.h"
#include "nsScanner.h"
#include "plstr.h"
#include "nsIChannel.h"
#include "nsIInputStream.h"
#include "CNavDTD.h"
#include "prenv.h"
#include "prlock.h"
#include "prcvar.h"
#include "nsParserCIID.h"
#include "nsReadableUtils.h"
#include "nsCOMPtr.h"
#include "nsExpatDriver.h"
#include "nsIFragmentContentSink.h"
#include "nsStreamUtils.h"
#include "nsHTMLTokenizer.h"
#include "nsXPCOMCIDInternal.h"
#include "nsMimeTypes.h"
#include "nsCharsetSource.h"
#include "nsThreadUtils.h"
#include "nsIHTMLContentSink.h"

#include "mozilla/BinarySearch.h"
#include "mozilla/CondVar.h"
#include "mozilla/dom/ScriptLoader.h"
#include "mozilla/Encoding.h"
#include "mozilla/Mutex.h"

using namespace mozilla;

#define NS_PARSER_FLAG_OBSERVERS_ENABLED 0x00000004
#define NS_PARSER_FLAG_PENDING_CONTINUE_EVENT 0x00000008
#define NS_PARSER_FLAG_FLUSH_TOKENS 0x00000020
#define NS_PARSER_FLAG_CAN_TOKENIZE 0x00000040

//-------------- Begin ParseContinue Event Definition ------------------------
/*
The parser can be explicitly interrupted by passing a return value of
NS_ERROR_HTMLPARSER_INTERRUPTED from BuildModel on the DTD. This will cause
the parser to stop processing and allow the application to return to the event
loop. The data which was left at the time of interruption will be processed
the next time OnDataAvailable is called. If the parser has received its final
chunk of data then OnDataAvailable will no longer be called by the networking
module, so the parser will schedule a nsParserContinueEvent which will call
the parser to process the remaining data after returning to the event loop.
If the parser is interrupted while processing the remaining data it will
schedule another ParseContinueEvent. The processing of data followed by
scheduling of the continue events will proceed until either:

  1) All of the remaining data can be processed without interrupting
  2) The parser has been cancelled.


This capability is currently used in CNavDTD and nsHTMLContentSink. The
nsHTMLContentSink is notified by CNavDTD when a chunk of tokens is going to be
processed and when each token is processed. The nsHTML content sink records
the time when the chunk has started processing and will return
NS_ERROR_HTMLPARSER_INTERRUPTED if the token processing time has exceeded a
threshold called max tokenizing processing time. This allows the content sink
to limit how much data is processed in a single chunk which in turn gates how
much time is spent away from the event loop. Processing smaller chunks of data
also reduces the time spent in subsequent reflows.

This capability is most apparent when loading large documents. If the maximum
token processing time is set small enough the application will remain
responsive during document load.

A side-effect of this capability is that document load is not complete when
the last chunk of data is passed to OnDataAvailable since  the parser may have
been interrupted when the last chunk of data arrived. The document is complete
when all of the document has been tokenized and there aren't any pending
nsParserContinueEvents. This can cause problems if the application assumes
that it can monitor the load requests to determine when the document load has
been completed. This is what happens in Mozilla. The document is considered
completely loaded when all of the load requests have been satisfied. To delay
the document load until all of the parsing has been completed the
nsHTMLContentSink adds a dummy parser load request which is not removed until
the nsHTMLContentSink's DidBuildModel is called. The CNavDTD will not call
DidBuildModel until the final chunk of data has been passed to the parser
through the OnDataAvailable and there aren't any pending
nsParserContineEvents.

Currently the parser is ignores requests to be interrupted during the
processing of script.  This is because a document.write followed by JavaScript
calls to manipulate the DOM may fail if the parser was interrupted during the
document.write.

For more details @see bugzilla bug 76722
*/

class nsParserContinueEvent : public Runnable {
 public:
  RefPtr<nsParser> mParser;

  explicit nsParserContinueEvent(nsParser* aParser)
      : mozilla::Runnable("nsParserContinueEvent"), mParser(aParser) {}

  NS_IMETHOD Run() override {
    mParser->HandleParserContinueEvent(this);
    return NS_OK;
  }
};

//-------------- End ParseContinue Event Definition ------------------------

/**
 *  default constructor
 */
nsParser::nsParser()
    : mParserContext(nullptr), mCharset(WINDOWS_1252_ENCODING) {
  Initialize(true);
}

nsParser::~nsParser() { Cleanup(); }

void nsParser::Initialize(bool aConstructor) {
  if (aConstructor) {
    // Raw pointer
    mParserContext = 0;
  } else {
    mUnusedInput.Truncate();
  }

  mContinueEvent = nullptr;
  mCharsetSource = kCharsetUninitialized;
  mCharset = WINDOWS_1252_ENCODING;
  mInternalState = NS_OK;
  mStreamStatus = NS_OK;
  mCommand = eViewNormal;
  mBlocked = 0;
  mFlags = NS_PARSER_FLAG_OBSERVERS_ENABLED | NS_PARSER_FLAG_CAN_TOKENIZE;

  mProcessingNetworkData = false;
  mIsAboutBlank = false;
}

void nsParser::Cleanup() {
#ifdef DEBUG
  if (mParserContext && mParserContext->mPrevContext) {
    NS_WARNING("Extra parser contexts still on the parser stack");
  }
#endif

  while (mParserContext) {
    CParserContext* pc = mParserContext->mPrevContext;
    delete mParserContext;
    mParserContext = pc;
  }

  // It should not be possible for this flag to be set when we are getting
  // destroyed since this flag implies a pending nsParserContinueEvent, which
  // has an owning reference to |this|.
  NS_ASSERTION(!(mFlags & NS_PARSER_FLAG_PENDING_CONTINUE_EVENT), "bad");
}

NS_IMPL_CYCLE_COLLECTION_CLASS(nsParser)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(nsParser)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mDTD)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mSink)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_WEAK_REFERENCE
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(nsParser)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mDTD)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mSink)
  CParserContext* pc = tmp->mParserContext;
  while (pc) {
    cb.NoteXPCOMChild(pc->mTokenizer);
    pc = pc->mPrevContext;
  }
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsParser)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsParser)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsParser)
  NS_INTERFACE_MAP_ENTRY(nsIStreamListener)
  NS_INTERFACE_MAP_ENTRY(nsIParser)
  NS_INTERFACE_MAP_ENTRY(nsIRequestObserver)
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIParser)
NS_INTERFACE_MAP_END

// The parser continue event is posted only if
// all of the data to parse has been passed to ::OnDataAvailable
// and the parser has been interrupted by the content sink
// because the processing of tokens took too long.

nsresult nsParser::PostContinueEvent() {
  if (!(mFlags & NS_PARSER_FLAG_PENDING_CONTINUE_EVENT)) {
    // If this flag isn't set, then there shouldn't be a live continue event!
    NS_ASSERTION(!mContinueEvent, "bad");

    // This creates a reference cycle between this and the event that is
    // broken when the event fires.
    nsCOMPtr<nsIRunnable> event = new nsParserContinueEvent(this);
    if (NS_FAILED(NS_DispatchToCurrentThread(event))) {
      NS_WARNING("failed to dispatch parser continuation event");
    } else {
      mFlags |= NS_PARSER_FLAG_PENDING_CONTINUE_EVENT;
      mContinueEvent = event;
    }
  }
  return NS_OK;
}

NS_IMETHODIMP_(void)
nsParser::GetCommand(nsCString& aCommand) { aCommand = mCommandStr; }

/**
 *  Call this method once you've created a parser, and want to instruct it
 *  about the command which caused the parser to be constructed. For example,
 *  this allows us to select a DTD which can do, say, view-source.
 *
 *  @param   aCommand the command string to set
 */
NS_IMETHODIMP_(void)
nsParser::SetCommand(const char* aCommand) {
  mCommandStr.Assign(aCommand);
  if (mCommandStr.EqualsLiteral("view-source")) {
    mCommand = eViewSource;
  } else if (mCommandStr.EqualsLiteral("view-fragment")) {
    mCommand = eViewFragment;
  } else {
    mCommand = eViewNormal;
  }
}

/**
 *  Call this method once you've created a parser, and want to instruct it
 *  about the command which caused the parser to be constructed. For example,
 *  this allows us to select a DTD which can do, say, view-source.
 *
 *  @param   aParserCommand the command to set
 */
NS_IMETHODIMP_(void)
nsParser::SetCommand(eParserCommands aParserCommand) {
  mCommand = aParserCommand;
}

/**
 *  Call this method once you've created a parser, and want to instruct it
 *  about what charset to load
 *
 *  @param   aCharset- the charset of a document
 *  @param   aCharsetSource- the source of the charset
 */
void nsParser::SetDocumentCharset(NotNull<const Encoding*> aCharset,
                                  int32_t aCharsetSource,
                                  bool aForceAutoDetection) {
  mCharset = aCharset;
  mCharsetSource = aCharsetSource;
  if (mParserContext && mParserContext->mScanner) {
    mParserContext->mScanner->SetDocumentCharset(aCharset, aCharsetSource);
  }
}

void nsParser::SetSinkCharset(NotNull<const Encoding*> aCharset) {
  if (mSink) {
    mSink->SetDocumentCharset(aCharset);
  }
}

/**
 *  This method gets called in order to set the content
 *  sink for this parser to dump nodes to.
 *
 *  @param   nsIContentSink interface for node receiver
 */
NS_IMETHODIMP_(void)
nsParser::SetContentSink(nsIContentSink* aSink) {
  MOZ_ASSERT(aSink, "sink cannot be null!");
  mSink = aSink;

  if (mSink) {
    mSink->SetParser(this);
    nsCOMPtr<nsIHTMLContentSink> htmlSink = do_QueryInterface(mSink);
    if (htmlSink) {
      mIsAboutBlank = true;
    }
  }
}

/**
 * retrieve the sink set into the parser
 * @return  current sink
 */
NS_IMETHODIMP_(nsIContentSink*)
nsParser::GetContentSink() { return mSink; }

static nsIDTD* FindSuitableDTD(CParserContext& aParserContext) {
  // We always find a DTD.
  aParserContext.mAutoDetectStatus = ePrimaryDetect;

  // Quick check for view source.
  MOZ_ASSERT(aParserContext.mParserCommand != eViewSource,
             "The old parser is not supposed to be used for View Source "
             "anymore.");

  // Now see if we're parsing HTML (which, as far as we're concerned, simply
  // means "not XML").
  if (aParserContext.mDocType != eXML) {
    return new CNavDTD();
  }

  // If we're here, then we'd better be parsing XML.
  NS_ASSERTION(aParserContext.mDocType == eXML,
               "What are you trying to send me, here?");
  return new nsExpatDriver();
}

NS_IMETHODIMP
nsParser::CancelParsingEvents() {
  if (mFlags & NS_PARSER_FLAG_PENDING_CONTINUE_EVENT) {
    NS_ASSERTION(mContinueEvent, "mContinueEvent is null");
    // Revoke the pending continue parsing event
    mContinueEvent = nullptr;
    mFlags &= ~NS_PARSER_FLAG_PENDING_CONTINUE_EVENT;
  }
  return NS_OK;
}

////////////////////////////////////////////////////////////////////////

/**
 * Evalutes EXPR1 and EXPR2 exactly once each, in that order.  Stores the value
 * of EXPR2 in RV is EXPR2 fails, otherwise RV contains the result of EXPR1
 * (which could be success or failure).
 *
 * To understand the motivation for this construct, consider these example
 * methods:
 *
 *   nsresult nsSomething::DoThatThing(nsIWhatever* obj) {
 *     nsresult rv = NS_OK;
 *     ...
 *     return obj->DoThatThing();
 *     NS_ENSURE_SUCCESS(rv, rv);
 *     ...
 *     return rv;
 *   }
 *
 *   void nsCaller::MakeThingsHappen() {
 *     return mSomething->DoThatThing(mWhatever);
 *   }
 *
 * Suppose, for whatever reason*, we want to shift responsibility for calling
 * mWhatever->DoThatThing() from nsSomething::DoThatThing up to
 * nsCaller::MakeThingsHappen.  We might rewrite the two methods as follows:
 *
 *   nsresult nsSomething::DoThatThing() {
 *     nsresult rv = NS_OK;
 *     ...
 *     ...
 *     return rv;
 *   }
 *
 *   void nsCaller::MakeThingsHappen() {
 *     nsresult rv;
 *     PREFER_LATTER_ERROR_CODE(mSomething->DoThatThing(),
 *                              mWhatever->DoThatThing(),
 *                              rv);
 *     return rv;
 *   }
 *
 * *Possible reasons include: nsCaller doesn't want to give mSomething access
 * to mWhatever, nsCaller wants to guarantee that mWhatever->DoThatThing() will
 * be called regardless of how nsSomething::DoThatThing behaves, &c.
 */
#define PREFER_LATTER_ERROR_CODE(EXPR1, EXPR2, RV) \
  {                                                \
    nsresult RV##__temp = EXPR1;                   \
    RV = EXPR2;                                    \
    if (NS_FAILED(RV)) {                           \
      RV = RV##__temp;                             \
    }                                              \
  }

/**
 * This gets called just prior to the model actually
 * being constructed. It's important to make this the
 * last thing that happens right before parsing, so we
 * can delay until the last moment the resolution of
 * which DTD to use (unless of course we're assigned one).
 */
nsresult nsParser::WillBuildModel(nsString& aFilename) {
  if (!mParserContext) return NS_ERROR_HTMLPARSER_INVALIDPARSERCONTEXT;

  if (mInternalState == NS_ERROR_OUT_OF_MEMORY) {
    // Checking NS_ERROR_OUT_OF_MEMORY instead of NS_FAILED
    // to avoid introducing unintentional changes to behavior.
    return mInternalState;
  }

  if (eUnknownDetect != mParserContext->mAutoDetectStatus) return NS_OK;

  if (eDTDMode_unknown == mParserContext->mDTDMode ||
      eDTDMode_autodetect == mParserContext->mDTDMode) {
    if (mIsAboutBlank) {
      mParserContext->mDTDMode = eDTDMode_quirks;
      mParserContext->mDocType = eHTML_Quirks;
    } else {
      mParserContext->mDTDMode = eDTDMode_full_standards;
      mParserContext->mDocType = eXML;
    }
  }  // else XML fragment with nested parser context

  NS_ASSERTION(!mDTD || !mParserContext->mPrevContext,
               "Clobbering DTD for non-root parser context!");
  mDTD = FindSuitableDTD(*mParserContext);
  NS_ENSURE_TRUE(mDTD, NS_ERROR_OUT_OF_MEMORY);

  nsITokenizer* tokenizer;
  nsresult rv = mParserContext->GetTokenizer(mDTD, mSink, tokenizer);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mDTD->WillBuildModel(*mParserContext, tokenizer, mSink);
  nsresult sinkResult = mSink->WillBuildModel(mDTD->GetMode());
  // nsIDTD::WillBuildModel used to be responsible for calling
  // nsIContentSink::WillBuildModel, but that obligation isn't expressible
  // in the nsIDTD interface itself, so it's sounder and simpler to give that
  // responsibility back to the parser. The former behavior of the DTD was to
  // NS_ENSURE_SUCCESS the sink WillBuildModel call, so if the sink returns
  // failure we should use sinkResult instead of rv, to preserve the old error
  // handling behavior of the DTD:
  return NS_FAILED(sinkResult) ? sinkResult : rv;
}

/**
 * This gets called when the parser is done with its input.
 * Note that the parser may have been called recursively, so we
 * have to check for a prev. context before closing out the DTD/sink.
 */
nsresult nsParser::DidBuildModel(nsresult anErrorCode) {
  nsresult result = anErrorCode;

  if (IsComplete()) {
    if (mParserContext && !mParserContext->mPrevContext) {
      // Let sink know if we're about to end load because we've been terminated.
      // In that case we don't want it to run deferred scripts.
      bool terminated = mInternalState == NS_ERROR_HTMLPARSER_STOPPARSING;
      if (mDTD && mSink) {
        nsresult dtdResult = mDTD->DidBuildModel(anErrorCode),
                 sinkResult = mSink->DidBuildModel(terminated);
        // nsIDTD::DidBuildModel used to be responsible for calling
        // nsIContentSink::DidBuildModel, but that obligation isn't expressible
        // in the nsIDTD interface itself, so it's sounder and simpler to give
        // that responsibility back to the parser. The former behavior of the
        // DTD was to NS_ENSURE_SUCCESS the sink DidBuildModel call, so if the
        // sink returns failure we should use sinkResult instead of dtdResult,
        // to preserve the old error handling behavior of the DTD:
        result = NS_FAILED(sinkResult) ? sinkResult : dtdResult;
      }

      // Ref. to bug 61462.
      mParserContext->mRequest = nullptr;
    }
  }

  return result;
}

/**
 * This method adds a new parser context to the list,
 * pushing the current one to the next position.
 *
 * @param   ptr to new context
 */
void nsParser::PushContext(CParserContext& aContext) {
  NS_ASSERTION(aContext.mPrevContext == mParserContext,
               "Trying to push a context whose previous context differs from "
               "the current parser context.");
  mParserContext = &aContext;
}

/**
 * This method pops the topmost context off the stack,
 * returning it to the user. The next context  (if any)
 * becomes the current context.
 * @update	gess7/22/98
 * @return  prev. context
 */
CParserContext* nsParser::PopContext() {
  CParserContext* oldContext = mParserContext;
  if (oldContext) {
    mParserContext = oldContext->mPrevContext;
    if (mParserContext) {
      // If the old context was blocked, propagate the blocked state
      // back to the new one. Also, propagate the stream listener state
      // but don't override onStop state to guarantee the call to
      // DidBuildModel().
      if (mParserContext->mStreamListenerState != eOnStop) {
        mParserContext->mStreamListenerState = oldContext->mStreamListenerState;
      }
    }
  }
  return oldContext;
}

/**
 *  Call this when you want control whether or not the parser will parse
 *  and tokenize input (TRUE), or whether it just caches input to be
 *  parsed later (FALSE).
 *
 *  @param   aState determines whether we parse/tokenize or just cache.
 *  @return  current state
 */
void nsParser::SetUnusedInput(nsString& aBuffer) { mUnusedInput = aBuffer; }

/**
 *  Call this when you want to *force* the parser to terminate the
 *  parsing process altogether. This is binary -- so once you terminate
 *  you can't resume without restarting altogether.
 */
NS_IMETHODIMP
nsParser::Terminate(void) {
  // We should only call DidBuildModel once, so don't do anything if this is
  // the second time that Terminate has been called.
  if (mInternalState == NS_ERROR_HTMLPARSER_STOPPARSING) {
    return NS_OK;
  }

  nsresult result = NS_OK;
  // XXX - [ until we figure out a way to break parser-sink circularity ]
  // Hack - Hold a reference until we are completely done...
  nsCOMPtr<nsIParser> kungFuDeathGrip(this);
  mInternalState = result = NS_ERROR_HTMLPARSER_STOPPARSING;

  // CancelParsingEvents must be called to avoid leaking the nsParser object
  // @see bug 108049
  // If NS_PARSER_FLAG_PENDING_CONTINUE_EVENT is set then CancelParsingEvents
  // will reset it so DidBuildModel will call DidBuildModel on the DTD. Note:
  // The IsComplete() call inside of DidBuildModel looks at the
  // pendingContinueEvents flag.
  CancelParsingEvents();

  // If we got interrupted in the middle of a document.write, then we might
  // have more than one parser context on our parsercontext stack. This has
  // the effect of making DidBuildModel a no-op, meaning that we never call
  // our sink's DidBuildModel and break the reference cycle, causing a leak.
  // Since we're getting terminated, we manually clean up our context stack.
  while (mParserContext && mParserContext->mPrevContext) {
    CParserContext* prev = mParserContext->mPrevContext;
    delete mParserContext;
    mParserContext = prev;
  }

  if (mDTD) {
    mDTD->Terminate();
    DidBuildModel(result);
  } else if (mSink) {
    // We have no parser context or no DTD yet (so we got terminated before we
    // got any data).  Manually break the reference cycle with the sink.
    result = mSink->DidBuildModel(true);
    NS_ENSURE_SUCCESS(result, result);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsParser::ContinueInterruptedParsing() {
  if (mInternalState == NS_ERROR_OUT_OF_MEMORY) {
    // Checking NS_ERROR_OUT_OF_MEMORY instead of NS_FAILED
    // to avoid introducing unintentional changes to behavior.
    return mInternalState;
  }

  // If there are scripts executing, then the content sink is jumping the gun
  // (probably due to a synchronous XMLHttpRequest) and will re-enable us
  // later, see bug 460706.
  if (!IsOkToProcessNetworkData()) {
    return NS_OK;
  }

  // If the stream has already finished, there's a good chance
  // that we might start closing things down when the parser
  // is reenabled. To make sure that we're not deleted across
  // the reenabling process, hold a reference to ourselves.
  nsresult result = NS_OK;
  nsCOMPtr<nsIParser> kungFuDeathGrip(this);
  nsCOMPtr<nsIContentSink> sinkDeathGrip(mSink);

#ifdef DEBUG
  if (mBlocked) {
    NS_WARNING("Don't call ContinueInterruptedParsing on a blocked parser.");
  }
#endif

  bool isFinalChunk =
      mParserContext && mParserContext->mStreamListenerState == eOnStop;

  mProcessingNetworkData = true;
  if (sinkDeathGrip) {
    sinkDeathGrip->WillParse();
  }
  result = ResumeParse(true, isFinalChunk);  // Ref. bug 57999
  mProcessingNetworkData = false;

  if (result != NS_OK) {
    result = mInternalState;
  }

  return result;
}

/**
 *  Stops parsing temporarily. That is, it will prevent the
 *  parser from building up content model while scripts
 *  are being loaded (either an external script from a web
 *  page, or any number of extension content scripts).
 */
NS_IMETHODIMP_(void)
nsParser::BlockParser() { mBlocked++; }

/**
 *  Open up the parser for tokenization, building up content
 *  model..etc. However, this method does not resume parsing
 *  automatically. It's the callers' responsibility to restart
 *  the parsing engine.
 */
NS_IMETHODIMP_(void)
nsParser::UnblockParser() {
  MOZ_DIAGNOSTIC_ASSERT(mBlocked > 0);
  if (MOZ_LIKELY(mBlocked > 0)) {
    mBlocked--;
  }
}

NS_IMETHODIMP_(void)
nsParser::ContinueInterruptedParsingAsync() {
  MOZ_ASSERT(mSink);
  if (MOZ_LIKELY(mSink)) {
    mSink->ContinueInterruptedParsingAsync();
  }
}

/**
 * Call this to query whether the parser is enabled or not.
 */
NS_IMETHODIMP_(bool)
nsParser::IsParserEnabled() { return !mBlocked; }

/**
 * Call this to query whether the parser thinks it's done with parsing.
 */
NS_IMETHODIMP_(bool)
nsParser::IsComplete() {
  return !(mFlags & NS_PARSER_FLAG_PENDING_CONTINUE_EVENT);
}

void nsParser::HandleParserContinueEvent(nsParserContinueEvent* ev) {
  // Ignore any revoked continue events...
  if (mContinueEvent != ev) return;

  mFlags &= ~NS_PARSER_FLAG_PENDING_CONTINUE_EVENT;
  mContinueEvent = nullptr;

  NS_ASSERTION(IsOkToProcessNetworkData(),
               "Interrupted in the middle of a script?");
  ContinueInterruptedParsing();
}

bool nsParser::IsInsertionPointDefined() { return false; }

void nsParser::IncrementScriptNestingLevel() {}

void nsParser::DecrementScriptNestingLevel() {}

bool nsParser::HasNonzeroScriptNestingLevel() const { return false; }

void nsParser::MarkAsNotScriptCreated(const char* aCommand) {}

bool nsParser::IsScriptCreated() { return false; }

/**
 *  This is the main controlling routine in the parsing process.
 *  Note that it may get called multiple times for the same scanner,
 *  since this is a pushed based system, and all the tokens may
 *  not have been consumed by the scanner during a given invocation
 *  of this method.
 */
NS_IMETHODIMP
nsParser::Parse(nsIURI* aURL, void* aKey) {
  MOZ_ASSERT(aURL, "Error: Null URL given");

  if (mInternalState == NS_ERROR_OUT_OF_MEMORY) {
    // Checking NS_ERROR_OUT_OF_MEMORY instead of NS_FAILED
    // to avoid introducing unintentional changes to behavior.
    return mInternalState;
  }

  nsresult result = NS_ERROR_HTMLPARSER_BADURL;

  if (aURL) {
    nsAutoCString spec;
    nsresult rv = aURL->GetSpec(spec);
    if (rv != NS_OK) {
      return rv;
    }
    nsString theName;  // Not nsAutoString due to length and usage
    if (!CopyUTF8toUTF16(spec, theName, mozilla::fallible)) {
      mInternalState = NS_ERROR_OUT_OF_MEMORY;
      return mInternalState;
    }

    nsScanner* theScanner = new nsScanner(theName, false);
    CParserContext* pc =
        new CParserContext(mParserContext, theScanner, aKey, mCommand);
    if (pc && theScanner) {
      pc->mMultipart = true;
      pc->mContextType = CParserContext::eCTURL;
      pc->mDTDMode = eDTDMode_autodetect;
      PushContext(*pc);

      result = NS_OK;
    } else {
      result = mInternalState = NS_ERROR_HTMLPARSER_BADCONTEXT;
    }
  }
  return result;
}

/**
 * Used by XML fragment parsing below.
 *
 * @param   aSourceBuffer contains a string-full of real content
 */
nsresult nsParser::Parse(const nsAString& aSourceBuffer, void* aKey,
                         bool aLastCall) {
  nsresult result = NS_OK;

  if (mInternalState == NS_ERROR_OUT_OF_MEMORY) {
    // Checking NS_ERROR_OUT_OF_MEMORY instead of NS_FAILED
    // to avoid introducing unintentional changes to behavior.
    return mInternalState;
  }

  // Don't bother if we're never going to parse this.
  if (mInternalState == NS_ERROR_HTMLPARSER_STOPPARSING) {
    return result;
  }

  if (!aLastCall && aSourceBuffer.IsEmpty()) {
    // Nothing is being passed to the parser so return
    // immediately. mUnusedInput will get processed when
    // some data is actually passed in.
    // But if this is the last call, make sure to finish up
    // stuff correctly.
    return result;
  }

  // Maintain a reference to ourselves so we don't go away
  // till we're completely done.
  nsCOMPtr<nsIParser> kungFuDeathGrip(this);

  if (aLastCall || !aSourceBuffer.IsEmpty() || !mUnusedInput.IsEmpty()) {
    // Note: The following code will always find the parser context associated
    // with the given key, even if that context has been suspended (e.g., for
    // another document.write call). This doesn't appear to be exactly what IE
    // does in the case where this happens, but this makes more sense.
    CParserContext* pc = mParserContext;
    while (pc && pc->mKey != aKey) {
      pc = pc->mPrevContext;
    }

    if (!pc) {
      // Only make a new context if we don't have one, OR if we do, but has a
      // different context key.
      nsScanner* theScanner = new nsScanner(mUnusedInput);
      NS_ENSURE_TRUE(theScanner, NS_ERROR_OUT_OF_MEMORY);

      eAutoDetectResult theStatus = eUnknownDetect;

      if (mParserContext &&
          mParserContext->mMimeType.EqualsLiteral("application/xml")) {
        // Ref. Bug 90379
        NS_ASSERTION(mDTD, "How come the DTD is null?");

        if (mParserContext) {
          theStatus = mParserContext->mAutoDetectStatus;
          // Added this to fix bug 32022.
        }
      }

      pc = new CParserContext(mParserContext, theScanner, aKey, mCommand,
                              theStatus, aLastCall);
      NS_ENSURE_TRUE(pc, NS_ERROR_OUT_OF_MEMORY);

      PushContext(*pc);

      pc->mMultipart = !aLastCall;  // By default
      if (pc->mPrevContext) {
        pc->mMultipart |= pc->mPrevContext->mMultipart;
      }

      // Start fix bug 40143
      if (pc->mMultipart) {
        pc->mStreamListenerState = eOnDataAvail;
        if (pc->mScanner) {
          pc->mScanner->SetIncremental(true);
        }
      } else {
        pc->mStreamListenerState = eOnStop;
        if (pc->mScanner) {
          pc->mScanner->SetIncremental(false);
        }
      }
      // end fix for 40143

      pc->mContextType = CParserContext::eCTString;
      pc->SetMimeType("application/xml"_ns);
      pc->mDTDMode = eDTDMode_full_standards;

      mUnusedInput.Truncate();

      pc->mScanner->Append(aSourceBuffer);
      // Do not interrupt document.write() - bug 95487
      result = ResumeParse(false, false, false);
    } else {
      pc->mScanner->Append(aSourceBuffer);
      if (!pc->mPrevContext) {
        // Set stream listener state to eOnStop, on the final context - Fix
        // 68160, to guarantee DidBuildModel() call - Fix 36148
        if (aLastCall) {
          pc->mStreamListenerState = eOnStop;
          pc->mScanner->SetIncremental(false);
        }

        if (pc == mParserContext) {
          // If pc is not mParserContext, then this call to ResumeParse would
          // do the wrong thing and try to continue parsing using
          // mParserContext. We need to wait to actually resume parsing on pc.
          ResumeParse(false, false, false);
        }
      }
    }
  }

  return result;
}

NS_IMETHODIMP
nsParser::ParseFragment(const nsAString& aSourceBuffer,
                        nsTArray<nsString>& aTagStack) {
  if (mInternalState == NS_ERROR_OUT_OF_MEMORY) {
    // Checking NS_ERROR_OUT_OF_MEMORY instead of NS_FAILED
    // to avoid introducing unintentional changes to behavior.
    return mInternalState;
  }

  nsresult result = NS_OK;
  nsAutoString theContext;
  uint32_t theCount = aTagStack.Length();
  uint32_t theIndex = 0;

  // Disable observers for fragments
  mFlags &= ~NS_PARSER_FLAG_OBSERVERS_ENABLED;

  for (theIndex = 0; theIndex < theCount; theIndex++) {
    theContext.Append('<');
    theContext.Append(aTagStack[theCount - theIndex - 1]);
    theContext.Append('>');
  }

  if (theCount == 0) {
    // Ensure that the buffer is not empty. Because none of the DTDs care
    // about leading whitespace, this doesn't change the result.
    theContext.Assign(' ');
  }

  // First, parse the context to build up the DTD's tag stack. Note that we
  // pass false for the aLastCall parameter.
  result = Parse(theContext, (void*)&theContext, false);
  if (NS_FAILED(result)) {
    mFlags |= NS_PARSER_FLAG_OBSERVERS_ENABLED;
    return result;
  }

  if (!mSink) {
    // Parse must have failed in the XML case and so the sink was killed.
    return NS_ERROR_HTMLPARSER_STOPPARSING;
  }

  nsCOMPtr<nsIFragmentContentSink> fragSink = do_QueryInterface(mSink);
  NS_ASSERTION(fragSink, "ParseFragment requires a fragment content sink");

  fragSink->WillBuildContent();
  // Now, parse the actual content. Note that this is the last call
  // for HTML content, but for XML, we will want to build and parse
  // the end tags.  However, if tagStack is empty, it's the last call
  // for XML as well.
  if (theCount == 0) {
    result = Parse(aSourceBuffer, &theContext, true);
    fragSink->DidBuildContent();
  } else {
    // Add an end tag chunk, so expat will read the whole source buffer,
    // and not worry about ']]' etc.
    result = Parse(aSourceBuffer + u"</"_ns, &theContext, false);
    fragSink->DidBuildContent();

    if (NS_SUCCEEDED(result)) {
      nsAutoString endContext;
      for (theIndex = 0; theIndex < theCount; theIndex++) {
        // we already added an end tag chunk above
        if (theIndex > 0) {
          endContext.AppendLiteral("</");
        }

        nsString& thisTag = aTagStack[theIndex];
        // was there an xmlns=?
        int32_t endOfTag = thisTag.FindChar(char16_t(' '));
        if (endOfTag == -1) {
          endContext.Append(thisTag);
        } else {
          endContext.Append(Substring(thisTag, 0, endOfTag));
        }

        endContext.Append('>');
      }

      result = Parse(endContext, &theContext, true);
    }
  }

  mFlags |= NS_PARSER_FLAG_OBSERVERS_ENABLED;

  return result;
}

/**
 *  This routine is called to cause the parser to continue parsing its
 *  underlying stream.  This call allows the parse process to happen in
 *  chunks, such as when the content is push based, and we need to parse in
 *  pieces.
 *
 *  An interesting change in how the parser gets used has led us to add extra
 *  processing to this method.  The case occurs when the parser is blocked in
 *  one context, and gets a parse(string) call in another context.  In this
 *  case, the parserContexts are linked. No problem.
 *
 *  The problem is that Parse(string) assumes that it can proceed unabated,
 *  but if the parser is already blocked that assumption is false. So we
 *  needed to add a mechanism here to allow the parser to continue to process
 *  (the pop and free) contexts until 1) it get's blocked again; 2) it runs
 *  out of contexts.
 *
 *
 *  @param   allowItertion : set to true if non-script resumption is requested
 *  @param   aIsFinalChunk : tells us when the last chunk of data is provided.
 *  @return  error code -- 0 if ok, non-zero if error.
 */
nsresult nsParser::ResumeParse(bool allowIteration, bool aIsFinalChunk,
                               bool aCanInterrupt) {
  if (mInternalState == NS_ERROR_OUT_OF_MEMORY) {
    // Checking NS_ERROR_OUT_OF_MEMORY instead of NS_FAILED
    // to avoid introducing unintentional changes to behavior.
    return mInternalState;
  }

  nsresult result = NS_OK;

  if (!mBlocked && mInternalState != NS_ERROR_HTMLPARSER_STOPPARSING) {
    result = WillBuildModel(mParserContext->mScanner->GetFilename());
    if (NS_FAILED(result)) {
      mFlags &= ~NS_PARSER_FLAG_CAN_TOKENIZE;
      return result;
    }

    if (mDTD) {
      mSink->WillResume();
      bool theIterationIsOk = true;

      while (result == NS_OK && theIterationIsOk) {
        if (!mUnusedInput.IsEmpty() && mParserContext->mScanner) {
          // -- Ref: Bug# 22485 --
          // Insert the unused input into the source buffer
          // as if it was read from the input stream.
          // Adding UngetReadable() per vidur!!
          mParserContext->mScanner->UngetReadable(mUnusedInput);
          mUnusedInput.Truncate(0);
        }

        // Only allow parsing to be interrupted in the subsequent call to
        // build model.
        nsresult theTokenizerResult = (mFlags & NS_PARSER_FLAG_CAN_TOKENIZE)
                                          ? Tokenize(aIsFinalChunk)
                                          : NS_OK;
        result = BuildModel();

        if (result == NS_ERROR_HTMLPARSER_INTERRUPTED && aIsFinalChunk) {
          PostContinueEvent();
        }

        theIterationIsOk = theTokenizerResult != NS_ERROR_HTMLPARSER_EOF &&
                           result != NS_ERROR_HTMLPARSER_INTERRUPTED;

        // Make sure not to stop parsing too early. Therefore, before shutting
        // down the parser, it's important to check whether the input buffer
        // has been scanned to completion (theTokenizerResult should be kEOF).
        // kEOF -> End of buffer.

        // If we're told to block the parser, we disable all further parsing
        // (and cache any data coming in) until the parser is re-enabled.
        if (NS_ERROR_HTMLPARSER_BLOCK == result) {
          mSink->WillInterrupt();
          if (!mBlocked) {
            // If we were blocked by a recursive invocation, don't re-block.
            BlockParser();
          }
          return NS_OK;
        }
        if (NS_ERROR_HTMLPARSER_STOPPARSING == result) {
          // Note: Parser Terminate() calls DidBuildModel.
          if (mInternalState != NS_ERROR_HTMLPARSER_STOPPARSING) {
            DidBuildModel(mStreamStatus);
            mInternalState = result;
          }

          return NS_OK;
        }
        if ((NS_OK == result &&
             theTokenizerResult == NS_ERROR_HTMLPARSER_EOF) ||
            result == NS_ERROR_HTMLPARSER_INTERRUPTED) {
          bool theContextIsStringBased =
              CParserContext::eCTString == mParserContext->mContextType;

          if (mParserContext->mStreamListenerState == eOnStop ||
              !mParserContext->mMultipart || theContextIsStringBased) {
            if (!mParserContext->mPrevContext) {
              if (mParserContext->mStreamListenerState == eOnStop) {
                DidBuildModel(mStreamStatus);
                return NS_OK;
              }
            } else {
              CParserContext* theContext = PopContext();
              if (theContext) {
                theIterationIsOk = allowIteration && theContextIsStringBased;
                if (theContext->mCopyUnused) {
                  if (!theContext->mScanner->CopyUnusedData(mUnusedInput)) {
                    mInternalState = NS_ERROR_OUT_OF_MEMORY;
                  }
                }

                delete theContext;
              }

              result = mInternalState;
              aIsFinalChunk = mParserContext &&
                              mParserContext->mStreamListenerState == eOnStop;
              // ...then intentionally fall through to mSink->WillInterrupt()...
            }
          }
        }

        if (theTokenizerResult == NS_ERROR_HTMLPARSER_EOF ||
            result == NS_ERROR_HTMLPARSER_INTERRUPTED) {
          result = (result == NS_ERROR_HTMLPARSER_INTERRUPTED) ? NS_OK : result;
          mSink->WillInterrupt();
        }
      }
    } else {
      mInternalState = result = NS_ERROR_HTMLPARSER_UNRESOLVEDDTD;
    }
  }

  return (result == NS_ERROR_HTMLPARSER_INTERRUPTED) ? NS_OK : result;
}

/**
 *  This is where we loop over the tokens created in the
 *  tokenization phase, and try to make sense out of them.
 */
nsresult nsParser::BuildModel() {
  if (mInternalState == NS_ERROR_OUT_OF_MEMORY) {
    // Checking NS_ERROR_OUT_OF_MEMORY instead of NS_FAILED
    // to avoid introducing unintentional changes to behavior.
    return mInternalState;
  }

  nsITokenizer* theTokenizer = nullptr;

  nsresult result = NS_OK;
  if (mParserContext) {
    result = mParserContext->GetTokenizer(mDTD, mSink, theTokenizer);
  }

  if (NS_SUCCEEDED(result)) {
    if (mDTD) {
      result = mDTD->BuildModel(theTokenizer, mSink);
    }
  } else {
    mInternalState = result = NS_ERROR_HTMLPARSER_BADTOKENIZER;
  }
  return result;
}

/*******************************************************************
  These methods are used to talk to the netlib system...
 *******************************************************************/

nsresult nsParser::OnStartRequest(nsIRequest* request) {
  if (mInternalState == NS_ERROR_OUT_OF_MEMORY) {
    // Checking NS_ERROR_OUT_OF_MEMORY instead of NS_FAILED
    // to avoid introducing unintentional changes to behavior.
    return mInternalState;
  }

  MOZ_ASSERT(eNone == mParserContext->mStreamListenerState,
             "Parser's nsIStreamListener API was not setup "
             "correctly in constructor.");

  mParserContext->mStreamListenerState = eOnStart;
  mParserContext->mAutoDetectStatus = eUnknownDetect;
  mParserContext->mRequest = request;

  NS_ASSERTION(!mParserContext->mPrevContext,
               "Clobbering DTD for non-root parser context!");
  mDTD = nullptr;

  nsresult rv;
  nsAutoCString contentType;
  nsCOMPtr<nsIChannel> channel = do_QueryInterface(request);
  if (channel) {
    rv = channel->GetContentType(contentType);
    if (NS_SUCCEEDED(rv)) {
      mParserContext->SetMimeType(contentType);
    }
  }

  rv = NS_OK;

  return rv;
}

static bool ExtractCharsetFromXmlDeclaration(const unsigned char* aBytes,
                                             int32_t aLen,
                                             nsCString& oCharset) {
  // This code is rather pointless to have. Might as well reuse expat as
  // seen in nsHtml5StreamParser. -- hsivonen
  oCharset.Truncate();
  if ((aLen >= 5) && ('<' == aBytes[0]) && ('?' == aBytes[1]) &&
      ('x' == aBytes[2]) && ('m' == aBytes[3]) && ('l' == aBytes[4])) {
    int32_t i;
    bool versionFound = false, encodingFound = false;
    for (i = 6; i < aLen && !encodingFound; ++i) {
      // end of XML declaration?
      if ((((char*)aBytes)[i] == '?') && ((i + 1) < aLen) &&
          (((char*)aBytes)[i + 1] == '>')) {
        break;
      }
      // Version is required.
      if (!versionFound) {
        // Want to avoid string comparisons, hence looking for 'n'
        // and only if found check the string leading to it. Not
        // foolproof, but fast.
        // The shortest string allowed before this is  (strlen==13):
        // <?xml version
        if ((((char*)aBytes)[i] == 'n') && (i >= 12) &&
            (0 == strncmp("versio", (char*)(aBytes + i - 6), 6))) {
          // Fast forward through version
          char q = 0;
          for (++i; i < aLen; ++i) {
            char qi = ((char*)aBytes)[i];
            if (qi == '\'' || qi == '"') {
              if (q && q == qi) {
                //  ending quote
                versionFound = true;
                break;
              } else {
                // Starting quote
                q = qi;
              }
            }
          }
        }
      } else {
        // encoding must follow version
        // Want to avoid string comparisons, hence looking for 'g'
        // and only if found check the string leading to it. Not
        // foolproof, but fast.
        // The shortest allowed string before this (strlen==26):
        // <?xml version="1" encoding
        if ((((char*)aBytes)[i] == 'g') && (i >= 25) &&
            (0 == strncmp("encodin", (char*)(aBytes + i - 7), 7))) {
          int32_t encStart = 0;
          char q = 0;
          for (++i; i < aLen; ++i) {
            char qi = ((char*)aBytes)[i];
            if (qi == '\'' || qi == '"') {
              if (q && q == qi) {
                int32_t count = i - encStart;
                // encoding value is invalid if it is UTF-16
                if (count > 0 &&
                    PL_strncasecmp("UTF-16", (char*)(aBytes + encStart),
                                   count)) {
                  oCharset.Assign((char*)(aBytes + encStart), count);
                }
                encodingFound = true;
                break;
              } else {
                encStart = i + 1;
                q = qi;
              }
            }
          }
        }
      }  // if (!versionFound)
    }    // for
  }
  return !oCharset.IsEmpty();
}

inline char GetNextChar(nsACString::const_iterator& aStart,
                        nsACString::const_iterator& aEnd) {
  NS_ASSERTION(aStart != aEnd, "end of buffer");
  return (++aStart != aEnd) ? *aStart : '\0';
}

static nsresult NoOpParserWriteFunc(nsIInputStream* in, void* closure,
                                    const char* fromRawSegment,
                                    uint32_t toOffset, uint32_t count,
                                    uint32_t* writeCount) {
  *writeCount = count;
  return NS_OK;
}

typedef struct {
  bool mNeedCharsetCheck;
  nsParser* mParser;
  nsScanner* mScanner;
  nsIRequest* mRequest;
} ParserWriteStruct;

/*
 * This function is invoked as a result of a call to a stream's
 * ReadSegments() method. It is called for each contiguous buffer
 * of data in the underlying stream or pipe. Using ReadSegments
 * allows us to avoid copying data to read out of the stream.
 */
static nsresult ParserWriteFunc(nsIInputStream* in, void* closure,
                                const char* fromRawSegment, uint32_t toOffset,
                                uint32_t count, uint32_t* writeCount) {
  nsresult result;
  ParserWriteStruct* pws = static_cast<ParserWriteStruct*>(closure);
  const unsigned char* buf =
      reinterpret_cast<const unsigned char*>(fromRawSegment);
  uint32_t theNumRead = count;

  if (!pws) {
    return NS_ERROR_FAILURE;
  }

  if (pws->mNeedCharsetCheck) {
    pws->mNeedCharsetCheck = false;
    int32_t source;
    auto preferred = pws->mParser->GetDocumentCharset(source);

    // This code was bogus when I found it. It expects the BOM or the XML
    // declaration to be entirely in the first network buffer. -- hsivonen
    const Encoding* encoding;
    std::tie(encoding, std::ignore) = Encoding::ForBOM(Span(buf, count));
    if (encoding) {
      // The decoder will swallow the BOM. The UTF-16 will re-sniff for
      // endianness. The value of preferred is now "UTF-8", "UTF-16LE"
      // or "UTF-16BE".
      preferred = WrapNotNull(encoding);
      source = kCharsetFromByteOrderMark;
    } else if (source < kCharsetFromChannel) {
      nsAutoCString declCharset;

      if (ExtractCharsetFromXmlDeclaration(buf, count, declCharset)) {
        encoding = Encoding::ForLabel(declCharset);
        if (encoding) {
          preferred = WrapNotNull(encoding);
          source = kCharsetFromMetaTag;
        }
      }
    }

    pws->mParser->SetDocumentCharset(preferred, source, false);
    pws->mParser->SetSinkCharset(preferred);
  }

  result = pws->mScanner->Append(fromRawSegment, theNumRead);
  if (NS_SUCCEEDED(result)) {
    *writeCount = count;
  }

  return result;
}

nsresult nsParser::OnDataAvailable(nsIRequest* request,
                                   nsIInputStream* pIStream,
                                   uint64_t sourceOffset, uint32_t aLength) {
  if (mInternalState == NS_ERROR_OUT_OF_MEMORY) {
    // Checking NS_ERROR_OUT_OF_MEMORY instead of NS_FAILED
    // to avoid introducing unintentional changes to behavior.
    return mInternalState;
  }

  MOZ_ASSERT((eOnStart == mParserContext->mStreamListenerState ||
              eOnDataAvail == mParserContext->mStreamListenerState),
             "Error: OnStartRequest() must be called before OnDataAvailable()");
  MOZ_ASSERT(NS_InputStreamIsBuffered(pIStream),
             "Must have a buffered input stream");

  nsresult rv = NS_OK;

  if (mIsAboutBlank) {
    MOZ_ASSERT(false, "Must not get OnDataAvailable for about:blank");
    // ... but if an extension tries to feed us data for about:blank in a
    // release build, silently ignore the data.
    uint32_t totalRead;
    rv = pIStream->ReadSegments(NoOpParserWriteFunc, nullptr, aLength,
                                &totalRead);
    return rv;
  }

  CParserContext* theContext = mParserContext;

  while (theContext && theContext->mRequest != request) {
    theContext = theContext->mPrevContext;
  }

  if (theContext) {
    theContext->mStreamListenerState = eOnDataAvail;

    if (eInvalidDetect == theContext->mAutoDetectStatus) {
      if (theContext->mScanner) {
        nsScannerIterator iter;
        theContext->mScanner->EndReading(iter);
        theContext->mScanner->SetPosition(iter, true);
      }
    }

    uint32_t totalRead;
    ParserWriteStruct pws;
    pws.mNeedCharsetCheck = true;
    pws.mParser = this;
    pws.mScanner = theContext->mScanner.get();
    pws.mRequest = request;

    rv = pIStream->ReadSegments(ParserWriteFunc, &pws, aLength, &totalRead);
    if (NS_FAILED(rv)) {
      return rv;
    }

    if (IsOkToProcessNetworkData()) {
      nsCOMPtr<nsIParser> kungFuDeathGrip(this);
      nsCOMPtr<nsIContentSink> sinkDeathGrip(mSink);
      mProcessingNetworkData = true;
      if (sinkDeathGrip) {
        sinkDeathGrip->WillParse();
      }
      rv = ResumeParse();
      mProcessingNetworkData = false;
    }
  } else {
    rv = NS_ERROR_UNEXPECTED;
  }

  return rv;
}

/**
 *  This is called by the networking library once the last block of data
 *  has been collected from the net.
 */
nsresult nsParser::OnStopRequest(nsIRequest* request, nsresult status) {
  if (mInternalState == NS_ERROR_OUT_OF_MEMORY) {
    // Checking NS_ERROR_OUT_OF_MEMORY instead of NS_FAILED
    // to avoid introducing unintentional changes to behavior.
    return mInternalState;
  }

  nsresult rv = NS_OK;

  CParserContext* pc = mParserContext;
  while (pc) {
    if (pc->mRequest == request) {
      pc->mStreamListenerState = eOnStop;
      pc->mScanner->SetIncremental(false);
      break;
    }

    pc = pc->mPrevContext;
  }

  mStreamStatus = status;

  if (IsOkToProcessNetworkData() && NS_SUCCEEDED(rv)) {
    mProcessingNetworkData = true;
    if (mSink) {
      mSink->WillParse();
    }
    rv = ResumeParse(true, true);
    mProcessingNetworkData = false;
  }

  // If the parser isn't enabled, we don't finish parsing till
  // it is reenabled.

  return rv;
}

/*******************************************************************
  Here come the tokenization methods...
 *******************************************************************/

/**
 *  Part of the code sandwich, this gets called right before
 *  the tokenization process begins. The main reason for
 *  this call is to allow the delegate to do initialization.
 */
bool nsParser::WillTokenize(bool aIsFinalChunk) {
  if (!mParserContext) {
    return true;
  }

  nsITokenizer* theTokenizer;
  nsresult result = mParserContext->GetTokenizer(mDTD, mSink, theTokenizer);
  NS_ENSURE_SUCCESS(result, false);
  return NS_SUCCEEDED(theTokenizer->WillTokenize(aIsFinalChunk));
}

/**
 * This is the primary control routine to consume tokens.
 * It iteratively consumes tokens until an error occurs or
 * you run out of data.
 */
nsresult nsParser::Tokenize(bool aIsFinalChunk) {
  if (mInternalState == NS_ERROR_OUT_OF_MEMORY) {
    // Checking NS_ERROR_OUT_OF_MEMORY instead of NS_FAILED
    // to avoid introducing unintentional changes to behavior.
    return mInternalState;
  }

  nsITokenizer* theTokenizer;

  nsresult result = NS_ERROR_NOT_AVAILABLE;
  if (mParserContext) {
    result = mParserContext->GetTokenizer(mDTD, mSink, theTokenizer);
  }

  if (NS_SUCCEEDED(result)) {
    bool flushTokens = false;

    bool killSink = false;

    WillTokenize(aIsFinalChunk);
    while (NS_SUCCEEDED(result)) {
      mParserContext->mScanner->Mark();
      result =
          theTokenizer->ConsumeToken(*mParserContext->mScanner, flushTokens);
      if (NS_FAILED(result)) {
        mParserContext->mScanner->RewindToMark();
        if (NS_ERROR_HTMLPARSER_EOF == result) {
          break;
        }
        if (NS_ERROR_HTMLPARSER_STOPPARSING == result) {
          killSink = true;
          result = Terminate();
          break;
        }
      } else if (flushTokens && (mFlags & NS_PARSER_FLAG_OBSERVERS_ENABLED)) {
        // I added the extra test of NS_PARSER_FLAG_OBSERVERS_ENABLED to fix
        // Bug# 23931. Flush tokens on seeing </SCRIPT> -- Ref: Bug# 22485 --
        // Also remember to update the marked position.
        mFlags |= NS_PARSER_FLAG_FLUSH_TOKENS;
        mParserContext->mScanner->Mark();
        break;
      }
    }

    if (killSink) {
      mSink = nullptr;
    }
  } else {
    result = mInternalState = NS_ERROR_HTMLPARSER_BADTOKENIZER;
  }

  return result;
}

/**
 * Get the channel associated with this parser
 *
 * @param aChannel out param that will contain the result
 * @return NS_OK if successful
 */
NS_IMETHODIMP
nsParser::GetChannel(nsIChannel** aChannel) {
  nsresult result = NS_ERROR_NOT_AVAILABLE;
  if (mParserContext && mParserContext->mRequest) {
    result = CallQueryInterface(mParserContext->mRequest, aChannel);
  }
  return result;
}

/**
 * Get the DTD associated with this parser
 */
NS_IMETHODIMP
nsParser::GetDTD(nsIDTD** aDTD) {
  if (mParserContext) {
    NS_IF_ADDREF(*aDTD = mDTD);
  }

  return NS_OK;
}

/**
 * Get this as nsIStreamListener
 */
nsIStreamListener* nsParser::GetStreamListener() { return this; }
