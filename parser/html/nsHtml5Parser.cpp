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
#include "nsICharsetConverterManager.h"
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

static NS_DEFINE_CID(kCharsetAliasCID, NS_CHARSETALIAS_CID);

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

NS_IMPL_CYCLE_COLLECTION_CLASS(nsHtml5Parser)

NS_INTERFACE_TABLE_HEAD_CYCLE_COLLECTION_INHERITED(nsHtml5Parser) \
  NS_INTERFACE_TABLE_INHERITED4(nsHtml5Parser, 
                                nsIParser, 
                                nsIStreamListener, 
                                nsICharsetDetectionObserver, 
                                nsIContentSink)
NS_INTERFACE_TABLE_TAIL_INHERITING(nsContentSink)

NS_IMPL_ADDREF_INHERITED(nsHtml5Parser, nsContentSink)

NS_IMPL_RELEASE_INHERITED(nsHtml5Parser, nsContentSink)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(nsHtml5Parser, nsContentSink)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mScriptElement)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mRequest)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mObserver)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mUnicodeDecoder)
  tmp->mTreeBuilder->DoTraverse(cb);
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(nsHtml5Parser, nsContentSink)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mScriptElement)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mRequest)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mObserver)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mUnicodeDecoder)
  tmp->mTreeBuilder->DoUnlink();
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

nsHtml5Parser::nsHtml5Parser()
  : mFirstBuffer(new nsHtml5UTF16Buffer(NS_HTML5_PARSER_READ_BUFFER_SIZE)),
    // XXX avoid allocating the buffer at all for fragment parser?
    mLastBuffer(mFirstBuffer),
    mTreeBuilder(new nsHtml5TreeBuilder(this)),
    mTokenizer(new nsHtml5Tokenizer(mTreeBuilder))
{
  mTokenizer->setEncodingDeclarationHandler(this);
  // There's a zeroing operator new for everything else
}

nsHtml5Parser::~nsHtml5Parser()
{
  while (mFirstBuffer) {
     nsHtml5UTF16Buffer* old = mFirstBuffer;
     mFirstBuffer = mFirstBuffer->next;
     delete old;
  }
#ifdef GATHER_DOCWRITE_STATISTICS
  delete mSnapshot;
#endif
}

// copied from HTML content sink
static PRBool
IsScriptEnabled(nsIDocument *aDoc, nsIDocShell *aContainer)
{
  NS_ENSURE_TRUE(aDoc && aContainer, PR_TRUE);
  nsCOMPtr<nsIScriptGlobalObject> globalObject = aDoc->GetScriptGlobalObject();
  // Getting context is tricky if the document hasn't had its
  // GlobalObject set yet
  if (!globalObject) {
    nsCOMPtr<nsIScriptGlobalObjectOwner> owner = do_GetInterface(aContainer);
    NS_ENSURE_TRUE(owner, PR_TRUE);
    globalObject = owner->GetScriptGlobalObject();
    NS_ENSURE_TRUE(globalObject, PR_TRUE);
  }
  nsIScriptContext *scriptContext = globalObject->GetContext();
  NS_ENSURE_TRUE(scriptContext, PR_TRUE);
  JSContext* cx = (JSContext *) scriptContext->GetNativeContext();
  NS_ENSURE_TRUE(cx, PR_TRUE);
  PRBool enabled = PR_TRUE;
  nsContentUtils::GetSecurityManager()->
    CanExecuteScripts(cx, aDoc->NodePrincipal(), &enabled);
  return enabled;
}

NS_IMETHODIMP_(void)
nsHtml5Parser::SetContentSink(nsIContentSink* aSink)
{
  NS_ASSERTION(aSink == static_cast<nsIContentSink*> (this), 
               "Attempt to set a foreign sink.");
}

NS_IMETHODIMP_(nsIContentSink*)
nsHtml5Parser::GetContentSink(void)
{
  return static_cast<nsIContentSink*> (this);
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
  mCharset = aCharset;
  mCharsetSource = aCharsetSource;
}

NS_IMETHODIMP_(void)
nsHtml5Parser::SetParserFilter(nsIParserFilter* aFilter)
{
  NS_ERROR("Attempt to set a parser filter on HTML5 parser.");
}

NS_IMETHODIMP
nsHtml5Parser::GetChannel(nsIChannel** aChannel)
{
  return mRequest ? CallQueryInterface(mRequest, aChannel) :
                    NS_ERROR_NOT_AVAILABLE;
}

NS_IMETHODIMP
nsHtml5Parser::GetDTD(nsIDTD** aDTD)
{
  *aDTD = nsnull;
  return NS_OK;
}

NS_IMETHODIMP
nsHtml5Parser::ContinueParsing()
{
  mBlocked = PR_FALSE;
  return ContinueInterruptedParsing();
}

NS_IMETHODIMP
nsHtml5Parser::ContinueInterruptedParsing()
{
  // If there are scripts executing, then the content sink is jumping the gun
  // (probably due to a synchronous XMLHttpRequest) and will re-enable us
  // later, see bug 460706.
  if (IsScriptExecutingImpl()) {
    return NS_OK;
  }
  // If the stream has already finished, there's a good chance
  // that we might start closing things down when the parser
  // is reenabled. To make sure that we're not deleted across
  // the reenabling process, hold a reference to ourselves.
  // XXX is this really necessary? -- hsivonen?
  nsCOMPtr<nsIParser> kungFuDeathGrip(this);
  // XXX Stop speculative script thread but why?
  mTreeBuilder->MaybeFlush();
  ParseUntilSuspend();
  return NS_OK;
}

NS_IMETHODIMP_(void)
nsHtml5Parser::BlockParser()
{
  NS_NOTREACHED("No one should call this");
}

NS_IMETHODIMP_(void)
nsHtml5Parser::UnblockParser()
{
  mBlocked = PR_FALSE;
}

NS_IMETHODIMP_(PRBool)
nsHtml5Parser::IsParserEnabled()
{
  return !mBlocked;
}

NS_IMETHODIMP_(PRBool)
nsHtml5Parser::IsComplete()
{
  // XXX old parser says
  // return !(mFlags & NS_PARSER_FLAG_PENDING_CONTINUE_EVENT);
  return (mLifeCycle == TERMINATED);
}

NS_IMETHODIMP
nsHtml5Parser::Parse(nsIURI* aURL, // legacy parameter; ignored
                     nsIRequestObserver* aObserver,
                     void* aKey,
                     nsDTDMode aMode) // legacy; ignored
{
  mObserver = aObserver;
  mRootContextKey = aKey;
  mCanInterruptParser = PR_TRUE;
  NS_PRECONDITION(mLifeCycle == NOT_STARTED, 
                  "Tried to start parse without initializing the parser properly.");
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
  // Return early if the parser has processed EOF
  switch (mLifeCycle) {
    case TERMINATED:
      return NS_OK;
    case NOT_STARTED:
      mTreeBuilder->setScriptingEnabled(IsScriptEnabled(mDocument, mDocShell));
      mTokenizer->start();
      mLifeCycle = PARSING;
      mParser = this;
      mCharsetSource = kCharsetFromOtherComponent;
      break;
    default:
      break;
  }

  if (aLastCall && aSourceBuffer.IsEmpty() && aKey == GetRootContextKey()) {
    // document.close()
    mLifeCycle = STREAM_ENDING;
    MaybePostContinueEvent();
    return NS_OK;
  }

  // XXX stop speculative script thread here

  PRInt32 lineNumberSave = mTokenizer->getLineNumber();

  // Maintain a reference to ourselves so we don't go away
  // till we're completely done.
  // XXX is this still necessary? -- hsivonen
  nsCOMPtr<nsIParser> kungFuDeathGrip(this);

  if (!aSourceBuffer.IsEmpty()) {
    nsHtml5UTF16Buffer* buffer = new nsHtml5UTF16Buffer(aSourceBuffer.Length());
    memcpy(buffer->getBuffer(), aSourceBuffer.BeginReading(), aSourceBuffer.Length() * sizeof(PRUnichar));
    buffer->setEnd(aSourceBuffer.Length());
    if (!mBlocked) {
      WillResumeImpl();
      WillParseImpl();
      while (buffer->hasMore()) {
        buffer->adjust(mLastWasCR);
        mLastWasCR = PR_FALSE;
        if (buffer->hasMore()) {
          mUninterruptibleDocWrite = PR_TRUE;
          mLastWasCR = mTokenizer->tokenizeBuffer(buffer);
          if (mScriptElement) {
            mTreeBuilder->Flush();
            mUninterruptibleDocWrite = PR_FALSE;
            ExecuteScript();
          }
          if (mBlocked) {
            // XXX is the tail insertion and script exec in the wrong order?
            WillInterruptImpl();
            break;
          }
          // Ignore suspension requests
        }
      }
    }

    mUninterruptibleDocWrite = PR_FALSE;

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
  mTreeBuilder->Flush();
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
  if (mLifeCycle == TERMINATED) {
    return NS_OK;
  }
  mSuppressEOF = PR_TRUE;
  
  // XXX - [ until we figure out a way to break parser-sink circularity ]
  // Hack - Hold a reference until we are completely done...
  nsCOMPtr<nsIParser> kungFuDeathGrip(this);
  // CancelParsingEvents must be called to avoid leaking the nsParser object
  // @see bug 108049
  CancelParsingEvents();
#ifdef DEBUG
  PRBool ready =
#endif
  ReadyToCallDidBuildModelImpl(PR_TRUE);
  NS_ASSERTION(ready, "Should always be ready to call DidBuildModel here.");
  return DidBuildModel();
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
  mDocumentBaseURI = doc->GetBaseURI();

  mTreeBuilder->setFragmentContext(aContextLocalName, aContextNamespace, target, aQuirks);
  mFragmentMode = PR_TRUE;
  mCanInterruptParser = PR_FALSE;
  NS_PRECONDITION(mLifeCycle == NOT_STARTED, "Tried to start parse without initializing the parser properly.");
  mTreeBuilder->setScriptingEnabled(IsScriptEnabled(mDocument, mDocShell));
  mTokenizer->start();
  mLifeCycle = PARSING;
  mParser = this;
  mNodeInfoManager = target->GetOwnerDoc()->NodeInfoManager();
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
        if (mScriptElement) {
          nsCOMPtr<nsIScriptElement> script = do_QueryInterface(mScriptElement);
          NS_ASSERTION(script, "mScriptElement didn't QI to nsIScriptElement!");
          script->PreventExecution();
          mScriptElement = nsnull;
        }
      }
    }
  }
  mLifeCycle = TERMINATED;
  mTokenizer->eof();
  mTreeBuilder->Flush();
  mTokenizer->end();
  DropParserAndPerfHint();
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
  mNeedsCharsetSwitch = PR_FALSE;
  mLastWasCR = PR_FALSE;
  mFragmentMode = PR_FALSE;
  mBlocked = PR_FALSE;
  mSuspending = PR_FALSE;
  mSuppressEOF = PR_FALSE;
  mLifeCycle = NOT_STARTED;
  mScriptElement = nsnull;
  mUninterruptibleDocWrite = PR_FALSE;
  mRootContextKey = nsnull;
  mRequest = nsnull;
  mObserver = nsnull;
  mContinueEvent = nsnull;  // weak ref
  // encoding-related stuff
  mCharsetSource = kCharsetUninitialized;
  mCharset.Truncate();
  mPendingCharset.Truncate();
  mUnicodeDecoder = nsnull;
  mSniffingBuffer = nsnull;
  mSniffingLength = 0;
  mBomState = BOM_SNIFFING_NOT_STARTED;
  mMetaScanner = nsnull;
  // Portable parser objects
  while (mFirstBuffer->next) {
    nsHtml5UTF16Buffer* oldBuf = mFirstBuffer;
    mFirstBuffer = mFirstBuffer->next;
    delete oldBuf;
  }
  mFirstBuffer->setStart(0);
  mFirstBuffer->setEnd(0);
#ifdef DEBUG
  mStreamListenerState = eNone;
#endif
}

PRBool
nsHtml5Parser::CanInterrupt()
{
  return !(mFragmentMode || mUninterruptibleDocWrite);
}

/* End nsIParser  */

// nsIRequestObserver methods:
nsresult
nsHtml5Parser::OnStartRequest(nsIRequest* aRequest, nsISupports* aContext)
{
  NS_PRECONDITION(eNone == mStreamListenerState,
                  "Parser's nsIStreamListener API was not setup "
                  "correctly in constructor.");
  if (mObserver) {
    mObserver->OnStartRequest(aRequest, aContext);
  }
#ifdef DEBUG
  mStreamListenerState = eOnStart;
#endif
  mRequest = aRequest;
  
  if (mCharsetSource < kCharsetFromChannel) {
    // we aren't ready to commit to an encoding yet
    // leave converter uninstantiated for now
    return NS_OK;
  }
  
  nsresult rv = NS_OK;
  nsCOMPtr<nsICharsetConverterManager> convManager = do_GetService(NS_CHARSETCONVERTERMANAGER_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = convManager->GetUnicodeDecoder(mCharset.get(), getter_AddRefs(mUnicodeDecoder));
  NS_ENSURE_SUCCESS(rv, rv);
  mUnicodeDecoder->SetInputErrorBehavior(nsIUnicodeDecoder::kOnError_Recover);
  return NS_OK;
}

nsresult
nsHtml5Parser::OnStopRequest(nsIRequest* aRequest,
                             nsISupports* aContext,
                             nsresult status)
{
  mTreeBuilder->MaybeFlush();
  NS_ASSERTION(mRequest == aRequest, "Got Stop on wrong stream.");
  nsresult rv = NS_OK;
  if (!mUnicodeDecoder) {
    PRUint32 writeCount;
    rv = FinalizeSniffing(nsnull, 0, &writeCount, 0);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  switch (mLifeCycle) {
    case TERMINATED:
      break;
    case NOT_STARTED:
      mTreeBuilder->setScriptingEnabled(IsScriptEnabled(mDocument, mDocShell));
      mTokenizer->start();
      mLifeCycle = STREAM_ENDING;
      mParser = this;
      break;
    case STREAM_ENDING:
      NS_ERROR("OnStopRequest when the stream lifecycle was already ending.");
      break;
    default:
      mLifeCycle = STREAM_ENDING;
      break;
  }
#ifdef DEBUG
  mStreamListenerState = eOnStop;
#endif
  if (!IsScriptExecutingImpl()) {
    ParseUntilSuspend();
  }
  if (mObserver) {
    mObserver->OnStopRequest(aRequest, aContext, status);
  }
  return NS_OK;
}

// nsIStreamListener method:
/*
 * This function is invoked as a result of a call to a stream's
 * ReadSegments() method. It is called for each contiguous buffer
 * of data in the underlying stream or pipe. Using ReadSegments
 * allows us to avoid copying data to read out of the stream.
 */
static NS_METHOD
ParserWriteFunc(nsIInputStream* aInStream,
                void* aHtml5Parser,
                const char* aFromSegment,
                PRUint32 aToOffset,
                PRUint32 aCount,
                PRUint32* aWriteCount)
{
  nsHtml5Parser* parser = static_cast<nsHtml5Parser*> (aHtml5Parser);
  if (parser->HasDecoder()) {
    return parser->WriteStreamBytes((const PRUint8*)aFromSegment, aCount, aWriteCount);
  } else {
    return parser->SniffStreamBytes((const PRUint8*)aFromSegment, aCount, aWriteCount);
  }
}

nsresult
nsHtml5Parser::OnDataAvailable(nsIRequest* aRequest,
                               nsISupports* aContext,
                               nsIInputStream* aInStream,
                               PRUint32 aSourceOffset,
                               PRUint32 aLength)
{
  mTreeBuilder->MaybeFlush();
  NS_PRECONDITION(eOnStart == mStreamListenerState ||
                  eOnDataAvail == mStreamListenerState,
            "Error: OnStartRequest() must be called before OnDataAvailable()");
  NS_ASSERTION(mRequest == aRequest, "Got data on wrong stream.");
  PRUint32 totalRead;
  nsresult rv = aInStream->ReadSegments(ParserWriteFunc, static_cast<void*> (this), aLength, &totalRead);
  NS_ASSERTION(totalRead == aLength, "ReadSegments read the wrong number of bytes.");
  if (!IsScriptExecutingImpl()) {
    ParseUntilSuspend();
  }
  return rv;
}

// EncodingDeclarationHandler
void
nsHtml5Parser::internalEncodingDeclaration(nsString* aEncoding)
{
  if (mCharsetSource >= kCharsetFromMetaTag) { // this threshold corresponds to "confident" in the HTML5 spec
    return;
  }
  nsresult rv = NS_OK;
  nsCOMPtr<nsICharsetAlias> calias(do_GetService(kCharsetAliasCID, &rv));
  if (NS_FAILED(rv)) {
    return;
  }
  nsCAutoString newEncoding;
  CopyUTF16toUTF8(*aEncoding, newEncoding);
  PRBool eq;
  rv = calias->Equals(newEncoding, mCharset, &eq);
  if (NS_FAILED(rv)) {
    return;
  }
  if (eq) {
    mCharsetSource = kCharsetFromMetaTag; // become confident
    return;
  }
  
  // XXX check HTML5 non-IANA aliases here

  // The encodings are different. We want to reparse.
  nsCOMPtr<nsIHttpChannel> httpChannel(do_QueryInterface(mRequest, &rv));
  if (NS_SUCCEEDED(rv)) {
    nsCAutoString method;
    httpChannel->GetRequestMethod(method);
    // XXX does Necko have a way to renavigate POST, etc. without hitting
    // the network?
    if (!method.EqualsLiteral("GET")) {
      // This is the old Gecko behavior but the spec disagrees.
      // Don't reparse on POST.
      return;
    }
  }
  
  // we still want to reparse
  mNeedsCharsetSwitch = PR_TRUE;
  mPendingCharset.Assign(newEncoding);
}

// DocumentModeHandler
void
nsHtml5Parser::documentMode(nsHtml5DocumentMode m)
{
  nsCompatibility mode = eCompatibility_NavQuirks;
  switch (m) {
    case STANDARDS_MODE:
      mode = eCompatibility_FullStandards;
      break;
    case ALMOST_STANDARDS_MODE:
      mode = eCompatibility_AlmostStandards;
      break;
    case QUIRKS_MODE:
      mode = eCompatibility_NavQuirks;
      break;
  }
  nsCOMPtr<nsIHTMLDocument> htmlDocument = do_QueryInterface(mDocument);
  NS_ASSERTION(htmlDocument, "Document didn't QI into HTML document.");
  htmlDocument->SetCompatibilityMode(mode);
}

// nsIContentSink
NS_IMETHODIMP
nsHtml5Parser::WillParse()
{
  NS_NOTREACHED("No one should call this");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsHtml5Parser::WillBuildModel(nsDTDMode aDTDMode)
{
  NS_NOTREACHED("No one should call this");
  return NS_ERROR_NOT_IMPLEMENTED;
}

// This is called when the tree construction has ended
NS_IMETHODIMP
nsHtml5Parser::DidBuildModel()
{
  NS_ASSERTION(mLifeCycle == STREAM_ENDING, "Bad life cycle.");
  mLifeCycle = TERMINATED;
  if (!mSuppressEOF) {
    mTokenizer->eof();
    mTreeBuilder->Flush();
  }
  mTokenizer->end();
  // This is comes from nsXMLContentSink
  DidBuildModelImpl();
  mDocument->ScriptLoader()->RemoveObserver(this);
  StartLayout(PR_FALSE);
  ScrollToRef();
  mDocument->RemoveObserver(this);
  mDocument->EndLoad();
  DropParserAndPerfHint();
#ifdef GATHER_DOCWRITE_STATISTICS
  printf("UNSAFE SCRIPTS: %d\n", sUnsafeDocWrites);
  printf("TOKENIZER-SAFE SCRIPTS: %d\n", sTokenSafeDocWrites);
  printf("TREEBUILDER-SAFE SCRIPTS: %d\n", sTreeSafeDocWrites);
#endif
  return NS_OK;
}

NS_IMETHODIMP
nsHtml5Parser::WillInterrupt()
{
  return WillInterruptImpl();
}

NS_IMETHODIMP
nsHtml5Parser::WillResume()
{
  NS_NOTREACHED("No one should call this.");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsHtml5Parser::SetParser(nsIParser* aParser)
{
  NS_NOTREACHED("No one should be setting a parser on the HTML5 pseudosink.");
  return NS_ERROR_NOT_IMPLEMENTED;
}

void
nsHtml5Parser::FlushPendingNotifications(mozFlushType aType)
{
}

NS_IMETHODIMP
nsHtml5Parser::SetDocumentCharset(nsACString& aCharset)
{
  if (mDocShell) {
    // the following logic to get muCV is copied from
    // nsHTMLDocument::StartDocumentLoad
    // We need to call muCV->SetPrevDocCharacterSet here in case
    // the charset is detected by parser DetectMetaTag
    nsCOMPtr<nsIMarkupDocumentViewer> muCV;
    nsCOMPtr<nsIContentViewer> cv;
    mDocShell->GetContentViewer(getter_AddRefs(cv));
    if (cv) {
      muCV = do_QueryInterface(cv);
    } else {
      // in this block of code, if we get an error result, we return
      // it but if we get a null pointer, that's perfectly legal for
      // parent and parentContentViewer
      nsCOMPtr<nsIDocShellTreeItem> docShellAsItem =
        do_QueryInterface(mDocShell);
      NS_ENSURE_TRUE(docShellAsItem, NS_ERROR_FAILURE);
      nsCOMPtr<nsIDocShellTreeItem> parentAsItem;
      docShellAsItem->GetSameTypeParent(getter_AddRefs(parentAsItem));
      nsCOMPtr<nsIDocShell> parent(do_QueryInterface(parentAsItem));
      if (parent) {
        nsCOMPtr<nsIContentViewer> parentContentViewer;
        nsresult rv =
          parent->GetContentViewer(getter_AddRefs(parentContentViewer));
        if (NS_SUCCEEDED(rv) && parentContentViewer) {
          muCV = do_QueryInterface(parentContentViewer);
        }
      }
    }
    if (muCV) {
      muCV->SetPrevDocCharacterSet(aCharset);
    }
  }
  if (mDocument) {
    mDocument->SetDocumentCharacterSet(aCharset);
  }
  return NS_OK;
}

nsISupports*
nsHtml5Parser::GetTarget()
{
  return mDocument;
}

// not from interface
void
nsHtml5Parser::HandleParserContinueEvent(nsHtml5ParserContinueEvent* ev)
{
  // Ignore any revoked continue events...
  if (mContinueEvent != ev)
    return;
  mContinueEvent = nsnull;
  NS_ASSERTION(!IsScriptExecutingImpl(), "Interrupted in the middle of a script?");
  ContinueInterruptedParsing();
}

NS_IMETHODIMP
nsHtml5Parser::Notify(const char* aCharset, nsDetectionConfident aConf)
{
  if (aConf == eBestAnswer || aConf == eSureAnswer) {
    mCharset.Assign(aCharset);
    mCharsetSource = kCharsetFromAutoDetection;
    SetDocumentCharset(mCharset);
  }
  return NS_OK;
}

nsresult
nsHtml5Parser::SetupDecodingAndWriteSniffingBufferAndCurrentSegment(const PRUint8* aFromSegment, // can be null
                                                                    PRUint32 aCount,
                                                                    PRUint32* aWriteCount)
{
  nsresult rv = NS_OK;
  nsCOMPtr<nsICharsetConverterManager> convManager = do_GetService(NS_CHARSETCONVERTERMANAGER_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = convManager->GetUnicodeDecoder(mCharset.get(), getter_AddRefs(mUnicodeDecoder));
  if (rv == NS_ERROR_UCONV_NOCONV) {
    mCharset.Assign("windows-1252"); // lower case is the raw form
    mCharsetSource = kCharsetFromWeakDocTypeDefault;
    rv = convManager->GetUnicodeDecoderRaw(mCharset.get(), getter_AddRefs(mUnicodeDecoder));
    SetDocumentCharset(mCharset);
  }
  NS_ENSURE_SUCCESS(rv, rv);
  mUnicodeDecoder->SetInputErrorBehavior(nsIUnicodeDecoder::kOnError_Recover);
  return WriteSniffingBufferAndCurrentSegment(aFromSegment, aCount, aWriteCount);
}

nsresult
nsHtml5Parser::WriteSniffingBufferAndCurrentSegment(const PRUint8* aFromSegment, // can be null
                                                    PRUint32 aCount,
                                                    PRUint32* aWriteCount)
{
  nsresult rv = NS_OK;
  if (mSniffingBuffer) {
    PRUint32 writeCount;
    rv = WriteStreamBytes(mSniffingBuffer, mSniffingLength, &writeCount);
    NS_ENSURE_SUCCESS(rv, rv);
    mSniffingBuffer = nsnull;
  }
  mMetaScanner = nsnull;
  if (aFromSegment) {
    rv = WriteStreamBytes(aFromSegment, aCount, aWriteCount);
  }
  return rv;
}

nsresult
nsHtml5Parser::SetupDecodingFromBom(const char* aCharsetName, const char* aDecoderCharsetName)
{
  nsresult rv = NS_OK;
  nsCOMPtr<nsICharsetConverterManager> convManager = do_GetService(NS_CHARSETCONVERTERMANAGER_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = convManager->GetUnicodeDecoderRaw(aDecoderCharsetName, getter_AddRefs(mUnicodeDecoder));
  NS_ENSURE_SUCCESS(rv, rv);
  mCharset.Assign(aCharsetName);
  mCharsetSource = kCharsetFromByteOrderMark;
  SetDocumentCharset(mCharset);
  mSniffingBuffer = nsnull;
  mMetaScanner = nsnull;
  mBomState = BOM_SNIFFING_OVER;
  return rv;
}

nsresult
nsHtml5Parser::FinalizeSniffing(const PRUint8* aFromSegment, // can be null
                                PRUint32 aCount,
                                PRUint32* aWriteCount,
                                PRUint32 aCountToSniffingLimit)
{
  // meta scan failed.
  if (mCharsetSource >= kCharsetFromHintPrevDoc) {
    return SetupDecodingAndWriteSniffingBufferAndCurrentSegment(aFromSegment, aCount, aWriteCount);
  }
  // maybe try chardet now; instantiation copied from nsDOMFile
  const nsAdoptingString& detectorName = nsContentUtils::GetLocalizedStringPref("intl.charset.detector");
  if (!detectorName.IsEmpty()) {
    nsCAutoString detectorContractID;
    detectorContractID.AssignLiteral(NS_CHARSET_DETECTOR_CONTRACTID_BASE);
    AppendUTF16toUTF8(detectorName, detectorContractID);
    nsCOMPtr<nsICharsetDetector> detector = do_CreateInstance(detectorContractID.get());
    if (detector) {
      nsresult rv = detector->Init(this);
      NS_ENSURE_SUCCESS(rv, rv);
      PRBool dontFeed = PR_FALSE;
      if (mSniffingBuffer) {
        rv = detector->DoIt((const char*)mSniffingBuffer.get(), mSniffingLength, &dontFeed);
        NS_ENSURE_SUCCESS(rv, rv);
      }
      if (!dontFeed && aFromSegment) {
        rv = detector->DoIt((const char*)aFromSegment, aCountToSniffingLimit, &dontFeed);
        NS_ENSURE_SUCCESS(rv, rv);
      }
      rv = detector->Done();
      NS_ENSURE_SUCCESS(rv, rv);
      // fall thru; callback may have changed charset
    } else {
      NS_ERROR("Could not instantiate charset detector.");
    }
  }
  if (mCharsetSource == kCharsetUninitialized) {
    // Hopefully this case is never needed, but dealing with it anyway
    mCharset.Assign("windows-1252");
    mCharsetSource = kCharsetFromWeakDocTypeDefault;
    SetDocumentCharset(mCharset);
  }
  return SetupDecodingAndWriteSniffingBufferAndCurrentSegment(aFromSegment, aCount, aWriteCount);
}

nsresult
nsHtml5Parser::SniffStreamBytes(const PRUint8* aFromSegment,
                                PRUint32 aCount,
                                PRUint32* aWriteCount)
{
  nsresult rv = NS_OK;
  PRUint32 writeCount;
  for (PRUint32 i = 0; i < aCount; i++) {
    switch (mBomState) {
      case BOM_SNIFFING_NOT_STARTED:
        NS_ASSERTION(i == 0, "Bad BOM sniffing state.");
        switch (*aFromSegment) {
          case 0xEF:
            mBomState = SEEN_UTF_8_FIRST_BYTE;
            break;
          case 0xFF:
            mBomState = SEEN_UTF_16_LE_FIRST_BYTE;
            break;
          case 0xFE:
            mBomState = SEEN_UTF_16_BE_FIRST_BYTE;
            break;
          default:
            mBomState = BOM_SNIFFING_OVER;
            break;
        }
        break;
      case SEEN_UTF_16_LE_FIRST_BYTE:
        if (aFromSegment[i] == 0xFE) {
          rv = SetupDecodingFromBom("UTF-16", "UTF-16LE"); // upper case is the raw form
          NS_ENSURE_SUCCESS(rv, rv);
          PRUint32 count = aCount - (i + 1);
          rv = WriteStreamBytes(aFromSegment + (i + 1), count, &writeCount);
          NS_ENSURE_SUCCESS(rv, rv);
          *aWriteCount = writeCount + (i + 1);
          return rv;
        }
        mBomState = BOM_SNIFFING_OVER;
        break;
      case SEEN_UTF_16_BE_FIRST_BYTE:
        if (aFromSegment[i] == 0xFF) {
          rv = SetupDecodingFromBom("UTF-16", "UTF-16BE"); // upper case is the raw form
          NS_ENSURE_SUCCESS(rv, rv);
          PRUint32 count = aCount - (i + 1);
          rv = WriteStreamBytes(aFromSegment + (i + 1), count, &writeCount);
          NS_ENSURE_SUCCESS(rv, rv);
          *aWriteCount = writeCount + (i + 1);
          return rv;
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
          rv = SetupDecodingFromBom("UTF-8", "UTF-8"); // upper case is the raw form
          NS_ENSURE_SUCCESS(rv, rv);
          PRUint32 count = aCount - (i + 1);
          rv = WriteStreamBytes(aFromSegment + (i + 1), count, &writeCount);
          NS_ENSURE_SUCCESS(rv, rv);
          *aWriteCount = writeCount + (i + 1);
          return rv;
        }
        mBomState = BOM_SNIFFING_OVER;
        break;
      default:
        goto bom_loop_end;
    }
  }
  // if we get here, there either was no BOM or the BOM sniffing isn't complete yet
  bom_loop_end:
  
  if (!mMetaScanner) {
    mMetaScanner = new nsHtml5MetaScanner();
  }
  
  if (mSniffingLength + aCount >= NS_HTML5_PARSER_SNIFFING_BUFFER_SIZE) {
    // this is the last buffer
    PRUint32 countToSniffingLimit = NS_HTML5_PARSER_SNIFFING_BUFFER_SIZE - mSniffingLength;
    nsHtml5ByteReadable readable(aFromSegment, aFromSegment + countToSniffingLimit);
    mMetaScanner->sniff(&readable, getter_AddRefs(mUnicodeDecoder), mCharset);
    if (mUnicodeDecoder) {
      mUnicodeDecoder->SetInputErrorBehavior(nsIUnicodeDecoder::kOnError_Recover);
      // meta scan successful
      mCharsetSource = kCharsetFromMetaPrescan;
      SetDocumentCharset(mCharset);
      mMetaScanner = nsnull;
      return WriteSniffingBufferAndCurrentSegment(aFromSegment, aCount, aWriteCount);
    }
    return FinalizeSniffing(aFromSegment, aCount, aWriteCount, countToSniffingLimit);
  }

  // not the last buffer
  nsHtml5ByteReadable readable(aFromSegment, aFromSegment + aCount);
  mMetaScanner->sniff(&readable, getter_AddRefs(mUnicodeDecoder), mCharset);
  if (mUnicodeDecoder) {
    // meta scan successful
    mCharsetSource = kCharsetFromMetaPrescan;
    SetDocumentCharset(mCharset);
    mMetaScanner = nsnull;
    return WriteSniffingBufferAndCurrentSegment(aFromSegment, aCount, aWriteCount);
  }
  if (!mSniffingBuffer) {
    mSniffingBuffer = new PRUint8[NS_HTML5_PARSER_SNIFFING_BUFFER_SIZE];
  }
  memcpy(mSniffingBuffer + mSniffingLength, aFromSegment, aCount);
  mSniffingLength += aCount;
  *aWriteCount = aCount;
  return NS_OK;
}

nsresult
nsHtml5Parser::WriteStreamBytes(const PRUint8* aFromSegment,
                                PRUint32 aCount,
                                PRUint32* aWriteCount)
{
  // mLastBuffer always points to a buffer of the size NS_HTML5_PARSER_READ_BUFFER_SIZE.
  if (mLastBuffer->getEnd() == NS_HTML5_PARSER_READ_BUFFER_SIZE) {
    mLastBuffer = (mLastBuffer->next = new nsHtml5UTF16Buffer(NS_HTML5_PARSER_READ_BUFFER_SIZE));
  }
  PRUint32 totalByteCount = 0;
  for (;;) {
    PRInt32 end = mLastBuffer->getEnd();
    PRInt32 byteCount = aCount - totalByteCount;
    PRInt32 utf16Count = NS_HTML5_PARSER_READ_BUFFER_SIZE - end;

    NS_ASSERTION(utf16Count, "Trying to convert into a buffer with no free space!");

    nsresult convResult = mUnicodeDecoder->Convert((const char*)aFromSegment, &byteCount, mLastBuffer->getBuffer() + end, &utf16Count);

    end += utf16Count;
    mLastBuffer->setEnd(end);
    totalByteCount += byteCount;
    aFromSegment += byteCount;

    NS_ASSERTION(mLastBuffer->getEnd() <= NS_HTML5_PARSER_READ_BUFFER_SIZE, "The Unicode decoder wrote too much data.");

    if (NS_FAILED(convResult)) {
      if (totalByteCount < aCount) { // mimicking nsScanner even though this seems wrong
        ++totalByteCount;
        ++aFromSegment;
      }
      mLastBuffer->getBuffer()[end] = 0xFFFD;
      ++end;
      mLastBuffer->setEnd(end);
      if (end == NS_HTML5_PARSER_READ_BUFFER_SIZE) {
          mLastBuffer = (mLastBuffer->next = new nsHtml5UTF16Buffer(NS_HTML5_PARSER_READ_BUFFER_SIZE));
      }
      mUnicodeDecoder->Reset();
      if (totalByteCount == aCount) {
        *aWriteCount = totalByteCount;
        return NS_OK;
      }
    } else if (convResult == NS_PARTIAL_MORE_OUTPUT) {
      mLastBuffer = (mLastBuffer->next = new nsHtml5UTF16Buffer(NS_HTML5_PARSER_READ_BUFFER_SIZE));
      NS_ASSERTION(totalByteCount < aCount, "The Unicode decoder has consumed too many bytes.");
    } else {
      NS_ASSERTION(totalByteCount == aCount, "The Unicode decoder consumed the wrong number of bytes.");
      *aWriteCount = totalByteCount;
      return NS_OK;
    }
  }
}

void
nsHtml5Parser::ParseUntilSuspend()
{
  NS_PRECONDITION(!mFragmentMode, "ParseUntilSuspend called in fragment mode.");
  NS_PRECONDITION(!mNeedsCharsetSwitch, "ParseUntilSuspend called when charset switch needed.");

  if (mBlocked) {
    return;
  }

  switch (mLifeCycle) {
    case TERMINATED:
      return;
    case NOT_STARTED:
      mTreeBuilder->setScriptingEnabled(IsScriptEnabled(mDocument, mDocShell));
      mTokenizer->start();
      mLifeCycle = PARSING;
      mParser = this;
      break;
    default:
      break;
  }

  WillResumeImpl();
  WillParseImpl();
  mSuspending = PR_FALSE;
  for (;;) {
    if (!mFirstBuffer->hasMore()) {
      if (mFirstBuffer == mLastBuffer) {
        switch (mLifeCycle) {
          case TERMINATED:
            // something like cache manisfests stopped the parse in mid-flight
            return;
          case PARSING:
            // never release the last buffer. instead just zero its indeces for refill
            mFirstBuffer->setStart(0);
            mFirstBuffer->setEnd(0);
            return; // no more data for now but expecting more
          case STREAM_ENDING:
            if (ReadyToCallDidBuildModelImpl(PR_FALSE)) {
              DidBuildModel();
            }
            return; // no more data and not expecting more
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

    if (mBlocked || (mLifeCycle == TERMINATED)) {
      return;
    }

#ifdef GATHER_DOCWRITE_STATISTICS
    if (mSnapshot && mFirstBuffer->key == GetRootContextKey()) {
      if (mTokenizer->isInDataState()) {
        if (mTreeBuilder->snapshotMatches(mSnapshot)) {
          sTreeSafeDocWrites++;
        } else {
          sTokenSafeDocWrites++;
        }
      } else {
        sUnsafeDocWrites++;
      }
      delete mSnapshot;
      mSnapshot = nsnull;
    }
#endif

    // now we have a non-empty buffer
    mFirstBuffer->adjust(mLastWasCR);
    mLastWasCR = PR_FALSE;
    if (mFirstBuffer->hasMore()) {
      mLastWasCR = mTokenizer->tokenizeBuffer(mFirstBuffer);
      NS_ASSERTION(!(mScriptElement && mNeedsCharsetSwitch), "Can't have both script and charset switch.");
      if (mScriptElement) {
        mTreeBuilder->Flush();
        ExecuteScript();
      } else if (mNeedsCharsetSwitch) {
        if (PerformCharsetSwitch() == NS_ERROR_HTMLPARSER_STOPPARSING) {
          return;
        } else {
          // let's continue if we failed to restart
          mNeedsCharsetSwitch = PR_FALSE;
        }
      }
      if (mBlocked) {
        WillInterruptImpl();
        return;
      }
      if (mSuspending) {
        MaybePostContinueEvent();
        WillInterruptImpl();
        return;
      }
    }
    continue;
  }
}

nsresult
nsHtml5Parser::PerformCharsetSwitch()
{
  // this code comes from nsObserverBase.cpp
  nsresult rv = NS_OK;
  nsCOMPtr<nsIWebShellServices> wss = do_QueryInterface(mDocShell);
  if (!wss) {
    return NS_ERROR_HTMLPARSER_CONTINUE;
  }
#ifndef DONT_INFORM_WEBSHELL
  // ask the webshellservice to load the URL
  if (NS_FAILED(rv = wss->SetRendering(PR_FALSE))) {
    // do nothing and fall thru
  } else if (NS_FAILED(rv = wss->StopDocumentLoad())) {
    rv = wss->SetRendering(PR_TRUE); // turn on the rendering so at least we will see something.
  } else if (NS_FAILED(rv = wss->ReloadDocument(mPendingCharset.get(), kCharsetFromMetaTag))) {
    rv = wss->SetRendering(PR_TRUE); // turn on the rendering so at least we will see something.
  } else {
    rv = NS_ERROR_HTMLPARSER_STOPPARSING; // We're reloading a new document...stop loading the current.
  }
#endif
  // if our reload request is not accepted, we should tell parser to go on
  if (rv != NS_ERROR_HTMLPARSER_STOPPARSING)
    rv = NS_ERROR_HTMLPARSER_CONTINUE;
  return rv;
}

/**
 * This method executes a script element set by nsHtml5TreeBuilder. The reason
 * why this code is here and not in the tree builder is to allow the control
 * to return from the tokenizer before scripts run. This way, the tokenizer
 * is not invoked re-entrantly although the parser is.
 */
void
nsHtml5Parser::ExecuteScript()
{
  NS_PRECONDITION(mScriptElement, "Trying to run a script without having one!");
#ifdef GATHER_DOCWRITE_STATISTICS
  if (!mSnapshot) {
    mSnapshot = mTreeBuilder->newSnapshot();
  }
#endif
  nsCOMPtr<nsIScriptElement> sele = do_QueryInterface(mScriptElement);
   // Notify our document that we're loading this script.
  nsCOMPtr<nsIHTMLDocument> htmlDocument = do_QueryInterface(mDocument);
  NS_ASSERTION(htmlDocument, "Document didn't QI into HTML document.");
  htmlDocument->ScriptLoading(sele);
   // Copied from nsXMLContentSink
  // Now tell the script that it's ready to go. This may execute the script
  // or return NS_ERROR_HTMLPARSER_BLOCK. Or neither if the script doesn't
  // need executing.
  nsresult rv = mScriptElement->DoneAddingChildren(PR_TRUE);
  // If the act of insertion evaluated the script, we're fine.
  // Else, block the parser till the script has loaded.
  if (rv == NS_ERROR_HTMLPARSER_BLOCK) {
    mScriptElements.AppendObject(sele);
    mBlocked = PR_TRUE;
  } else {
    // This may have already happened if the script executed, but in case
    // it didn't then remove the element so that it doesn't get stuck forever.
    htmlDocument->ScriptExecuted(sele);
  }
  mScriptElement = nsnull;
}

void
nsHtml5Parser::MaybePostContinueEvent()
{
  NS_PRECONDITION(mLifeCycle != TERMINATED, 
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
}

nsresult
nsHtml5Parser::Initialize(nsIDocument* aDoc,
                          nsIURI* aURI,
                          nsISupports* aContainer,
                          nsIChannel* aChannel)
{
  nsresult rv = nsContentSink::Init(aDoc, aURI, aContainer, aChannel);
  NS_ENSURE_SUCCESS(rv, rv);
  aDoc->AddObserver(this);
  return NS_OK;
}

nsresult
nsHtml5Parser::ProcessBASETag(nsIContent* aContent)
{
  NS_ASSERTION(aContent, "missing base-element");
  nsresult rv = NS_OK;
  if (mDocument) {
    nsAutoString value;
    if (aContent->GetAttr(kNameSpaceID_None, nsHtml5Atoms::target, value)) {
      mDocument->SetBaseTarget(value);
    }
    if (aContent->GetAttr(kNameSpaceID_None, nsHtml5Atoms::href, value)) {
      nsCOMPtr<nsIURI> baseURI;
      rv = NS_NewURI(getter_AddRefs(baseURI), value);
      if (NS_SUCCEEDED(rv)) {
        rv = mDocument->SetBaseURI(baseURI); // The document checks if it is legal to set this base
        if (NS_SUCCEEDED(rv)) {
          mDocumentBaseURI = mDocument->GetBaseURI();
        }
      }
    }
  }
  return rv;
}

void
nsHtml5Parser::UpdateStyleSheet(nsIContent* aElement)
{
  nsCOMPtr<nsIStyleSheetLinkingElement> ssle(do_QueryInterface(aElement));
  if (ssle) {
    ssle->SetEnableUpdates(PR_TRUE);
    PRBool willNotify;
    PRBool isAlternate;
    nsresult rv = ssle->UpdateStyleSheet(this, &willNotify, &isAlternate);
    if (NS_SUCCEEDED(rv) && willNotify && !isAlternate) {
      ++mPendingSheetCount;
      mScriptLoader->AddExecuteBlocker();
    }
  }
}

void
nsHtml5Parser::SetScriptElement(nsIContent* aScript)
{
  mScriptElement = aScript;
}

// nsContentSink overrides

void
nsHtml5Parser::UpdateChildCounts()
{
  // No-op
}

nsresult
nsHtml5Parser::FlushTags()
{
    return NS_OK;
}

void
nsHtml5Parser::PostEvaluateScript(nsIScriptElement *aElement)
{
  nsCOMPtr<nsIHTMLDocument> htmlDocument = do_QueryInterface(mDocument);
  NS_ASSERTION(htmlDocument, "Document didn't QI into HTML document.");
  htmlDocument->ScriptExecuted(aElement);
}


#ifdef GATHER_DOCWRITE_STATISTICS
PRUint32 nsHtml5Parser::sUnsafeDocWrites = 0;
PRUint32 nsHtml5Parser::sTokenSafeDocWrites = 0;
PRUint32 nsHtml5Parser::sTreeSafeDocWrites = 0;
#endif
