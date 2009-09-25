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

//-------------- Begin ParseContinue Event Definition ------------------------
/*
The parser can be explicitly interrupted by calling Suspend(). This will cause
the parser to stop processing and allow the application to return to the event
loop. The parser will schedule a nsHtml5ParserContinueEvent which will call
the parser to process the remaining data after returning to the event loop.
*/
class nsHtml5ParserContinueEvent : public nsRunnable
{
public:
  nsRefPtr<nsHtml5Parser> mParser;
  nsHtml5ParserContinueEvent(nsHtml5Parser* aParser)
    : mParser(aParser)
  {}
  NS_IMETHODIMP Run()
  {
    mParser->HandleParserContinueEvent(this);
    return NS_OK;
  }
};
//-------------- End ParseContinue Event Definition ------------------------


NS_INTERFACE_TABLE_HEAD(nsHtml5Parser)
  NS_INTERFACE_TABLE1(nsHtml5Parser, nsIParser)
  NS_INTERFACE_TABLE_TO_MAP_SEGUE_CYCLE_COLLECTION(nsHtml5Parser)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsHtml5Parser)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsHtml5Parser)

NS_IMPL_CYCLE_COLLECTION_CLASS(nsHtml5Parser)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(nsHtml5Parser)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR_AMBIGUOUS(mExecutor, nsIContentSink)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR_AMBIGUOUS(mStreamParser, nsIStreamListener)
  tmp->mTreeBuilder->DoTraverse(cb);
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(nsHtml5Parser)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mExecutor)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mStreamParser)
  tmp->mTreeBuilder->DoUnlink();
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

nsHtml5Parser::nsHtml5Parser()
  : mFirstBuffer(new nsHtml5UTF16Buffer(0))
  , mLastBuffer(mFirstBuffer)
  , mExecutor(new nsHtml5TreeOpExecutor())
  , mTreeBuilder(new nsHtml5TreeBuilder(mExecutor))
  , mTokenizer(new nsHtml5Tokenizer(mTreeBuilder))
{
  mExecutor->SetTreeBuilder(mTreeBuilder);
  // There's a zeroing operator new for everything else
}

nsHtml5Parser::~nsHtml5Parser()
{
  while (mFirstBuffer) {
     nsHtml5UTF16Buffer* old = mFirstBuffer;
     mFirstBuffer = mFirstBuffer->next;
     delete old;
  }
}

NS_IMETHODIMP_(void)
nsHtml5Parser::SetContentSink(nsIContentSink* aSink)
{
  NS_ASSERTION(aSink == static_cast<nsIContentSink*> (mExecutor), 
               "Attempt to set a foreign sink.");
}

NS_IMETHODIMP_(nsIContentSink*)
nsHtml5Parser::GetContentSink(void)
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
nsHtml5Parser::SetDocumentCharset(const nsACString& aCharset, PRInt32 aCharsetSource)
{
  NS_PRECONDITION(mExecutor->GetLifeCycle() == NOT_STARTED,
                  "Document charset set too late.");
  NS_PRECONDITION(mStreamParser, "Tried to set charset on a script-only parser.");
  mStreamParser->SetDocumentCharset(aCharset, aCharsetSource);
  mExecutor->SetDocumentCharset((nsACString&)aCharset);
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
  if (!mStreamParser) {
    mStreamParser = new nsHtml5StreamParser(mTokenizer, mExecutor, this);
  }
  NS_ADDREF(*aListener = mStreamParser);
  return NS_OK;
}

NS_IMETHODIMP
nsHtml5Parser::ContinueParsing()
{
  UnblockParser();
  return ContinueInterruptedParsing();
}

NS_IMETHODIMP
nsHtml5Parser::ContinueInterruptedParsing()
{
  // If there are scripts executing, then the content sink is jumping the gun
  // (probably due to a synchronous XMLHttpRequest) and will re-enable us
  // later, see bug 460706.
  if (mExecutor->IsScriptExecuting()) {
    return NS_OK;
  }
  // If the stream has already finished, there's a good chance
  // that we might start closing things down when the parser
  // is reenabled. To make sure that we're not deleted across
  // the reenabling process, hold a reference to ourselves.
  nsCOMPtr<nsIParser> kungFuDeathGrip(this);
  nsRefPtr<nsHtml5StreamParser> streamKungFuDeathGrip(mStreamParser);
  nsRefPtr<nsHtml5TreeOpExecutor> treeOpKungFuDeathGrip(mExecutor);
  // XXX Stop speculative script thread but why?
  mExecutor->MaybeFlush();
  ParseUntilSuspend();
  return NS_OK;
}

NS_IMETHODIMP_(void)
nsHtml5Parser::BlockParser()
{
  mBlocked = PR_TRUE;
  if (mStreamParser) {
    mStreamParser->Block();
  }
}

NS_IMETHODIMP_(void)
nsHtml5Parser::UnblockParser()
{
  mBlocked = PR_FALSE;
  if (mStreamParser) {
    mStreamParser->Unblock();
  }
}

NS_IMETHODIMP_(PRBool)
nsHtml5Parser::IsParserEnabled()
{
  return !mBlocked;
}

NS_IMETHODIMP_(PRBool)
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
  NS_PRECONDITION(mExecutor->GetLifeCycle() == NOT_STARTED, 
                  "Tried to start parse without initializing the parser properly.");
  if (!mStreamParser) {
    mStreamParser = new nsHtml5StreamParser(mTokenizer, mExecutor, this);
  }
  mStreamParser->SetObserver(aObserver);
  mTokenizer->setEncodingDeclarationHandler(mStreamParser);
  mExecutor->SetStreamParser(mStreamParser);
  mExecutor->SetParser(this);
  mTreeBuilder->setScriptingEnabled(mExecutor->IsScriptEnabled());
  mExecutor->AllowInterrupts();
  mRootContextKey = aKey;
  mExecutor->SetParser(this);
  return NS_OK;
}

NS_IMETHODIMP
nsHtml5Parser::Parse(const nsAString& aSourceBuffer,
                     void* aKey,
                     const nsACString& aContentType, // ignored
                     PRBool aLastCall,
                     nsDTDMode aMode) // ignored
{
  NS_PRECONDITION(!mFragmentMode, "Document.write called in fragment mode!");

  // Maintain a reference to ourselves so we don't go away
  // till we're completely done. The old parser grips itself in this method.
  nsCOMPtr<nsIParser> kungFuDeathGrip(this);
  
  // Gripping the other objects just in case, since the other old grip
  // required grips to these, too.
  nsRefPtr<nsHtml5StreamParser> streamKungFuDeathGrip(mStreamParser);
  nsRefPtr<nsHtml5TreeOpExecutor> treeOpKungFuDeathGrip(mExecutor);

  // Return early if the parser has processed EOF
  switch (mExecutor->GetLifeCycle()) {
    case TERMINATED:
      return NS_OK;
    case NOT_STARTED:
      mExecutor->SetParser(this);
      mTreeBuilder->setScriptingEnabled(mExecutor->IsScriptEnabled());
      mTokenizer->start();
      mExecutor->SetLifeCycle(PARSING);
      break;
    default:
      break;
  }

  if (aLastCall && aSourceBuffer.IsEmpty() && aKey == GetRootContextKey()) {
    // document.close()
    mExecutor->SetLifeCycle(STREAM_ENDING);
    MaybePostContinueEvent();
    return NS_OK;
  }

  // XXX stop speculative script thread here

  PRInt32 lineNumberSave = mTokenizer->getLineNumber();

  if (!aSourceBuffer.IsEmpty()) {
    nsHtml5UTF16Buffer* buffer = new nsHtml5UTF16Buffer(aSourceBuffer.Length());
    memcpy(buffer->getBuffer(), aSourceBuffer.BeginReading(), aSourceBuffer.Length() * sizeof(PRUnichar));
    buffer->setEnd(aSourceBuffer.Length());
    if (!mBlocked) {
      mExecutor->WillResume();
      while (buffer->hasMore()) {
        buffer->adjust(mLastWasCR);
        mLastWasCR = PR_FALSE;
        if (buffer->hasMore()) {
          mLastWasCR = mTokenizer->tokenizeBuffer(buffer);
          mExecutor->MaybeExecuteScript();
          if (mBlocked) {
            // XXX is the tail insertion and script exec in the wrong order?
            mExecutor->WillInterrupt();
            break;
          }
          // Ignore suspension requests
        }
      }
    }

    if (buffer->hasMore()) {
      // If we got here, the buffer wasn't parsed synchronously to completion
      // and its tail needs to go into the chain of pending buffers.
      // The script is identified by aKey. If there's nothing in the buffer
      // chain for that key, we'll insert at the head of the queue.
      // When the script leaves something in the queue, a zero-length
      // key-holder "buffer" is inserted in the queue. If the same script
      // leaves something in the chain again, it will be inserted immediately
      // before the old key holder belonging to the same script.
      nsHtml5UTF16Buffer* prevSearchBuf = nsnull;
      nsHtml5UTF16Buffer* searchBuf = mFirstBuffer;
      if (aKey) { // after document.open, the first level of document.write has null key
        while (searchBuf != mLastBuffer) {
          if (searchBuf->key == aKey) {
            // found a key holder
            // now insert the new buffer between the previous buffer
            // and the key holder.
            buffer->next = searchBuf;
            if (prevSearchBuf) {
              prevSearchBuf->next = buffer;
            } else {
              mFirstBuffer = buffer;
            }
            break;
          }
          prevSearchBuf = searchBuf;
          searchBuf = searchBuf->next;
        }
      }
      if (searchBuf == mLastBuffer || !aKey) {
        // key was not found or we have a first-level write after document.open
        // we'll insert to the head of the queue
        nsHtml5UTF16Buffer* keyHolder = new nsHtml5UTF16Buffer(aKey);
        keyHolder->next = mFirstBuffer;
        buffer->next = keyHolder;
        mFirstBuffer = buffer;
      }
      MaybePostContinueEvent();
    } else {
      delete buffer;
    }
  }

  // Scripting semantics require a forced tree builder flush here
  mExecutor->Flush();
  mTokenizer->setLineNumber(lineNumberSave);
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
nsHtml5Parser::Terminate(void)
{
  // We should only call DidBuildModel once, so don't do anything if this is
  // the second time that Terminate has been called.
  if (mExecutor->GetLifeCycle() == TERMINATED) {
    return NS_OK;
  }
  // XXX - [ until we figure out a way to break parser-sink circularity ]
  // Hack - Hold a reference until we are completely done...
  nsCOMPtr<nsIParser> kungFuDeathGrip(this);
  nsRefPtr<nsHtml5StreamParser> streamKungFuDeathGrip(mStreamParser);
  nsRefPtr<nsHtml5TreeOpExecutor> treeOpKungFuDeathGrip(mExecutor);
  // CancelParsingEvents must be called to avoid leaking the nsParser object
  // @see bug 108049
  CancelParsingEvents();
  
#ifdef DEBUG
  PRBool ready =
#endif
  mExecutor->ReadyToCallDidBuildModel(PR_TRUE);
  NS_ASSERTION(ready, "Should always be ready to call DidBuildModel here.");
  return mExecutor->DidBuildModel();
}

NS_IMETHODIMP
nsHtml5Parser::ParseFragment(const nsAString& aSourceBuffer,
                             void* aKey,
                             nsTArray<nsString>& aTagStack,
                             PRBool aXMLMode,
                             const nsACString& aContentType,
                             nsDTDMode aMode)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsHtml5Parser::ParseFragment(const nsAString& aSourceBuffer,
                             nsISupports* aTargetNode,
                             nsIAtom* aContextLocalName,
                             PRInt32 aContextNamespace,
                             PRBool aQuirks)
{
  nsCOMPtr<nsIContent> target = do_QueryInterface(aTargetNode);
  NS_ASSERTION(target, "Target did not QI to nsIContent");

  nsIDocument* doc = target->GetOwnerDoc();
  NS_ENSURE_TRUE(doc, NS_ERROR_NOT_AVAILABLE);
  
  nsIURI* uri = doc->GetDocumentURI();
  NS_ENSURE_TRUE(uri, NS_ERROR_NOT_AVAILABLE);

  Initialize(doc, uri, nsnull, nsnull);

  // Initialize() doesn't deal with base URI
  mExecutor->SetBaseUriFromDocument();
  mExecutor->ProhibitInterrupts();
  mExecutor->SetParser(this);
  mExecutor->SetNodeInfoManager(target->GetOwnerDoc()->NodeInfoManager());

  mTreeBuilder->setFragmentContext(aContextLocalName, aContextNamespace, target, aQuirks);
  mFragmentMode = PR_TRUE;
  
  NS_PRECONDITION(mExecutor->GetLifeCycle() == NOT_STARTED, "Tried to start parse without initializing the parser properly.");
  mTreeBuilder->setScriptingEnabled(mExecutor->IsScriptEnabled());
  mTokenizer->start();
  mExecutor->SetLifeCycle(PARSING);
  if (!aSourceBuffer.IsEmpty()) {
    PRBool lastWasCR = PR_FALSE;
    nsHtml5UTF16Buffer buffer(aSourceBuffer.Length());
    memcpy(buffer.getBuffer(), aSourceBuffer.BeginReading(), aSourceBuffer.Length() * sizeof(PRUnichar));
    buffer.setEnd(aSourceBuffer.Length());
    while (buffer.hasMore()) {
      buffer.adjust(lastWasCR);
      lastWasCR = PR_FALSE;
      if (buffer.hasMore()) {
        lastWasCR = mTokenizer->tokenizeBuffer(&buffer);
        mExecutor->MaybePreventExecution();
      }
    }
  }
  mExecutor->SetLifeCycle(TERMINATED);
  mTokenizer->eof();
  mExecutor->Flush();
  mTokenizer->end();
  mExecutor->DropParserAndPerfHint();
  return NS_OK;
}

NS_IMETHODIMP
nsHtml5Parser::BuildModel(void)
{
  // XXX who calls this? Should this be a no-op?
  ParseUntilSuspend();
  return NS_OK;
}

NS_IMETHODIMP
nsHtml5Parser::CancelParsingEvents()
{
  mContinueEvent = nsnull;
  return NS_OK;
}

void
nsHtml5Parser::Reset()
{
  mExecutor->Reset();
  mLastWasCR = PR_FALSE;
  mFragmentMode = PR_FALSE;
  UnblockParser();
  mSuspending = PR_FALSE;
  mStreamParser = nsnull;
  mRootContextKey = nsnull;
  mContinueEvent = nsnull;  // weak ref
  // Portable parser objects
  while (mFirstBuffer->next) {
    nsHtml5UTF16Buffer* oldBuf = mFirstBuffer;
    mFirstBuffer = mFirstBuffer->next;
    delete oldBuf;
  }
  mFirstBuffer->setStart(0);
  mFirstBuffer->setEnd(0);
}

PRBool
nsHtml5Parser::CanInterrupt()
{
  return !mFragmentMode;
}

/* End nsIParser  */

// not from interface
void
nsHtml5Parser::HandleParserContinueEvent(nsHtml5ParserContinueEvent* ev)
{
  // Ignore any revoked continue events...
  if (mContinueEvent != ev)
    return;
  mContinueEvent = nsnull;
  NS_ASSERTION(!mExecutor->IsScriptExecuting(), "Interrupted in the middle of a script?");
  ContinueInterruptedParsing();
}

void
nsHtml5Parser::ParseUntilSuspend()
{
  NS_PRECONDITION(!mFragmentMode, "ParseUntilSuspend called in fragment mode.");
  NS_PRECONDITION(!mExecutor->NeedsCharsetSwitch(), "ParseUntilSuspend called when charset switch needed.");

  if (mBlocked) {
    return;
  }

  switch (mExecutor->GetLifeCycle()) {
    case TERMINATED:
      return;
    case NOT_STARTED:
      NS_NOTREACHED("Bad life cycle!");
      break;
    default:
      break;
  }

  mExecutor->WillResume();
  mSuspending = PR_FALSE;
  for (;;) {
    if (!mFirstBuffer->hasMore()) {
      if (mFirstBuffer == mLastBuffer) {
        switch (mExecutor->GetLifeCycle()) {
          case TERMINATED:
            // something like cache manisfests stopped the parse in mid-flight
            return;
          case PARSING:
            // never release the last buffer. instead just zero its indeces for refill
            mFirstBuffer->setStart(0);
            mFirstBuffer->setEnd(0);
            if (mStreamParser) {
              mStreamParser->ParseUntilSuspend();
            }
            return; // no more data for now but expecting more
          case STREAM_ENDING:
            if (mStreamParser && !mStreamParser->IsDone()) { // may still have stream data left
              mStreamParser->ParseUntilSuspend();            
            } else if (mExecutor->ReadyToCallDidBuildModel(PR_FALSE)) {
              // no more data and not expecting more
              mExecutor->DidBuildModel();
            }
            return;
          default:
            NS_NOTREACHED("It should be impossible to reach this.");
            return;
        }
      } else {
        nsHtml5UTF16Buffer* oldBuf = mFirstBuffer;
        mFirstBuffer = mFirstBuffer->next;
        delete oldBuf;
        continue;
      }
    }

    if (mBlocked || (mExecutor->GetLifeCycle() == TERMINATED)) {
      return;
    }

    // now we have a non-empty buffer
    mFirstBuffer->adjust(mLastWasCR);
    mLastWasCR = PR_FALSE;
    if (mFirstBuffer->hasMore()) {
      mLastWasCR = mTokenizer->tokenizeBuffer(mFirstBuffer);
      NS_ASSERTION(!(mExecutor->HasScriptElement() && mExecutor->NeedsCharsetSwitch()), "Can't have both script and charset switch.");
      mExecutor->IgnoreCharsetSwitch();
      mExecutor->MaybeExecuteScript();
      if (mBlocked) {
        mExecutor->WillInterrupt();
        return;
      }
      if (mSuspending) {
        MaybePostContinueEvent();
        mExecutor->WillInterrupt();
        return;
      }
    }
    continue;
  }
}

void
nsHtml5Parser::MaybePostContinueEvent()
{
  NS_PRECONDITION(mExecutor->GetLifeCycle() != TERMINATED, 
                  "Tried to post continue event when the parser is done.");
  if (mContinueEvent) {
    return; // we already have a pending event
  }
  // This creates a reference cycle between this and the event that is
  // broken when the event fires.
  nsCOMPtr<nsIRunnable> event = new nsHtml5ParserContinueEvent(this);
  if (NS_FAILED(NS_DispatchToCurrentThread(event))) {
    NS_WARNING("failed to dispatch parser continuation event");
  } else {
    mContinueEvent = event;
  }
}

void
nsHtml5Parser::Suspend()
{
  mSuspending = PR_TRUE;
  if (mStreamParser) {
    mStreamParser->Suspend();
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

