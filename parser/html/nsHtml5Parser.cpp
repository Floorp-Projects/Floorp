/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=2 et tw=79: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsContentUtils.h"
#include "nsHtml5Tokenizer.h"
#include "nsHtml5TreeBuilder.h"
#include "nsHtml5Parser.h"
#include "nsHtml5AtomTable.h"
#include "nsHtml5DependentUTF16Buffer.h"
#include "nsIInputStreamChannel.h"

NS_INTERFACE_TABLE_HEAD(nsHtml5Parser)
  NS_INTERFACE_TABLE2(nsHtml5Parser, nsIParser, nsISupportsWeakReference)
  NS_INTERFACE_TABLE_TO_MAP_SEGUE_CYCLE_COLLECTION(nsHtml5Parser)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsHtml5Parser)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsHtml5Parser)

NS_IMPL_CYCLE_COLLECTION_CLASS(nsHtml5Parser)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(nsHtml5Parser)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mExecutor)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mStreamParser)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(nsHtml5Parser)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mExecutor)
  tmp->DropStreamParser();
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

nsHtml5Parser::nsHtml5Parser()
  : mFirstBuffer(new nsHtml5OwningUTF16Buffer((void*)nullptr))
  , mLastBuffer(mFirstBuffer)
  , mExecutor(new nsHtml5TreeOpExecutor())
  , mTreeBuilder(new nsHtml5TreeBuilder(mExecutor, nullptr))
  , mTokenizer(new nsHtml5Tokenizer(mTreeBuilder, false))
  , mRootContextLineNumber(1)
{
  mAtomTable.Init(); // we aren't checking for OOM anyway...
  mTokenizer->setInterner(&mAtomTable);
  // There's a zeroing operator new for everything else
}

nsHtml5Parser::~nsHtml5Parser()
{
  mTokenizer->end();
  if (mDocWriteSpeculativeTokenizer) {
    mDocWriteSpeculativeTokenizer->end();
  }
}

NS_IMETHODIMP_(void)
nsHtml5Parser::SetContentSink(nsIContentSink* aSink)
{
  NS_ASSERTION(aSink == static_cast<nsIContentSink*> (mExecutor), 
               "Attempt to set a foreign sink.");
}

NS_IMETHODIMP_(nsIContentSink*)
nsHtml5Parser::GetContentSink()
{
  return static_cast<nsIContentSink*> (mExecutor);
}

NS_IMETHODIMP_(void)
nsHtml5Parser::GetCommand(nsCString& aCommand)
{
  aCommand.Assign("view");
}

NS_IMETHODIMP_(void)
nsHtml5Parser::SetCommand(const char* aCommand)
{
  NS_ASSERTION(!strcmp(aCommand, "view") ||
               !strcmp(aCommand, "view-source") ||
               !strcmp(aCommand, "external-resource") ||
               !strcmp(aCommand, kLoadAsData),
               "Unsupported parser command");
}

NS_IMETHODIMP_(void)
nsHtml5Parser::SetCommand(eParserCommands aParserCommand)
{
  NS_ASSERTION(aParserCommand == eViewNormal, 
               "Parser command was not eViewNormal.");
}

NS_IMETHODIMP_(void)
nsHtml5Parser::SetDocumentCharset(const nsACString& aCharset,
                                  int32_t aCharsetSource)
{
  NS_PRECONDITION(!mExecutor->HasStarted(),
                  "Document charset set too late.");
  NS_PRECONDITION(mStreamParser, "Setting charset on a script-only parser.");
  nsAutoCString trimmed;
  trimmed.Assign(aCharset);
  trimmed.Trim(" \t\r\n\f");
  mStreamParser->SetDocumentCharset(trimmed, aCharsetSource);
  mExecutor->SetDocumentCharsetAndSource(trimmed,
                                         aCharsetSource);
}

NS_IMETHODIMP
nsHtml5Parser::GetChannel(nsIChannel** aChannel)
{
  if (mStreamParser) {
    return mStreamParser->GetChannel(aChannel);
  } else {
    return NS_ERROR_NOT_AVAILABLE;
  }
}

NS_IMETHODIMP
nsHtml5Parser::GetDTD(nsIDTD** aDTD)
{
  *aDTD = nullptr;
  return NS_OK;
}

nsIStreamListener*
nsHtml5Parser::GetStreamListener()
{
  return mStreamParser;
}

NS_IMETHODIMP
nsHtml5Parser::ContinueInterruptedParsing()
{
  NS_NOTREACHED("Don't call. For interface compat only.");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP_(void)
nsHtml5Parser::BlockParser()
{
  mBlocked = true;
}

NS_IMETHODIMP_(void)
nsHtml5Parser::UnblockParser()
{
  mBlocked = false;
  mExecutor->ContinueInterruptedParsingAsync();
}

NS_IMETHODIMP_(void)
nsHtml5Parser::ContinueInterruptedParsingAsync()
{
  mExecutor->ContinueInterruptedParsingAsync();
}

NS_IMETHODIMP_(bool)
nsHtml5Parser::IsParserEnabled()
{
  return !mBlocked;
}

NS_IMETHODIMP_(bool)
nsHtml5Parser::IsComplete()
{
  return mExecutor->IsComplete();
}

NS_IMETHODIMP
nsHtml5Parser::Parse(nsIURI* aURL,
                     nsIRequestObserver* aObserver,
                     void* aKey, // legacy; ignored
                     nsDTDMode aMode) // legacy; ignored
{
  /*
   * Do NOT cause WillBuildModel to be called synchronously from here!
   * The document won't be ready for it until OnStartRequest!
   */
  NS_PRECONDITION(!mExecutor->HasStarted(), 
                  "Tried to start parse without initializing the parser.");
  NS_PRECONDITION(mStreamParser, 
                  "Can't call this Parse() variant on script-created parser");
  mStreamParser->SetObserver(aObserver);
  mStreamParser->SetViewSourceTitle(aURL); // In case we're viewing source
  mExecutor->SetStreamParser(mStreamParser);
  mExecutor->SetParser(this);
  return NS_OK;
}

NS_IMETHODIMP
nsHtml5Parser::Parse(const nsAString& aSourceBuffer,
                     void* aKey,
                     const nsACString& aContentType,
                     bool aLastCall,
                     nsDTDMode aMode) // ignored
{
  nsresult rv;
  if (NS_FAILED(rv = mExecutor->IsBroken())) {
    return rv;
  }
  if (aSourceBuffer.Length() > INT32_MAX) {
    return mExecutor->MarkAsBroken(NS_ERROR_OUT_OF_MEMORY);
  }

  // Maintain a reference to ourselves so we don't go away
  // till we're completely done. The old parser grips itself in this method.
  nsCOMPtr<nsIParser> kungFuDeathGrip(this);
  
  // Gripping the other objects just in case, since the other old grip
  // required grips to these, too.
  nsRefPtr<nsHtml5StreamParser> streamKungFuDeathGrip(mStreamParser);
  nsRefPtr<nsHtml5TreeOpExecutor> treeOpKungFuDeathGrip(mExecutor);

  if (!mExecutor->HasStarted()) {
    NS_ASSERTION(!mStreamParser,
                 "Had stream parser but document.write started life cycle.");
    // This is the first document.write() on a document.open()ed document
    mExecutor->SetParser(this);
    mTreeBuilder->setScriptingEnabled(mExecutor->IsScriptEnabled());

    mTreeBuilder->setIsSrcdocDocument(IsSrcdocDocument());

    mTokenizer->start();
    mExecutor->Start();
    if (!aContentType.EqualsLiteral("text/html")) {
      mTreeBuilder->StartPlainText();
      mTokenizer->StartPlainText();
    }
    /*
     * If you move the following line, be very careful not to cause 
     * WillBuildModel to be called before the document has had its 
     * script global object set.
     */
    mExecutor->WillBuildModel(eDTDMode_unknown);
  }

  // Return early if the parser has processed EOF
  if (mExecutor->IsComplete()) {
    return NS_OK;
  }

  if (aLastCall && aSourceBuffer.IsEmpty() && !aKey) {
    // document.close()
    NS_ASSERTION(!mStreamParser,
                 "Had stream parser but got document.close().");
    if (mDocumentClosed) {
      // already closed
      return NS_OK;
    }
    mDocumentClosed = true;
    if (!mBlocked && !mInDocumentWrite) {
      ParseUntilBlocked();
    }
    return NS_OK;
  }

  // If we got this far, we are dealing with a document.write or
  // document.writeln call--not document.close().

  NS_ASSERTION(IsInsertionPointDefined(),
               "Doc.write reached parser with undefined insertion point.");

  NS_ASSERTION(!(mStreamParser && !aKey),
               "Got a null key in a non-script-created parser");

  // XXX is this optimization bogus?
  if (aSourceBuffer.IsEmpty()) {
    return NS_OK;
  }

  // This guard is here to prevent document.close from tokenizing synchronously
  // while a document.write (that wrote the script that called document.close!)
  // is still on the call stack.
  mozilla::AutoRestore<bool> guard(mInDocumentWrite);
  mInDocumentWrite = true;

  // The script is identified by aKey. If there's nothing in the buffer
  // chain for that key, we'll insert at the head of the queue.
  // When the script leaves something in the queue, a zero-length
  // key-holder "buffer" is inserted in the queue. If the same script
  // leaves something in the chain again, it will be inserted immediately
  // before the old key holder belonging to the same script.
  //
  // We don't do the actual data insertion yet in the hope that the data gets
  // tokenized and there no data or less data to copy to the heap after
  // tokenization. Also, this way, we avoid inserting one empty data buffer
  // per document.write, which matters for performance when the parser isn't
  // blocked and a badly-authored script calls document.write() once per
  // input character. (As seen in a benchmark!)
  //
  // The insertion into the input stream happens conceptually before anything
  // gets tokenized. To make sure multi-level document.write works right,
  // it's necessary to establish the location of our parser key up front
  // in case this is the first write with this key.
  //
  // In a document.open() case, the first write level has a null key, so that
  // case is handled separately, because normal buffers containing data
  // have null keys.

  // These don't need to be owning references, because they always point to
  // the buffer queue and buffers can't be removed from the buffer queue
  // before document.write() returns. The buffer queue clean-up happens the
  // next time ParseUntilBlocked() is called.
  // However, they are made owning just in case the reasoning above is flawed
  // and a flaw would lead to worse problems with plain pointers. If this
  // turns out to be a perf problem, it's worthwhile to consider making
  // prevSearchbuf a plain pointer again.
  nsRefPtr<nsHtml5OwningUTF16Buffer> prevSearchBuf;
  nsRefPtr<nsHtml5OwningUTF16Buffer> firstLevelMarker;

  if (aKey) {
    if (mFirstBuffer == mLastBuffer) {
      nsHtml5OwningUTF16Buffer* keyHolder = new nsHtml5OwningUTF16Buffer(aKey);
      keyHolder->next = mLastBuffer;
      mFirstBuffer = keyHolder;
    } else if (mFirstBuffer->key != aKey) {
      prevSearchBuf = mFirstBuffer;
      for (;;) {
        if (prevSearchBuf->next == mLastBuffer) {
          // key was not found
          nsHtml5OwningUTF16Buffer* keyHolder =
            new nsHtml5OwningUTF16Buffer(aKey);
          keyHolder->next = mFirstBuffer;
          mFirstBuffer = keyHolder;
          prevSearchBuf = nullptr;
          break;
        }
        if (prevSearchBuf->next->key == aKey) {
          // found a key holder
          break;
        }
        prevSearchBuf = prevSearchBuf->next;
      }
    } // else mFirstBuffer is the keyholder

    // prevSearchBuf is the previous buffer before the keyholder or null if
    // there isn't one.
  } else {
    // We have a first-level write in the document.open() case. We insert before
    // mLastBuffer, effectively, by making mLastBuffer be a new sentinel object
    // and redesignating the previous mLastBuffer as our firstLevelMarker.  We
    // need to put a marker there, because otherwise additional document.writes
    // from nested event loops would insert in the wrong place. Sigh.
    mLastBuffer->next = new nsHtml5OwningUTF16Buffer((void*)nullptr);
    firstLevelMarker = mLastBuffer;
    mLastBuffer = mLastBuffer->next;
  }

  nsHtml5DependentUTF16Buffer stackBuffer(aSourceBuffer);

  while (!mBlocked && stackBuffer.hasMore()) {
    stackBuffer.adjust(mLastWasCR);
    mLastWasCR = false;
    if (stackBuffer.hasMore()) {
      int32_t lineNumberSave;
      bool inRootContext = (!mStreamParser && !aKey);
      if (inRootContext) {
        mTokenizer->setLineNumber(mRootContextLineNumber);
      } else {
        // we aren't the root context, so save the line number on the
        // *stack* so that we can restore it.
        lineNumberSave = mTokenizer->getLineNumber();
      }

      mLastWasCR = mTokenizer->tokenizeBuffer(&stackBuffer);

      if (inRootContext) {
        mRootContextLineNumber = mTokenizer->getLineNumber();
      } else {
        mTokenizer->setLineNumber(lineNumberSave);
      }

      if (mTreeBuilder->HasScript()) {
        mTreeBuilder->Flush(); // Move ops to the executor
        mExecutor->FlushDocumentWrite(); // run the ops
        // Flushing tree ops can cause all sorts of things.
        // Return early if the parser got terminated.
        if (mExecutor->IsComplete()) {
          return NS_OK;
        }
      }
      // Ignore suspension requests
    }
  }

  nsRefPtr<nsHtml5OwningUTF16Buffer> heapBuffer;
  if (stackBuffer.hasMore()) {
    // The buffer wasn't tokenized to completion. Create a copy of the tail
    // on the heap.
    heapBuffer = stackBuffer.FalliblyCopyAsOwningBuffer();
    if (!heapBuffer) {
      // Allocation failed. The parser is now broken.
      return mExecutor->MarkAsBroken(NS_ERROR_OUT_OF_MEMORY);
    }
  }

  if (heapBuffer) {
    // We have something to insert before the keyholder holding in the non-null
    // aKey case and we have something to swap into firstLevelMarker in the
    // null aKey case.
    if (aKey) {
      NS_ASSERTION(mFirstBuffer != mLastBuffer,
        "Where's the keyholder?");
      // the key holder is still somewhere further down the list from
      // prevSearchBuf (which may be null)
      if (mFirstBuffer->key == aKey) {
        NS_ASSERTION(!prevSearchBuf,
          "Non-null prevSearchBuf when mFirstBuffer is the key holder?");
        heapBuffer->next = mFirstBuffer;
        mFirstBuffer = heapBuffer;
      } else {
        if (!prevSearchBuf) {
          prevSearchBuf = mFirstBuffer;
        }
        // We created a key holder earlier, so we will find it without walking
        // past the end of the list.
        while (prevSearchBuf->next->key != aKey) {
          prevSearchBuf = prevSearchBuf->next;
        }
        heapBuffer->next = prevSearchBuf->next;
        prevSearchBuf->next = heapBuffer;
      }
    } else {
      NS_ASSERTION(firstLevelMarker, "How come we don't have a marker.");
      firstLevelMarker->Swap(heapBuffer);
    }
  }

  if (!mBlocked) { // buffer was tokenized to completion
    NS_ASSERTION(!stackBuffer.hasMore(),
      "Buffer wasn't tokenized to completion?");
    // Scripting semantics require a forced tree builder flush here
    mTreeBuilder->Flush(); // Move ops to the executor
    mExecutor->FlushDocumentWrite(); // run the ops
  } else if (stackBuffer.hasMore()) {
    // The buffer wasn't tokenized to completion. Tokenize the untokenized
    // content in order to preload stuff. This content will be retokenized
    // later for normal parsing.
    if (!mDocWriteSpeculatorActive) {
      mDocWriteSpeculatorActive = true;
      if (!mDocWriteSpeculativeTreeBuilder) {
        // Lazily initialize if uninitialized
        mDocWriteSpeculativeTreeBuilder =
            new nsHtml5TreeBuilder(nullptr, mExecutor->GetStage());
        mDocWriteSpeculativeTreeBuilder->setScriptingEnabled(
            mTreeBuilder->isScriptingEnabled());
        mDocWriteSpeculativeTokenizer =
            new nsHtml5Tokenizer(mDocWriteSpeculativeTreeBuilder, false);
        mDocWriteSpeculativeTokenizer->setInterner(&mAtomTable);
        mDocWriteSpeculativeTokenizer->start();
      }
      mDocWriteSpeculativeTokenizer->resetToDataState();
      mDocWriteSpeculativeTreeBuilder->loadState(mTreeBuilder, &mAtomTable);
      mDocWriteSpeculativeLastWasCR = false;
    }

    // Note that with multilevel document.write if we didn't just activate the
    // speculator, it's possible that the speculator is now in the wrong state.
    // That's OK for the sake of simplicity. The worst that can happen is
    // that the speculative loads aren't exactly right. The content will be
    // reparsed anyway for non-preload purposes.

    // The buffer position for subsequent non-speculative parsing now lives
    // in heapBuffer, so it's ok to let the buffer position of stackBuffer
    // to be overwritten and not restored below.
    while (stackBuffer.hasMore()) {
      stackBuffer.adjust(mDocWriteSpeculativeLastWasCR);
      if (stackBuffer.hasMore()) {
        mDocWriteSpeculativeLastWasCR =
            mDocWriteSpeculativeTokenizer->tokenizeBuffer(&stackBuffer);
      }
    }

    mDocWriteSpeculativeTreeBuilder->Flush();
    mDocWriteSpeculativeTreeBuilder->DropHandles();
    mExecutor->FlushSpeculativeLoads();
  }

  return NS_OK;
}

NS_IMETHODIMP
nsHtml5Parser::Terminate()
{
  // We should only call DidBuildModel once, so don't do anything if this is
  // the second time that Terminate has been called.
  if (mExecutor->IsComplete()) {
    return NS_OK;
  }
  // XXX - [ until we figure out a way to break parser-sink circularity ]
  // Hack - Hold a reference until we are completely done...
  nsCOMPtr<nsIParser> kungFuDeathGrip(this);
  nsRefPtr<nsHtml5StreamParser> streamKungFuDeathGrip(mStreamParser);
  nsRefPtr<nsHtml5TreeOpExecutor> treeOpKungFuDeathGrip(mExecutor);
  if (mStreamParser) {
    mStreamParser->Terminate();
  }
  return mExecutor->DidBuildModel(true);
}

NS_IMETHODIMP
nsHtml5Parser::ParseFragment(const nsAString& aSourceBuffer,
                             nsTArray<nsString>& aTagStack)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsHtml5Parser::BuildModel()
{
  NS_NOTREACHED("Don't call this!");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsHtml5Parser::CancelParsingEvents()
{
  NS_NOTREACHED("Don't call this!");
  return NS_ERROR_NOT_IMPLEMENTED;
}

void
nsHtml5Parser::Reset()
{
  NS_NOTREACHED("Don't call this!");
}

bool
nsHtml5Parser::CanInterrupt()
{
  // nsContentSink needs this to let nsContentSink::DidProcessATokenImpl
  // interrupt.
  return true;
}

bool
nsHtml5Parser::IsInsertionPointDefined()
{
  return !mExecutor->IsFlushing() &&
    (!mStreamParser || mParserInsertedScriptsBeingEvaluated);
}

void
nsHtml5Parser::BeginEvaluatingParserInsertedScript()
{
  ++mParserInsertedScriptsBeingEvaluated;
}

void
nsHtml5Parser::EndEvaluatingParserInsertedScript()
{
  --mParserInsertedScriptsBeingEvaluated;
}

void
nsHtml5Parser::MarkAsNotScriptCreated(const char* aCommand)
{
  NS_PRECONDITION(!mStreamParser, "Must not call this twice.");
  eParserMode mode = NORMAL;
  if (!nsCRT::strcmp(aCommand, "view-source")) {
    mode = VIEW_SOURCE_HTML;
  } else if (!nsCRT::strcmp(aCommand, "view-source-xml")) {
    mode = VIEW_SOURCE_XML;
  } else if (!nsCRT::strcmp(aCommand, "view-source-plain")) {
    mode = VIEW_SOURCE_PLAIN;
  } else if (!nsCRT::strcmp(aCommand, "plain-text")) {
    mode = PLAIN_TEXT;
  } else if (!nsCRT::strcmp(aCommand, kLoadAsData)) {
    mode = LOAD_AS_DATA;
  }
#ifdef DEBUG
  else {
    NS_ASSERTION(!nsCRT::strcmp(aCommand, "view") ||
                 !nsCRT::strcmp(aCommand, "external-resource"),
                 "Unsupported parser command!");
  }
#endif
  mStreamParser = new nsHtml5StreamParser(mExecutor, this, mode);
}

bool
nsHtml5Parser::IsScriptCreated()
{
  return !mStreamParser;
}

/* End nsIParser  */

// not from interface
void
nsHtml5Parser::ParseUntilBlocked()
{
  if (mBlocked || mExecutor->IsComplete() || NS_FAILED(mExecutor->IsBroken())) {
    return;
  }
  NS_ASSERTION(mExecutor->HasStarted(), "Bad life cycle.");
  NS_ASSERTION(!mInDocumentWrite,
    "ParseUntilBlocked entered while in doc.write!");

  mDocWriteSpeculatorActive = false;

  for (;;) {
    if (!mFirstBuffer->hasMore()) {
      if (mFirstBuffer == mLastBuffer) {
        if (mExecutor->IsComplete()) {
          // something like cache manisfests stopped the parse in mid-flight
          return;
        }
        if (mDocumentClosed) {
          NS_ASSERTION(!mStreamParser,
                       "This should only happen with script-created parser.");
          mTokenizer->eof();
          mTreeBuilder->StreamEnded();
          mTreeBuilder->Flush();
          mExecutor->FlushDocumentWrite();
          mTokenizer->end();
          return;            
        }
        // never release the last buffer.
        NS_ASSERTION(!mLastBuffer->getStart() && !mLastBuffer->getEnd(),
                     "Sentinel buffer had its indeces changed.");
        if (mStreamParser) {
          if (mReturnToStreamParserPermitted &&
              !mExecutor->IsScriptExecuting()) {
            mTreeBuilder->Flush();
            mReturnToStreamParserPermitted = false;
            mStreamParser->ContinueAfterScripts(mTokenizer,
                                                mTreeBuilder,
                                                mLastWasCR);
          }
        } else {
          // Script-created parser
          mTreeBuilder->Flush();
          // No need to flush the executor, because the executor is already
          // in a flush
          NS_ASSERTION(mExecutor->IsInFlushLoop(),
              "How did we come here without being in the flush loop?");
        }
        return; // no more data for now but expecting more
      }
      mFirstBuffer = mFirstBuffer->next;
      continue;
    }

    if (mBlocked || mExecutor->IsComplete()) {
      return;
    }

    // now we have a non-empty buffer
    mFirstBuffer->adjust(mLastWasCR);
    mLastWasCR = false;
    if (mFirstBuffer->hasMore()) {
      bool inRootContext = (!mStreamParser && !mFirstBuffer->key);
      if (inRootContext) {
        mTokenizer->setLineNumber(mRootContextLineNumber);
      }
      mLastWasCR = mTokenizer->tokenizeBuffer(mFirstBuffer);
      if (inRootContext) {
        mRootContextLineNumber = mTokenizer->getLineNumber();
      }
      if (mTreeBuilder->HasScript()) {
        mTreeBuilder->Flush();
        mExecutor->FlushDocumentWrite();
      }
      if (mBlocked) {
        return;
      }
    }
    continue;
  }
}

nsresult
nsHtml5Parser::Initialize(nsIDocument* aDoc,
                          nsIURI* aURI,
                          nsISupports* aContainer,
                          nsIChannel* aChannel)
{
  return mExecutor->Init(aDoc, aURI, aContainer, aChannel);
}

void
nsHtml5Parser::StartTokenizer(bool aScriptingEnabled) {

  mTreeBuilder->setIsSrcdocDocument(IsSrcdocDocument());

  mTreeBuilder->SetPreventScriptExecution(!aScriptingEnabled);
  mTreeBuilder->setScriptingEnabled(aScriptingEnabled);
  mTokenizer->start();
}

void
nsHtml5Parser::InitializeDocWriteParserState(nsAHtml5TreeBuilderState* aState,
                                             int32_t aLine)
{
  mTokenizer->resetToDataState();
  mTokenizer->setLineNumber(aLine);
  mTreeBuilder->loadState(aState, &mAtomTable);
  mLastWasCR = false;
  mReturnToStreamParserPermitted = true;
}

void
nsHtml5Parser::ContinueAfterFailedCharsetSwitch()
{
  NS_PRECONDITION(mStreamParser, 
    "Tried to continue after failed charset switch without a stream parser");
  mStreamParser->ContinueAfterFailedCharsetSwitch();
}

bool
nsHtml5Parser::IsSrcdocDocument()
{
  nsresult rv;

  bool isSrcdoc = false;
  nsCOMPtr<nsIChannel> channel;
  rv = GetChannel(getter_AddRefs(channel));
  if (NS_SUCCEEDED(rv)) {
    nsCOMPtr<nsIInputStreamChannel> isr = do_QueryInterface(channel);
    if (isr) {
      isr->GetIsSrcdocChannel(&isSrcdoc);
    }
  }
  return isSrcdoc;
}

