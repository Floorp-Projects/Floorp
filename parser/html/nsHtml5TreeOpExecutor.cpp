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

#include "nsHtml5TreeOpExecutor.h"
#include "nsScriptLoader.h"
#include "nsIMarkupDocumentViewer.h"
#include "nsIContentViewer.h"
#include "nsIDocShellTreeItem.h"
#include "nsIStyleSheetLinkingElement.h"
#include "nsIDocShell.h"
#include "nsIScriptGlobalObject.h"
#include "nsIScriptGlobalObjectOwner.h"
#include "nsIScriptSecurityManager.h"
#include "nsIWebShellServices.h"
#include "nsContentUtils.h"
#include "mozAutoDocUpdate.h"
#include "nsNetUtil.h"
#include "nsHtml5Parser.h"
#include "nsHtml5Tokenizer.h"
#include "nsHtml5TreeBuilder.h"
#include "nsHtml5StreamParser.h"

NS_IMPL_CYCLE_COLLECTION_CLASS(nsHtml5TreeOpExecutor)

NS_INTERFACE_TABLE_HEAD_CYCLE_COLLECTION_INHERITED(nsHtml5TreeOpExecutor)
  NS_INTERFACE_TABLE_INHERITED1(nsHtml5TreeOpExecutor, 
                                nsIContentSink)
NS_INTERFACE_TABLE_TAIL_INHERITING(nsContentSink)

NS_IMPL_ADDREF_INHERITED(nsHtml5TreeOpExecutor, nsContentSink)

NS_IMPL_RELEASE_INHERITED(nsHtml5TreeOpExecutor, nsContentSink)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(nsHtml5TreeOpExecutor, nsContentSink)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMARRAY(mOwnedElements)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(nsHtml5TreeOpExecutor, nsContentSink)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMARRAY(mOwnedElements)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

PRInt32 nsHtml5TreeOpExecutor::sTreeOpQueueLengthLimit = 200;
PRInt32 nsHtml5TreeOpExecutor::sTreeOpQueueMaxTime = 100; // milliseconds
PRInt32 nsHtml5TreeOpExecutor::sTreeOpQueueMinLength = 100;
PRInt32 nsHtml5TreeOpExecutor::sTreeOpQueueMaxLength = 4500;

// static
void
nsHtml5TreeOpExecutor::InitializeStatics()
{
  // Changes to the initial max length pref are intentionally allowed
  // to reset the run-time-calibrated value.
  nsContentUtils::AddIntPrefVarCache("html5.opqueue.initiallengthlimit", 
                                     &sTreeOpQueueLengthLimit);
  nsContentUtils::AddIntPrefVarCache("html5.opqueue.maxtime", 
                                     &sTreeOpQueueMaxTime);
  nsContentUtils::AddIntPrefVarCache("html5.opqueue.minlength",
                                     &sTreeOpQueueMinLength);
  nsContentUtils::AddIntPrefVarCache("html5.opqueue.maxlength",
                                     &sTreeOpQueueMaxLength);
  // Now do some sanity checking to prevent breaking the app via
  // about:config so badly that it can't be recovered via about:config.
  if (sTreeOpQueueMinLength <= 0) {
    sTreeOpQueueMinLength = 200;
  }
  if (sTreeOpQueueLengthLimit < sTreeOpQueueMinLength) {
    sTreeOpQueueLengthLimit = sTreeOpQueueMinLength;
  }
  if (sTreeOpQueueMaxLength < sTreeOpQueueMinLength) {
    sTreeOpQueueMaxLength = sTreeOpQueueMinLength;
  }
  if (sTreeOpQueueMaxTime <= 0) {
    sTreeOpQueueMaxTime = 200;
  }
}

nsHtml5TreeOpExecutor::nsHtml5TreeOpExecutor()
{
  // zeroing operator new for everything
}

nsHtml5TreeOpExecutor::~nsHtml5TreeOpExecutor()
{
  NS_ASSERTION(mOpQueue.IsEmpty(), "Somehow there's stuff in the op queue.");
}

// nsIContentSink
NS_IMETHODIMP
nsHtml5TreeOpExecutor::WillParse()
{
  NS_NOTREACHED("No one should call this");
  return NS_ERROR_NOT_IMPLEMENTED;
}

// This is called when the tree construction has ended
NS_IMETHODIMP
nsHtml5TreeOpExecutor::DidBuildModel(PRBool aTerminated)
{
  NS_PRECONDITION(mStarted, "Bad life cycle.");

  if (!aTerminated) {
    // Break out of update batch if we are in one 
    // and aren't forcibly terminating
    EndDocUpdate();
    
    // If the above caused a call to nsIParser::Terminate(), let that call
    // win.
    if (!mParser) {
      return NS_OK;
    }
  }
  
  static_cast<nsHtml5Parser*> (mParser.get())->DropStreamParser();

  // This is comes from nsXMLContentSink
  DidBuildModelImpl(aTerminated);
  mDocument->ScriptLoader()->RemoveObserver(this);
  ScrollToRef();
  mDocument->RemoveObserver(this);
  if (!mParser) {
    // DidBuildModelImpl may cause mParser to be nulled out
    // Return early to avoid unblocking the onload event too many times.
    return NS_OK;
  }
  mDocument->EndLoad();
  DropParserAndPerfHint();
#ifdef GATHER_DOCWRITE_STATISTICS
  printf("UNSAFE SCRIPTS: %d\n", sUnsafeDocWrites);
  printf("TOKENIZER-SAFE SCRIPTS: %d\n", sTokenSafeDocWrites);
  printf("TREEBUILDER-SAFE SCRIPTS: %d\n", sTreeSafeDocWrites);
#endif
#ifdef DEBUG_NS_HTML5_TREE_OP_EXECUTOR_FLUSH
  printf("MAX NOTIFICATION BATCH LEN: %d\n", sAppendBatchMaxSize);
  if (sAppendBatchExaminations != 0) {
    printf("AVERAGE SLOTS EXAMINED: %d\n", sAppendBatchSlotsExamined / sAppendBatchExaminations);
  }
#endif
  return NS_OK;
}

NS_IMETHODIMP
nsHtml5TreeOpExecutor::WillInterrupt()
{
  return WillInterruptImpl();
}

NS_IMETHODIMP
nsHtml5TreeOpExecutor::WillResume()
{
  WillResumeImpl();
  return WillParseImpl();
}

NS_IMETHODIMP
nsHtml5TreeOpExecutor::SetParser(nsIParser* aParser)
{
  mParser = aParser;
  return NS_OK;
}

void
nsHtml5TreeOpExecutor::FlushPendingNotifications(mozFlushType aType)
{
}

NS_IMETHODIMP
nsHtml5TreeOpExecutor::SetDocumentCharset(nsACString& aCharset)
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
nsHtml5TreeOpExecutor::GetTarget()
{
  return mDocument;
}

// nsContentSink overrides

void
nsHtml5TreeOpExecutor::UpdateChildCounts()
{
  // No-op
}

nsresult
nsHtml5TreeOpExecutor::FlushTags()
{
    return NS_OK;
}

void
nsHtml5TreeOpExecutor::PostEvaluateScript(nsIScriptElement *aElement)
{
  nsCOMPtr<nsIHTMLDocument> htmlDocument = do_QueryInterface(mDocument);
  NS_ASSERTION(htmlDocument, "Document didn't QI into HTML document.");
  htmlDocument->ScriptExecuted(aElement);
}

void
nsHtml5TreeOpExecutor::UpdateStyleSheet(nsIContent* aElement)
{
  // Break out of the doc update created by Flush() to zap a runnable 
  // waiting to call UpdateStyleSheet without the right observer
  EndDocUpdate();

  if (NS_UNLIKELY(!mParser)) {
    // EndDocUpdate ran stuff that called nsIParser::Terminate()
    return;
  }

  nsCOMPtr<nsIStyleSheetLinkingElement> ssle(do_QueryInterface(aElement));
  NS_ASSERTION(ssle, "Node didn't QI to style.");

  ssle->SetEnableUpdates(PR_TRUE);

  PRBool willNotify;
  PRBool isAlternate;
  nsresult rv = ssle->UpdateStyleSheet(this, &willNotify, &isAlternate);
  if (NS_SUCCEEDED(rv) && willNotify && !isAlternate) {
    ++mPendingSheetCount;
    mScriptLoader->AddExecuteBlocker();
  }

  // Re-open update
  BeginDocUpdate();
}

class nsHtml5ExecutorReflusher : public nsRunnable
{
  private:
    nsRefPtr<nsHtml5TreeOpExecutor> mExecutor;
  public:
    nsHtml5ExecutorReflusher(nsHtml5TreeOpExecutor* aExecutor)
      : mExecutor(aExecutor)
    {}
    NS_IMETHODIMP Run()
    {
      mExecutor->Flush(PR_FALSE);
      return NS_OK;
    }
};

void
nsHtml5TreeOpExecutor::Flush(PRBool aForceWholeQueue)
{
  if (!mParser) {
    mOpQueue.Clear(); // clear in order to be able to assert in destructor
    return;
  }
  if (mFlushState != eNotFlushing) {
    return;
  }
  
  mFlushState = eInFlush;

  nsRefPtr<nsHtml5TreeOpExecutor> kungFuDeathGrip(this); // avoid crashing near EOF
  nsCOMPtr<nsIParser> parserKungFuDeathGrip(mParser);

  if (mReadingFromStage) {
    mStage.MoveOpsTo(mOpQueue);
  }
  
  nsIContent* scriptElement = nsnull;
  
  BeginDocUpdate();

  PRIntervalTime flushStart = 0;
  PRUint32 numberOfOpsToFlush = mOpQueue.Length();
  PRBool reflushNeeded = PR_FALSE;

#ifdef DEBUG_NS_HTML5_TREE_OP_EXECUTOR_FLUSH
  if (numberOfOpsToFlush > sOpQueueMaxLength) {
    sOpQueueMaxLength = numberOfOpsToFlush;
  }
  printf("QUEUE LENGTH: %d\n", numberOfOpsToFlush);
  printf("MAX QUEUE LENGTH: %d\n", sOpQueueMaxLength);
#endif

  if (aForceWholeQueue) {
    if (numberOfOpsToFlush > (PRUint32)sTreeOpQueueMinLength) {
      flushStart = PR_IntervalNow(); // compute averages only if enough ops
    }    
  } else {
    if (numberOfOpsToFlush > (PRUint32)sTreeOpQueueMinLength) {
      flushStart = PR_IntervalNow(); // compute averages only if enough ops
      if (numberOfOpsToFlush > (PRUint32)sTreeOpQueueLengthLimit) {
        numberOfOpsToFlush = (PRUint32)sTreeOpQueueLengthLimit;
        reflushNeeded = PR_TRUE;
      }
    }
  }

  mElementsSeenInThisAppendBatch.SetCapacity(numberOfOpsToFlush * 2);

  const nsHtml5TreeOperation* start = mOpQueue.Elements();
  const nsHtml5TreeOperation* end = start + numberOfOpsToFlush;
  for (nsHtml5TreeOperation* iter = (nsHtml5TreeOperation*)start; iter < end; ++iter) {
    if (NS_UNLIKELY(!mParser)) {
      // The previous tree op caused a call to nsIParser::Terminate();
      break;
    }
    NS_ASSERTION(mFlushState == eInDocUpdate, "Tried to perform tree op outside update batch.");
    iter->Perform(this, &scriptElement);
  }

  if (NS_LIKELY(mParser)) {
    mOpQueue.RemoveElementsAt(0, numberOfOpsToFlush);  
  } else {
    mOpQueue.Clear(); // only for diagnostics in the destructor of this class
  }
  
  if (flushStart) {
    PRUint32 delta = PR_IntervalToMilliseconds(PR_IntervalNow() - flushStart);
    sTreeOpQueueLengthLimit = delta ?
      (PRUint32)(((PRUint64)sTreeOpQueueMaxTime * (PRUint64)numberOfOpsToFlush)
                 / delta) :
      sTreeOpQueueMaxLength; // if the delta is less than one ms, use max
    if (sTreeOpQueueLengthLimit < sTreeOpQueueMinLength) {
      // both are signed and sTreeOpQueueMinLength is always positive, so this
      // also takes care of the theoretical overflow of sTreeOpQueueLengthLimit
      sTreeOpQueueLengthLimit = sTreeOpQueueMinLength;
    }
    if (sTreeOpQueueLengthLimit > sTreeOpQueueMaxLength) {
      sTreeOpQueueLengthLimit = sTreeOpQueueMaxLength;
    }
#ifdef DEBUG_NS_HTML5_TREE_OP_EXECUTOR_FLUSH
    printf("FLUSH DURATION (millis): %d\n", delta);
    printf("QUEUE NEW MAX LENGTH: %d\n", sTreeOpQueueLengthLimit);      
#endif
  }

  EndDocUpdate();

  mFlushState = eNotFlushing;

  if (NS_UNLIKELY(!mParser)) {
    return;
  }

  if (scriptElement) {
    NS_ASSERTION(!reflushNeeded, "Got scriptElement when queue not fully flushed.");
    RunScript(scriptElement); // must be tail call when mFlushState is eNotFlushing
  } else if (reflushNeeded) {
#ifdef DEBUG_NS_HTML5_TREE_OP_EXECUTOR_FLUSH
    printf("REFLUSH SCHEDULED.\n");
#endif
    nsCOMPtr<nsIRunnable> flusher = new nsHtml5ExecutorReflusher(this);  
    if (NS_FAILED(NS_DispatchToMainThread(flusher))) {
      NS_WARNING("failed to dispatch executor flush event");
    }
  }
}

nsresult
nsHtml5TreeOpExecutor::ProcessBASETag(nsIContent* aContent)
{
  NS_ASSERTION(aContent, "missing base-element");
  if (mHasProcessedBase) {
    return NS_OK;
  }
  mHasProcessedBase = PR_TRUE;
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

// copied from HTML content sink
PRBool
nsHtml5TreeOpExecutor::IsScriptEnabled()
{
  NS_ENSURE_TRUE(mDocument && mDocShell, PR_TRUE);
  nsCOMPtr<nsIScriptGlobalObject> globalObject = mDocument->GetScriptGlobalObject();
  // Getting context is tricky if the document hasn't had its
  // GlobalObject set yet
  if (!globalObject) {
    nsCOMPtr<nsIScriptGlobalObjectOwner> owner = do_GetInterface(mDocShell);
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
    CanExecuteScripts(cx, mDocument->NodePrincipal(), &enabled);
  return enabled;
}

void
nsHtml5TreeOpExecutor::DocumentMode(nsHtml5DocumentMode m)
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
void
nsHtml5TreeOpExecutor::RunScript(nsIContent* aScriptElement)
{
  NS_ASSERTION(aScriptElement, "No script to run");
  nsCOMPtr<nsIScriptElement> sele = do_QueryInterface(aScriptElement);
  
  if (!mParser) {
    NS_ASSERTION(sele->IsMalformed(), "Script wasn't marked as malformed.");
    // We got here not because of an end tag but because the tree builder
    // popped an incomplete script element on EOF. Returning here to avoid
    // calling back into mParser anymore.
    return;
  }
  
  if (mFragmentMode) {
    // ending the doc update called nsIParser::Terminate or we are in the
    // fragment mode
    sele->PreventExecution();
    return;
  }

  if (sele->GetScriptDeferred() || sele->GetScriptAsync()) {
    #ifdef DEBUG
    nsresult rv = 
    #endif
    aScriptElement->DoneAddingChildren(PR_TRUE); // scripts ignore the argument
    NS_ASSERTION(rv != NS_ERROR_HTMLPARSER_BLOCK, 
                 "Defer or async script tried to block.");
    return;
  }
  
  NS_ASSERTION(mFlushState == eNotFlushing, "Tried to run script when flushing.");

  mReadingFromStage = PR_FALSE;
  
  sele->SetCreatorParser(mParser);

  // Notify our document that we're loading this script.
  nsCOMPtr<nsIHTMLDocument> htmlDocument = do_QueryInterface(mDocument);
  NS_ASSERTION(htmlDocument, "Document didn't QI into HTML document.");
  htmlDocument->ScriptLoading(sele);

  // Copied from nsXMLContentSink
  // Now tell the script that it's ready to go. This may execute the script
  // or return NS_ERROR_HTMLPARSER_BLOCK. Or neither if the script doesn't
  // need executing.
  nsresult rv = aScriptElement->DoneAddingChildren(PR_TRUE);

  // If the act of insertion evaluated the script, we're fine.
  // Else, block the parser till the script has loaded.
  if (rv == NS_ERROR_HTMLPARSER_BLOCK) {
    mScriptElements.AppendObject(sele);
    mParser->BlockParser();
  } else {
    // This may have already happened if the script executed, but in case
    // it didn't then remove the element so that it doesn't get stuck forever.
    htmlDocument->ScriptExecuted(sele);
    // mParser may have been nulled out by now, but nsContentSink deals
    ContinueInterruptedParsingAsync();
  }
}

nsresult
nsHtml5TreeOpExecutor::Init(nsIDocument* aDoc,
                            nsIURI* aURI,
                            nsISupports* aContainer,
                            nsIChannel* aChannel)
{
  nsresult rv = nsContentSink::Init(aDoc, aURI, aContainer, aChannel);
  NS_ENSURE_SUCCESS(rv, rv);
  mCanInterruptParser = PR_FALSE; // without this, nsContentSink calls
                                  // UnblockOnload from DropParserAndPerfHint
  return rv;
}

void
nsHtml5TreeOpExecutor::Start()
{
  NS_PRECONDITION(!mStarted, "Tried to start when already started.");
  mStarted = PR_TRUE;
}

void
nsHtml5TreeOpExecutor::NeedsCharsetSwitchTo(const char* aEncoding)
{
  EndDocUpdate();

  if(NS_UNLIKELY(!mParser)) {
    // got terminate
    return;
  }
  
  nsresult rv = NS_OK;
  nsCOMPtr<nsIWebShellServices> wss = do_QueryInterface(mDocShell);
  if (!wss) {
    return;
  }
#ifndef DONT_INFORM_WEBSHELL
  // ask the webshellservice to load the URL
  if (NS_FAILED(rv = wss->SetRendering(PR_FALSE))) {
    // do nothing and fall thru
  } else if (NS_FAILED(rv = wss->StopDocumentLoad())) {
    rv = wss->SetRendering(PR_TRUE); // turn on the rendering so at least we will see something.
  } else if (NS_FAILED(rv = wss->ReloadDocument(aEncoding, kCharsetFromMetaTag))) {
    rv = wss->SetRendering(PR_TRUE); // turn on the rendering so at least we will see something.
  }
  // if the charset switch was accepted, wss has called Terminate() on the
  // parser by now
#endif

  if (!mParser) {
    // success
    return;
  }

  (static_cast<nsHtml5Parser*> (mParser.get()))->ContinueAfterFailedCharsetSwitch();

  BeginDocUpdate();
}

nsHtml5Tokenizer*
nsHtml5TreeOpExecutor::GetTokenizer()
{
  return (static_cast<nsHtml5Parser*> (mParser.get()))->GetTokenizer();
}

void
nsHtml5TreeOpExecutor::Reset() {
  mHasProcessedBase = PR_FALSE;
  mReadingFromStage = PR_FALSE;
  mOpQueue.Clear();
  mStarted = PR_FALSE;
  mFlushState = eNotFlushing;
  mFragmentMode = PR_FALSE;
}

void
nsHtml5TreeOpExecutor::MoveOpsFrom(nsTArray<nsHtml5TreeOperation>& aOpQueue)
{
  NS_PRECONDITION(mFlushState == eNotFlushing, "mOpQueue modified during tree op execution.");
  if (mOpQueue.IsEmpty()) {
    mOpQueue.SwapElements(aOpQueue);
    return;
  }
  mOpQueue.MoveElementsFrom(aOpQueue);
}

void
nsHtml5TreeOpExecutor::InitializeDocWriteParserState(nsAHtml5TreeBuilderState* aState, PRInt32 aLine)
{
  static_cast<nsHtml5Parser*> (mParser.get())->InitializeDocWriteParserState(aState, aLine);
}

#ifdef DEBUG_NS_HTML5_TREE_OP_EXECUTOR_FLUSH
PRUint32 nsHtml5TreeOpExecutor::sOpQueueMaxLength = 0;
PRUint32 nsHtml5TreeOpExecutor::sAppendBatchMaxSize = 0;
PRUint32 nsHtml5TreeOpExecutor::sAppendBatchSlotsExamined = 0;
PRUint32 nsHtml5TreeOpExecutor::sAppendBatchExaminations = 0;
#endif
