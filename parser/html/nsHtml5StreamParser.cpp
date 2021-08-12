/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsHtml5StreamParser.h"

#include <stdlib.h>
#include <string.h>
#include <algorithm>
#include <new>
#include <type_traits>
#include <utility>
#include "GeckoProfiler.h"
#include "js/GCAPI.h"
#include "mozilla/ArrayIterator.h"
#include "mozilla/Buffer.h"
#include "mozilla/CheckedInt.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/Encoding.h"
#include "mozilla/EncodingDetector.h"
#include "mozilla/Likely.h"
#include "mozilla/Maybe.h"
#include "mozilla/SchedulerGroup.h"
#include "mozilla/ScopeExit.h"
#include "mozilla/Services.h"
#include "mozilla/StaticPrefs_html5.h"
#include "mozilla/StaticPrefs_intl.h"
#include "mozilla/TaskCategory.h"
#include "mozilla/Tuple.h"
#include "mozilla/UniquePtrExtensions.h"
#include "mozilla/Unused.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/dom/BrowsingContext.h"
#include "mozilla/dom/DebuggerUtilsBinding.h"
#include "mozilla/dom/DocGroup.h"
#include "mozilla/dom/Document.h"
#include "mozilla/mozalloc.h"
#include "nsContentSink.h"
#include "nsContentUtils.h"
#include "nsCycleCollectionTraversalCallback.h"
#include "nsHtml5AtomTable.h"
#include "nsHtml5ByteReadable.h"
#include "nsHtml5Highlighter.h"
#include "nsHtml5MetaScanner.h"
#include "nsHtml5Module.h"
#include "nsHtml5OwningUTF16Buffer.h"
#include "nsHtml5Parser.h"
#include "nsHtml5Speculation.h"
#include "nsHtml5StreamParserPtr.h"
#include "nsHtml5Tokenizer.h"
#include "nsHtml5TreeBuilder.h"
#include "nsHtml5TreeOpExecutor.h"
#include "nsHtml5TreeOpStage.h"
#include "nsIChannel.h"
#include "nsIContentSink.h"
#include "nsID.h"
#include "nsIDTD.h"
#include "nsIDocShell.h"
#include "nsIEventTarget.h"
#include "nsIHttpChannel.h"
#include "nsIInputStream.h"
#include "nsINestedURI.h"
#include "nsIObserverService.h"
#include "nsIRequest.h"
#include "nsIRunnable.h"
#include "nsIScriptError.h"
#include "nsIThread.h"
#include "nsIThreadRetargetableRequest.h"
#include "nsIThreadRetargetableStreamListener.h"
#include "nsITimer.h"
#include "nsIURI.h"
#include "nsJSEnvironment.h"
#include "nsLiteralString.h"
#include "nsNetUtil.h"
#include "nsString.h"
#include "nsTPromiseFlatString.h"
#include "nsThreadUtils.h"
#include "nsXULAppAPI.h"

// Include expat after the other, since it defines XML_NS, which conflicts with
// our symbol names.
#include "expat_config.h"
#include "expat.h"

extern "C" {
// Defined in intl/encoding_glue/src/lib.rs
const mozilla::Encoding* xmldecl_parse(const uint8_t* buf, size_t buf_len);
};

using namespace mozilla;
using namespace mozilla::dom;

/*
 * Note that nsHtml5StreamParser implements cycle collecting AddRef and
 * Release. Therefore, nsHtml5StreamParser must never be refcounted from
 * the parser thread!
 *
 * To work around this limitation, runnables posted by the main thread to the
 * parser thread hold their reference to the stream parser in an
 * nsHtml5StreamParserPtr. Upon creation, nsHtml5StreamParserPtr addrefs the
 * object it holds
 * just like a regular nsRefPtr. This is OK, since the creation of the
 * runnable and the nsHtml5StreamParserPtr happens on the main thread.
 *
 * When the runnable is done on the parser thread, the destructor of
 * nsHtml5StreamParserPtr runs there. It doesn't call Release on the held object
 * directly. Instead, it posts another runnable back to the main thread where
 * that runnable calls Release on the wrapped object.
 *
 * When posting runnables in the other direction, the runnables have to be
 * created on the main thread when nsHtml5StreamParser is instantiated and
 * held for the lifetime of the nsHtml5StreamParser. This works, because the
 * same runnabled can be dispatched multiple times and currently runnables
 * posted from the parser thread to main thread don't need to wrap any
 * runnable-specific data. (In the other direction, the runnables most notably
 * wrap the byte data of the stream.)
 */
NS_IMPL_CYCLE_COLLECTING_ADDREF(nsHtml5StreamParser)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsHtml5StreamParser)

NS_INTERFACE_TABLE_HEAD(nsHtml5StreamParser)
  NS_INTERFACE_TABLE(nsHtml5StreamParser, nsISupports)
  NS_INTERFACE_TABLE_TO_MAP_SEGUE_CYCLE_COLLECTION(nsHtml5StreamParser)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTION_CLASS(nsHtml5StreamParser)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(nsHtml5StreamParser)
  tmp->DropTimer();
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mRequest)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mOwner)
  tmp->mExecutorFlusher = nullptr;
  tmp->mLoadFlusher = nullptr;
  tmp->mExecutor = nullptr;
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(nsHtml5StreamParser)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mRequest)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mOwner)
  // hack: count the strongly owned edge wrapped in the runnable
  if (tmp->mExecutorFlusher) {
    NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(cb, "mExecutorFlusher->mExecutor");
    cb.NoteXPCOMChild(static_cast<nsIContentSink*>(tmp->mExecutor));
  }
  // hack: count the strongly owned edge wrapped in the runnable
  if (tmp->mLoadFlusher) {
    NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(cb, "mLoadFlusher->mExecutor");
    cb.NoteXPCOMChild(static_cast<nsIContentSink*>(tmp->mExecutor));
  }
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

class nsHtml5ExecutorFlusher : public Runnable {
 private:
  RefPtr<nsHtml5TreeOpExecutor> mExecutor;

 public:
  explicit nsHtml5ExecutorFlusher(nsHtml5TreeOpExecutor* aExecutor)
      : Runnable("nsHtml5ExecutorFlusher"), mExecutor(aExecutor) {}
  NS_IMETHOD Run() override {
    if (!mExecutor->isInList()) {
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
          PROFILER_MARKER_UNTYPED("HighPrio blocking parser flushing(1)", DOM);
          return NS_OK;
        }
      }
      mExecutor->RunFlushLoop();
    }
    return NS_OK;
  }
};

class nsHtml5LoadFlusher : public Runnable {
 private:
  RefPtr<nsHtml5TreeOpExecutor> mExecutor;

 public:
  explicit nsHtml5LoadFlusher(nsHtml5TreeOpExecutor* aExecutor)
      : Runnable("nsHtml5LoadFlusher"), mExecutor(aExecutor) {}
  NS_IMETHOD Run() override {
    mExecutor->FlushSpeculativeLoads();
    return NS_OK;
  }
};

nsHtml5StreamParser::nsHtml5StreamParser(nsHtml5TreeOpExecutor* aExecutor,
                                         nsHtml5Parser* aOwner,
                                         eParserMode aMode)
    : mSniffingLength(0),
      mBomState(eBomState::BOM_SNIFFING_NOT_STARTED),
      mCharsetSource(kCharsetUninitialized),
      mEncoding(WINDOWS_1252_ENCODING),
      mFeedChardet(true),
      mGuessEncoding(true),
      mReparseForbidden(false),
      mChannelHadCharset(false),
      mLastBuffer(nullptr),  // Will be filled when starting
      mExecutor(aExecutor),
      mTreeBuilder(new nsHtml5TreeBuilder(
          (aMode == VIEW_SOURCE_HTML || aMode == VIEW_SOURCE_XML)
              ? nullptr
              : mExecutor->GetStage(),
          aMode == NORMAL ? mExecutor->GetStage() : nullptr)),
      mTokenizer(
          new nsHtml5Tokenizer(mTreeBuilder.get(), aMode == VIEW_SOURCE_XML)),
      mTokenizerMutex("nsHtml5StreamParser mTokenizerMutex"),
      mOwner(aOwner),
      mLastWasCR(false),
      mStreamState(eHtml5StreamState::STREAM_NOT_STARTED),
      mSpeculating(false),
      mAtEOF(false),
      mSpeculationMutex("nsHtml5StreamParser mSpeculationMutex"),
      mSpeculationFailureCount(0),
      mLocalFileBytesBuffered(0),
      mTerminated(false),
      mInterrupted(false),
      mTerminatedMutex("nsHtml5StreamParser mTerminatedMutex"),
      mEventTarget(nsHtml5Module::GetStreamParserThread()->SerialEventTarget()),
      mExecutorFlusher(new nsHtml5ExecutorFlusher(aExecutor)),
      mLoadFlusher(new nsHtml5LoadFlusher(aExecutor)),
      mInitialEncodingWasFromParentFrame(false),
      mHasHadErrors(false),
      mDetectorHasSeenNonAscii(false),
      mDetectorHadOnlySeenAsciiWhenFirstGuessing(false),
      mDecodingLocalFileWithoutTokenizing(false),
      mFlushTimer(NS_NewTimer(mEventTarget)),
      mFlushTimerMutex("nsHtml5StreamParser mFlushTimerMutex"),
      mFlushTimerArmed(false),
      mFlushTimerEverFired(false),
      mMode(aMode) {
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
#ifdef DEBUG
  mAtomTable.SetPermittedLookupEventTarget(mEventTarget);
#endif
  mTokenizer->setInterner(&mAtomTable);
  mTokenizer->setEncodingDeclarationHandler(this);

  if (aMode == VIEW_SOURCE_HTML || aMode == VIEW_SOURCE_XML) {
    nsHtml5Highlighter* highlighter =
        new nsHtml5Highlighter(mExecutor->GetStage());
    mTokenizer->EnableViewSource(highlighter);    // takes ownership
    mTreeBuilder->EnableViewSource(highlighter);  // doesn't own
  }

  // There's a zeroing operator new for everything else
}

nsHtml5StreamParser::~nsHtml5StreamParser() {
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  mTokenizer->end();
#ifdef DEBUG
  {
    mozilla::MutexAutoLock flushTimerLock(mFlushTimerMutex);
    MOZ_ASSERT(!mFlushTimer, "Flush timer was not dropped before dtor!");
  }
  mRequest = nullptr;
  mUnicodeDecoder = nullptr;
  mSniffingBuffer = nullptr;
  mMetaScanner = nullptr;
  mFirstBuffer = nullptr;
  mExecutor = nullptr;
  mTreeBuilder = nullptr;
  mTokenizer = nullptr;
  mOwner = nullptr;
#endif
}

nsresult nsHtml5StreamParser::GetChannel(nsIChannel** aChannel) {
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  return mRequest ? CallQueryInterface(mRequest, aChannel)
                  : NS_ERROR_NOT_AVAILABLE;
}

int32_t nsHtml5StreamParser::MaybeRollBackSource(int32_t aSource) {
  if (aSource ==
      kCharsetFromFinalAutoDetectionWouldNotHaveBeenUTF8DependedOnTLD) {
    return kCharsetFromInitialAutoDetectionWouldNotHaveBeenUTF8DependedOnTLD;
  }
  if (aSource == kCharsetFromFinalAutoDetectionWouldNotHaveBeenUTF8Generic) {
    return kCharsetFromInitialAutoDetectionWouldNotHaveBeenUTF8Generic;
  }
  if (aSource == kCharsetFromFinalAutoDetectionWouldNotHaveBeenUTF8Content) {
    return kCharsetFromInitialAutoDetectionWouldNotHaveBeenUTF8Content;
  }
  if (aSource == kCharsetFromFinalAutoDetectionWouldHaveBeenUTF8 &&
      !mDetectorHadOnlySeenAsciiWhenFirstGuessing) {
    return kCharsetFromInitialAutoDetectionWouldHaveBeenUTF8;
  }
  if (aSource == kCharsetFromFinalUserForcedAutoDetection) {
    aSource = kCharsetFromInitialUserForcedAutoDetection;
  }
  return aSource;
}

void nsHtml5StreamParser::GuessEncoding(bool aEof, bool aInitial) {
  if (aInitial) {
    if (!mDetectorHasSeenNonAscii) {
      mDetectorHadOnlySeenAsciiWhenFirstGuessing = true;
    }
  } else {
    mGuessEncoding = false;
  }
  bool forced = (mCharsetSource == kCharsetFromPendingUserForcedAutoDetection ||
                 mCharsetSource == kCharsetFromInitialUserForcedAutoDetection);
  MOZ_ASSERT(
      mCharsetSource != kCharsetFromFinalUserForcedAutoDetection &&
      mCharsetSource != kCharsetFromFinalAutoDetectionWouldHaveBeenUTF8 &&
      mCharsetSource !=
          kCharsetFromFinalAutoDetectionWouldNotHaveBeenUTF8Generic &&
      mCharsetSource !=
          kCharsetFromFinalAutoDetectionWouldNotHaveBeenUTF8Content &&
      mCharsetSource !=
          kCharsetFromFinalAutoDetectionWouldNotHaveBeenUTF8DependedOnTLD &&
      mCharsetSource != kCharsetFromFinalAutoDetectionFile);
  auto ifHadBeenForced = mDetector->Guess(EmptyCString(), true);
  auto encoding =
      forced ? ifHadBeenForced
             : mDetector->Guess(mTLD, mDecodingLocalFileWithoutTokenizing);
  int32_t source =
      aInitial
          ? (forced
                 ? kCharsetFromInitialUserForcedAutoDetection
                 : kCharsetFromInitialAutoDetectionWouldNotHaveBeenUTF8Generic)
          : (forced
                 ? kCharsetFromFinalUserForcedAutoDetection
                 : (mDecodingLocalFileWithoutTokenizing
                        ? kCharsetFromFinalAutoDetectionFile
                        : kCharsetFromFinalAutoDetectionWouldNotHaveBeenUTF8Generic));
  if (source == kCharsetFromFinalAutoDetectionWouldNotHaveBeenUTF8Generic) {
    if (encoding == ISO_2022_JP_ENCODING) {
      if (EncodingDetector::TldMayAffectGuess(mTLD)) {
        source = kCharsetFromFinalAutoDetectionWouldNotHaveBeenUTF8Content;
      }
    } else if (!mDetectorHasSeenNonAscii) {
      source = kCharsetFromInitialAutoDetectionASCII;  // deliberately Initial
    } else if (ifHadBeenForced == UTF_8_ENCODING) {
      source = kCharsetFromFinalAutoDetectionWouldHaveBeenUTF8;
    } else if (encoding != ifHadBeenForced) {
      source = kCharsetFromFinalAutoDetectionWouldNotHaveBeenUTF8DependedOnTLD;
    } else if (EncodingDetector::TldMayAffectGuess(mTLD)) {
      source = kCharsetFromFinalAutoDetectionWouldNotHaveBeenUTF8Content;
    }
  } else if (source ==
             kCharsetFromInitialAutoDetectionWouldNotHaveBeenUTF8Generic) {
    if (encoding == ISO_2022_JP_ENCODING) {
      if (EncodingDetector::TldMayAffectGuess(mTLD)) {
        source = kCharsetFromInitialAutoDetectionWouldNotHaveBeenUTF8Content;
      }
    } else if (!mDetectorHasSeenNonAscii) {
      source = kCharsetFromInitialAutoDetectionASCII;
    } else if (ifHadBeenForced == UTF_8_ENCODING) {
      source = kCharsetFromInitialAutoDetectionWouldHaveBeenUTF8;
    } else if (encoding != ifHadBeenForced) {
      source =
          kCharsetFromInitialAutoDetectionWouldNotHaveBeenUTF8DependedOnTLD;
    } else if (EncodingDetector::TldMayAffectGuess(mTLD)) {
      source = kCharsetFromInitialAutoDetectionWouldNotHaveBeenUTF8Content;
    }
  }
  if (HasDecoder() && !mDecodingLocalFileWithoutTokenizing) {
    if (mEncoding == encoding) {
      MOZ_ASSERT(mCharsetSource == kCharsetFromInitialAutoDetectionASCII ||
                     mCharsetSource < source,
                 "Why are we running chardet at all?");
      // Source didn't actually change between initial and final, so roll it
      // back for telemetry purposes.
      mCharsetSource = MaybeRollBackSource(source);
      mTreeBuilder->SetDocumentCharset(mEncoding, mCharsetSource);
    } else {
      MOZ_ASSERT(mCharsetSource < kCharsetFromXmlDeclarationUtf16 || forced);
      // We've already committed to a decoder. Request a reload from the
      // docshell.
      mTreeBuilder->NeedsCharsetSwitchTo(encoding, source, 0);
      FlushTreeOpsAndDisarmTimer();
      Interrupt();
    }
  } else {
    // Got a confident answer from the sniffing buffer. That code will
    // take care of setting up the decoder.
    if (mCharsetSource == kCharsetUninitialized && aEof) {
      // The document is so short that the initial buffer is the last
      // buffer.
      source = MaybeRollBackSource(source);
    }
    mEncoding = encoding;
    mCharsetSource = source;
    mTreeBuilder->SetDocumentCharset(mEncoding, mCharsetSource);
  }
}

void nsHtml5StreamParser::FeedDetector(Span<const uint8_t> aBuffer,
                                       bool aLast) {
  mDetectorHasSeenNonAscii = mDetector->Feed(aBuffer, aLast);
}

void nsHtml5StreamParser::SetViewSourceTitle(nsIURI* aURL) {
  MOZ_ASSERT(NS_IsMainThread());

  BrowsingContext* browsingContext =
      mExecutor->GetDocument()->GetBrowsingContext();
  if (browsingContext && browsingContext->WatchedByDevTools()) {
    mURIToSendToDevtools = aURL;

    nsID uuid;
    nsresult rv = nsContentUtils::GenerateUUIDInPlace(uuid);
    if (!NS_FAILED(rv)) {
      char buffer[NSID_LENGTH];
      uuid.ToProvidedString(buffer);
      mUUIDForDevtools = NS_ConvertASCIItoUTF16(buffer);
    }
  }

  if (aURL) {
    nsCOMPtr<nsIURI> temp;
    if (aURL->SchemeIs("view-source")) {
      nsCOMPtr<nsINestedURI> nested = do_QueryInterface(aURL);
      nested->GetInnerURI(getter_AddRefs(temp));
    } else {
      temp = aURL;
    }
    if (temp->SchemeIs("data")) {
      // Avoid showing potentially huge data: URLs. The three last bytes are
      // UTF-8 for an ellipsis.
      mViewSourceTitle.AssignLiteral("data:\xE2\x80\xA6");
    } else {
      nsresult rv = temp->GetSpec(mViewSourceTitle);
      if (NS_FAILED(rv)) {
        mViewSourceTitle.AssignLiteral("\xE2\x80\xA6");
      }
    }
  }
}

nsresult
nsHtml5StreamParser::SetupDecodingAndWriteSniffingBufferAndCurrentSegment(
    Span<const uint8_t> aFromSegment) {
  NS_ASSERTION(IsParserThread(), "Wrong thread!");
  nsresult rv = NS_OK;
  if (mDecodingLocalFileWithoutTokenizing &&
      mCharsetSource <= kCharsetFromFallback) {
    MOZ_ASSERT(mEncoding != UTF_8_ENCODING);
    mUnicodeDecoder = UTF_8_ENCODING->NewDecoderWithBOMRemoval();
  } else {
    if (mCharsetSource >= kCharsetFromFinalAutoDetectionWouldHaveBeenUTF8) {
      if (!(mCharsetSource == kCharsetFromPendingUserForcedAutoDetection ||
            mCharsetSource == kCharsetFromInitialUserForcedAutoDetection)) {
        DontGuessEncoding();
      }
      mDecodingLocalFileWithoutTokenizing = false;
    }
    mUnicodeDecoder = mEncoding->NewDecoderWithBOMRemoval();
  }
  if (mSniffingBuffer) {
    rv = WriteStreamBytes(Span(mSniffingBuffer.get(), mSniffingLength));
    NS_ENSURE_SUCCESS(rv, rv);
    mSniffingBuffer = nullptr;
  }
  mMetaScanner = nullptr;
  return WriteStreamBytes(aFromSegment);
}

void nsHtml5StreamParser::SetupDecodingFromBom(
    NotNull<const Encoding*> aEncoding) {
  MOZ_ASSERT(IsParserThread(), "Wrong thread!");
  mEncoding = aEncoding;
  mDecodingLocalFileWithoutTokenizing = false;
  mUnicodeDecoder = mEncoding->NewDecoderWithoutBOMHandling();
  mCharsetSource = kCharsetFromByteOrderMark;
  DontGuessEncoding();
  mTreeBuilder->SetDocumentCharset(mEncoding, mCharsetSource);
  mSniffingBuffer = nullptr;
  mMetaScanner = nullptr;
  mBomState = BOM_SNIFFING_OVER;
}

void nsHtml5StreamParser::SetupDecodingFromUtf16BogoXml(
    NotNull<const Encoding*> aEncoding) {
  MOZ_ASSERT(IsParserThread(), "Wrong thread!");
  mEncoding = aEncoding;
  mDecodingLocalFileWithoutTokenizing = false;
  mUnicodeDecoder = mEncoding->NewDecoderWithoutBOMHandling();
  mCharsetSource = kCharsetFromXmlDeclarationUtf16;
  DontGuessEncoding();
  mTreeBuilder->SetDocumentCharset(mEncoding, mCharsetSource);
  mSniffingBuffer = nullptr;
  mMetaScanner = nullptr;
  mBomState = BOM_SNIFFING_OVER;
  auto dst = mLastBuffer->TailAsSpan(READ_BUFFER_SIZE);
  dst[0] = '<';
  dst[1] = '?';
  dst[2] = 'x';
  mLastBuffer->AdvanceEnd(3);
}

void nsHtml5StreamParser::SniffBOMlessUTF16BasicLatin(const uint8_t* aBuf,
                                                      size_t aBufLen) {
  // Avoid underspecified heuristic craziness for XHR
  if (mMode == LOAD_AS_DATA) {
    return;
  }
  // Make sure there's enough data. Require room for "<title></title>"
  if (aBufLen < 30) {
    return;
  }
  // even-numbered bytes tracked at 0, odd-numbered bytes tracked at 1
  bool byteZero[2] = {false, false};
  bool byteNonZero[2] = {false, false};
  uint32_t i = 0;
  for (; i < aBufLen; ++i) {
    if (aBuf[i]) {
      if (byteNonZero[1 - (i % 2)]) {
        return;
      }
      byteNonZero[i % 2] = true;
    } else {
      if (byteZero[1 - (i % 2)]) {
        return;
      }
      byteZero[i % 2] = true;
    }
  }
  if (byteNonZero[0]) {
    mEncoding = UTF_16LE_ENCODING;
  } else {
    mEncoding = UTF_16BE_ENCODING;
  }
  mCharsetSource = kCharsetFromIrreversibleAutoDetection;
  mTreeBuilder->SetDocumentCharset(mEncoding, mCharsetSource);
  DontGuessEncoding();
  mTreeBuilder->MaybeComplainAboutCharset("EncBomlessUtf16", true, 0);
}

void nsHtml5StreamParser::SetEncodingFromExpat(const char16_t* aEncoding) {
  if (aEncoding) {
    nsDependentString utf16(aEncoding);
    nsAutoCString utf8;
    CopyUTF16toUTF8(utf16, utf8);
    auto encoding = PreferredForInternalEncodingDecl(utf8);
    if (encoding) {
      mEncoding = WrapNotNull(encoding);
      mCharsetSource = kCharsetFromMetaTag;  // closest for XML
      mTreeBuilder->SetDocumentCharset(mEncoding, mCharsetSource);
      return;
    }
    // else the page declared an encoding Gecko doesn't support and we'd
    // end up defaulting to UTF-8 anyway. Might as well fall through here
    // right away and let the encoding be set to UTF-8 which we'd default to
    // anyway.
  }
  mEncoding = UTF_8_ENCODING;            // XML defaults to UTF-8 without a BOM
  mCharsetSource = kCharsetFromMetaTag;  // means confident
  mTreeBuilder->SetDocumentCharset(mEncoding, mCharsetSource);
}

// A separate user data struct is used instead of passing the
// nsHtml5StreamParser instance as user data in order to avoid including
// expat.h in nsHtml5StreamParser.h. Doing that would cause naming conflicts.
// Using a separate user data struct also avoids bloating nsHtml5StreamParser
// by one pointer.
struct UserData {
  XML_Parser mExpat;
  nsHtml5StreamParser* mStreamParser;
};

// Using no-namespace handler callbacks to avoid including expat.h in
// nsHtml5StreamParser.h, since doing so would cause naming conclicts.
static void HandleXMLDeclaration(void* aUserData, const XML_Char* aVersion,
                                 const XML_Char* aEncoding, int aStandalone) {
  UserData* ud = static_cast<UserData*>(aUserData);
  ud->mStreamParser->SetEncodingFromExpat(
      reinterpret_cast<const char16_t*>(aEncoding));
  XML_StopParser(ud->mExpat, false);
}

static void HandleStartElement(void* aUserData, const XML_Char* aName,
                               const XML_Char** aAtts) {
  UserData* ud = static_cast<UserData*>(aUserData);
  XML_StopParser(ud->mExpat, false);
}

static void HandleEndElement(void* aUserData, const XML_Char* aName) {
  UserData* ud = static_cast<UserData*>(aUserData);
  XML_StopParser(ud->mExpat, false);
}

static void HandleComment(void* aUserData, const XML_Char* aName) {
  UserData* ud = static_cast<UserData*>(aUserData);
  XML_StopParser(ud->mExpat, false);
}

static void HandleProcessingInstruction(void* aUserData,
                                        const XML_Char* aTarget,
                                        const XML_Char* aData) {
  UserData* ud = static_cast<UserData*>(aUserData);
  XML_StopParser(ud->mExpat, false);
}

void nsHtml5StreamParser::FinalizeSniffingWithDetector(
    Span<const uint8_t> aFromSegment, uint32_t aCountToSniffingLimit,
    bool aEof) {
  if (mFeedChardet && mSniffingBuffer) {
    FeedDetector(Span(mSniffingBuffer.get(), mSniffingLength), false);
  }
  if (mFeedChardet && !aFromSegment.IsEmpty()) {
    // Avoid buffer boundary-dependent behavior.
    FeedDetector(aFromSegment.To(aCountToSniffingLimit), false);
  }
  bool guess = mFeedChardet;
  if (mFeedChardet && aEof && aCountToSniffingLimit <= aFromSegment.Length()) {
    FeedDetector(Span<const uint8_t>(), true);
    mFeedChardet = false;
  }
  if (guess) {
    GuessEncoding(aEof, (guess == mFeedChardet));
  }
  if (mReparseForbidden) {
    DontGuessEncoding();
  }
  if (mFeedChardet && !aEof && aCountToSniffingLimit < aFromSegment.Length()) {
    // Avoid buffer boundary-dependent behavior.
    FeedDetector(aFromSegment.From(aCountToSniffingLimit), false);
  }
}

nsresult nsHtml5StreamParser::FinalizeSniffing(Span<const uint8_t> aFromSegment,
                                               uint32_t aCountToSniffingLimit,
                                               bool aEof) {
  MOZ_ASSERT(IsParserThread(), "Wrong thread!");
  MOZ_ASSERT(mCharsetSource < kCharsetFromXmlDeclarationUtf16,
             "Should not finalize sniffing with strong decision already made.");
  if (mMode == VIEW_SOURCE_XML) {
    static const XML_Memory_Handling_Suite memsuite = {
        (void* (*)(size_t))moz_xmalloc, (void* (*)(void*, size_t))moz_xrealloc,
        free};

    static const char16_t kExpatSeparator[] = {0xFFFF, '\0'};

    static const char16_t kISO88591[] = {'I', 'S', 'O', '-', '8', '8',
                                         '5', '9', '-', '1', '\0'};

    UserData ud;
    ud.mStreamParser = this;

    // If we got this far, the stream didn't have a BOM. UTF-16-encoded XML
    // documents MUST begin with a BOM. We don't support EBCDIC and such.
    // Thus, at this point, what we have is garbage or something encoded using
    // a rough ASCII superset. ISO-8859-1 allows us to decode ASCII bytes
    // without throwing errors when bytes have the most significant bit set
    // and without triggering expat's unknown encoding code paths. This is
    // enough to be able to use expat to parse the XML declaration in order
    // to extract the encoding name from it.
    ud.mExpat = XML_ParserCreate_MM(kISO88591, &memsuite, kExpatSeparator);
    XML_SetXmlDeclHandler(ud.mExpat, HandleXMLDeclaration);
    XML_SetElementHandler(ud.mExpat, HandleStartElement, HandleEndElement);
    XML_SetCommentHandler(ud.mExpat, HandleComment);
    XML_SetProcessingInstructionHandler(ud.mExpat, HandleProcessingInstruction);
    XML_SetUserData(ud.mExpat, static_cast<void*>(&ud));

    XML_Status status = XML_STATUS_OK;

    // aFromSegment points to the data obtained from the current network
    // event. mSniffingBuffer (if it exists) contains the data obtained before
    // the current event. Thus, mSniffingLenth bytes of mSniffingBuffer
    // followed by aCountToSniffingLimit bytes from aFromSegment are the
    // first 1024 bytes of the file (or the file as a whole if the file is
    // 1024 bytes long or shorter). Thus, we parse both buffers, but if the
    // first call succeeds already, we skip parsing the second buffer.
    if (mSniffingBuffer) {
      status = XML_Parse(ud.mExpat,
                         reinterpret_cast<const char*>(mSniffingBuffer.get()),
                         mSniffingLength, false);
    }
    if (status == XML_STATUS_OK && mCharsetSource < kCharsetFromMetaTag) {
      mozilla::Unused << XML_Parse(
          ud.mExpat, reinterpret_cast<const char*>(aFromSegment.Elements()),
          aCountToSniffingLimit, false);
    }
    XML_ParserFree(ud.mExpat);

    if (mCharsetSource < kCharsetFromMetaTag) {
      // Failed to get an encoding from the XML declaration. XML defaults
      // confidently to UTF-8 in this case.
      // It is also possible that the document has an XML declaration that is
      // longer than 1024 bytes, but that case is not worth worrying about.
      mEncoding = UTF_8_ENCODING;
      mCharsetSource = kCharsetFromMetaTag;  // means confident
      mTreeBuilder->SetDocumentCharset(mEncoding, mCharsetSource);
    }

    return SetupDecodingAndWriteSniffingBufferAndCurrentSegment(aFromSegment);
  }
  bool forced = (mCharsetSource == kCharsetFromPendingUserForcedAutoDetection ||
                 mCharsetSource == kCharsetFromInitialUserForcedAutoDetection ||
                 mCharsetSource == kCharsetFromFinalUserForcedAutoDetection);
  if (!mChannelHadCharset &&
      (forced || mCharsetSource < kCharsetFromMetaPrescan) &&
      (mMode == NORMAL || mMode == VIEW_SOURCE_HTML || mMode == LOAD_AS_DATA)) {
    // Look for XML declaration in text/html.

    const uint8_t* buf;
    size_t bufLen;
    if (mSniffingLength) {
      // Copy data to a contiguous buffer if we already have something buffered
      // up.
      memcpy(mSniffingBuffer.get() + mSniffingLength, aFromSegment.Elements(),
             aCountToSniffingLimit);
      mSniffingLength += aCountToSniffingLimit;
      aFromSegment = aFromSegment.From(aCountToSniffingLimit);
      aCountToSniffingLimit = 0;
      buf = mSniffingBuffer.get();
      bufLen = mSniffingLength;
    } else {
      buf = aFromSegment.Elements();
      bufLen = aCountToSniffingLimit;
    }
    const Encoding* encoding = xmldecl_parse(buf, bufLen);
    if (encoding) {
      if (forced &&
          (encoding->IsAsciiCompatible() || encoding == ISO_2022_JP_ENCODING)) {
        // Honor override
        if (mCharsetSource == kCharsetFromFinalUserForcedAutoDetection) {
          DontGuessEncoding();
        } else {
          FinalizeSniffingWithDetector(aFromSegment, aCountToSniffingLimit,
                                       false);
        }
        return SetupDecodingAndWriteSniffingBufferAndCurrentSegment(
            aFromSegment);
      }
      DontGuessEncoding();
      mEncoding = WrapNotNull(encoding);
      mCharsetSource = kCharsetFromXmlDeclaration;
      mTreeBuilder->SetDocumentCharset(mEncoding, mCharsetSource);
    } else if (mCharsetSource < kCharsetFromIrreversibleAutoDetection) {
      // meta scan and XML declaration check failed.
      // Check for BOMless UTF-16 with Basic
      // Latin content for compat with IE. See bug 631751.
      SniffBOMlessUTF16BasicLatin(buf, bufLen);
    }
  }
  if (forced && mCharsetSource != kCharsetFromIrreversibleAutoDetection) {
    // neither meta nor XML declaration found, honor override
    if (mCharsetSource == kCharsetFromFinalUserForcedAutoDetection) {
      DontGuessEncoding();
    } else {
      FinalizeSniffingWithDetector(aFromSegment, aCountToSniffingLimit, false);
    }
    return SetupDecodingAndWriteSniffingBufferAndCurrentSegment(aFromSegment);
  }

  // the charset may have been set now
  // maybe try chardet now;
  if (mFeedChardet) {
    FinalizeSniffingWithDetector(aFromSegment, aCountToSniffingLimit, aEof);
    // fall thru; charset may have changed
  }
  if (mCharsetSource == kCharsetUninitialized) {
    // Hopefully this case is never needed, but dealing with it anyway
    mEncoding = WINDOWS_1252_ENCODING;
    mCharsetSource = kCharsetFromFallback;
    mTreeBuilder->SetDocumentCharset(mEncoding, mCharsetSource);
  } else if (mMode == LOAD_AS_DATA && mCharsetSource == kCharsetFromFallback) {
    NS_ASSERTION(mReparseForbidden, "Reparse should be forbidden for XHR");
    NS_ASSERTION(!mFeedChardet, "Should not feed chardet for XHR");
    NS_ASSERTION(mEncoding == UTF_8_ENCODING, "XHR should default to UTF-8");
    // Now mark charset source as non-weak to signal that we have a decision
    mCharsetSource = kCharsetFromDocTypeDefault;
    mTreeBuilder->SetDocumentCharset(mEncoding, mCharsetSource);
  }
  return SetupDecodingAndWriteSniffingBufferAndCurrentSegment(aFromSegment);
}

nsresult nsHtml5StreamParser::SniffStreamBytes(
    Span<const uint8_t> aFromSegment) {
  MOZ_ASSERT(IsParserThread(), "Wrong thread!");
  // mEncoding and mCharsetSource potentially have come from channel or higher
  // by now. If we find a BOM, SetupDecodingFromBom() will overwrite them.
  // If we don't find a BOM, the previously set values of mEncoding and
  // mCharsetSource are not modified by the BOM sniffing here.
  for (uint32_t i = 0;
       i < aFromSegment.Length() && mBomState != BOM_SNIFFING_OVER; i++) {
    switch (mBomState) {
      case BOM_SNIFFING_NOT_STARTED:
        MOZ_ASSERT(i == 0, "Bad BOM sniffing state.");
        switch (aFromSegment[0]) {
          case 0xEF:
            mBomState = SEEN_UTF_8_FIRST_BYTE;
            break;
          case 0xFF:
            mBomState = SEEN_UTF_16_LE_FIRST_BYTE;
            break;
          case 0xFE:
            mBomState = SEEN_UTF_16_BE_FIRST_BYTE;
            break;
          case 0x00:
            if (mCharsetSource < kCharsetFromXmlDeclarationUtf16 &&
                !mChannelHadCharset) {
              mBomState = SEEN_UTF_16_BE_XML_FIRST;
            } else {
              mBomState = BOM_SNIFFING_OVER;
            }
            break;
          case 0x3C:
            if (mCharsetSource < kCharsetFromXmlDeclarationUtf16 &&
                !mChannelHadCharset) {
              mBomState = SEEN_UTF_16_LE_XML_FIRST;
            } else {
              mBomState = BOM_SNIFFING_OVER;
            }
            break;
          default:
            mBomState = BOM_SNIFFING_OVER;
            break;
        }
        break;
      case SEEN_UTF_16_LE_FIRST_BYTE:
        if (aFromSegment[i] == 0xFE) {
          SetupDecodingFromBom(UTF_16LE_ENCODING);
          return WriteStreamBytes(aFromSegment.From(i + 1));
        }
        mBomState = BOM_SNIFFING_OVER;
        break;
      case SEEN_UTF_16_BE_FIRST_BYTE:
        if (aFromSegment[i] == 0xFF) {
          SetupDecodingFromBom(UTF_16BE_ENCODING);
          return WriteStreamBytes(aFromSegment.From(i + 1));
        }
        mBomState = BOM_SNIFFING_OVER;
        break;
      case SEEN_UTF_8_FIRST_BYTE:
        if (aFromSegment[i] == 0xBB) {
          mBomState = SEEN_UTF_8_SECOND_BYTE;
        } else {
          mBomState = BOM_SNIFFING_OVER;
        }
        break;
      case SEEN_UTF_8_SECOND_BYTE:
        if (aFromSegment[i] == 0xBF) {
          SetupDecodingFromBom(UTF_8_ENCODING);
          return WriteStreamBytes(aFromSegment.From(i + 1));
        }
        mBomState = BOM_SNIFFING_OVER;
        break;
      case SEEN_UTF_16_BE_XML_FIRST:
        if (aFromSegment[i] == 0x3C) {
          mBomState = SEEN_UTF_16_BE_XML_SECOND;
        } else {
          mBomState = BOM_SNIFFING_OVER;
        }
        break;
      case SEEN_UTF_16_BE_XML_SECOND:
        if (aFromSegment[i] == 0x00) {
          mBomState = SEEN_UTF_16_BE_XML_THIRD;
        } else {
          mBomState = BOM_SNIFFING_OVER;
        }
        break;
      case SEEN_UTF_16_BE_XML_THIRD:
        if (aFromSegment[i] == 0x3F) {
          mBomState = SEEN_UTF_16_BE_XML_FOURTH;
        } else {
          mBomState = BOM_SNIFFING_OVER;
        }
        break;
      case SEEN_UTF_16_BE_XML_FOURTH:
        if (aFromSegment[i] == 0x00) {
          mBomState = SEEN_UTF_16_BE_XML_FIFTH;
        } else {
          mBomState = BOM_SNIFFING_OVER;
        }
        break;
      case SEEN_UTF_16_BE_XML_FIFTH:
        if (aFromSegment[i] == 0x78) {
          SetupDecodingFromUtf16BogoXml(UTF_16BE_ENCODING);
          return WriteStreamBytes(aFromSegment.From(i + 1));
        }
        mBomState = BOM_SNIFFING_OVER;
        break;
      case SEEN_UTF_16_LE_XML_FIRST:
        if (aFromSegment[i] == 0x00) {
          mBomState = SEEN_UTF_16_LE_XML_SECOND;
        } else {
          mBomState = BOM_SNIFFING_OVER;
        }
        break;
      case SEEN_UTF_16_LE_XML_SECOND:
        if (aFromSegment[i] == 0x3F) {
          mBomState = SEEN_UTF_16_LE_XML_THIRD;
        } else {
          mBomState = BOM_SNIFFING_OVER;
        }
        break;
      case SEEN_UTF_16_LE_XML_THIRD:
        if (aFromSegment[i] == 0x00) {
          mBomState = SEEN_UTF_16_LE_XML_FOURTH;
        } else {
          mBomState = BOM_SNIFFING_OVER;
        }
        break;
      case SEEN_UTF_16_LE_XML_FOURTH:
        if (aFromSegment[i] == 0x78) {
          mBomState = SEEN_UTF_16_LE_XML_FIFTH;
        } else {
          mBomState = BOM_SNIFFING_OVER;
        }
        break;
      case SEEN_UTF_16_LE_XML_FIFTH:
        if (aFromSegment[i] == 0x00) {
          SetupDecodingFromUtf16BogoXml(UTF_16LE_ENCODING);
          return WriteStreamBytes(aFromSegment.From(i + 1));
        }
        mBomState = BOM_SNIFFING_OVER;
        break;
      default:
        mBomState = BOM_SNIFFING_OVER;
        break;
    }
  }
  // if we get here, there either was no BOM or the BOM sniffing isn't complete
  // yet

  MOZ_ASSERT(mCharsetSource != kCharsetFromByteOrderMark,
             "Should not come here if BOM was found.");
  MOZ_ASSERT(mCharsetSource != kCharsetFromXmlDeclarationUtf16,
             "Should not come here if UTF-16 bogo-XML declaration was found.");
  MOZ_ASSERT(mCharsetSource != kCharsetFromOtherComponent,
             "kCharsetFromOtherComponent is for XSLT.");

  if (mBomState == BOM_SNIFFING_OVER && mCharsetSource == kCharsetFromChannel) {
    // There was no BOM and the charset came from channel. mEncoding
    // still contains the charset from the channel as set by an
    // earlier call to SetDocumentCharset(), since we didn't find a BOM and
    // overwrite mEncoding. (Note that if the user has overridden the charset,
    // we don't come here but check <meta> for XSS-dangerous charsets first.)
    mTreeBuilder->SetDocumentCharset(mEncoding, mCharsetSource);
    return SetupDecodingAndWriteSniffingBufferAndCurrentSegment(aFromSegment);
  }

  if (!mChannelHadCharset && !mMetaScanner &&
      (mMode == NORMAL || mMode == VIEW_SOURCE_HTML || mMode == LOAD_AS_DATA)) {
    mMetaScanner = MakeUnique<nsHtml5MetaScanner>(mTreeBuilder.get());
  }

  if (mSniffingLength + aFromSegment.Length() >= SNIFFING_BUFFER_SIZE) {
    // this is the last buffer
    uint32_t countToSniffingLimit = SNIFFING_BUFFER_SIZE - mSniffingLength;
    bool forced =
        (mCharsetSource == kCharsetFromPendingUserForcedAutoDetection ||
         mCharsetSource == kCharsetFromInitialUserForcedAutoDetection ||
         mCharsetSource == kCharsetFromFinalUserForcedAutoDetection);
    if (!mChannelHadCharset && (mMode == NORMAL || mMode == VIEW_SOURCE_HTML ||
                                mMode == LOAD_AS_DATA)) {
      nsHtml5ByteReadable readable(
          aFromSegment.Elements(),
          aFromSegment.Elements() + countToSniffingLimit);
      nsAutoCString charset;
      auto encoding = mMetaScanner->sniff(&readable);
      // Due to the way nsHtml5Portability reports OOM, ask the tree buider
      nsresult rv;
      if (NS_FAILED((rv = mTreeBuilder->IsBroken()))) {
        MarkAsBroken(rv);
        return rv;
      }

      if (encoding) {
        // meta scan successful; honor overrides unless meta is XSS-dangerous
        if (forced && (encoding->IsAsciiCompatible() ||
                       encoding == ISO_2022_JP_ENCODING)) {
          // Honor override
          if (mCharsetSource == kCharsetFromFinalUserForcedAutoDetection) {
            DontGuessEncoding();
          } else {
            FinalizeSniffingWithDetector(aFromSegment, countToSniffingLimit,
                                         false);
          }
          return SetupDecodingAndWriteSniffingBufferAndCurrentSegment(
              aFromSegment);
        }
        DontGuessEncoding();
        mEncoding = WrapNotNull(encoding);
        mCharsetSource = kCharsetFromMetaPrescan;
        mTreeBuilder->SetDocumentCharset(mEncoding, mCharsetSource);
        return SetupDecodingAndWriteSniffingBufferAndCurrentSegment(
            aFromSegment);
      }
    }
    return FinalizeSniffing(aFromSegment, countToSniffingLimit, false);
  }

  // not the last buffer
  if (!mChannelHadCharset &&
      (mMode == NORMAL || mMode == VIEW_SOURCE_HTML || mMode == LOAD_AS_DATA)) {
    nsHtml5ByteReadable readable(
        aFromSegment.Elements(),
        aFromSegment.Elements() + aFromSegment.Length());
    auto encoding = mMetaScanner->sniff(&readable);
    // Due to the way nsHtml5Portability reports OOM, ask the tree buider
    nsresult rv;
    if (NS_FAILED((rv = mTreeBuilder->IsBroken()))) {
      MarkAsBroken(rv);
      return rv;
    }
    if (encoding) {
      // meta scan successful; honor overrides unless meta is XSS-dangerous
      if ((mCharsetSource == kCharsetFromFinalUserForcedAutoDetection) &&
          (encoding->IsAsciiCompatible() || encoding == ISO_2022_JP_ENCODING)) {
        // Honor override
        return SetupDecodingAndWriteSniffingBufferAndCurrentSegment(
            aFromSegment);
      }
      if ((mCharsetSource == kCharsetFromPendingUserForcedAutoDetection ||
           mCharsetSource == kCharsetFromInitialUserForcedAutoDetection) &&
          (encoding->IsAsciiCompatible() || encoding == ISO_2022_JP_ENCODING)) {
        FinalizeSniffingWithDetector(aFromSegment, aFromSegment.Length(),
                                     false);
      } else {
        DontGuessEncoding();
        mEncoding = WrapNotNull(encoding);
        mCharsetSource = kCharsetFromMetaPrescan;
        mTreeBuilder->SetDocumentCharset(mEncoding, mCharsetSource);
      }
      return SetupDecodingAndWriteSniffingBufferAndCurrentSegment(aFromSegment);
    }
  }

  if (!mSniffingBuffer) {
    mSniffingBuffer = MakeUniqueFallible<uint8_t[]>(SNIFFING_BUFFER_SIZE);
    if (!mSniffingBuffer) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
  }
  memcpy(&mSniffingBuffer[mSniffingLength], aFromSegment.Elements(),
         aFromSegment.Length());
  mSniffingLength += aFromSegment.Length();
  return NS_OK;
}

class AddContentRunnable : public Runnable {
 public:
  AddContentRunnable(const nsAString& aParserID, nsIURI* aURI,
                     Span<const char16_t> aData, bool aComplete)
      : Runnable("AddContent") {
    nsAutoCString spec;
    aURI->GetSpec(spec);
    mData.mUri.Construct(NS_ConvertUTF8toUTF16(spec));
    mData.mParserID.Construct(aParserID);
    mData.mContents.Construct(aData.Elements(), aData.Length());
    mData.mComplete.Construct(aComplete);
  }

  NS_IMETHOD Run() override {
    nsAutoString json;
    if (!mData.ToJSON(json)) {
      return NS_ERROR_FAILURE;
    }

    nsCOMPtr<nsIObserverService> obsService = services::GetObserverService();
    if (obsService) {
      obsService->NotifyObservers(nullptr, "devtools-html-content",
                                  PromiseFlatString(json).get());
    }

    return NS_OK;
  }

  HTMLContent mData;
};

inline void nsHtml5StreamParser::OnNewContent(Span<const char16_t> aData) {
  if (mURIToSendToDevtools) {
    NS_DispatchToMainThread(new AddContentRunnable(mUUIDForDevtools,
                                                   mURIToSendToDevtools, aData,
                                                   /* aComplete */ false));
  }
}

inline void nsHtml5StreamParser::OnContentComplete() {
  if (mURIToSendToDevtools) {
    NS_DispatchToMainThread(new AddContentRunnable(
        mUUIDForDevtools, mURIToSendToDevtools, Span<const char16_t>(),
        /* aComplete */ true));
    mURIToSendToDevtools = nullptr;
  }
}

nsresult nsHtml5StreamParser::WriteStreamBytes(
    Span<const uint8_t> aFromSegment) {
  NS_ASSERTION(IsParserThread(), "Wrong thread!");
  // mLastBuffer should always point to a buffer of the size
  // READ_BUFFER_SIZE.
  if (!mLastBuffer) {
    NS_WARNING("mLastBuffer should not be null!");
    MarkAsBroken(NS_ERROR_NULL_POINTER);
    return NS_ERROR_NULL_POINTER;
  }
  size_t totalRead = 0;
  auto src = aFromSegment;
  for (;;) {
    auto dst = mLastBuffer->TailAsSpan(READ_BUFFER_SIZE);
    uint32_t result;
    size_t read;
    size_t written;
    bool hadErrors;
    Tie(result, read, written, hadErrors) =
        mUnicodeDecoder->DecodeToUTF16(src, dst, false);
    if (!mDecodingLocalFileWithoutTokenizing) {
      OnNewContent(dst.To(written));
    }
    if (hadErrors && !mHasHadErrors) {
      mHasHadErrors = true;
      if (mEncoding == UTF_8_ENCODING) {
        mTreeBuilder->TryToEnableEncodingMenu();
      }
    }
    src = src.From(read);
    totalRead += read;
    mLastBuffer->AdvanceEnd(written);
    if (result == kOutputFull) {
      RefPtr<nsHtml5OwningUTF16Buffer> newBuf =
          nsHtml5OwningUTF16Buffer::FalliblyCreate(READ_BUFFER_SIZE);
      if (!newBuf) {
        MarkAsBroken(NS_ERROR_OUT_OF_MEMORY);
        return NS_ERROR_OUT_OF_MEMORY;
      }
      mLastBuffer = (mLastBuffer->next = std::move(newBuf));
    } else {
      MOZ_ASSERT(totalRead == aFromSegment.Length(),
                 "The Unicode decoder consumed the wrong number of bytes.");
      if (mDecodingLocalFileWithoutTokenizing &&
          mLocalFileBytesBuffered == LOCAL_FILE_UTF_8_BUFFER_SIZE) {
        auto encoding = mEncoding;
        GuessEncoding(false, false);
        if (encoding == mEncoding) {
          CommitLocalFileToEncoding();
        } else {
          ReDecodeLocalFile();
        }
      }
      return NS_OK;
    }
  }
}

void nsHtml5StreamParser::ReDecodeLocalFile() {
  MOZ_ASSERT(mDecodingLocalFileWithoutTokenizing);
  mDecodingLocalFileWithoutTokenizing = false;
  mUnicodeDecoder = mEncoding->NewDecoderWithBOMRemoval();
  mHasHadErrors = false;

  DontGuessEncoding();

  // Throw away previous decoded data
  mLastBuffer = mFirstBuffer;
  mLastBuffer->next = nullptr;
  mLastBuffer->setStart(0);
  mLastBuffer->setEnd(0);

  // Decode again
  for (auto&& buffer : mBufferedLocalFileData) {
    DoDataAvailable(buffer);
  }
}

void nsHtml5StreamParser::CommitLocalFileToEncoding() {
  MOZ_ASSERT(mDecodingLocalFileWithoutTokenizing);
  mDecodingLocalFileWithoutTokenizing = false;
  mFeedChardet = false;
  mGuessEncoding = false;

  nsHtml5OwningUTF16Buffer* buffer = mFirstBuffer;
  while (buffer) {
    Span<const char16_t> data(buffer->getBuffer() + buffer->getStart(),
                              buffer->getLength());
    OnNewContent(data);
    buffer = buffer->next;
  }
}

class MaybeRunCollector : public Runnable {
 public:
  explicit MaybeRunCollector(nsIDocShell* aDocShell)
      : Runnable("MaybeRunCollector"), mDocShell(aDocShell) {}

  NS_IMETHOD Run() override {
    nsJSContext::MaybeRunNextCollectorSlice(mDocShell,
                                            JS::GCReason::HTML_PARSER);
    return NS_OK;
  }

  nsCOMPtr<nsIDocShell> mDocShell;
};

nsresult nsHtml5StreamParser::OnStartRequest(nsIRequest* aRequest) {
  MOZ_RELEASE_ASSERT(STREAM_NOT_STARTED == mStreamState,
                     "Got OnStartRequest when the stream had already started.");
  MOZ_ASSERT(
      !mExecutor->HasStarted(),
      "Got OnStartRequest at the wrong stage in the executor life cycle.");
  MOZ_ASSERT(NS_IsMainThread(), "Wrong thread!");

  // To avoid the cost of instantiating the detector when it's not needed,
  // let's instantiate only if we make it out of this method with the
  // intent to use it.
  auto detectorCreator = MakeScopeExit([&] {
    if (mFeedChardet) {
      mDetector = mozilla::EncodingDetector::Create();
    }
  });

  mRequest = aRequest;

  mStreamState = STREAM_BEING_READ;

  if (mMode == VIEW_SOURCE_HTML || mMode == VIEW_SOURCE_XML) {
    mTokenizer->StartViewSource(NS_ConvertUTF8toUTF16(mViewSourceTitle));
  }

  // For View Source, the parser should run with scripts "enabled" if a normal
  // load would have scripts enabled.
  bool scriptingEnabled =
      mMode == LOAD_AS_DATA ? false : mExecutor->IsScriptEnabled();
  mOwner->StartTokenizer(scriptingEnabled);

  MOZ_ASSERT(!mDecodingLocalFileWithoutTokenizing);
  bool isSrcdoc = false;
  nsCOMPtr<nsIChannel> channel;
  nsresult rv = GetChannel(getter_AddRefs(channel));
  if (NS_SUCCEEDED(rv)) {
    isSrcdoc = NS_IsSrcdocChannel(channel);
    if (!isSrcdoc && mCharsetSource <= kCharsetFromFallback) {
      nsCOMPtr<nsIURI> originalURI;
      rv = channel->GetOriginalURI(getter_AddRefs(originalURI));
      if (NS_SUCCEEDED(rv)) {
        if (originalURI->SchemeIs("resource")) {
          mCharsetSource = kCharsetFromBuiltIn;
          mEncoding = UTF_8_ENCODING;
          mTreeBuilder->SetDocumentCharset(mEncoding, mCharsetSource);
        } else {
          nsCOMPtr<nsIURI> currentURI;
          rv = channel->GetURI(getter_AddRefs(currentURI));
          if (NS_SUCCEEDED(rv)) {
            nsCOMPtr<nsIURI> innermost = NS_GetInnermostURI(currentURI);
            if (innermost->SchemeIs("file")) {
              mDecodingLocalFileWithoutTokenizing = true;
            } else {
              nsAutoCString host;
              innermost->GetAsciiHost(host);
              if (!host.IsEmpty()) {
                // First let's see if the host is DNS-absolute and ends with a
                // dot and get rid of that one.
                if (host.Last() == '.') {
                  host.SetLength(host.Length() - 1);
                }
                int32_t index = host.RFindChar('.');
                if (index != kNotFound) {
                  // We tolerate an IPv4 component as generic "TLD", so don't
                  // bother checking.
                  ToLowerCase(
                      Substring(host, index + 1, host.Length() - (index + 1)),
                      mTLD);
                }
              }
            }
          }
        }
      }
    }
  }
  mTreeBuilder->setIsSrcdocDocument(isSrcdoc);
  mTreeBuilder->setScriptingEnabled(scriptingEnabled);
  mTreeBuilder->SetPreventScriptExecution(
      !((mMode == NORMAL) && scriptingEnabled));
  mTokenizer->start();
  mExecutor->Start();
  mExecutor->StartReadingFromStage();

  if (mMode == PLAIN_TEXT) {
    mTreeBuilder->StartPlainText();
    mTokenizer->StartPlainText();
  } else if (mMode == VIEW_SOURCE_PLAIN) {
    nsAutoString viewSourceTitle;
    CopyUTF8toUTF16(mViewSourceTitle, viewSourceTitle);
    mTreeBuilder->EnsureBufferSpace(viewSourceTitle.Length());
    mTreeBuilder->StartPlainTextViewSource(viewSourceTitle);
    mTokenizer->StartPlainText();
  }

  /*
   * If you move the following line, be very careful not to cause
   * WillBuildModel to be called before the document has had its
   * script global object set.
   */
  rv = mExecutor->WillBuildModel(eDTDMode_unknown);
  NS_ENSURE_SUCCESS(rv, rv);

  RefPtr<nsHtml5OwningUTF16Buffer> newBuf =
      nsHtml5OwningUTF16Buffer::FalliblyCreate(READ_BUFFER_SIZE);
  if (!newBuf) {
    // marks this stream parser as terminated,
    // which prevents entry to code paths that
    // would use mFirstBuffer or mLastBuffer.
    return mExecutor->MarkAsBroken(NS_ERROR_OUT_OF_MEMORY);
  }
  MOZ_ASSERT(!mFirstBuffer, "How come we have the first buffer set?");
  MOZ_ASSERT(!mLastBuffer, "How come we have the last buffer set?");
  mFirstBuffer = mLastBuffer = newBuf;

  rv = NS_OK;

  // The line below means that the encoding can end up being wrong if
  // a view-source URL is loaded without having the encoding hint from a
  // previous normal load in the history.
  mReparseForbidden = !(mMode == NORMAL || mMode == PLAIN_TEXT);

  mNetworkEventTarget =
      mExecutor->GetDocument()->EventTargetFor(TaskCategory::Network);

  nsCOMPtr<nsIHttpChannel> httpChannel(do_QueryInterface(mRequest, &rv));
  if (NS_SUCCEEDED(rv)) {
    // Non-HTTP channels are bogus enough that we let them work with unlabeled
    // runnables for now. Asserting for HTTP channels only.
    MOZ_ASSERT(mNetworkEventTarget || mMode == LOAD_AS_DATA,
               "How come the network event target is still null?");

    nsAutoCString method;
    Unused << httpChannel->GetRequestMethod(method);
    // XXX does Necko have a way to renavigate POST, etc. without hitting
    // the network?
    if (!method.EqualsLiteral("GET")) {
      // This is the old Gecko behavior but the HTML5 spec disagrees.
      // Don't reparse on POST.
      mReparseForbidden = true;
    }
  }

  // Attempt to retarget delivery of data (via OnDataAvailable) to the parser
  // thread, rather than through the main thread.
  nsCOMPtr<nsIThreadRetargetableRequest> threadRetargetableRequest =
      do_QueryInterface(mRequest, &rv);
  if (threadRetargetableRequest) {
    rv = threadRetargetableRequest->RetargetDeliveryTo(mEventTarget);
    if (NS_SUCCEEDED(rv)) {
      // Parser thread should be now ready to get data from necko and parse it
      // and main thread might have a chance to process a collector slice.
      // We need to do this asynchronously so that necko may continue processing
      // the request.
      nsCOMPtr<nsIRunnable> runnable =
          new MaybeRunCollector(mExecutor->GetDocument()->GetDocShell());
      mozilla::SchedulerGroup::Dispatch(
          mozilla::TaskCategory::GarbageCollection, runnable.forget());
    }
  }

  if (NS_FAILED(rv)) {
    NS_WARNING("Failed to retarget HTML data delivery to the parser thread.");
  }

  if (mCharsetSource == kCharsetFromParentFrame) {
    // Remember this for error reporting.
    mInitialEncodingWasFromParentFrame = true;
  }

  if (!(mCharsetSource == kCharsetFromPendingUserForcedAutoDetection ||
        mCharsetSource == kCharsetFromInitialUserForcedAutoDetection ||
        mCharsetSource == kCharsetFromFinalUserForcedAutoDetection)) {
    if (mCharsetSource >= kCharsetFromFinalAutoDetectionWouldHaveBeenUTF8) {
      DontGuessEncoding();
    }
  }

  if (mCharsetSource < kCharsetFromUtf8OnlyMime) {
    // we aren't ready to commit to an encoding yet
    // leave converter uninstantiated for now
    return NS_OK;
  }

  // We are loading JSON/WebVTT/etc. into a browsing context.
  // There's no need to remove the BOM manually here, because
  // the UTF-8 decoder removes it.
  mReparseForbidden = true;
  DontGuessEncoding();

  // Instantiate the converter here to avoid BOM sniffing.
  mDecodingLocalFileWithoutTokenizing = false;
  mUnicodeDecoder = mEncoding->NewDecoderWithBOMRemoval();
  return NS_OK;
}

void nsHtml5StreamParser::DoStopRequest() {
  NS_ASSERTION(IsParserThread(), "Wrong thread!");
  MOZ_RELEASE_ASSERT(STREAM_BEING_READ == mStreamState,
                     "Stream ended without being open.");
  mTokenizerMutex.AssertCurrentThreadOwns();

  auto guard = MakeScopeExit([&] { OnContentComplete(); });

  if (IsTerminated()) {
    return;
  }

  if (!mUnicodeDecoder) {
    nsresult rv;
    Span<const uint8_t> empty;
    if (NS_FAILED(rv = FinalizeSniffing(empty, 0, true))) {
      MarkAsBroken(rv);
      return;
    }
  }
  if (mFeedChardet) {
    mFeedChardet = false;
    FeedDetector(Span<uint8_t>(), true);
  }

  MOZ_ASSERT(mUnicodeDecoder,
             "Should have a decoder after finalizing sniffing.");

  // mLastBuffer should always point to a buffer of the size
  // READ_BUFFER_SIZE.
  if (!mLastBuffer) {
    NS_WARNING("mLastBuffer should not be null!");
    MarkAsBroken(NS_ERROR_NULL_POINTER);
    return;
  }

  Span<uint8_t> src;  // empty span
  for (;;) {
    auto dst = mLastBuffer->TailAsSpan(READ_BUFFER_SIZE);
    uint32_t result;
    size_t read;
    size_t written;
    bool hadErrors;
    Tie(result, read, written, hadErrors) =
        mUnicodeDecoder->DecodeToUTF16(src, dst, true);
    if (!mDecodingLocalFileWithoutTokenizing) {
      OnNewContent(dst.To(written));
    }
    if (hadErrors && !mHasHadErrors) {
      mHasHadErrors = true;
      if (mEncoding == UTF_8_ENCODING) {
        mTreeBuilder->TryToEnableEncodingMenu();
      }
    }
    MOZ_ASSERT(read == 0, "How come an empty span was read form?");
    mLastBuffer->AdvanceEnd(written);
    if (result == kOutputFull) {
      RefPtr<nsHtml5OwningUTF16Buffer> newBuf =
          nsHtml5OwningUTF16Buffer::FalliblyCreate(READ_BUFFER_SIZE);
      if (!newBuf) {
        MarkAsBroken(NS_ERROR_OUT_OF_MEMORY);
        return;
      }
      mLastBuffer = (mLastBuffer->next = std::move(newBuf));
    } else {
      if (mDecodingLocalFileWithoutTokenizing) {
        MOZ_ASSERT(mLocalFileBytesBuffered < LOCAL_FILE_UTF_8_BUFFER_SIZE);
        MOZ_ASSERT(mGuessEncoding);
        auto encoding = mEncoding;
        GuessEncoding(true, false);
        if (encoding == mEncoding) {
          CommitLocalFileToEncoding();
        } else {
          ReDecodeLocalFile();
          DoStopRequest();
          return;
        }
      } else if (mGuessEncoding) {
        GuessEncoding(true, false);
      }
      break;
    }
  }

  mStreamState = STREAM_ENDED;

  if (IsTerminatedOrInterrupted()) {
    return;
  }

  ParseAvailableData();
}

class nsHtml5RequestStopper : public Runnable {
 private:
  nsHtml5StreamParserPtr mStreamParser;

 public:
  explicit nsHtml5RequestStopper(nsHtml5StreamParser* aStreamParser)
      : Runnable("nsHtml5RequestStopper"), mStreamParser(aStreamParser) {}
  NS_IMETHOD Run() override {
    mozilla::MutexAutoLock autoLock(mStreamParser->mTokenizerMutex);
    mStreamParser->DoStopRequest();
    return NS_OK;
  }
};

nsresult nsHtml5StreamParser::OnStopRequest(nsIRequest* aRequest,
                                            nsresult status) {
  NS_ASSERTION(mRequest == aRequest, "Got Stop on wrong stream.");
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  nsCOMPtr<nsIRunnable> stopper = new nsHtml5RequestStopper(this);
  if (NS_FAILED(mEventTarget->Dispatch(stopper, nsIThread::DISPATCH_NORMAL))) {
    NS_WARNING("Dispatching StopRequest event failed.");
  }
  return NS_OK;
}

void nsHtml5StreamParser::DoDataAvailableBuffer(
    mozilla::Buffer<uint8_t>&& aBuffer) {
  if (MOZ_LIKELY(!mDecodingLocalFileWithoutTokenizing)) {
    DoDataAvailable(aBuffer);
    return;
  }
  CheckedInt<size_t> bufferedPlusLength(aBuffer.Length());
  bufferedPlusLength += mLocalFileBytesBuffered;
  if (!bufferedPlusLength.isValid()) {
    MarkAsBroken(NS_ERROR_OUT_OF_MEMORY);
    return;
  }
  // Ensure that WriteStreamBytes() sees a buffer ending
  // exactly at LOCAL_FILE_UTF_8_BUFFER_SIZE
  // if we are about to cross the threshold. This way,
  // Necko buffer boundaries don't affect user-visible
  // behavior.
  if (bufferedPlusLength.value() <= LOCAL_FILE_UTF_8_BUFFER_SIZE) {
    // Truncation OK, because we just checked the range.
    mLocalFileBytesBuffered = bufferedPlusLength.value();
    mBufferedLocalFileData.AppendElement(std::move(aBuffer));
    DoDataAvailable(mBufferedLocalFileData.LastElement());
  } else {
    // Truncation OK, because the constant is small enough.
    size_t overBoundary =
        bufferedPlusLength.value() - LOCAL_FILE_UTF_8_BUFFER_SIZE;
    MOZ_RELEASE_ASSERT(overBoundary < aBuffer.Length());
    size_t untilBoundary = aBuffer.Length() - overBoundary;
    auto span = aBuffer.AsSpan();
    auto head = span.To(untilBoundary);
    auto tail = span.From(untilBoundary);
    MOZ_RELEASE_ASSERT(mLocalFileBytesBuffered + untilBoundary ==
                       LOCAL_FILE_UTF_8_BUFFER_SIZE);
    // We make a theoretically useless copy here, because avoiding
    // the copy adds too much complexity.
    Maybe<Buffer<uint8_t>> maybe = Buffer<uint8_t>::CopyFrom(head);
    if (maybe.isNothing()) {
      MarkAsBroken(NS_ERROR_OUT_OF_MEMORY);
      return;
    }
    mLocalFileBytesBuffered = LOCAL_FILE_UTF_8_BUFFER_SIZE;
    mBufferedLocalFileData.AppendElement(std::move(*maybe));

    DoDataAvailable(head);
    // Re-decode may have happened here.
    DoDataAvailable(tail);
  }
  // Do this clean-up here to avoid use-after-free when
  // DoDataAvailable is passed a span pointing into an
  // element of mBufferedLocalFileData.
  if (!mDecodingLocalFileWithoutTokenizing) {
    mBufferedLocalFileData.Clear();
  }
}

void nsHtml5StreamParser::DoDataAvailable(Span<const uint8_t> aBuffer) {
  NS_ASSERTION(IsParserThread(), "Wrong thread!");
  MOZ_RELEASE_ASSERT(STREAM_BEING_READ == mStreamState,
                     "DoDataAvailable called when stream not open.");
  mTokenizerMutex.AssertCurrentThreadOwns();

  if (IsTerminated()) {
    return;
  }

  nsresult rv;
  if (HasDecoder()) {
    if (mFeedChardet) {
      FeedDetector(aBuffer, false);
    }
    rv = WriteStreamBytes(aBuffer);
  } else {
    rv = SniffStreamBytes(aBuffer);
  }
  if (NS_FAILED(rv)) {
    MarkAsBroken(rv);
    return;
  }

  if (IsTerminatedOrInterrupted()) {
    return;
  }

  if (mDecodingLocalFileWithoutTokenizing) {
    return;
  }

  ParseAvailableData();

  if (mFlushTimerArmed || mSpeculating) {
    return;
  }

  {
    mozilla::MutexAutoLock flushTimerLock(mFlushTimerMutex);
    mFlushTimer->InitWithNamedFuncCallback(
        nsHtml5StreamParser::TimerCallback, static_cast<void*>(this),
        mFlushTimerEverFired ? StaticPrefs::html5_flushtimer_initialdelay()
                             : StaticPrefs::html5_flushtimer_subsequentdelay(),
        nsITimer::TYPE_ONE_SHOT, "nsHtml5StreamParser::DoDataAvailable");
  }
  mFlushTimerArmed = true;
}

class nsHtml5DataAvailable : public Runnable {
 private:
  nsHtml5StreamParserPtr mStreamParser;
  Buffer<uint8_t> mData;

 public:
  nsHtml5DataAvailable(nsHtml5StreamParser* aStreamParser,
                       Buffer<uint8_t>&& aData)
      : Runnable("nsHtml5DataAvailable"),
        mStreamParser(aStreamParser),
        mData(std::move(aData)) {}
  NS_IMETHOD Run() override {
    mozilla::MutexAutoLock autoLock(mStreamParser->mTokenizerMutex);
    mStreamParser->DoDataAvailableBuffer(std::move(mData));
    return NS_OK;
  }
};

nsresult nsHtml5StreamParser::OnDataAvailable(nsIRequest* aRequest,
                                              nsIInputStream* aInStream,
                                              uint64_t aSourceOffset,
                                              uint32_t aLength) {
  nsresult rv;
  if (NS_FAILED(rv = mExecutor->IsBroken())) {
    return rv;
  }

  MOZ_ASSERT(mRequest == aRequest, "Got data on wrong stream.");
  uint32_t totalRead;
  // Main thread to parser thread dispatch requires copying to buffer first.
  if (MOZ_UNLIKELY(NS_IsMainThread())) {
    Maybe<Buffer<uint8_t>> maybe = Buffer<uint8_t>::Alloc(aLength);
    if (maybe.isNothing()) {
      return mExecutor->MarkAsBroken(NS_ERROR_OUT_OF_MEMORY);
    }
    Buffer<uint8_t> data(std::move(*maybe));
    rv = aInStream->Read(reinterpret_cast<char*>(data.Elements()),
                         data.Length(), &totalRead);
    NS_ENSURE_SUCCESS(rv, rv);
    MOZ_ASSERT(totalRead == aLength);

    nsCOMPtr<nsIRunnable> dataAvailable =
        new nsHtml5DataAvailable(this, std::move(data));
    if (NS_FAILED(mEventTarget->Dispatch(dataAvailable,
                                         nsIThread::DISPATCH_NORMAL))) {
      NS_WARNING("Dispatching DataAvailable event failed.");
    }
    return rv;
  }
  MOZ_ASSERT(IsParserThread(), "Wrong thread!");
  mozilla::MutexAutoLock autoLock(mTokenizerMutex);

  if (MOZ_UNLIKELY(mDecodingLocalFileWithoutTokenizing)) {
    // It's a bit sad to potentially buffer the first 1024
    // bytes in two places, but it's a lot simpler than trying
    // to optitize out that copy. It only happens for local files
    // and not for the http(s) content anyway.
    Maybe<Buffer<uint8_t>> maybe = Buffer<uint8_t>::Alloc(aLength);
    if (maybe.isNothing()) {
      MarkAsBroken(NS_ERROR_OUT_OF_MEMORY);
      return NS_ERROR_OUT_OF_MEMORY;
    }
    Buffer<uint8_t> data(std::move(*maybe));
    rv = aInStream->Read(reinterpret_cast<char*>(data.Elements()),
                         data.Length(), &totalRead);
    NS_ENSURE_SUCCESS(rv, rv);
    MOZ_ASSERT(totalRead == aLength);
    DoDataAvailableBuffer(std::move(data));
    return rv;
  }
  // Read directly from response buffer.
  rv = aInStream->ReadSegments(CopySegmentsToParser, this, aLength, &totalRead);
  NS_ENSURE_SUCCESS(rv, rv);
  MOZ_ASSERT(totalRead == aLength);
  return rv;
}

/* static */
nsresult nsHtml5StreamParser::CopySegmentsToParser(
    nsIInputStream* aInStream, void* aClosure, const char* aFromSegment,
    uint32_t aToOffset, uint32_t aCount, uint32_t* aWriteCount) {
  nsHtml5StreamParser* parser = static_cast<nsHtml5StreamParser*>(aClosure);

  parser->DoDataAvailable(AsBytes(Span(aFromSegment, aCount)));
  // Assume DoDataAvailable consumed all available bytes.
  *aWriteCount = aCount;
  return NS_OK;
}

const Encoding* nsHtml5StreamParser::PreferredForInternalEncodingDecl(
    const nsACString& aEncoding) {
  const Encoding* newEncoding = Encoding::ForLabel(aEncoding);
  if (!newEncoding) {
    // the encoding name is bogus
    mTreeBuilder->MaybeComplainAboutCharset("EncMetaUnsupported", true,
                                            mTokenizer->getLineNumber());
    return nullptr;
  }

  if (newEncoding == UTF_16BE_ENCODING || newEncoding == UTF_16LE_ENCODING) {
    mTreeBuilder->MaybeComplainAboutCharset("EncMetaUtf16", true,
                                            mTokenizer->getLineNumber());
    newEncoding = UTF_8_ENCODING;
  }

  if (newEncoding == X_USER_DEFINED_ENCODING) {
    // WebKit/Blink hack for Indian and Armenian legacy sites
    mTreeBuilder->MaybeComplainAboutCharset("EncMetaUserDefined", true,
                                            mTokenizer->getLineNumber());
    newEncoding = WINDOWS_1252_ENCODING;
  }

  if (newEncoding == mEncoding) {
    if (mCharsetSource < kCharsetFromMetaPrescan) {
      if (mInitialEncodingWasFromParentFrame) {
        mTreeBuilder->MaybeComplainAboutCharset("EncLateMetaFrame", false,
                                                mTokenizer->getLineNumber());
      } else {
        mTreeBuilder->MaybeComplainAboutCharset("EncLateMeta", false,
                                                mTokenizer->getLineNumber());
      }
    }
    mCharsetSource = kCharsetFromMetaTag;  // become confident
    mTreeBuilder->SetDocumentCharset(mEncoding, mCharsetSource);
    DontGuessEncoding();  // don't feed chardet when confident
    return nullptr;
  }

  return newEncoding;
}

bool nsHtml5StreamParser::internalEncodingDeclaration(nsHtml5String aEncoding) {
  // This code needs to stay in sync with
  // nsHtml5MetaScanner::tryCharset. Unfortunately, the
  // trickery with member fields there leads to some copy-paste reuse. :-(
  NS_ASSERTION(IsParserThread(), "Wrong thread!");
  if (mCharsetSource >= kCharsetFromMetaTag) {  // this threshold corresponds to
                                                // "confident" in the HTML5 spec
    return false;
  }

  nsString newEncoding16;  // Not Auto, because using it to hold nsStringBuffer*
  aEncoding.ToString(newEncoding16);
  nsAutoCString newEncoding;
  CopyUTF16toUTF8(newEncoding16, newEncoding);

  auto encoding = PreferredForInternalEncodingDecl(newEncoding);
  if (!encoding) {
    return false;
  }

  if (mReparseForbidden) {
    // This mReparseForbidden check happens after the call to
    // PreferredForInternalEncodingDecl so that if that method calls
    // MaybeComplainAboutCharset, its charset complaint wins over the one
    // below.
    mTreeBuilder->MaybeComplainAboutCharset("EncLateMetaTooLate", true,
                                            mTokenizer->getLineNumber());
    return false;  // not reparsing even if we wanted to
  }

  // Avoid having the chardet ask for another restart after this restart
  // request.
  DontGuessEncoding();
  mTreeBuilder->NeedsCharsetSwitchTo(WrapNotNull(encoding), kCharsetFromMetaTag,
                                     mTokenizer->getLineNumber());
  FlushTreeOpsAndDisarmTimer();
  Interrupt();
  // the tree op executor will cause the stream parser to terminate
  // if the charset switch request is accepted or it'll uninterrupt
  // if the request failed. Note that if the restart request fails,
  // we don't bother trying to make chardet resume. Might as well
  // assume that chardet-requested restarts would fail, too.
  return true;
}

void nsHtml5StreamParser::FlushTreeOpsAndDisarmTimer() {
  NS_ASSERTION(IsParserThread(), "Wrong thread!");
  if (mFlushTimerArmed) {
    // avoid calling Cancel if the flush timer isn't armed to avoid acquiring
    // a mutex
    {
      mozilla::MutexAutoLock flushTimerLock(mFlushTimerMutex);
      mFlushTimer->Cancel();
    }
    mFlushTimerArmed = false;
  }
  if (mMode == VIEW_SOURCE_HTML || mMode == VIEW_SOURCE_XML) {
    mTokenizer->FlushViewSource();
  }
  mTreeBuilder->Flush();
  nsCOMPtr<nsIRunnable> runnable(mExecutorFlusher);
  if (NS_FAILED(DispatchToMain(runnable.forget()))) {
    NS_WARNING("failed to dispatch executor flush event");
  }
}

void nsHtml5StreamParser::ParseAvailableData() {
  MOZ_ASSERT(IsParserThread(), "Wrong thread!");
  mTokenizerMutex.AssertCurrentThreadOwns();
  MOZ_ASSERT(!mDecodingLocalFileWithoutTokenizing);

  if (IsTerminatedOrInterrupted()) {
    return;
  }

  if (mSpeculating && !IsSpeculationEnabled()) {
    return;
  }

  for (;;) {
    if (!mFirstBuffer->hasMore()) {
      if (mFirstBuffer == mLastBuffer) {
        switch (mStreamState) {
          case STREAM_BEING_READ:
            // never release the last buffer.
            if (!mSpeculating) {
              // reuse buffer space if not speculating
              mFirstBuffer->setStart(0);
              mFirstBuffer->setEnd(0);
            }
            mTreeBuilder->FlushLoads();
            {
              // Dispatch this runnable unconditionally, because the loads
              // that need flushing may have been flushed earlier even if the
              // flush right above here did nothing.
              nsCOMPtr<nsIRunnable> runnable(mLoadFlusher);
              if (NS_FAILED(DispatchToMain(runnable.forget()))) {
                NS_WARNING("failed to dispatch load flush event");
              }
            }
            return;  // no more data for now but expecting more
          case STREAM_ENDED:
            if (mAtEOF) {
              return;
            }
            mAtEOF = true;
            if (mCharsetSource < kCharsetFromMetaTag) {
              if (mInitialEncodingWasFromParentFrame) {
                // Unfortunately, this check doesn't take effect for
                // cross-origin frames, so cross-origin ad frames that have
                // no text and only an image or a Flash embed get the more
                // severe message from the next if block. The message is
                // technically accurate, though.
                mTreeBuilder->MaybeComplainAboutCharset("EncNoDeclarationFrame",
                                                        false, 0);
              } else if (mMode == NORMAL) {
                mTreeBuilder->MaybeComplainAboutCharset("EncNoDeclaration",
                                                        true, 0);
              } else if (mMode == PLAIN_TEXT) {
                mTreeBuilder->MaybeComplainAboutCharset("EncNoDeclarationPlain",
                                                        true, 0);
              }
            }
            if (NS_SUCCEEDED(mTreeBuilder->IsBroken())) {
              mTokenizer->eof();
              nsresult rv;
              if (NS_FAILED((rv = mTreeBuilder->IsBroken()))) {
                MarkAsBroken(rv);
              } else {
                mTreeBuilder->StreamEnded();
                if (mMode == VIEW_SOURCE_HTML || mMode == VIEW_SOURCE_XML) {
                  mTokenizer->EndViewSource();
                }
              }
            }
            FlushTreeOpsAndDisarmTimer();
            return;  // no more data and not expecting more
          default:
            MOZ_ASSERT_UNREACHABLE("It should be impossible to reach this.");
            return;
        }
      }
      mFirstBuffer = mFirstBuffer->next;
      continue;
    }

    // now we have a non-empty buffer
    mFirstBuffer->adjust(mLastWasCR);
    mLastWasCR = false;
    if (mFirstBuffer->hasMore()) {
      if (!mTokenizer->EnsureBufferSpace(mFirstBuffer->getLength())) {
        MarkAsBroken(NS_ERROR_OUT_OF_MEMORY);
        return;
      }
      mLastWasCR = mTokenizer->tokenizeBuffer(mFirstBuffer);
      nsresult rv;
      if (NS_FAILED((rv = mTreeBuilder->IsBroken()))) {
        MarkAsBroken(rv);
        return;
      }
      // At this point, internalEncodingDeclaration() may have called
      // Terminate, but that never happens together with script.
      // Can't assert that here, though, because it's possible that the main
      // thread has called Terminate() while this thread was parsing.
      if (mTreeBuilder->HasScript()) {
        // HasScript() cannot return true if the tree builder is preventing
        // script execution.
        MOZ_ASSERT(mMode == NORMAL);
        mozilla::MutexAutoLock speculationAutoLock(mSpeculationMutex);
        nsHtml5Speculation* speculation = new nsHtml5Speculation(
            mFirstBuffer, mFirstBuffer->getStart(), mTokenizer->getLineNumber(),
            mTreeBuilder->newSnapshot());
        mTreeBuilder->AddSnapshotToScript(speculation->GetSnapshot(),
                                          speculation->GetStartLineNumber());
        FlushTreeOpsAndDisarmTimer();
        mTreeBuilder->SetOpSink(speculation);
        mSpeculations.AppendElement(speculation);  // adopts the pointer
        mSpeculating = true;
      }
      if (IsTerminatedOrInterrupted()) {
        return;
      }
    }
  }
}

class nsHtml5StreamParserContinuation : public Runnable {
 private:
  nsHtml5StreamParserPtr mStreamParser;

 public:
  explicit nsHtml5StreamParserContinuation(nsHtml5StreamParser* aStreamParser)
      : Runnable("nsHtml5StreamParserContinuation"),
        mStreamParser(aStreamParser) {}
  NS_IMETHOD Run() override {
    mozilla::MutexAutoLock autoLock(mStreamParser->mTokenizerMutex);
    mStreamParser->Uninterrupt();
    mStreamParser->ParseAvailableData();
    return NS_OK;
  }
};

void nsHtml5StreamParser::ContinueAfterScripts(nsHtml5Tokenizer* aTokenizer,
                                               nsHtml5TreeBuilder* aTreeBuilder,
                                               bool aLastWasCR) {
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(!(mMode == VIEW_SOURCE_HTML || mMode == VIEW_SOURCE_XML),
               "ContinueAfterScripts called in view source mode!");
  if (NS_FAILED(mExecutor->IsBroken())) {
    return;
  }
#ifdef DEBUG
  mExecutor->AssertStageEmpty();
#endif
  bool speculationFailed = false;
  {
    mozilla::MutexAutoLock speculationAutoLock(mSpeculationMutex);
    if (mSpeculations.IsEmpty()) {
      MOZ_ASSERT_UNREACHABLE(
          "ContinueAfterScripts called without "
          "speculations.");
      return;
    }

    const auto& speculation = mSpeculations.ElementAt(0);
    if (aLastWasCR || !aTokenizer->isInDataState() ||
        !aTreeBuilder->snapshotMatches(speculation->GetSnapshot())) {
      speculationFailed = true;
      // We've got a failed speculation :-(
      MaybeDisableFutureSpeculation();
      Interrupt();  // Make the parser thread release the tokenizer mutex sooner
      // now fall out of the speculationAutoLock into the tokenizerAutoLock
      // block
    } else {
      // We've got a successful speculation!
      if (mSpeculations.Length() > 1) {
        // the first speculation isn't the current speculation, so there's
        // no need to bother the parser thread.
        speculation->FlushToSink(mExecutor);
        NS_ASSERTION(!mExecutor->IsScriptExecuting(),
                     "ParseUntilBlocked() was supposed to ensure we don't come "
                     "here when scripts are executing.");
        NS_ASSERTION(
            mExecutor->IsInFlushLoop(),
            "How are we here if "
            "RunFlushLoop() didn't call ParseUntilBlocked() which is the "
            "only caller of this method?");
        mSpeculations.RemoveElementAt(0);
        return;
      }
      // else
      Interrupt();  // Make the parser thread release the tokenizer mutex sooner

      // now fall through
      // the first speculation is the current speculation. Need to
      // release the the speculation mutex and acquire the tokenizer
      // mutex. (Just acquiring the other mutex here would deadlock)
    }
  }
  {
    mozilla::MutexAutoLock tokenizerAutoLock(mTokenizerMutex);
#ifdef DEBUG
    {
      mAtomTable.SetPermittedLookupEventTarget(
          GetMainThreadSerialEventTarget());
    }
#endif
    // In principle, the speculation mutex should be acquired here,
    // but there's no point, because the parser thread only acquires it
    // when it has also acquired the tokenizer mutex and we are already
    // holding the tokenizer mutex.
    if (speculationFailed) {
      // Rewind the stream
      mAtEOF = false;
      const auto& speculation = mSpeculations.ElementAt(0);
      mFirstBuffer = speculation->GetBuffer();
      mFirstBuffer->setStart(speculation->GetStart());
      mTokenizer->setLineNumber(speculation->GetStartLineNumber());

      nsContentUtils::ReportToConsole(
          nsIScriptError::warningFlag, "DOM Events"_ns,
          mExecutor->GetDocument(), nsContentUtils::eDOM_PROPERTIES,
          "SpeculationFailed", nsTArray<nsString>(), nullptr, u""_ns,
          speculation->GetStartLineNumber());

      nsHtml5OwningUTF16Buffer* buffer = mFirstBuffer->next;
      while (buffer) {
        buffer->setStart(0);
        buffer = buffer->next;
      }

      mSpeculations.Clear();  // potentially a huge number of destructors
                              // run here synchronously on the main thread...

      mTreeBuilder->flushCharacters();  // empty the pending buffer
      mTreeBuilder->ClearOps();         // now get rid of the failed ops

      mTreeBuilder->SetOpSink(mExecutor->GetStage());
      mExecutor->StartReadingFromStage();
      mSpeculating = false;

      // Copy state over
      mLastWasCR = aLastWasCR;
      mTokenizer->loadState(aTokenizer);
      mTreeBuilder->loadState(aTreeBuilder);
    } else {
      // We've got a successful speculation and at least a moment ago it was
      // the current speculation
      mSpeculations.ElementAt(0)->FlushToSink(mExecutor);
      NS_ASSERTION(!mExecutor->IsScriptExecuting(),
                   "ParseUntilBlocked() was supposed to ensure we don't come "
                   "here when scripts are executing.");
      NS_ASSERTION(
          mExecutor->IsInFlushLoop(),
          "How are we here if "
          "RunFlushLoop() didn't call ParseUntilBlocked() which is the "
          "only caller of this method?");
      mSpeculations.RemoveElementAt(0);
      if (mSpeculations.IsEmpty()) {
        // yes, it was still the only speculation. Now stop speculating
        // However, before telling the executor to read from stage, flush
        // any pending ops straight to the executor, because otherwise
        // they remain unflushed until we get more data from the network.
        mTreeBuilder->SetOpSink(mExecutor);
        mTreeBuilder->Flush(true);
        mTreeBuilder->SetOpSink(mExecutor->GetStage());
        mExecutor->StartReadingFromStage();
        mSpeculating = false;
      }
    }
    nsCOMPtr<nsIRunnable> event = new nsHtml5StreamParserContinuation(this);
    if (NS_FAILED(mEventTarget->Dispatch(event, nsIThread::DISPATCH_NORMAL))) {
      NS_WARNING("Failed to dispatch nsHtml5StreamParserContinuation");
    }
// A stream event might run before this event runs, but that's harmless.
#ifdef DEBUG
    mAtomTable.SetPermittedLookupEventTarget(mEventTarget);
#endif
  }
}

void nsHtml5StreamParser::ContinueAfterFailedCharsetSwitch() {
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  nsCOMPtr<nsIRunnable> event = new nsHtml5StreamParserContinuation(this);
  if (NS_FAILED(mEventTarget->Dispatch(event, nsIThread::DISPATCH_NORMAL))) {
    NS_WARNING("Failed to dispatch nsHtml5StreamParserContinuation");
  }
}

class nsHtml5TimerKungFu : public Runnable {
 private:
  nsHtml5StreamParserPtr mStreamParser;

 public:
  explicit nsHtml5TimerKungFu(nsHtml5StreamParser* aStreamParser)
      : Runnable("nsHtml5TimerKungFu"), mStreamParser(aStreamParser) {}
  NS_IMETHOD Run() override {
    mozilla::MutexAutoLock flushTimerLock(mStreamParser->mFlushTimerMutex);
    if (mStreamParser->mFlushTimer) {
      mStreamParser->mFlushTimer->Cancel();
      mStreamParser->mFlushTimer = nullptr;
    }
    return NS_OK;
  }
};

void nsHtml5StreamParser::DropTimer() {
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  /*
   * Simply nulling out the timer wouldn't work, because if the timer is
   * armed, it needs to be canceled first. Simply canceling it first wouldn't
   * work, because nsTimerImpl::Cancel is not safe for calling from outside
   * the thread where nsTimerImpl::Fire would run. It's not safe to
   * dispatch a runnable to cancel the timer from the destructor of this
   * class, because the timer has a weak (void*) pointer back to this instance
   * of the stream parser and having the timer fire before the runnable
   * cancels it would make the timer access a deleted object.
   *
   * This DropTimer method addresses these issues. This method must be called
   * on the main thread before the destructor of this class is reached.
   * The nsHtml5TimerKungFu object has an nsHtml5StreamParserPtr that addrefs
   * this
   * stream parser object to keep it alive until the runnable is done.
   * The runnable cancels the timer on the parser thread, drops the timer
   * and lets nsHtml5StreamParserPtr send a runnable back to the main thread to
   * release the stream parser.
   */
  mozilla::MutexAutoLock flushTimerLock(mFlushTimerMutex);
  if (mFlushTimer) {
    nsCOMPtr<nsIRunnable> event = new nsHtml5TimerKungFu(this);
    if (NS_FAILED(mEventTarget->Dispatch(event, nsIThread::DISPATCH_NORMAL))) {
      NS_WARNING("Failed to dispatch TimerKungFu event");
    }
  }
}

// Using a static, because the method name Notify is taken by the chardet
// callback.
void nsHtml5StreamParser::TimerCallback(nsITimer* aTimer, void* aClosure) {
  (static_cast<nsHtml5StreamParser*>(aClosure))->TimerFlush();
}

void nsHtml5StreamParser::TimerFlush() {
  NS_ASSERTION(IsParserThread(), "Wrong thread!");
  mozilla::MutexAutoLock autoLock(mTokenizerMutex);

  NS_ASSERTION(!mSpeculating, "Flush timer fired while speculating.");

  // The timer fired if we got here. No need to cancel it. Mark it as
  // not armed, though.
  mFlushTimerArmed = false;

  mFlushTimerEverFired = true;

  if (IsTerminatedOrInterrupted()) {
    return;
  }

  if (mMode == VIEW_SOURCE_HTML || mMode == VIEW_SOURCE_XML) {
    mTreeBuilder->Flush();  // delete useless ops
    if (mTokenizer->FlushViewSource()) {
      nsCOMPtr<nsIRunnable> runnable(mExecutorFlusher);
      if (NS_FAILED(DispatchToMain(runnable.forget()))) {
        NS_WARNING("failed to dispatch executor flush event");
      }
    }
  } else {
    // we aren't speculating and we don't know when new data is
    // going to arrive. Send data to the main thread.
    if (mTreeBuilder->Flush(true)) {
      nsCOMPtr<nsIRunnable> runnable(mExecutorFlusher);
      if (NS_FAILED(DispatchToMain(runnable.forget()))) {
        NS_WARNING("failed to dispatch executor flush event");
      }
    }
  }
}

void nsHtml5StreamParser::MarkAsBroken(nsresult aRv) {
  NS_ASSERTION(IsParserThread(), "Wrong thread!");
  mTokenizerMutex.AssertCurrentThreadOwns();

  Terminate();
  mTreeBuilder->MarkAsBroken(aRv);
  mozilla::DebugOnly<bool> hadOps = mTreeBuilder->Flush(false);
  NS_ASSERTION(hadOps, "Should have had the markAsBroken op!");
  nsCOMPtr<nsIRunnable> runnable(mExecutorFlusher);
  if (NS_FAILED(DispatchToMain(runnable.forget()))) {
    NS_WARNING("failed to dispatch executor flush event");
  }
}

nsresult nsHtml5StreamParser::DispatchToMain(
    already_AddRefed<nsIRunnable>&& aRunnable) {
  if (mNetworkEventTarget) {
    return mNetworkEventTarget->Dispatch(std::move(aRunnable));
  }
  return SchedulerGroup::UnlabeledDispatch(TaskCategory::Network,
                                           std::move(aRunnable));
}
