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
 *   Henri Sivonen <hsivonen@iki.fi>
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

#include "nsCompatibility.h"
#include "nsScriptLoader.h"
#include "nsNetUtil.h"
#include "nsIStyleSheetLinkingElement.h"
#include "nsICharsetAlias.h"
#include "nsIWebShellServices.h"
#include "nsIDocShell.h"
#include "nsEncoderDecoderUtils.h"
#include "nsContentUtils.h"
#include "nsICharsetDetector.h"
#include "nsIScriptElement.h"
#include "nsIMarkupDocumentViewer.h"
#include "nsIDocShellTreeItem.h"
#include "nsIContentViewer.h"
#include "nsIScriptGlobalObjectOwner.h"
#include "nsIScriptSecurityManager.h"
#include "nsHtml5DocumentMode.h"
#include "nsHtml5Tokenizer.h"
#include "nsHtml5UTF16Buffer.h"
#include "nsHtml5TreeBuilder.h"
#include "nsHtml5Parser.h"
#include "nsHtml5AtomTable.h"
#include "nsIDOMDocumentFragment.h"
#include "nsHtml5DependentUTF16Buffer.h"

NS_INTERFACE_TABLE_HEAD(nsHtml5Parser)
  NS_INTERFACE_TABLE2(nsHtml5Parser, nsIParser, nsISupportsWeakReference)
  NS_INTERFACE_TABLE_TO_MAP_SEGUE_CYCLE_COLLECTION(nsHtml5Parser)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsHtml5Parser)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsHtml5Parser)

NS_IMPL_CYCLE_COLLECTION_CLASS(nsHtml5Parser)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(nsHtml5Parser)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR_AMBIGUOUS(mExecutor,
                                                       nsIContentSink)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR_AMBIGUOUS(mStreamParser,
                                                       nsIStreamListener)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(nsHtml5Parser)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mExecutor)
  tmp->DropStreamParser();
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

nsHtml5Parser::nsHtml5Parser()
  : mFirstBuffer(new nsHtml5OwningUTF16Buffer((void*)nsnull))
  , mLastBuffer(mFirstBuffer)
  , mExecutor(new nsHtml5TreeOpExecutor())
  , mTreeBuilder(new nsHtml5TreeBuilder(mExecutor, nsnull))
  , mTokenizer(new nsHtml5Tokenizer(mTreeBuilder))
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
  NS_ASSERTION(!strcmp(aCommand, "view"), "Parser command was not view");
}

NS_IMETHODIMP_(void)
nsHtml5Parser::SetCommand(eParserCommands aParserCommand)
{
  NS_ASSERTION(aParserCommand == eViewNormal, 
               "Parser command was not eViewNormal.");
}

NS_IMETHODIMP_(void)
nsHtml5Parser::SetDocumentCharset(const nsACString& aCharset,
                                  PRInt32 aCharsetSource)
{
  NS_PRECONDITION(!mExecutor->HasStarted(),
                  "Document charset set too late.");
  NS_PRECONDITION(mStreamParser, "Setting charset on a script-only parser.");
  nsCAutoString trimmed;
  trimmed.Assign(aCharset);
  trimmed.Trim(" \t\r\n\f");
  mStreamParser->SetDocumentCharset(trimmed, aCharsetSource);
  mExecutor->SetDocumentCharsetAndSource(trimmed,
                                         aCharsetSource);
}

NS_IMETHODIMP_(void)
nsHtml5Parser::SetParserFilter(nsIParserFilter* aFilter)
{
  NS_ERROR("Attempt to set a parser filter on HTML5 parser.");
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
  *aDTD = nsnull;
  return NS_OK;
}

NS_IMETHODIMP
nsHtml5Parser::GetStreamListener(nsIStreamListener** aListener)
{
  NS_IF_ADDREF(*aListener = mStreamParser);
  return NS_OK;
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
nsHtml5Parser::Parse(nsIURI* aURL, // legacy parameter; ignored
                     nsIRequestObserver* aObserver,
                     void* aKey,
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
  mExecutor->SetStreamParser(mStreamParser);
  mExecutor->SetParser(this);
  mRootContextKey = aKey;
  return NS_OK;
}

NS_IMETHODIMP
nsHtml5Parser::Parse(const nsAString& aSourceBuffer,
                     void* aKey,
                     const nsACString& aContentType, // ignored
                     bool aLastCall,
                     nsDTDMode aMode) // ignored
{
  NS_PRECONDITION(!mExecutor->IsFragmentMode(),
                  "Document.write called in fragment mode!");
  if (mExecutor->IsBroken()) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  if (aSourceBuffer.Length() > PR_INT32_MAX) {
    mExecutor->MarkAsBroken();
    return NS_ERROR_OUT_OF_MEMORY;
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
    mTokenizer->start();
    mExecutor->Start();
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

  if (aLastCall && aSourceBuffer.IsEmpty() && aKey == GetRootContextKey()) {
    // document.close()
    NS_ASSERTION(!mStreamParser,
                 "Had stream parser but got document.close().");
    mDocumentClosed = true;
    if (!mBlocked) {
      ParseUntilBlocked();
    }
    return NS_OK;
  }

  NS_ASSERTION(IsInsertionPointDefined(),
               "Doc.write reached parser with undefined insertion point.");

  NS_ASSERTION(!(mStreamParser && !aKey),
               "Got a null key in a non-script-created parser");

  if (aSourceBuffer.IsEmpty()) {
    return NS_OK;
  }

  nsHtml5DependentUTF16Buffer stackBuffer(aSourceBuffer);

  while (!mBlocked && stackBuffer.hasMore()) {
    stackBuffer.adjust(mLastWasCR);
    mLastWasCR = false;
    if (stackBuffer.hasMore()) {
      PRInt32 lineNumberSave;
      bool inRootContext = (!mStreamParser && (aKey == mRootContextKey));
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
      mExecutor->MarkAsBroken();
      return NS_ERROR_OUT_OF_MEMORY;
    }
  }

  // The buffer is inserted to the stream here in case it won't be parsed
  // to completion.
  // The script is identified by aKey. If there's nothing in the buffer
  // chain for that key, we'll insert at the head of the queue.
  // When the script leaves something in the queue, a zero-length
  // key-holder "buffer" is inserted in the queue. If the same script
  // leaves something in the chain again, it will be inserted immediately
  // before the old key holder belonging to the same script.
  nsHtml5OwningUTF16Buffer* prevSearchBuf = nsnull;
  nsHtml5OwningUTF16Buffer* searchBuf = mFirstBuffer;

  // after document.open, the first level of document.write has null key
  if (aKey) {
    while (searchBuf != mLastBuffer) {
      if (searchBuf->key == aKey) {
        // found a key holder
        // now insert the new buffer between the previous buffer
        // and the key holder if we have a buffer left.
        if (heapBuffer) {
          heapBuffer->next = searchBuf;
          if (prevSearchBuf) {
            prevSearchBuf->next = heapBuffer;
          } else {
            mFirstBuffer = heapBuffer;
          }
        }
        break;
      }
      prevSearchBuf = searchBuf;
      searchBuf = searchBuf->next;
    }
    if (searchBuf == mLastBuffer) {
      // key was not found
      nsHtml5OwningUTF16Buffer* keyHolder = new nsHtml5OwningUTF16Buffer(aKey);
      keyHolder->next = mFirstBuffer;
      if (heapBuffer) {
        heapBuffer->next = keyHolder;
        mFirstBuffer = heapBuffer;
      } else {
        mFirstBuffer = keyHolder;
      }
    }
  } else if (heapBuffer) {
    // we have a first level document.write after document.open()
    // insert immediately before mLastBuffer
    while (searchBuf != mLastBuffer) {
      prevSearchBuf = searchBuf;
      searchBuf = searchBuf->next;
    }
    heapBuffer->next = mLastBuffer;
    if (prevSearchBuf) {
      prevSearchBuf->next = heapBuffer;
    } else {
      mFirstBuffer = heapBuffer;
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
            new nsHtml5TreeBuilder(nsnull, mExecutor->GetStage());
        mDocWriteSpeculativeTreeBuilder->setScriptingEnabled(
            mTreeBuilder->isScriptingEnabled());
        mDocWriteSpeculativeTokenizer =
            new nsHtml5Tokenizer(mDocWriteSpeculativeTreeBuilder);
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

/**
 * This magic value is passed to the previous method on document.close()
 */
NS_IMETHODIMP_(void *)
nsHtml5Parser::GetRootContextKey()
{
  return mRootContextKey;
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

nsresult
nsHtml5Parser::ParseHtml5Fragment(const nsAString& aSourceBuffer,
                                  nsIContent* aTargetNode,
                                  nsIAtom* aContextLocalName,
                                  PRInt32 aContextNamespace,
                                  bool aQuirks,
                                  bool aPreventScriptExecution)
{
  NS_ENSURE_TRUE(aSourceBuffer.Length() <= PR_INT32_MAX,
      NS_ERROR_OUT_OF_MEMORY);
  nsIDocument* doc = aTargetNode->OwnerDoc();
  
  nsIURI* uri = doc->GetDocumentURI();
  NS_ENSURE_TRUE(uri, NS_ERROR_NOT_AVAILABLE);

  mExecutor->EnableFragmentMode(aPreventScriptExecution);

  Initialize(doc, uri, nsnull, nsnull);

  mExecutor->SetParser(this);
  mExecutor->SetNodeInfoManager(doc->NodeInfoManager());

  nsIContent* target = aTargetNode;
  mTreeBuilder->setFragmentContext(aContextLocalName,
                                   aContextNamespace,
                                   &target,
                                   aQuirks);

#ifdef DEBUG
  if (!aPreventScriptExecution) {
    NS_ASSERTION(!aTargetNode->IsInDoc(),
        "If script execution isn't prevented, "
        "the target node must not be in doc.");
    nsCOMPtr<nsIDOMDocumentFragment> domFrag = do_QueryInterface(aTargetNode);
    NS_ASSERTION(domFrag,
        "If script execution isn't prevented, must parse to DOM fragment.");
  }
#endif

  NS_PRECONDITION(!mExecutor->HasStarted(),
                  "Tried to start parse without initializing the parser.");
  mTreeBuilder->setScriptingEnabled(mExecutor->IsScriptEnabled());
  mTokenizer->start();
  mExecutor->Start(); // Don't call WillBuildModel in fragment case
  if (!aSourceBuffer.IsEmpty()) {
    bool lastWasCR = false;
    nsHtml5DependentUTF16Buffer buffer(aSourceBuffer);    
    while (buffer.hasMore()) {
      buffer.adjust(lastWasCR);
      lastWasCR = false;
      if (buffer.hasMore()) {
        lastWasCR = mTokenizer->tokenizeBuffer(&buffer);
        if (mTreeBuilder->HasScript()) {
          // Flush on each script, because the execution prevention code
          // can handle at most one script per flush.
          mTreeBuilder->Flush(); // Move ops to the executor
          mExecutor->FlushDocumentWrite(); // run the ops
        }
      }
    }
  }
  mTokenizer->eof();
  mTreeBuilder->StreamEnded();
  mTreeBuilder->Flush();
  mExecutor->FlushDocumentWrite();
  mTokenizer->end();
  mExecutor->DropParserAndPerfHint();
  mExecutor->DropHeldElements();
  mTreeBuilder->DropHandles();
  mAtomTable.Clear();
  return NS_OK;
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
  NS_PRECONDITION(mExecutor->IsFragmentMode(),
                  "Reset called on a non-fragment parser.");
  mExecutor->Reset();
  mLastWasCR = false;
  UnblockParser();
  mDocumentClosed = false;
  mStreamParser = nsnull;
  mRootContextLineNumber = 1;
  mParserInsertedScriptsBeingEvaluated = 0;
  mRootContextKey = nsnull;
  mAtomTable.Clear(); // should be already cleared in the fragment case anyway
  // Portable parser objects
  mFirstBuffer->next = nsnull;
  mFirstBuffer->setStart(0);
  mFirstBuffer->setEnd(0);
  mLastBuffer = mFirstBuffer;
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
nsHtml5Parser::MarkAsNotScriptCreated()
{
  NS_PRECONDITION(!mStreamParser, "Must not call this twice.");
  mStreamParser = new nsHtml5StreamParser(mExecutor, this);
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
  NS_PRECONDITION(!mExecutor->IsFragmentMode(),
                  "ParseUntilBlocked called in fragment mode.");

  if (mBlocked || mExecutor->IsComplete() || mExecutor->IsBroken()) {
    return;
  }
  NS_ASSERTION(mExecutor->HasStarted(), "Bad life cycle.");

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
      bool inRootContext = (!mStreamParser &&
                              (mFirstBuffer->key == mRootContextKey));
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
  mTreeBuilder->setScriptingEnabled(aScriptingEnabled);
  mTokenizer->start();
}

void
nsHtml5Parser::InitializeDocWriteParserState(nsAHtml5TreeBuilderState* aState,
                                             PRInt32 aLine)
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
