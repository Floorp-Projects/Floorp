/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=2 et tw=79: */
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Pierre Phaneuf <pp@ludusdesign.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include "nsIAtom.h"
#include "nsParser.h"
#include "nsString.h"
#include "nsCRT.h"
#include "nsScanner.h"
#include "plstr.h"
#include "nsIStringStream.h"
#include "nsIChannel.h"
#include "nsICachingChannel.h"
#include "nsICacheEntryDescriptor.h"
#include "nsICharsetAlias.h"
#include "nsICharsetConverterManager.h"
#include "nsIInputStream.h"
#include "CNavDTD.h"
#include "prenv.h"
#include "prlock.h"
#include "prcvar.h"
#include "nsParserCIID.h"
#include "nsReadableUtils.h"
#include "nsCOMPtr.h"
#include "nsExpatDriver.h"
#include "nsIServiceManager.h"
#include "nsICategoryManager.h"
#include "nsISupportsPrimitives.h"
#include "nsIFragmentContentSink.h"
#include "nsStreamUtils.h"
#include "nsHTMLTokenizer.h"
#include "nsIDocument.h"
#include "nsNetUtil.h"
#include "nsScriptLoader.h"
#include "nsDataHashtable.h"
#include "nsIThreadPool.h"
#include "nsXPCOMCIDInternal.h"
#include "nsMimeTypes.h"
#include "nsViewSourceHTML.h"
#include "mozilla/CondVar.h"
#include "mozilla/Mutex.h"
#include "nsParserConstants.h"

using namespace mozilla;

#define NS_PARSER_FLAG_PARSER_ENABLED         0x00000002
#define NS_PARSER_FLAG_OBSERVERS_ENABLED      0x00000004
#define NS_PARSER_FLAG_PENDING_CONTINUE_EVENT 0x00000008
#define NS_PARSER_FLAG_CAN_INTERRUPT          0x00000010
#define NS_PARSER_FLAG_FLUSH_TOKENS           0x00000020
#define NS_PARSER_FLAG_CAN_TOKENIZE           0x00000040

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_CID(kCParserCID, NS_PARSER_CID);
static NS_DEFINE_IID(kIParserIID, NS_IPARSER_IID);

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


class nsParserContinueEvent : public nsRunnable
{
public:
  nsRefPtr<nsParser> mParser;

  nsParserContinueEvent(nsParser* aParser)
    : mParser(aParser)
  {}

  NS_IMETHOD Run()
  {
    mParser->HandleParserContinueEvent(this);
    return NS_OK;
  }
};

//-------------- End ParseContinue Event Definition ------------------------

template <class Type>
class Holder {
public:
  typedef void (*Reaper)(Type *);

  Holder(Reaper aReaper)
    : mHoldee(nsnull), mReaper(aReaper)
  {
  }

  ~Holder() {
    if (mHoldee) {
      mReaper(mHoldee);
    }
  }

  Type *get() {
    return mHoldee;
  }
  const Holder &operator =(Type *aHoldee) {
    if (mHoldee && aHoldee != mHoldee) {
      mReaper(mHoldee);
    }
    mHoldee = aHoldee;
    return *this;
  }

private:
  Type *mHoldee;
  Reaper mReaper;
};

class nsSpeculativeScriptThread : public nsIRunnable {
public:
  nsSpeculativeScriptThread()
    : mLock("nsSpeculativeScriptThread.mLock"),
      mCVar(mLock, "nsSpeculativeScriptThread.mCVar"),
      mKeepParsing(PR_FALSE),
      mCurrentlyParsing(PR_FALSE),
      mNumConsumed(0),
      mContext(nsnull),
      mTerminated(PR_FALSE) {
  }

  ~nsSpeculativeScriptThread() {
    NS_ASSERTION(NS_IsMainThread() || !mDocument,
                 "Destroying the document on the wrong thread");
  }

  NS_DECL_ISUPPORTS
  NS_DECL_NSIRUNNABLE

  nsresult StartParsing(nsParser *aParser);
  void StopParsing(bool aFromDocWrite);

  enum PrefetchType { NONE, SCRIPT, STYLESHEET, IMAGE };
  struct PrefetchEntry {
    PrefetchType type;
    nsString uri;
    nsString charset;
    nsString elementType;
  };

  nsIDocument *GetDocument() {
    NS_ASSERTION(NS_IsMainThread(), "Potential threadsafety hazard");
    return mDocument;
  }

  bool Parsing() {
    return mCurrentlyParsing;
  }

  CParserContext *Context() {
    return mContext;
  }

  typedef nsDataHashtable<nsCStringHashKey, bool> PreloadedType;
  PreloadedType& GetPreloadedURIs() {
    return mPreloadedURIs;
  }

  void Terminate() {
    mTerminated = PR_TRUE;
    StopParsing(PR_FALSE);
  }
  bool Terminated() {
    return mTerminated;
  }

private:

  void ProcessToken(CToken *aToken);

  void AddToPrefetchList(const nsAString &src,
                         const nsAString &charset,
                         const nsAString &elementType,
                         PrefetchType type);

  void FlushURIs();

  // These members are only accessed on the speculatively parsing thread.
  nsTokenAllocator mTokenAllocator;

  // The following members are shared across the main thread and the
  // speculatively parsing thread.
  Mutex mLock;
  CondVar mCVar;

  volatile bool mKeepParsing;
  volatile bool mCurrentlyParsing;
  nsRefPtr<nsHTMLTokenizer> mTokenizer;
  nsAutoPtr<nsScanner> mScanner;

  enum { kBatchPrefetchURIs = 5 };
  nsAutoTArray<PrefetchEntry, kBatchPrefetchURIs> mURIs;

  // Number of characters consumed by the last speculative parse.
  PRUint32 mNumConsumed;

  // These members are only accessed on the main thread.
  nsCOMPtr<nsIDocument> mDocument;
  CParserContext *mContext;
  PreloadedType mPreloadedURIs;
  bool mTerminated;
};

class nsPreloadURIs : public nsIRunnable {
public:
  nsPreloadURIs(nsAutoTArray<nsSpeculativeScriptThread::PrefetchEntry, 5> &aURIs,
                nsSpeculativeScriptThread *aScriptThread)
    : mURIs(aURIs),
      mScriptThread(aScriptThread) {
  }

  NS_DECL_ISUPPORTS
  NS_DECL_NSIRUNNABLE

  static void PreloadURIs(const nsAutoTArray<nsSpeculativeScriptThread::PrefetchEntry, 5> &aURIs,
                          nsSpeculativeScriptThread *aScriptThread);

private:
  nsAutoTArray<nsSpeculativeScriptThread::PrefetchEntry, 5> mURIs;
  nsRefPtr<nsSpeculativeScriptThread> mScriptThread;
};

NS_IMPL_THREADSAFE_ISUPPORTS1(nsPreloadURIs, nsIRunnable)

NS_IMETHODIMP
nsPreloadURIs::Run()
{
  PreloadURIs(mURIs, mScriptThread);
  return NS_OK;
}

void
nsPreloadURIs::PreloadURIs(const nsAutoTArray<nsSpeculativeScriptThread::PrefetchEntry, 5> &aURIs,
                           nsSpeculativeScriptThread *aScriptThread)
{
  NS_ASSERTION(NS_IsMainThread(), "Touching non-threadsafe objects off thread");

  if (aScriptThread->Terminated()) {
    return;
  }

  nsIDocument *doc = aScriptThread->GetDocument();
  NS_ASSERTION(doc, "We shouldn't have started preloading without a document");

  // Note: Per the code in the HTML content sink, we should be keeping track
  // of each <base href> as it comes. However, because we do our speculative
  // parsing off the main thread, this is hard to emulate. For now, just load
  // the URIs using the document's base URI at the potential cost of being
  // wrong and having to re-load a given relative URI later.
  nsIURI *base = doc->GetDocBaseURI();
  const nsCString &charset = doc->GetDocumentCharacterSet();
  nsSpeculativeScriptThread::PreloadedType &alreadyPreloaded =
    aScriptThread->GetPreloadedURIs();
  for (PRUint32 i = 0, e = aURIs.Length(); i < e; ++i) {
    const nsSpeculativeScriptThread::PrefetchEntry &pe = aURIs[i];
    nsCOMPtr<nsIURI> uri;
    nsresult rv = NS_NewURI(getter_AddRefs(uri), pe.uri, charset.get(), base);
    if (NS_FAILED(rv)) {
      NS_WARNING("Failed to create a URI");
      continue;
    }

    nsCAutoString spec;
    uri->GetSpec(spec);
    bool answer;
    if (alreadyPreloaded.Get(spec, &answer)) {
      // Already preloaded. Don't preload again.
      continue;
    }

    alreadyPreloaded.Put(spec, PR_TRUE);

    switch (pe.type) {
      case nsSpeculativeScriptThread::SCRIPT:
        doc->ScriptLoader()->PreloadURI(uri, pe.charset, pe.elementType);
        break;
      case nsSpeculativeScriptThread::IMAGE:
        doc->MaybePreLoadImage(uri, EmptyString());
        break;
      case nsSpeculativeScriptThread::STYLESHEET:
        doc->PreloadStyle(uri, pe.charset);
        break;
      case nsSpeculativeScriptThread::NONE:
        NS_NOTREACHED("Uninitialized preload entry?");
        break;
    }
  }
}

NS_IMPL_THREADSAFE_ISUPPORTS1(nsSpeculativeScriptThread, nsIRunnable)

NS_IMETHODIMP
nsSpeculativeScriptThread::Run()
{
  NS_ASSERTION(!NS_IsMainThread(), "Speculative parsing on the main thread?");

  mNumConsumed = 0;

  mTokenizer->WillTokenize(PR_FALSE, &mTokenAllocator);
  while (mKeepParsing) {
    bool flushTokens = false;
    nsresult rv = mTokenizer->ConsumeToken(*mScanner, flushTokens);
    if (NS_FAILED(rv)) {
      break;
    }

    mNumConsumed += mScanner->Mark();

    // TODO Don't pop the tokens.
    CToken *token;
    while (mKeepParsing && (token = mTokenizer->PopToken())) {
      ProcessToken(token);
    }
  }
  mTokenizer->DidTokenize(PR_FALSE);

  if (mKeepParsing) {
    // Ran out of room in this part of the document -- flush out the URIs we
    // gathered so far so we don't end up waiting for the parser's current
    // load to finish.
    if (!mURIs.IsEmpty()) {
      FlushURIs();
    }
  }

  {
    MutexAutoLock al(mLock);

    mCurrentlyParsing = PR_FALSE;
    mCVar.Notify();
  }
  return NS_OK;
}

nsresult
nsSpeculativeScriptThread::StartParsing(nsParser *aParser)
{
  NS_ASSERTION(NS_IsMainThread(), "Called on the wrong thread");
  NS_ASSERTION(!mCurrentlyParsing, "Bad race happening");

  if (!aParser->ThreadPool()) {
    return NS_OK;
  }

  nsIContentSink *sink = aParser->GetContentSink();
  if (!sink) {
    return NS_OK;
  }

  nsCOMPtr<nsIDocument> doc = do_QueryInterface(sink->GetTarget());
  if (!doc) {
    return NS_OK;
  }

  nsAutoString toScan;
  CParserContext *context = aParser->PeekContext();
  if (!mTokenizer) {
    if (!mPreloadedURIs.Init(15)) {
      return NS_ERROR_OUT_OF_MEMORY;
    }

    mTokenizer = new nsHTMLTokenizer(context->mDTDMode, context->mDocType,
                                     context->mParserCommand, 0);
    if (!mTokenizer) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
    mTokenizer->CopyState(context->mTokenizer);
    context->mScanner->CopyUnusedData(toScan);
    if (toScan.IsEmpty()) {
      return NS_OK;
    }
  } else if (context == mContext) {
    // Don't parse the same part of the document twice.
    nsScannerIterator end;
    context->mScanner->EndReading(end);

    nsScannerIterator start;
    context->mScanner->CurrentPosition(start);

    if (mNumConsumed > context->mNumConsumed) {
      // We consumed more the last time we tried speculatively parsing than we
      // did the last time we actually parsed.
      PRUint32 distance = Distance(start, end);
      start.advance(NS_MIN(mNumConsumed - context->mNumConsumed, distance));
    }

    if (start == end) {
      // We're at the end of this context's buffer, nothing else to do.
      return NS_OK;
    }

    CopyUnicodeTo(start, end, toScan);
  } else {
    // Grab all of the context.
    context->mScanner->CopyUnusedData(toScan);
    if (toScan.IsEmpty()) {
      // Nothing to parse, don't do anything.
      return NS_OK;
    }
  }

  nsCAutoString charset;
  PRInt32 source;
  aParser->GetDocumentCharset(charset, source);

  mScanner = new nsScanner(toScan, charset, source);
  if (!mScanner) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  mScanner->SetIncremental(PR_TRUE);

  mDocument.swap(doc);
  mKeepParsing = PR_TRUE;
  mCurrentlyParsing = PR_TRUE;
  mContext = context;
  return aParser->ThreadPool()->Dispatch(this, NS_DISPATCH_NORMAL);
}

void
nsSpeculativeScriptThread::StopParsing(bool /*aFromDocWrite*/)
{
  NS_ASSERTION(NS_IsMainThread(), "Can't stop parsing from another thread");

  {
    MutexAutoLock al(mLock);

    mKeepParsing = PR_FALSE;
    if (mCurrentlyParsing) {
      mCVar.Wait();
      NS_ASSERTION(!mCurrentlyParsing, "Didn't actually stop parsing?");
    }
  }

  // The thread is now idle.
  if (mTerminated) {
    // If we're terminated, then we need to ensure that we release our document
    // and tokenizer here on the main thread so that our last reference to them
    // isn't our alter-ego rescheduled on another thread.
    mDocument = nsnull;
    mTokenizer = nsnull;
    mScanner = nsnull;
  } else if (mURIs.Length()) {
    // Note: Don't do this if we're terminated.
    nsPreloadURIs::PreloadURIs(mURIs, this);
    mURIs.Clear();
  }

  // Note: Currently, we pop the tokens off (see the comment in Run) so this
  // isn't a problem. If and when we actually use the tokens created
  // off-thread, we'll need to use aFromDocWrite for real.
}

void
nsSpeculativeScriptThread::ProcessToken(CToken *aToken)
{
  // Only called on the speculative script thread.

  CHTMLToken *token = static_cast<CHTMLToken *>(aToken);
  switch (static_cast<eHTMLTokenTypes>(token->GetTokenType())) {
    case eToken_start: {
        CStartToken *start = static_cast<CStartToken *>(aToken);
        nsHTMLTag tag = static_cast<nsHTMLTag>(start->GetTypeID());
        PRInt16 attrs = start->GetAttributeCount();
        PRInt16 i = 0;
        nsAutoString src;
        nsAutoString elementType;
        nsAutoString charset;
        nsAutoString href;
        nsAutoString rel;
        PrefetchType ptype = NONE;

        switch (tag) {
          case eHTMLTag_link:
              ptype = STYLESHEET;
              break;

          case eHTMLTag_img:
              ptype = IMAGE;
              break;

          case eHTMLTag_script:
              ptype = SCRIPT;
              break;

          default:
              break;
        }

        // We currently handle the following element/attribute combos :
        //     <link rel="stylesheet" href= charset= type>
        //     <script src= charset= type=>
        if (ptype != NONE) {
            // loop over all attributes to extract relevant info
            for (; i < attrs ; ++i) {
              CAttributeToken *attr = static_cast<CAttributeToken *>(mTokenizer->PopToken());
              NS_ASSERTION(attr->GetTokenType() == eToken_attribute, "Weird token");

              if (attr->GetKey().EqualsLiteral("src")) {
                src.Assign(attr->GetValue());
              } else if (attr->GetKey().EqualsLiteral("href")) {
                href.Assign(attr->GetValue());
              } else if (attr->GetKey().EqualsLiteral("rel")) {
                rel.Assign(attr->GetValue());
              } else if (attr->GetKey().EqualsLiteral("charset")) {
                charset.Assign(attr->GetValue());
              } else if (attr->GetKey().EqualsLiteral("type")) {
                elementType.Assign(attr->GetValue());
              }

              IF_FREE(attr, &mTokenAllocator);
            }

            // ensure we have the right kind if it's a link-element
            if (ptype == STYLESHEET) {
              if (rel.EqualsLiteral("stylesheet")) {
                src = href; // src is the important variable below
              } else {
                src.Truncate(); // clear src if wrong kind of link
              }
            }

            // add to list if we have a valid src
            if (!src.IsEmpty()) {
              AddToPrefetchList(src, charset, elementType, ptype);
            }
        } else {
            // Irrelevant tag, but pop and free all its attributes in any case
            for (; i < attrs ; ++i) {
              CToken *attr = mTokenizer->PopToken();
              IF_FREE(attr, &mTokenAllocator);
            }
        }
        break;
      }

    default:
      break;
  }

  IF_FREE(aToken, &mTokenAllocator);
}

void
nsSpeculativeScriptThread::AddToPrefetchList(const nsAString &src,
                                             const nsAString &charset,
                                             const nsAString &elementType,
                                             PrefetchType type)
{
  PrefetchEntry *pe = mURIs.AppendElement();
  pe->type = type;
  pe->uri = src;
  pe->charset = charset;
  pe->elementType = elementType;

  if (mURIs.Length() == kBatchPrefetchURIs) {
    FlushURIs();
  }
}

void
nsSpeculativeScriptThread::FlushURIs()
{
  nsCOMPtr<nsIRunnable> r = new nsPreloadURIs(mURIs, this);
  if (!r) {
    return;
  }

  mURIs.Clear();
  NS_DispatchToMainThread(r, NS_DISPATCH_NORMAL);
}

nsICharsetAlias* nsParser::sCharsetAliasService = nsnull;
nsICharsetConverterManager* nsParser::sCharsetConverterManager = nsnull;
nsIThreadPool* nsParser::sSpeculativeThreadPool = nsnull;

/**
 *  This gets called when the htmlparser module is initialized.
 */
// static
nsresult
nsParser::Init()
{
  nsresult rv;

  nsCOMPtr<nsICharsetAlias> charsetAlias =
    do_GetService(NS_CHARSETALIAS_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsICharsetConverterManager> charsetConverter =
    do_GetService(NS_CHARSETCONVERTERMANAGER_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  charsetAlias.swap(sCharsetAliasService);
  charsetConverter.swap(sCharsetConverterManager);

  nsCOMPtr<nsIThreadPool> threadPool =
    do_CreateInstance(NS_THREADPOOL_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = threadPool->SetThreadLimit(kSpeculativeThreadLimit);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = threadPool->SetIdleThreadLimit(kIdleThreadLimit);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = threadPool->SetIdleThreadTimeout(kIdleThreadTimeout);
  NS_ENSURE_SUCCESS(rv, rv);

  threadPool.swap(sSpeculativeThreadPool);

  return NS_OK;
}


/**
 *  This gets called when the htmlparser module is shutdown.
 */
// static
void nsParser::Shutdown()
{
  NS_IF_RELEASE(sCharsetAliasService);
  NS_IF_RELEASE(sCharsetConverterManager);
  if (sSpeculativeThreadPool) {
    sSpeculativeThreadPool->Shutdown();
    NS_RELEASE(sSpeculativeThreadPool);
  }
}

#ifdef DEBUG
static bool gDumpContent=false;
#endif

/**
 *  default constructor
 */
nsParser::nsParser()
{
  Initialize(PR_TRUE);
}

nsParser::~nsParser()
{
  Cleanup();
}

void
nsParser::Initialize(bool aConstructor)
{
#ifdef NS_DEBUG
  if (!gDumpContent) {
    gDumpContent = PR_GetEnv("PARSER_DUMP_CONTENT") != nsnull;
  }
#endif

  if (aConstructor) {
    // Raw pointer
    mParserContext = 0;
  }
  else {
    // nsCOMPtrs
    mObserver = nsnull;
    mParserFilter = nsnull;
    mUnusedInput.Truncate();
  }

  mContinueEvent = nsnull;
  mCharsetSource = kCharsetUninitialized;
  mCharset.AssignLiteral("ISO-8859-1");
  mInternalState = NS_OK;
  mStreamStatus = 0;
  mCommand = eViewNormal;
  mFlags = NS_PARSER_FLAG_OBSERVERS_ENABLED |
           NS_PARSER_FLAG_PARSER_ENABLED |
           NS_PARSER_FLAG_CAN_TOKENIZE;

  mProcessingNetworkData = PR_FALSE;
}

void
nsParser::Cleanup()
{
#ifdef NS_DEBUG
  if (gDumpContent) {
    if (mSink) {
      // Sink (HTMLContentSink at this time) supports nsIDebugDumpContent
      // interface. We can get to the content model through the sink.
      nsresult result = NS_OK;
      nsCOMPtr<nsIDebugDumpContent> trigger = do_QueryInterface(mSink, &result);
      if (NS_SUCCEEDED(result)) {
        trigger->DumpContentModel();
      }
    }
  }
#endif

#ifdef DEBUG
  if (mParserContext && mParserContext->mPrevContext) {
    NS_WARNING("Extra parser contexts still on the parser stack");
  }
#endif

  while (mParserContext) {
    CParserContext *pc = mParserContext->mPrevContext;
    delete mParserContext;
    mParserContext = pc;
  }

  // It should not be possible for this flag to be set when we are getting
  // destroyed since this flag implies a pending nsParserContinueEvent, which
  // has an owning reference to |this|.
  NS_ASSERTION(!(mFlags & NS_PARSER_FLAG_PENDING_CONTINUE_EVENT), "bad");
  if (mSpeculativeScriptThread) {
    mSpeculativeScriptThread->Terminate();
    mSpeculativeScriptThread = nsnull;
  }
}

NS_IMPL_CYCLE_COLLECTION_CLASS(nsParser)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(nsParser)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mDTD)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mSink)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mObserver)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(nsParser)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mDTD)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mSink)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mObserver)
  CParserContext *pc = tmp->mParserContext;
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

nsresult
nsParser::PostContinueEvent()
{
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
nsParser::SetParserFilter(nsIParserFilter * aFilter)
{
  mParserFilter = aFilter;
}

NS_IMETHODIMP_(void)
nsParser::GetCommand(nsCString& aCommand)
{
  aCommand = mCommandStr;
}

/**
 *  Call this method once you've created a parser, and want to instruct it
 *  about the command which caused the parser to be constructed. For example,
 *  this allows us to select a DTD which can do, say, view-source.
 *
 *  @param   aCommand the command string to set
 */
NS_IMETHODIMP_(void)
nsParser::SetCommand(const char* aCommand)
{
  mCommandStr.Assign(aCommand);
  if (mCommandStr.Equals("view-source")) {
    mCommand = eViewSource;
  } else if (mCommandStr.Equals("view-fragment")) {
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
nsParser::SetCommand(eParserCommands aParserCommand)
{
  mCommand = aParserCommand;
}

/**
 *  Call this method once you've created a parser, and want to instruct it
 *  about what charset to load
 *
 *  @param   aCharset- the charset of a document
 *  @param   aCharsetSource- the source of the charset
 */
NS_IMETHODIMP_(void)
nsParser::SetDocumentCharset(const nsACString& aCharset, PRInt32 aCharsetSource)
{
  mCharset = aCharset;
  mCharsetSource = aCharsetSource;
  if (mParserContext && mParserContext->mScanner) {
     mParserContext->mScanner->SetDocumentCharset(aCharset, aCharsetSource);
  }
}

void
nsParser::SetSinkCharset(nsACString& aCharset)
{
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
nsParser::SetContentSink(nsIContentSink* aSink)
{
  NS_PRECONDITION(aSink, "sink cannot be null!");
  mSink = aSink;

  if (mSink) {
    mSink->SetParser(this);
  }
}

/**
 * retrieve the sink set into the parser
 * @return  current sink
 */
NS_IMETHODIMP_(nsIContentSink*)
nsParser::GetContentSink()
{
  return mSink;
}

/**
 * Determine what DTD mode (and thus what layout nsCompatibility mode)
 * to use for this document based on the first chunk of data received
 * from the network (each parsercontext can have its own mode).  (No,
 * this is not an optimal solution -- we really don't need to know until
 * after we've received the DOCTYPE, and this could easily be part of
 * the regular parsing process if the parser were designed in a way that
 * made such modifications easy.)
 */

// Parse the PS production in the SGML spec (excluding the part dealing
// with entity references) starting at theIndex into theBuffer, and
// return the first index after the end of the production.
static PRInt32
ParsePS(const nsString& aBuffer, PRInt32 aIndex)
{
  for (;;) {
    PRUnichar ch = aBuffer.CharAt(aIndex);
    if ((ch == PRUnichar(' ')) || (ch == PRUnichar('\t')) ||
        (ch == PRUnichar('\n')) || (ch == PRUnichar('\r'))) {
      ++aIndex;
    } else if (ch == PRUnichar('-')) {
      PRInt32 tmpIndex;
      if (aBuffer.CharAt(aIndex+1) == PRUnichar('-') &&
          kNotFound != (tmpIndex=aBuffer.Find("--",PR_FALSE,aIndex+2,-1))) {
        aIndex = tmpIndex + 2;
      } else {
        return aIndex;
      }
    } else {
      return aIndex;
    }
  }
}

#define PARSE_DTD_HAVE_DOCTYPE          (1<<0)
#define PARSE_DTD_HAVE_PUBLIC_ID        (1<<1)
#define PARSE_DTD_HAVE_SYSTEM_ID        (1<<2)
#define PARSE_DTD_HAVE_INTERNAL_SUBSET  (1<<3)

// return PR_TRUE on success (includes not present), PR_FALSE on failure
static bool
ParseDocTypeDecl(const nsString &aBuffer,
                 PRInt32 *aResultFlags,
                 nsString &aPublicID,
                 nsString &aSystemID)
{
  bool haveDoctype = false;
  *aResultFlags = 0;

  // Skip through any comments and processing instructions
  // The PI-skipping is a bit of a hack.
  PRInt32 theIndex = 0;
  do {
    theIndex = aBuffer.FindChar('<', theIndex);
    if (theIndex == kNotFound) break;
    PRUnichar nextChar = aBuffer.CharAt(theIndex+1);
    if (nextChar == PRUnichar('!')) {
      PRInt32 tmpIndex = theIndex + 2;
      if (kNotFound !=
          (theIndex=aBuffer.Find("DOCTYPE", PR_TRUE, tmpIndex, 0))) {
        haveDoctype = PR_TRUE;
        theIndex += 7; // skip "DOCTYPE"
        break;
      }
      theIndex = ParsePS(aBuffer, tmpIndex);
      theIndex = aBuffer.FindChar('>', theIndex);
    } else if (nextChar == PRUnichar('?')) {
      theIndex = aBuffer.FindChar('>', theIndex);
    } else {
      break;
    }
  } while (theIndex != kNotFound);

  if (!haveDoctype)
    return PR_TRUE;
  *aResultFlags |= PARSE_DTD_HAVE_DOCTYPE;

  theIndex = ParsePS(aBuffer, theIndex);
  theIndex = aBuffer.Find("HTML", PR_TRUE, theIndex, 0);
  if (kNotFound == theIndex)
    return PR_FALSE;
  theIndex = ParsePS(aBuffer, theIndex+4);
  PRInt32 tmpIndex = aBuffer.Find("PUBLIC", PR_TRUE, theIndex, 0);

  if (kNotFound != tmpIndex) {
    theIndex = ParsePS(aBuffer, tmpIndex+6);

    // We get here only if we've read <!DOCTYPE HTML PUBLIC
    // (not case sensitive) possibly with comments within.

    // Now find the beginning and end of the public identifier
    // and the system identifier (if present).

    PRUnichar lit = aBuffer.CharAt(theIndex);
    if ((lit != PRUnichar('\"')) && (lit != PRUnichar('\'')))
      return PR_FALSE;

    // Start is the first character, excluding the quote, and End is
    // the final quote, so there are (end-start) characters.

    PRInt32 PublicIDStart = theIndex + 1;
    PRInt32 PublicIDEnd = aBuffer.FindChar(lit, PublicIDStart);
    if (kNotFound == PublicIDEnd)
      return PR_FALSE;
    theIndex = ParsePS(aBuffer, PublicIDEnd + 1);
    PRUnichar next = aBuffer.CharAt(theIndex);
    if (next == PRUnichar('>')) {
      // There was a public identifier, but no system
      // identifier,
      // so do nothing.
      // This is needed to avoid the else at the end, and it's
      // also the most common case.
    } else if ((next == PRUnichar('\"')) ||
               (next == PRUnichar('\''))) {
      // We found a system identifier.
      *aResultFlags |= PARSE_DTD_HAVE_SYSTEM_ID;
      PRInt32 SystemIDStart = theIndex + 1;
      PRInt32 SystemIDEnd = aBuffer.FindChar(next, SystemIDStart);
      if (kNotFound == SystemIDEnd)
        return PR_FALSE;
      aSystemID =
        Substring(aBuffer, SystemIDStart, SystemIDEnd - SystemIDStart);
    } else if (next == PRUnichar('[')) {
      // We found an internal subset.
      *aResultFlags |= PARSE_DTD_HAVE_INTERNAL_SUBSET;
    } else {
      // Something's wrong.
      return PR_FALSE;
    }

    // Since a public ID is a minimum literal, we must trim
    // and collapse whitespace
    aPublicID = Substring(aBuffer, PublicIDStart, PublicIDEnd - PublicIDStart);
    aPublicID.CompressWhitespace(PR_TRUE, PR_TRUE);
    *aResultFlags |= PARSE_DTD_HAVE_PUBLIC_ID;
  } else {
    tmpIndex=aBuffer.Find("SYSTEM", PR_TRUE, theIndex, 0);
    if (kNotFound != tmpIndex) {
      // DOCTYPES with system ID but no Public ID
      *aResultFlags |= PARSE_DTD_HAVE_SYSTEM_ID;

      theIndex = ParsePS(aBuffer, tmpIndex+6);
      PRUnichar next = aBuffer.CharAt(theIndex);
      if (next != PRUnichar('\"') && next != PRUnichar('\''))
        return PR_FALSE;

      PRInt32 SystemIDStart = theIndex + 1;
      PRInt32 SystemIDEnd = aBuffer.FindChar(next, SystemIDStart);

      if (kNotFound == SystemIDEnd)
        return PR_FALSE;
      aSystemID =
        Substring(aBuffer, SystemIDStart, SystemIDEnd - SystemIDStart);
      theIndex = ParsePS(aBuffer, SystemIDEnd + 1);
    }

    PRUnichar nextChar = aBuffer.CharAt(theIndex);
    if (nextChar == PRUnichar('['))
      *aResultFlags |= PARSE_DTD_HAVE_INTERNAL_SUBSET;
    else if (nextChar != PRUnichar('>'))
      return PR_FALSE;
  }
  return PR_TRUE;
}

struct PubIDInfo
{
  enum eMode {
    eQuirks,         /* always quirks mode, unless there's an internal subset */
    eAlmostStandards,/* eCompatibility_AlmostStandards */
    eFullStandards   /* eCompatibility_FullStandards */
      /*
       * public IDs that should trigger strict mode are not listed
       * since we want all future public IDs to trigger strict mode as
       * well
       */
  };

  const char* name;
  eMode mode_if_no_sysid;
  eMode mode_if_sysid;
};

#define ELEMENTS_OF(array_) (sizeof(array_)/sizeof(array_[0]))

// These must be in nsCRT::strcmp order so binary-search can be used.
// This is verified, |#ifdef DEBUG|, below.

// Even though public identifiers should be case sensitive, we will do
// all comparisons after converting to lower case in order to do
// case-insensitive comparison since there are a number of existing web
// sites that use the incorrect case.  Therefore all of the public
// identifiers below are in lower case (with the correct case following,
// in comments).  The case is verified, |#ifdef DEBUG|, below.
static const PubIDInfo kPublicIDs[] = {
  {"+//silmaril//dtd html pro v0r11 19970101//en" /* "+//Silmaril//dtd html Pro v0r11 19970101//EN" */, PubIDInfo::eQuirks, PubIDInfo::eQuirks},
  {"-//advasoft ltd//dtd html 3.0 aswedit + extensions//en" /* "-//AdvaSoft Ltd//DTD HTML 3.0 asWedit + extensions//EN" */, PubIDInfo::eQuirks, PubIDInfo::eQuirks},
  {"-//as//dtd html 3.0 aswedit + extensions//en" /* "-//AS//DTD HTML 3.0 asWedit + extensions//EN" */, PubIDInfo::eQuirks, PubIDInfo::eQuirks},
  {"-//ietf//dtd html 2.0 level 1//en" /* "-//IETF//DTD HTML 2.0 Level 1//EN" */, PubIDInfo::eQuirks, PubIDInfo::eQuirks},
  {"-//ietf//dtd html 2.0 level 2//en" /* "-//IETF//DTD HTML 2.0 Level 2//EN" */, PubIDInfo::eQuirks, PubIDInfo::eQuirks},
  {"-//ietf//dtd html 2.0 strict level 1//en" /* "-//IETF//DTD HTML 2.0 Strict Level 1//EN" */, PubIDInfo::eQuirks, PubIDInfo::eQuirks},
  {"-//ietf//dtd html 2.0 strict level 2//en" /* "-//IETF//DTD HTML 2.0 Strict Level 2//EN" */, PubIDInfo::eQuirks, PubIDInfo::eQuirks},
  {"-//ietf//dtd html 2.0 strict//en" /* "-//IETF//DTD HTML 2.0 Strict//EN" */, PubIDInfo::eQuirks, PubIDInfo::eQuirks},
  {"-//ietf//dtd html 2.0//en" /* "-//IETF//DTD HTML 2.0//EN" */, PubIDInfo::eQuirks, PubIDInfo::eQuirks},
  {"-//ietf//dtd html 2.1e//en" /* "-//IETF//DTD HTML 2.1E//EN" */, PubIDInfo::eQuirks, PubIDInfo::eQuirks},
  {"-//ietf//dtd html 3.0//en" /* "-//IETF//DTD HTML 3.0//EN" */, PubIDInfo::eQuirks, PubIDInfo::eQuirks},
  {"-//ietf//dtd html 3.0//en//" /* "-//IETF//DTD HTML 3.0//EN//" */, PubIDInfo::eQuirks, PubIDInfo::eQuirks},
  {"-//ietf//dtd html 3.2 final//en" /* "-//IETF//DTD HTML 3.2 Final//EN" */, PubIDInfo::eQuirks, PubIDInfo::eQuirks},
  {"-//ietf//dtd html 3.2//en" /* "-//IETF//DTD HTML 3.2//EN" */, PubIDInfo::eQuirks, PubIDInfo::eQuirks},
  {"-//ietf//dtd html 3//en" /* "-//IETF//DTD HTML 3//EN" */, PubIDInfo::eQuirks, PubIDInfo::eQuirks},
  {"-//ietf//dtd html level 0//en" /* "-//IETF//DTD HTML Level 0//EN" */, PubIDInfo::eQuirks, PubIDInfo::eQuirks},
  {"-//ietf//dtd html level 0//en//2.0" /* "-//IETF//DTD HTML Level 0//EN//2.0" */, PubIDInfo::eQuirks, PubIDInfo::eQuirks},
  {"-//ietf//dtd html level 1//en" /* "-//IETF//DTD HTML Level 1//EN" */, PubIDInfo::eQuirks, PubIDInfo::eQuirks},
  {"-//ietf//dtd html level 1//en//2.0" /* "-//IETF//DTD HTML Level 1//EN//2.0" */, PubIDInfo::eQuirks, PubIDInfo::eQuirks},
  {"-//ietf//dtd html level 2//en" /* "-//IETF//DTD HTML Level 2//EN" */, PubIDInfo::eQuirks, PubIDInfo::eQuirks},
  {"-//ietf//dtd html level 2//en//2.0" /* "-//IETF//DTD HTML Level 2//EN//2.0" */, PubIDInfo::eQuirks, PubIDInfo::eQuirks},
  {"-//ietf//dtd html level 3//en" /* "-//IETF//DTD HTML Level 3//EN" */, PubIDInfo::eQuirks, PubIDInfo::eQuirks},
  {"-//ietf//dtd html level 3//en//3.0" /* "-//IETF//DTD HTML Level 3//EN//3.0" */, PubIDInfo::eQuirks, PubIDInfo::eQuirks},
  {"-//ietf//dtd html strict level 0//en" /* "-//IETF//DTD HTML Strict Level 0//EN" */, PubIDInfo::eQuirks, PubIDInfo::eQuirks},
  {"-//ietf//dtd html strict level 0//en//2.0" /* "-//IETF//DTD HTML Strict Level 0//EN//2.0" */, PubIDInfo::eQuirks, PubIDInfo::eQuirks},
  {"-//ietf//dtd html strict level 1//en" /* "-//IETF//DTD HTML Strict Level 1//EN" */, PubIDInfo::eQuirks, PubIDInfo::eQuirks},
  {"-//ietf//dtd html strict level 1//en//2.0" /* "-//IETF//DTD HTML Strict Level 1//EN//2.0" */, PubIDInfo::eQuirks, PubIDInfo::eQuirks},
  {"-//ietf//dtd html strict level 2//en" /* "-//IETF//DTD HTML Strict Level 2//EN" */, PubIDInfo::eQuirks, PubIDInfo::eQuirks},
  {"-//ietf//dtd html strict level 2//en//2.0" /* "-//IETF//DTD HTML Strict Level 2//EN//2.0" */, PubIDInfo::eQuirks, PubIDInfo::eQuirks},
  {"-//ietf//dtd html strict level 3//en" /* "-//IETF//DTD HTML Strict Level 3//EN" */, PubIDInfo::eQuirks, PubIDInfo::eQuirks},
  {"-//ietf//dtd html strict level 3//en//3.0" /* "-//IETF//DTD HTML Strict Level 3//EN//3.0" */, PubIDInfo::eQuirks, PubIDInfo::eQuirks},
  {"-//ietf//dtd html strict//en" /* "-//IETF//DTD HTML Strict//EN" */, PubIDInfo::eQuirks, PubIDInfo::eQuirks},
  {"-//ietf//dtd html strict//en//2.0" /* "-//IETF//DTD HTML Strict//EN//2.0" */, PubIDInfo::eQuirks, PubIDInfo::eQuirks},
  {"-//ietf//dtd html strict//en//3.0" /* "-//IETF//DTD HTML Strict//EN//3.0" */, PubIDInfo::eQuirks, PubIDInfo::eQuirks},
  {"-//ietf//dtd html//en" /* "-//IETF//DTD HTML//EN" */, PubIDInfo::eQuirks, PubIDInfo::eQuirks},
  {"-//ietf//dtd html//en//2.0" /* "-//IETF//DTD HTML//EN//2.0" */, PubIDInfo::eQuirks, PubIDInfo::eQuirks},
  {"-//ietf//dtd html//en//3.0" /* "-//IETF//DTD HTML//EN//3.0" */, PubIDInfo::eQuirks, PubIDInfo::eQuirks},
  {"-//metrius//dtd metrius presentational//en" /* "-//Metrius//DTD Metrius Presentational//EN" */, PubIDInfo::eQuirks, PubIDInfo::eQuirks},
  {"-//microsoft//dtd internet explorer 2.0 html strict//en" /* "-//Microsoft//DTD Internet Explorer 2.0 HTML Strict//EN" */, PubIDInfo::eQuirks, PubIDInfo::eQuirks},
  {"-//microsoft//dtd internet explorer 2.0 html//en" /* "-//Microsoft//DTD Internet Explorer 2.0 HTML//EN" */, PubIDInfo::eQuirks, PubIDInfo::eQuirks},
  {"-//microsoft//dtd internet explorer 2.0 tables//en" /* "-//Microsoft//DTD Internet Explorer 2.0 Tables//EN" */, PubIDInfo::eQuirks, PubIDInfo::eQuirks},
  {"-//microsoft//dtd internet explorer 3.0 html strict//en" /* "-//Microsoft//DTD Internet Explorer 3.0 HTML Strict//EN" */, PubIDInfo::eQuirks, PubIDInfo::eQuirks},
  {"-//microsoft//dtd internet explorer 3.0 html//en" /* "-//Microsoft//DTD Internet Explorer 3.0 HTML//EN" */, PubIDInfo::eQuirks, PubIDInfo::eQuirks},
  {"-//microsoft//dtd internet explorer 3.0 tables//en" /* "-//Microsoft//DTD Internet Explorer 3.0 Tables//EN" */, PubIDInfo::eQuirks, PubIDInfo::eQuirks},
  {"-//netscape comm. corp.//dtd html//en" /* "-//Netscape Comm. Corp.//DTD HTML//EN" */, PubIDInfo::eQuirks, PubIDInfo::eQuirks},
  {"-//netscape comm. corp.//dtd strict html//en" /* "-//Netscape Comm. Corp.//DTD Strict HTML//EN" */, PubIDInfo::eQuirks, PubIDInfo::eQuirks},
  {"-//o'reilly and associates//dtd html 2.0//en" /* "-//O'Reilly and Associates//DTD HTML 2.0//EN" */, PubIDInfo::eQuirks, PubIDInfo::eQuirks},
  {"-//o'reilly and associates//dtd html extended 1.0//en" /* "-//O'Reilly and Associates//DTD HTML Extended 1.0//EN" */, PubIDInfo::eQuirks, PubIDInfo::eQuirks},
  {"-//o'reilly and associates//dtd html extended relaxed 1.0//en" /* "-//O'Reilly and Associates//DTD HTML Extended Relaxed 1.0//EN" */, PubIDInfo::eQuirks, PubIDInfo::eQuirks},
  {"-//softquad software//dtd hotmetal pro 6.0::19990601::extensions to html 4.0//en" /* "-//SoftQuad Software//DTD HoTMetaL PRO 6.0::19990601::extensions to HTML 4.0//EN" */, PubIDInfo::eQuirks, PubIDInfo::eQuirks},
  {"-//softquad//dtd hotmetal pro 4.0::19971010::extensions to html 4.0//en" /* "-//SoftQuad//DTD HoTMetaL PRO 4.0::19971010::extensions to HTML 4.0//EN" */, PubIDInfo::eQuirks, PubIDInfo::eQuirks},
  {"-//spyglass//dtd html 2.0 extended//en" /* "-//Spyglass//DTD HTML 2.0 Extended//EN" */, PubIDInfo::eQuirks, PubIDInfo::eQuirks},
  {"-//sq//dtd html 2.0 hotmetal + extensions//en" /* "-//SQ//DTD HTML 2.0 HoTMetaL + extensions//EN" */, PubIDInfo::eQuirks, PubIDInfo::eQuirks},
  {"-//sun microsystems corp.//dtd hotjava html//en" /* "-//Sun Microsystems Corp.//DTD HotJava HTML//EN" */, PubIDInfo::eQuirks, PubIDInfo::eQuirks},
  {"-//sun microsystems corp.//dtd hotjava strict html//en" /* "-//Sun Microsystems Corp.//DTD HotJava Strict HTML//EN" */, PubIDInfo::eQuirks, PubIDInfo::eQuirks},
  {"-//w3c//dtd html 3 1995-03-24//en" /* "-//W3C//DTD HTML 3 1995-03-24//EN" */, PubIDInfo::eQuirks, PubIDInfo::eQuirks},
  {"-//w3c//dtd html 3.2 draft//en" /* "-//W3C//DTD HTML 3.2 Draft//EN" */, PubIDInfo::eQuirks, PubIDInfo::eQuirks},
  {"-//w3c//dtd html 3.2 final//en" /* "-//W3C//DTD HTML 3.2 Final//EN" */, PubIDInfo::eQuirks, PubIDInfo::eQuirks},
  {"-//w3c//dtd html 3.2//en" /* "-//W3C//DTD HTML 3.2//EN" */, PubIDInfo::eQuirks, PubIDInfo::eQuirks},
  {"-//w3c//dtd html 3.2s draft//en" /* "-//W3C//DTD HTML 3.2S Draft//EN" */, PubIDInfo::eQuirks, PubIDInfo::eQuirks},
  {"-//w3c//dtd html 4.0 frameset//en" /* "-//W3C//DTD HTML 4.0 Frameset//EN" */, PubIDInfo::eQuirks, PubIDInfo::eQuirks},
  {"-//w3c//dtd html 4.0 transitional//en" /* "-//W3C//DTD HTML 4.0 Transitional//EN" */, PubIDInfo::eQuirks, PubIDInfo::eQuirks},
  {"-//w3c//dtd html 4.01 frameset//en" /* "-//W3C//DTD HTML 4.01 Frameset//EN" */, PubIDInfo::eQuirks, PubIDInfo::eAlmostStandards},
  {"-//w3c//dtd html 4.01 transitional//en" /* "-//W3C//DTD HTML 4.01 Transitional//EN" */, PubIDInfo::eQuirks, PubIDInfo::eAlmostStandards},
  {"-//w3c//dtd html experimental 19960712//en" /* "-//W3C//DTD HTML Experimental 19960712//EN" */, PubIDInfo::eQuirks, PubIDInfo::eQuirks},
  {"-//w3c//dtd html experimental 970421//en" /* "-//W3C//DTD HTML Experimental 970421//EN" */, PubIDInfo::eQuirks, PubIDInfo::eQuirks},
  {"-//w3c//dtd w3 html//en" /* "-//W3C//DTD W3 HTML//EN" */, PubIDInfo::eQuirks, PubIDInfo::eQuirks},
  {"-//w3c//dtd xhtml 1.0 frameset//en" /* "-//W3C//DTD XHTML 1.0 Frameset//EN" */, PubIDInfo::eAlmostStandards, PubIDInfo::eAlmostStandards},
  {"-//w3c//dtd xhtml 1.0 transitional//en" /* "-//W3C//DTD XHTML 1.0 Transitional//EN" */, PubIDInfo::eAlmostStandards, PubIDInfo::eAlmostStandards},
  {"-//w3o//dtd w3 html 3.0//en" /* "-//W3O//DTD W3 HTML 3.0//EN" */, PubIDInfo::eQuirks, PubIDInfo::eQuirks},
  {"-//w3o//dtd w3 html 3.0//en//" /* "-//W3O//DTD W3 HTML 3.0//EN//" */, PubIDInfo::eQuirks, PubIDInfo::eQuirks},
  {"-//w3o//dtd w3 html strict 3.0//en//" /* "-//W3O//DTD W3 HTML Strict 3.0//EN//" */, PubIDInfo::eQuirks, PubIDInfo::eQuirks},
  {"-//webtechs//dtd mozilla html 2.0//en" /* "-//WebTechs//DTD Mozilla HTML 2.0//EN" */, PubIDInfo::eQuirks, PubIDInfo::eQuirks},
  {"-//webtechs//dtd mozilla html//en" /* "-//WebTechs//DTD Mozilla HTML//EN" */, PubIDInfo::eQuirks, PubIDInfo::eQuirks},
  {"-/w3c/dtd html 4.0 transitional/en" /* "-/W3C/DTD HTML 4.0 Transitional/EN" */, PubIDInfo::eQuirks, PubIDInfo::eQuirks},
  {"html" /* "HTML" */, PubIDInfo::eQuirks, PubIDInfo::eQuirks},
};

#ifdef DEBUG
static void
VerifyPublicIDs()
{
  static bool gVerified = false;
  if (!gVerified) {
    gVerified = PR_TRUE;
    PRUint32 i;
    for (i = 0; i < ELEMENTS_OF(kPublicIDs) - 1; ++i) {
      if (nsCRT::strcmp(kPublicIDs[i].name, kPublicIDs[i+1].name) >= 0) {
        NS_NOTREACHED("doctypes out of order");
        printf("Doctypes %s and %s out of order.\n",
               kPublicIDs[i].name, kPublicIDs[i+1].name);
      }
    }
    for (i = 0; i < ELEMENTS_OF(kPublicIDs); ++i) {
      nsCAutoString lcPubID(kPublicIDs[i].name);
      ToLowerCase(lcPubID);
      if (nsCRT::strcmp(kPublicIDs[i].name, lcPubID.get()) != 0) {
        NS_NOTREACHED("doctype not lower case");
        printf("Doctype %s not lower case.\n", kPublicIDs[i].name);
      }
    }
  }
}
#endif

static void
DetermineHTMLParseMode(const nsString& aBuffer,
                       nsDTDMode& aParseMode,
                       eParserDocType& aDocType)
{
#ifdef DEBUG
  VerifyPublicIDs();
#endif
  PRInt32 resultFlags;
  nsAutoString publicIDUCS2, sysIDUCS2;
  if (ParseDocTypeDecl(aBuffer, &resultFlags, publicIDUCS2, sysIDUCS2)) {
    if (!(resultFlags & PARSE_DTD_HAVE_DOCTYPE)) {
      // no DOCTYPE
      aParseMode = eDTDMode_quirks;
      aDocType = eHTML_Quirks;
    } else if ((resultFlags & PARSE_DTD_HAVE_INTERNAL_SUBSET) ||
               !(resultFlags & PARSE_DTD_HAVE_PUBLIC_ID)) {
      // A doctype with an internal subset is always full_standards.
      // A doctype without a public ID is always full_standards.
      aDocType = eHTML_Strict;
      aParseMode = eDTDMode_full_standards;

      // Special hack for IBM's custom DOCTYPE.
      if (!(resultFlags & PARSE_DTD_HAVE_INTERNAL_SUBSET) &&
          sysIDUCS2 == NS_LITERAL_STRING(
               "http://www.ibm.com/data/dtd/v11/ibmxhtml1-transitional.dtd")) {
        aParseMode = eDTDMode_quirks;
        aDocType = eHTML_Quirks;
      }

    } else {
      // We have to check our list of public IDs to see what to do.
      // Yes, we want UCS2 to ASCII lossy conversion.
      nsCAutoString publicID;
      publicID.AssignWithConversion(publicIDUCS2);

      // See comment above definition of kPublicIDs about case
      // sensitivity.
      ToLowerCase(publicID);

      // Binary search to see if we can find the correct public ID
      // These must be signed since maximum can go below zero and we'll
      // crash if it's unsigned.
      PRInt32 minimum = 0;
      PRInt32 maximum = ELEMENTS_OF(kPublicIDs) - 1;
      PRInt32 index;
      for (;;) {
        index = (minimum + maximum) / 2;
        PRInt32 comparison =
            nsCRT::strcmp(publicID.get(), kPublicIDs[index].name);
        if (comparison == 0)
          break;
        if (comparison < 0)
          maximum = index - 1;
        else
          minimum = index + 1;

        if (maximum < minimum) {
          // The DOCTYPE is not in our list, so it must be full_standards.
          aParseMode = eDTDMode_full_standards;
          aDocType = eHTML_Strict;
          return;
        }
      }

      switch ((resultFlags & PARSE_DTD_HAVE_SYSTEM_ID)
                ? kPublicIDs[index].mode_if_sysid
                : kPublicIDs[index].mode_if_no_sysid)
      {
        case PubIDInfo::eQuirks:
          aParseMode = eDTDMode_quirks;
          aDocType = eHTML_Quirks;
          break;
        case PubIDInfo::eAlmostStandards:
          aParseMode = eDTDMode_almost_standards;
          aDocType = eHTML_Strict;
          break;
        case PubIDInfo::eFullStandards:
          aParseMode = eDTDMode_full_standards;
          aDocType = eHTML_Strict;
          break;
        default:
          NS_NOTREACHED("no other cases!");
      }
    }
  } else {
    // badly formed DOCTYPE -> quirks
    aParseMode = eDTDMode_quirks;
    aDocType = eHTML_Quirks;
  }
}

static void
DetermineParseMode(const nsString& aBuffer, nsDTDMode& aParseMode,
                   eParserDocType& aDocType, const nsACString& aMimeType)
{
  if (aMimeType.EqualsLiteral(TEXT_HTML)) {
    DetermineHTMLParseMode(aBuffer, aParseMode, aDocType);
  } else if (aMimeType.EqualsLiteral(TEXT_PLAIN) ||
             aMimeType.EqualsLiteral(TEXT_CSS) ||
             aMimeType.EqualsLiteral(APPLICATION_JAVASCRIPT) ||
             aMimeType.EqualsLiteral(APPLICATION_XJAVASCRIPT) ||
             aMimeType.EqualsLiteral(APPLICATION_JSON) ||
             aMimeType.EqualsLiteral(TEXT_ECMASCRIPT) ||
             aMimeType.EqualsLiteral(APPLICATION_ECMASCRIPT) ||
             aMimeType.EqualsLiteral(TEXT_JAVASCRIPT)) {
    aDocType = ePlainText;
    aParseMode = eDTDMode_quirks;
  } else { // Some form of XML
    aDocType = eXML;
    aParseMode = eDTDMode_full_standards;
  }
}

static nsIDTD*
FindSuitableDTD(CParserContext& aParserContext)
{
  // We always find a DTD.
  aParserContext.mAutoDetectStatus = ePrimaryDetect;

  // Quick check for view source.
  if (aParserContext.mParserCommand == eViewSource) {
    return new CViewSourceHTML();
  }

  // Now see if we're parsing HTML (which, as far as we're concerned, simply
  // means "not XML").
  if (aParserContext.mDocType != eXML) {
    return new CNavDTD();
  }

  // If we're here, then we'd better be parsing XML.
  NS_ASSERTION(aParserContext.mDocType == eXML, "What are you trying to send me, here?");
  return new nsExpatDriver();
}

NS_IMETHODIMP
nsParser::CancelParsingEvents()
{
  if (mFlags & NS_PARSER_FLAG_PENDING_CONTINUE_EVENT) {
    NS_ASSERTION(mContinueEvent, "mContinueEvent is null");
    // Revoke the pending continue parsing event
    mContinueEvent = nsnull;
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
#define PREFER_LATTER_ERROR_CODE(EXPR1, EXPR2, RV) {                          \
  nsresult RV##__temp = EXPR1;                                                \
  RV = EXPR2;                                                                 \
  if (NS_FAILED(RV)) {                                                        \
    RV = RV##__temp;                                                          \
  }                                                                           \
}

/**
 * This gets called just prior to the model actually
 * being constructed. It's important to make this the
 * last thing that happens right before parsing, so we
 * can delay until the last moment the resolution of
 * which DTD to use (unless of course we're assigned one).
 */
nsresult
nsParser::WillBuildModel(nsString& aFilename)
{
  if (!mParserContext)
    return kInvalidParserContext;

  if (eUnknownDetect != mParserContext->mAutoDetectStatus)
    return NS_OK;

  if (eDTDMode_unknown == mParserContext->mDTDMode ||
      eDTDMode_autodetect == mParserContext->mDTDMode) {
    PRUnichar buf[1025];
    nsFixedString theBuffer(buf, 1024, 0);

    // Grab 1024 characters, starting at the first non-whitespace
    // character, to look for the doctype in.
    mParserContext->mScanner->Peek(theBuffer, 1024, mParserContext->mScanner->FirstNonWhitespacePosition());
    DetermineParseMode(theBuffer, mParserContext->mDTDMode,
                       mParserContext->mDocType, mParserContext->mMimeType);
  }

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
nsresult
nsParser::DidBuildModel(nsresult anErrorCode)
{
  nsresult result = anErrorCode;

  if (IsComplete()) {
    if (mParserContext && !mParserContext->mPrevContext) {
      // Let sink know if we're about to end load because we've been terminated.
      // In that case we don't want it to run deferred scripts.
      bool terminated = mInternalState == NS_ERROR_HTMLPARSER_STOPPARSING;
      if (mDTD && mSink) {
        nsresult dtdResult =  mDTD->DidBuildModel(anErrorCode),
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

      //Ref. to bug 61462.
      mParserContext->mRequest = 0;

      if (mSpeculativeScriptThread) {
        mSpeculativeScriptThread->Terminate();
        mSpeculativeScriptThread = nsnull;
      }
    }
  }

  return result;
}

void
nsParser::SpeculativelyParse()
{
  if (mParserContext->mParserCommand == eViewNormal &&
      !mParserContext->mMimeType.EqualsLiteral("text/html")) {
    return;
  }

  if (!mSpeculativeScriptThread) {
    mSpeculativeScriptThread = new nsSpeculativeScriptThread();
    if (!mSpeculativeScriptThread) {
      return;
    }
  }

  nsresult rv = mSpeculativeScriptThread->StartParsing(this);
  if (NS_FAILED(rv)) {
    mSpeculativeScriptThread = nsnull;
  }
}

/**
 * This method adds a new parser context to the list,
 * pushing the current one to the next position.
 *
 * @param   ptr to new context
 */
void
nsParser::PushContext(CParserContext& aContext)
{
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
CParserContext*
nsParser::PopContext()
{
  CParserContext* oldContext = mParserContext;
  if (oldContext) {
    mParserContext = oldContext->mPrevContext;
    if (mParserContext) {
      // If the old context was blocked, propagate the blocked state
      // back to the new one. Also, propagate the stream listener state
      // but don't override onStop state to guarantee the call to DidBuildModel().
      if (mParserContext->mStreamListenerState != eOnStop) {
        mParserContext->mStreamListenerState = oldContext->mStreamListenerState;
      }
      // Update the current context's tokenizer to any information gleaned
      // while parsing document.write() calls (such as "a plaintext tag was
      // found")
      if (mParserContext->mTokenizer) {
        mParserContext->mTokenizer->CopyState(oldContext->mTokenizer);
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
void
nsParser::SetUnusedInput(nsString& aBuffer)
{
  mUnusedInput = aBuffer;
}

NS_IMETHODIMP_(void *)
nsParser::GetRootContextKey()
{
  CParserContext* pc = mParserContext;
  if (!pc) {
    return nsnull;
  }

  while (pc->mPrevContext) {
    pc = pc->mPrevContext;
  }

  return pc->mKey;
}

/**
 *  Call this when you want to *force* the parser to terminate the
 *  parsing process altogether. This is binary -- so once you terminate
 *  you can't resume without restarting altogether.
 */
NS_IMETHODIMP
nsParser::Terminate(void)
{
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
  // The IsComplete() call inside of DidBuildModel looks at the pendingContinueEvents flag.
  CancelParsingEvents();
  if (mSpeculativeScriptThread) {
    mSpeculativeScriptThread->Terminate();
    mSpeculativeScriptThread = nsnull;
  }

  // If we got interrupted in the middle of a document.write, then we might
  // have more than one parser context on our parsercontext stack. This has
  // the effect of making DidBuildModel a no-op, meaning that we never call
  // our sink's DidBuildModel and break the reference cycle, causing a leak.
  // Since we're getting terminated, we manually clean up our context stack.
  while (mParserContext && mParserContext->mPrevContext) {
    CParserContext *prev = mParserContext->mPrevContext;
    delete mParserContext;
    mParserContext = prev;
  }

  if (mDTD) {
    mDTD->Terminate();
    DidBuildModel(result);
  } else if (mSink) {
    // We have no parser context or no DTD yet (so we got terminated before we
    // got any data).  Manually break the reference cycle with the sink.
    result = mSink->DidBuildModel(PR_TRUE);
    NS_ENSURE_SUCCESS(result, result);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsParser::ContinueInterruptedParsing()
{
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
  nsresult result=NS_OK;
  nsCOMPtr<nsIParser> kungFuDeathGrip(this);

#ifdef DEBUG
  if (!(mFlags & NS_PARSER_FLAG_PARSER_ENABLED)) {
    NS_WARNING("Don't call ContinueInterruptedParsing on a blocked parser.");
  }
#endif

  if (mSpeculativeScriptThread) {
    mSpeculativeScriptThread->StopParsing(PR_FALSE);
  }

  bool isFinalChunk = mParserContext &&
                        mParserContext->mStreamListenerState == eOnStop;

  mProcessingNetworkData = PR_TRUE;
  if (mSink) {
    mSink->WillParse();
  }
  result = ResumeParse(PR_TRUE, isFinalChunk); // Ref. bug 57999
  mProcessingNetworkData = PR_FALSE;

  if (result != NS_OK) {
    result=mInternalState;
  }

  return result;
}

/**
 *  Stops parsing temporarily. That's it will prevent the
 *  parser from building up content model.
 */
NS_IMETHODIMP_(void)
nsParser::BlockParser()
{
  mFlags &= ~NS_PARSER_FLAG_PARSER_ENABLED;
}

/**
 *  Open up the parser for tokenization, building up content
 *  model..etc. However, this method does not resume parsing
 *  automatically. It's the callers' responsibility to restart
 *  the parsing engine.
 */
NS_IMETHODIMP_(void)
nsParser::UnblockParser()
{
  if (!(mFlags & NS_PARSER_FLAG_PARSER_ENABLED)) {
    mFlags |= NS_PARSER_FLAG_PARSER_ENABLED;
  } else {
    NS_WARNING("Trying to unblock an unblocked parser.");
  }
}

/**
 * Call this to query whether the parser is enabled or not.
 */
NS_IMETHODIMP_(bool)
nsParser::IsParserEnabled()
{
  return (mFlags & NS_PARSER_FLAG_PARSER_ENABLED) != 0;
}

/**
 * Call this to query whether the parser thinks it's done with parsing.
 */
NS_IMETHODIMP_(bool)
nsParser::IsComplete()
{
  return !(mFlags & NS_PARSER_FLAG_PENDING_CONTINUE_EVENT);
}


void nsParser::HandleParserContinueEvent(nsParserContinueEvent *ev)
{
  // Ignore any revoked continue events...
  if (mContinueEvent != ev)
    return;

  mFlags &= ~NS_PARSER_FLAG_PENDING_CONTINUE_EVENT;
  mContinueEvent = nsnull;

  NS_ASSERTION(IsOkToProcessNetworkData(),
               "Interrupted in the middle of a script?");
  ContinueInterruptedParsing();
}

bool
nsParser::CanInterrupt()
{
  return (mFlags & NS_PARSER_FLAG_CAN_INTERRUPT) != 0;
}

bool
nsParser::IsInsertionPointDefined()
{
  return PR_TRUE;
}

void
nsParser::BeginEvaluatingParserInsertedScript()
{
}

void
nsParser::EndEvaluatingParserInsertedScript()
{
}

void
nsParser::MarkAsNotScriptCreated()
{
}

bool
nsParser::IsScriptCreated()
{
  return PR_FALSE;
}

void
nsParser::SetCanInterrupt(bool aCanInterrupt)
{
  if (aCanInterrupt) {
    mFlags |= NS_PARSER_FLAG_CAN_INTERRUPT;
  } else {
    mFlags &= ~NS_PARSER_FLAG_CAN_INTERRUPT;
  }
}

/**
 *  This is the main controlling routine in the parsing process.
 *  Note that it may get called multiple times for the same scanner,
 *  since this is a pushed based system, and all the tokens may
 *  not have been consumed by the scanner during a given invocation
 *  of this method.
 */
NS_IMETHODIMP
nsParser::Parse(nsIURI* aURL,
                nsIRequestObserver* aListener,
                void* aKey,
                nsDTDMode aMode)
{

  NS_PRECONDITION(aURL, "Error: Null URL given");
  NS_ASSERTION(!mSpeculativeScriptThread, "Can't reuse a parser like this");

  nsresult result=kBadURL;
  mObserver = aListener;

  if (aURL) {
    nsCAutoString spec;
    nsresult rv = aURL->GetSpec(spec);
    if (rv != NS_OK) {
      return rv;
    }
    NS_ConvertUTF8toUTF16 theName(spec);

    nsScanner* theScanner = new nsScanner(theName, PR_FALSE, mCharset,
                                          mCharsetSource);
    CParserContext* pc = new CParserContext(mParserContext, theScanner, aKey,
                                            mCommand, aListener);
    if (pc && theScanner) {
      pc->mMultipart = PR_TRUE;
      pc->mContextType = CParserContext::eCTURL;
      pc->mDTDMode = aMode;
      PushContext(*pc);

      result = NS_OK;
    } else {
      result = mInternalState = NS_ERROR_HTMLPARSER_BADCONTEXT;
    }
  }
  return result;
}

/**
 * Call this method if all you want to do is parse 1 string full of HTML text.
 * In particular, this method should be called by the DOM when it has an HTML
 * string to feed to the parser in real-time.
 *
 * @param   aSourceBuffer contains a string-full of real content
 * @param   aMimeType tells us what type of content to expect in the given string
 */
NS_IMETHODIMP
nsParser::Parse(const nsAString& aSourceBuffer,
                void* aKey,
                const nsACString& aMimeType,
                bool aLastCall,
                nsDTDMode aMode)
{
  nsresult result = NS_OK;

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

  if (mSpeculativeScriptThread) {
    mSpeculativeScriptThread->StopParsing(PR_TRUE);
  }

  // Hack to pass on to the dtd the caller's desire to
  // parse a fragment without worrying about containment rules
  if (aMode == eDTDMode_fragment)
    mCommand = eViewFragment;

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
      nsScanner* theScanner = new nsScanner(mUnusedInput, mCharset, mCharsetSource);
      NS_ENSURE_TRUE(theScanner, NS_ERROR_OUT_OF_MEMORY);

      eAutoDetectResult theStatus = eUnknownDetect;

      if (mParserContext && mParserContext->mMimeType == aMimeType) {
        // Ref. Bug 90379
        NS_ASSERTION(mDTD, "How come the DTD is null?");

        if (mParserContext) {
          theStatus = mParserContext->mAutoDetectStatus;
          // Added this to fix bug 32022.
        }
      }

      pc = new CParserContext(mParserContext, theScanner, aKey, mCommand,
                              0, theStatus, aLastCall);
      NS_ENSURE_TRUE(pc, NS_ERROR_OUT_OF_MEMORY);

      PushContext(*pc);

      pc->mMultipart = !aLastCall; // By default
      if (pc->mPrevContext) {
        pc->mMultipart |= pc->mPrevContext->mMultipart;
      }

      // Start fix bug 40143
      if (pc->mMultipart) {
        pc->mStreamListenerState = eOnDataAvail;
        if (pc->mScanner) {
          pc->mScanner->SetIncremental(PR_TRUE);
        }
      } else {
        pc->mStreamListenerState = eOnStop;
        if (pc->mScanner) {
          pc->mScanner->SetIncremental(PR_FALSE);
        }
      }
      // end fix for 40143

      pc->mContextType=CParserContext::eCTString;
      pc->SetMimeType(aMimeType);
      if (pc->mPrevContext && aMode == eDTDMode_autodetect) {
        // Preserve the DTD mode from the last context, bug 265814.
        pc->mDTDMode = pc->mPrevContext->mDTDMode;
      } else {
        pc->mDTDMode = aMode;
      }

      mUnusedInput.Truncate();

      pc->mScanner->Append(aSourceBuffer);
      // Do not interrupt document.write() - bug 95487
      result = ResumeParse(PR_FALSE, PR_FALSE, PR_FALSE);
    } else {
      pc->mScanner->Append(aSourceBuffer);
      if (!pc->mPrevContext) {
        // Set stream listener state to eOnStop, on the final context - Fix 68160,
        // to guarantee DidBuildModel() call - Fix 36148
        if (aLastCall) {
          pc->mStreamListenerState = eOnStop;
          pc->mScanner->SetIncremental(PR_FALSE);
        }

        if (pc == mParserContext) {
          // If pc is not mParserContext, then this call to ResumeParse would
          // do the wrong thing and try to continue parsing using
          // mParserContext. We need to wait to actually resume parsing on pc.
          ResumeParse(PR_FALSE, PR_FALSE, PR_FALSE);
        }
      }
    }
  }

  return result;
}

NS_IMETHODIMP
nsParser::ParseFragment(const nsAString& aSourceBuffer,
                        nsTArray<nsString>& aTagStack)
{
  nsresult result = NS_OK;
  nsAutoString  theContext;
  PRUint32 theCount = aTagStack.Length();
  PRUint32 theIndex = 0;

  // Disable observers for fragments
  mFlags &= ~NS_PARSER_FLAG_OBSERVERS_ENABLED;

  NS_ASSERTION(!mSpeculativeScriptThread, "Can't reuse a parser like this");

  for (theIndex = 0; theIndex < theCount; theIndex++) {
    theContext.AppendLiteral("<");
    theContext.Append(aTagStack[theCount - theIndex - 1]);
    theContext.AppendLiteral(">");
  }

  if (theCount == 0) {
    // Ensure that the buffer is not empty. Because none of the DTDs care
    // about leading whitespace, this doesn't change the result.
    theContext.AssignLiteral(" ");
  }

  // First, parse the context to build up the DTD's tag stack. Note that we
  // pass PR_FALSE for the aLastCall parameter.
  result = Parse(theContext,
                 (void*)&theContext,
                 NS_LITERAL_CSTRING("application/xml"),
                 PR_FALSE,
                 eDTDMode_full_standards);
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
    result = Parse(aSourceBuffer,
                   &theContext,
                   NS_LITERAL_CSTRING("application/xml"),
                   PR_TRUE,
                   eDTDMode_full_standards);
    fragSink->DidBuildContent();
  } else {
    // Add an end tag chunk, so expat will read the whole source buffer,
    // and not worry about ']]' etc.
    result = Parse(aSourceBuffer + NS_LITERAL_STRING("</"),
                   &theContext,
                   NS_LITERAL_CSTRING("application/xml"),
                   PR_FALSE,
                   eDTDMode_full_standards);
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
        PRInt32 endOfTag = thisTag.FindChar(PRUnichar(' '));
        if (endOfTag == -1) {
          endContext.Append(thisTag);
        } else {
          endContext.Append(Substring(thisTag,0,endOfTag));
        }

        endContext.AppendLiteral(">");
      }

      result = Parse(endContext,
                     &theContext,
                     NS_LITERAL_CSTRING("application/xml"),
                     PR_TRUE,
                     eDTDMode_full_standards);
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
nsresult
nsParser::ResumeParse(bool allowIteration, bool aIsFinalChunk,
                      bool aCanInterrupt)
{
  nsresult result = NS_OK;

  if ((mFlags & NS_PARSER_FLAG_PARSER_ENABLED) &&
      mInternalState != NS_ERROR_HTMLPARSER_STOPPARSING) {
    NS_ASSERTION(!mSpeculativeScriptThread || !mSpeculativeScriptThread->Parsing(),
                 "Bad races happening, expect to crash!");

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
        SetCanInterrupt(aCanInterrupt);
        nsresult theTokenizerResult = (mFlags & NS_PARSER_FLAG_CAN_TOKENIZE)
                                      ? Tokenize(aIsFinalChunk)
                                      : NS_OK;
        result = BuildModel();

        if (result == NS_ERROR_HTMLPARSER_INTERRUPTED && aIsFinalChunk) {
          PostContinueEvent();
        }
        SetCanInterrupt(PR_FALSE);

        theIterationIsOk = theTokenizerResult != kEOF &&
                           result != NS_ERROR_HTMLPARSER_INTERRUPTED;

        // Make sure not to stop parsing too early. Therefore, before shutting
        // down the parser, it's important to check whether the input buffer
        // has been scanned to completion (theTokenizerResult should be kEOF).
        // kEOF -> End of buffer.

        // If we're told to block the parser, we disable all further parsing
        // (and cache any data coming in) until the parser is re-enabled.
        if (NS_ERROR_HTMLPARSER_BLOCK == result) {
          mSink->WillInterrupt();
          if (mFlags & NS_PARSER_FLAG_PARSER_ENABLED) {
            // If we were blocked by a recursive invocation, don't re-block.
            BlockParser();
            SpeculativelyParse();
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
        if ((NS_OK == result && theTokenizerResult == kEOF) ||
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
                  theContext->mScanner->CopyUnusedData(mUnusedInput);
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

        if (theTokenizerResult == kEOF ||
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
nsresult
nsParser::BuildModel()
{
  nsITokenizer* theTokenizer = nsnull;

  nsresult result = NS_OK;
  if (mParserContext) {
    result = mParserContext->GetTokenizer(mDTD, mSink, theTokenizer);
  }

  if (NS_SUCCEEDED(result)) {
    if (mDTD) {
      // XXXbenjamn CanInterrupt() and !inDocWrite appear to be covariant.
      bool inDocWrite = !!mParserContext->mPrevContext;
      result = mDTD->BuildModel(theTokenizer,
                                // ignore interruptions in document.write
                                CanInterrupt() && !inDocWrite,
                                !inDocWrite, // don't count lines in document.write
                                &mCharset);
    }
  } else {
    mInternalState = result = NS_ERROR_HTMLPARSER_BADTOKENIZER;
  }
  return result;
}

/*******************************************************************
  These methods are used to talk to the netlib system...
 *******************************************************************/

nsresult
nsParser::OnStartRequest(nsIRequest *request, nsISupports* aContext)
{
  NS_PRECONDITION(eNone == mParserContext->mStreamListenerState,
                  "Parser's nsIStreamListener API was not setup "
                  "correctly in constructor.");
  if (mObserver) {
    mObserver->OnStartRequest(request, aContext);
  }
  mParserContext->mStreamListenerState = eOnStart;
  mParserContext->mAutoDetectStatus = eUnknownDetect;
  mParserContext->mRequest = request;

  NS_ASSERTION(!mParserContext->mPrevContext,
               "Clobbering DTD for non-root parser context!");
  mDTD = nsnull;

  nsresult rv;
  nsCAutoString contentType;
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


#define UTF16_BOM "UTF-16"
#define UTF16_BE "UTF-16BE"
#define UTF16_LE "UTF-16LE"
#define UTF8 "UTF-8"

static inline bool IsSecondMarker(unsigned char aChar)
{
  switch (aChar) {
    case '!':
    case '?':
    case 'h':
    case 'H':
      return PR_TRUE;
    default:
      return PR_FALSE;
  }
}

static bool
DetectByteOrderMark(const unsigned char* aBytes, PRInt32 aLen,
                    nsCString& oCharset, PRInt32& oCharsetSource)
{
 oCharsetSource= kCharsetFromAutoDetection;
 oCharset.Truncate();
 // See http://www.w3.org/TR/2000/REC-xml-20001006#sec-guessing
 // for details
 // Also, MS Win2K notepad now generate 3 bytes BOM in UTF8 as UTF8 signature
 // We need to check that
 // UCS2 BOM FEFF = UTF8 EF BB BF
 switch(aBytes[0])
	 {
   case 0x00:
     if((0x3C==aBytes[1]) && (0x00==aBytes[2])) {
        // 00 3C 00
        if(IsSecondMarker(aBytes[3])) {
           // 00 3C 00 SM UTF-16,  big-endian, no Byte Order Mark 
           oCharset.Assign(UTF16_BE); 
           oCharsetSource = kCharsetFromByteOrderMark;
        } 
     }
   break;
   case 0x3C:
     if(0x00==aBytes[1] && (0x00==aBytes[3])) {
        // 3C 00 XX 00
        if(IsSecondMarker(aBytes[2])) {
           // 3C 00 SM 00 UTF-16,  little-endian, no Byte Order Mark 
           oCharset.Assign(UTF16_LE); 
           oCharsetSource = kCharsetFromByteOrderMark;
        } 
     // For html, meta tag detector is invoked before this so that we have 
     // to deal only with XML here.
     } else if(                     (0x3F==aBytes[1]) &&
               (0x78==aBytes[2]) && (0x6D==aBytes[3]) &&
               (0 == PL_strncmp("<?xml", (char*)aBytes, 5 ))) {
       // 3C 3F 78 6D
       // ASCII characters are in their normal positions, so we can safely
       // deal with the XML declaration in the old C way
       // The shortest string so far (strlen==5):
       // <?xml
       PRInt32 i;
       bool versionFound = false, encodingFound = false;
       for (i=6; i < aLen && !encodingFound; ++i) {
         // end of XML declaration?
         if ((((char*)aBytes)[i] == '?') && 
           ((i+1) < aLen) &&
           (((char*)aBytes)[i+1] == '>')) {
           break;
         }
         // Version is required.
         if (!versionFound) {
           // Want to avoid string comparisons, hence looking for 'n'
           // and only if found check the string leading to it. Not
           // foolproof, but fast.
           // The shortest string allowed before this is  (strlen==13):
           // <?xml version
           if ((((char*)aBytes)[i] == 'n') &&
             (i >= 12) && 
             (0 == PL_strncmp("versio", (char*)(aBytes+i-6), 6 ))) {
             // Fast forward through version
             char q = 0;
             for (++i; i < aLen; ++i) {
               char qi = ((char*)aBytes)[i];
               if (qi == '\'' || qi == '"') {
                 if (q && q == qi) {
                   //  ending quote
                   versionFound = PR_TRUE;
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
           if ((((char*)aBytes)[i] == 'g') &&
             (i >= 25) && 
             (0 == PL_strncmp("encodin", (char*)(aBytes+i-7), 7 ))) {
             PRInt32 encStart = 0;
             char q = 0;
             for (++i; i < aLen; ++i) {
               char qi = ((char*)aBytes)[i];
               if (qi == '\'' || qi == '"') {
                 if (q && q == qi) {
                   PRInt32 count = i - encStart;
                   // encoding value is invalid if it is UTF-16
                   if (count > 0 && 
                     (0 != PL_strcmp("UTF-16", (char*)(aBytes+encStart)))) {
                     oCharset.Assign((char*)(aBytes+encStart),count);
                     oCharsetSource = kCharsetFromMetaTag;
                   }
                   encodingFound = PR_TRUE;
                   break;
                 } else {
                   encStart = i+1;
                   q = qi;
                 }
               }
             }
           }
         } // if (!versionFound)
       } // for
     }
   break;
   case 0xEF:  
     if((0xBB==aBytes[1]) && (0xBF==aBytes[2])) {
        // EF BB BF
        // Win2K UTF-8 BOM
        oCharset.Assign(UTF8); 
        oCharsetSource= kCharsetFromByteOrderMark;
     }
   break;
   case 0xFE:
     if(0xFF==aBytes[1]) {
        // FE FF UTF-16, big-endian 
        oCharset.Assign(UTF16_BOM); 
        oCharsetSource= kCharsetFromByteOrderMark;
     }
   break;
   case 0xFF:
     if(0xFE==aBytes[1]) {
       // FF FE
       // UTF-16, little-endian 
       oCharset.Assign(UTF16_BOM); 
       oCharsetSource= kCharsetFromByteOrderMark;
     }
   break;
   // case 0x4C: if((0x6F==aBytes[1]) && ((0xA7==aBytes[2] && (0x94==aBytes[3])) {
   //   We do not care EBCIDIC here....
   // }
   // break;
 }  // switch
 return !oCharset.IsEmpty();
}

inline const char
GetNextChar(nsACString::const_iterator& aStart,
            nsACString::const_iterator& aEnd)
{
  NS_ASSERTION(aStart != aEnd, "end of buffer");
  return (++aStart != aEnd) ? *aStart : '\0';
}

bool
nsParser::DetectMetaTag(const char* aBytes,
                        PRInt32 aLen,
                        nsCString& aCharset,
                        PRInt32& aCharsetSource)
{
  aCharsetSource= kCharsetFromMetaTag;
  aCharset.SetLength(0);

  // XXX Only look inside HTML documents for now. For XML
  // documents we should be looking inside the XMLDecl.
  if (!mParserContext->mMimeType.EqualsLiteral(TEXT_HTML)) {
    return PR_FALSE;
  }

  // Fast and loose parsing to determine if we have a complete
  // META tag in this block, looking upto 2k into it.
  const nsASingleFragmentCString& str =
      Substring(aBytes, aBytes + NS_MIN(aLen, 2048));
  // XXXldb Should be const_char_iterator when FindInReadable supports it.
  nsACString::const_iterator begin, end;

  str.BeginReading(begin);
  str.EndReading(end);
  nsACString::const_iterator currPos(begin);
  nsACString::const_iterator tokEnd;
  nsACString::const_iterator tagEnd(begin);

  while (currPos != end) {
    if (!FindCharInReadable('<', currPos, end))
      break; // no tag found in this buffer

    if (GetNextChar(currPos, end) == '!') {
      if (GetNextChar(currPos, end) != '-' ||
          GetNextChar(currPos, end) != '-') {
        // If we only see a <! not followed by --, just skip to the next >.
        if (!FindCharInReadable('>', currPos, end)) {
          return PR_FALSE; // No more tags to follow.
        }

        // Continue searching for a meta tag following this "comment".
        ++currPos;
        continue;
      }

      // Found MDO ( <!-- ). Now search for MDC ( --[*s]> )
      bool foundMDC = false;
      bool foundMatch = false;
      while (!foundMDC) {
        if (GetNextChar(currPos, end) == '-' &&
            GetNextChar(currPos, end) == '-') {
          foundMatch = !foundMatch; // toggle until we've matching "--"
        } else if (currPos == end) {
          return PR_FALSE; // Couldn't find --[*s]> in this buffer
        } else if (foundMatch && *currPos == '>') {
          foundMDC = PR_TRUE; // found comment end delimiter.
          ++currPos;
        }
      }
      continue; // continue searching for META tag.
    }

    // Find the end of the tag, break if incomplete
    tagEnd = currPos;
    if (!FindCharInReadable('>', tagEnd, end))
      break;

    // If this is not a META tag, continue to next loop
    if ( (*currPos != 'm' && *currPos != 'M') ||
         (*(++currPos) != 'e' && *currPos != 'E') ||
         (*(++currPos) != 't' && *currPos != 'T') ||
         (*(++currPos) != 'a' && *currPos != 'A') ||
         !nsCRT::IsAsciiSpace(*(++currPos))) {
      currPos = tagEnd;
      continue;
    }

    // If could not find "charset" in this tag, skip this tag and try next
    tokEnd = tagEnd;
    if (!CaseInsensitiveFindInReadable(NS_LITERAL_CSTRING("CHARSET"),
                                       currPos, tokEnd)) {
      currPos = tagEnd;
      continue;
    }
    currPos = tokEnd;

    // skip spaces before '='
    while (*currPos == kSpace || *currPos == kNewLine ||
           *currPos == kCR || *currPos == kTab) {
      ++currPos;
    }
    // skip '='
    if (*currPos != '=') {
      currPos = tagEnd;
      continue;
    }
    ++currPos;
    // skip spaces after '='
    while (*currPos == kSpace || *currPos == kNewLine ||
           *currPos == kCR || *currPos == kTab) {
      ++currPos;
    }

    // skip open quote
    if (*currPos == '\'' || *currPos == '\"')
      ++currPos;

    // find the end of charset string
    tokEnd = currPos;
    while (*tokEnd != '\'' && *tokEnd != '\"' && tokEnd != tagEnd)
      ++tokEnd;

    // return true if we successfully got something for charset
    if (currPos != tokEnd) {
      aCharset.Assign(currPos.get(), tokEnd.get() - currPos.get());
      return PR_TRUE;
    }

    // Nothing specified as charset, continue next loop
    currPos = tagEnd;
  }

  return PR_FALSE;
}

typedef struct {
  bool mNeedCharsetCheck;
  nsParser* mParser;
  nsIParserFilter* mParserFilter;
  nsScanner* mScanner;
  nsIRequest* mRequest;
} ParserWriteStruct;

/*
 * This function is invoked as a result of a call to a stream's
 * ReadSegments() method. It is called for each contiguous buffer
 * of data in the underlying stream or pipe. Using ReadSegments
 * allows us to avoid copying data to read out of the stream.
 */
static NS_METHOD
ParserWriteFunc(nsIInputStream* in,
                void* closure,
                const char* fromRawSegment,
                PRUint32 toOffset,
                PRUint32 count,
                PRUint32 *writeCount)
{
  nsresult result;
  ParserWriteStruct* pws = static_cast<ParserWriteStruct*>(closure);
  const char* buf = fromRawSegment;
  PRUint32 theNumRead = count;

  if (!pws) {
    return NS_ERROR_FAILURE;
  }

  if (pws->mNeedCharsetCheck) {
    PRInt32 guessSource;
    nsCAutoString guess;
    nsCAutoString preferred;

    pws->mNeedCharsetCheck = PR_FALSE;
    if (pws->mParser->DetectMetaTag(buf, theNumRead, guess, guessSource) ||
        ((count >= 4) &&
         DetectByteOrderMark((const unsigned char*)buf,
                             theNumRead, guess, guessSource))) {
      nsCOMPtr<nsICharsetAlias> alias(do_GetService(NS_CHARSETALIAS_CONTRACTID));
      result = alias->GetPreferred(guess, preferred);
      // Only continue if it's a recognized charset and not
      // one of a designated set that we ignore.
      if (NS_SUCCEEDED(result) &&
          ((kCharsetFromByteOrderMark == guessSource) ||
           (!preferred.EqualsLiteral("UTF-16") &&
            !preferred.EqualsLiteral("UTF-16BE") &&
            !preferred.EqualsLiteral("UTF-16LE")))) {
        guess = preferred;
        pws->mParser->SetDocumentCharset(guess, guessSource);
        pws->mParser->SetSinkCharset(preferred);
        nsCOMPtr<nsICachingChannel> channel(do_QueryInterface(pws->mRequest));
        if (channel) {
          nsCOMPtr<nsISupports> cacheToken;
          channel->GetCacheToken(getter_AddRefs(cacheToken));
          if (cacheToken) {
            nsCOMPtr<nsICacheEntryDescriptor> cacheDescriptor(do_QueryInterface(cacheToken));
            if (cacheDescriptor) {
#ifdef DEBUG
              nsresult rv =
#endif
                cacheDescriptor->SetMetaDataElement("charset",
                                                    guess.get());
              NS_ASSERTION(NS_SUCCEEDED(rv),"cannot SetMetaDataElement");
            }
          }
        }
      }
    }
  }

  if (pws->mParserFilter)
    pws->mParserFilter->RawBuffer(buf, &theNumRead);

  result = pws->mScanner->Append(buf, theNumRead, pws->mRequest);
  if (NS_SUCCEEDED(result)) {
    *writeCount = count;
  }

  return result;
}

nsresult
nsParser::OnDataAvailable(nsIRequest *request, nsISupports* aContext,
                          nsIInputStream *pIStream, PRUint32 sourceOffset,
                          PRUint32 aLength)
{
  NS_PRECONDITION((eOnStart == mParserContext->mStreamListenerState ||
                   eOnDataAvail == mParserContext->mStreamListenerState),
            "Error: OnStartRequest() must be called before OnDataAvailable()");
  NS_PRECONDITION(NS_InputStreamIsBuffered(pIStream),
                  "Must have a buffered input stream");

  nsresult rv = NS_OK;

  CParserContext *theContext = mParserContext;

  while (theContext && theContext->mRequest != request) {
    theContext = theContext->mPrevContext;
  }

  if (theContext) {
    theContext->mStreamListenerState = eOnDataAvail;

    if ((mFlags & NS_PARSER_FLAG_PARSER_ENABLED) &&
        mSpeculativeScriptThread) {
      mSpeculativeScriptThread->StopParsing(PR_FALSE);
    }

    if (eInvalidDetect == theContext->mAutoDetectStatus) {
      if (theContext->mScanner) {
        nsScannerIterator iter;
        theContext->mScanner->EndReading(iter);
        theContext->mScanner->SetPosition(iter, PR_TRUE);
      }
    }

    PRUint32 totalRead;
    ParserWriteStruct pws;
    pws.mNeedCharsetCheck =
      (0 == sourceOffset) && (mCharsetSource < kCharsetFromMetaTag);
    pws.mParser = this;
    pws.mParserFilter = mParserFilter;
    pws.mScanner = theContext->mScanner;
    pws.mRequest = request;

    rv = pIStream->ReadSegments(ParserWriteFunc, &pws, aLength, &totalRead);
    if (NS_FAILED(rv)) {
      return rv;
    }

    // Don't bother to start parsing until we've seen some
    // non-whitespace data
    if (IsOkToProcessNetworkData() &&
        theContext->mScanner->FirstNonWhitespacePosition() >= 0) {
      mProcessingNetworkData = PR_TRUE;
      if (mSink) {
        mSink->WillParse();
      }
      rv = ResumeParse();
      mProcessingNetworkData = PR_FALSE;
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
nsresult
nsParser::OnStopRequest(nsIRequest *request, nsISupports* aContext,
                        nsresult status)
{
  nsresult rv = NS_OK;

  if ((mFlags & NS_PARSER_FLAG_PARSER_ENABLED) &&
      mSpeculativeScriptThread) {
    mSpeculativeScriptThread->StopParsing(PR_FALSE);
  }

  CParserContext *pc = mParserContext;
  while (pc) {
    if (pc->mRequest == request) {
      pc->mStreamListenerState = eOnStop;
      pc->mScanner->SetIncremental(PR_FALSE);
      break;
    }

    pc = pc->mPrevContext;
  }

  mStreamStatus = status;

  if (mParserFilter)
    mParserFilter->Finish();

  if (IsOkToProcessNetworkData() && NS_SUCCEEDED(rv)) {
    mProcessingNetworkData = PR_TRUE;
    if (mSink) {
      mSink->WillParse();
    }
    rv = ResumeParse(PR_TRUE, PR_TRUE);
    mProcessingNetworkData = PR_FALSE;
  }

  // If the parser isn't enabled, we don't finish parsing till
  // it is reenabled.


  // XXX Should we wait to notify our observers as well if the
  // parser isn't yet enabled?
  if (mObserver) {
    mObserver->OnStopRequest(request, aContext, status);
  }

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
bool
nsParser::WillTokenize(bool aIsFinalChunk)
{
  if (!mParserContext) {
    return PR_TRUE;
  }

  nsITokenizer* theTokenizer;
  nsresult result = mParserContext->GetTokenizer(mDTD, mSink, theTokenizer);
  NS_ENSURE_SUCCESS(result, PR_FALSE);
  return NS_SUCCEEDED(theTokenizer->WillTokenize(aIsFinalChunk,
                                                 &mTokenAllocator));
}


/**
 * This is the primary control routine to consume tokens.
 * It iteratively consumes tokens until an error occurs or
 * you run out of data.
 */
nsresult nsParser::Tokenize(bool aIsFinalChunk)
{
  nsITokenizer* theTokenizer;

  nsresult result = NS_ERROR_NOT_AVAILABLE;
  if (mParserContext) {
    result = mParserContext->GetTokenizer(mDTD, mSink, theTokenizer);
  }

  if (NS_SUCCEEDED(result)) {
    if (mFlags & NS_PARSER_FLAG_FLUSH_TOKENS) {
      // For some reason tokens didn't get flushed (probably
      // the parser got blocked before all the tokens in the
      // stack got handled). Flush 'em now. Ref. bug 104856
      if (theTokenizer->GetCount() != 0) {
        return result;
      }

      // Reset since the tokens have been flushed.
      mFlags &= ~NS_PARSER_FLAG_FLUSH_TOKENS;
    }

    bool flushTokens = false;

    mParserContext->mNumConsumed = 0;

    bool killSink = false;

    WillTokenize(aIsFinalChunk);
    while (NS_SUCCEEDED(result)) {
      mParserContext->mNumConsumed += mParserContext->mScanner->Mark();
      result = theTokenizer->ConsumeToken(*mParserContext->mScanner,
                                          flushTokens);
      if (NS_FAILED(result)) {
        mParserContext->mScanner->RewindToMark();
        if (kEOF == result){
          break;
        }
        if (NS_ERROR_HTMLPARSER_STOPPARSING == result) {
          killSink = PR_TRUE;
          result = Terminate();
          break;
        }
      } else if (flushTokens && (mFlags & NS_PARSER_FLAG_OBSERVERS_ENABLED)) {
        // I added the extra test of NS_PARSER_FLAG_OBSERVERS_ENABLED to fix Bug# 23931.
        // Flush tokens on seeing </SCRIPT> -- Ref: Bug# 22485 --
        // Also remember to update the marked position.
        mFlags |= NS_PARSER_FLAG_FLUSH_TOKENS;
        mParserContext->mNumConsumed += mParserContext->mScanner->Mark();
        break;
      }
    }
    DidTokenize(aIsFinalChunk);

    if (killSink) {
      mSink = nsnull;
    }
  } else {
    result = mInternalState = NS_ERROR_HTMLPARSER_BADTOKENIZER;
  }

  return result;
}

/**
 *  This is the tail-end of the code sandwich for the
 *  tokenization process. It gets called once tokenziation
 *  has completed for each phase.
 */
bool
nsParser::DidTokenize(bool aIsFinalChunk)
{
  if (!mParserContext) {
    return PR_TRUE;
  }

  nsITokenizer* theTokenizer;
  nsresult rv = mParserContext->GetTokenizer(mDTD, mSink, theTokenizer);
  NS_ENSURE_SUCCESS(rv, PR_FALSE);

  rv = theTokenizer->DidTokenize(aIsFinalChunk);
  return NS_SUCCEEDED(rv);
}

/**
 * Get the channel associated with this parser
 *
 * @param aChannel out param that will contain the result
 * @return NS_OK if successful
 */
NS_IMETHODIMP
nsParser::GetChannel(nsIChannel** aChannel)
{
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
nsParser::GetDTD(nsIDTD** aDTD)
{
  if (mParserContext) {
    NS_IF_ADDREF(*aDTD = mDTD);
  }

  return NS_OK;
}

/**
 * Get this as nsIStreamListener
 */
NS_IMETHODIMP
nsParser::GetStreamListener(nsIStreamListener** aListener)
{
  NS_ADDREF(*aListener = this);
  return NS_OK;
}
