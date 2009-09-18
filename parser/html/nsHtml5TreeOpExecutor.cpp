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

#define NS_HTML5_TREE_OP_EXECUTOR_MAX_QUEUE_TIME 3000UL // milliseconds
#define NS_HTML5_TREE_OP_EXECUTOR_DEFAULT_QUEUE_LENGTH 200
#define NS_HTML5_TREE_OP_EXECUTOR_MIN_QUEUE_LENGTH 100
#define NS_HTML5_TREE_OP_EXECUTOR_MAX_TIME_WITHOUT_FLUSH 5000 // milliseconds

NS_IMPL_CYCLE_COLLECTION_CLASS(nsHtml5TreeOpExecutor)

NS_INTERFACE_TABLE_HEAD_CYCLE_COLLECTION_INHERITED(nsHtml5TreeOpExecutor)
  NS_INTERFACE_TABLE_INHERITED1(nsHtml5TreeOpExecutor, 
                                nsIContentSink)
NS_INTERFACE_TABLE_TAIL_INHERITING(nsContentSink)

NS_IMPL_ADDREF_INHERITED(nsHtml5TreeOpExecutor, nsContentSink)

NS_IMPL_RELEASE_INHERITED(nsHtml5TreeOpExecutor, nsContentSink)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(nsHtml5TreeOpExecutor, nsContentSink)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mFlushTimer);
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mScriptElement)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(nsHtml5TreeOpExecutor, nsContentSink)
  if (tmp->mFlushTimer) {
    tmp->mFlushTimer->Cancel();
  }
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mFlushTimer);
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mScriptElement)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END


nsHtml5TreeOpExecutor::nsHtml5TreeOpExecutor()
  : mSuppressEOF(PR_FALSE)
  , mHasProcessedBase(PR_FALSE)
  , mNeedsFlush(PR_FALSE)
  , mFlushTimer(do_CreateInstance("@mozilla.org/timer;1"))
  , mNeedsCharsetSwitch(PR_FALSE)
{

}

nsHtml5TreeOpExecutor::~nsHtml5TreeOpExecutor()
{
  NS_ASSERTION(mOpQueue.IsEmpty(), "Somehow there's stuff in the op queue.");
  if (mFlushTimer) {
    mFlushTimer->Cancel(); // XXX why is this even necessary? it is, though.
  }
}

static void
TimerCallbackFunc(nsITimer* aTimer, void* aClosure)
{
  (static_cast<nsHtml5TreeOpExecutor*> (aClosure))->DeferredTimerFlush();
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
nsHtml5TreeOpExecutor::DidBuildModel()
{
  NS_ASSERTION(mLifeCycle == STREAM_ENDING, "Bad life cycle.");
  mLifeCycle = TERMINATED;
  if (!mSuppressEOF) {
    GetTokenizer()->eof();
    Flush();
  }
  GetTokenizer()->end();
  // This is comes from nsXMLContentSink
  DidBuildModelImpl();
  mDocument->ScriptLoader()->RemoveObserver(this);
  nsContentSink::StartLayout(PR_FALSE);
  ScrollToRef();
  mDocument->RemoveObserver(this);
  mDocument->EndLoad();
  static_cast<nsHtml5Parser*> (mParser.get())->DropStreamParser();
  static_cast<nsHtml5Parser*> (mParser.get())->CancelParsingEvents();
  DropParserAndPerfHint();
#ifdef GATHER_DOCWRITE_STATISTICS
  printf("UNSAFE SCRIPTS: %d\n", sUnsafeDocWrites);
  printf("TOKENIZER-SAFE SCRIPTS: %d\n", sTokenSafeDocWrites);
  printf("TREEBUILDER-SAFE SCRIPTS: %d\n", sTreeSafeDocWrites);
#endif
#ifdef DEBUG_hsivonen
  printf("MAX INSERTION BATCH LEN: %d\n", sInsertionBatchMaxLength);
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
nsHtml5TreeOpExecutor::Flush()
{
  mNeedsFlush = PR_FALSE;
  FillQueue();
  
  MOZ_AUTO_DOC_UPDATE(GetDocument(), UPDATE_CONTENT_MODEL, PR_TRUE);
  PRIntervalTime flushStart = 0;
  PRUint32 opQueueLength = mOpQueue.Length();
  if (opQueueLength > NS_HTML5_TREE_OP_EXECUTOR_MIN_QUEUE_LENGTH) { // avoid computing averages with too few ops
    flushStart = PR_IntervalNow();
  }
  mElementsSeenInThisAppendBatch.SetCapacity(opQueueLength * 2);
  // XXX alloc failure
  const nsHtml5TreeOperation* start = mOpQueue.Elements();
  const nsHtml5TreeOperation* end = start + opQueueLength;
  for (nsHtml5TreeOperation* iter = (nsHtml5TreeOperation*)start; iter < end; ++iter) {
    iter->Perform(this);
  }
  FlushPendingAppendNotifications();
#ifdef DEBUG_hsivonen
  if (mOpQueue.Length() > sInsertionBatchMaxLength) {
    sInsertionBatchMaxLength = opQueueLength;
  }
#endif
  mOpQueue.Clear();
  if (flushStart) {
    PRUint32 delta = PR_IntervalToMilliseconds(PR_IntervalNow() - flushStart);
    sTreeOpQueueMaxLength = delta ?
      (PRUint32)((NS_HTML5_TREE_OP_EXECUTOR_MAX_QUEUE_TIME * (PRUint64)opQueueLength) / delta) :
      0;
    if (sTreeOpQueueMaxLength < NS_HTML5_TREE_OP_EXECUTOR_MIN_QUEUE_LENGTH) {
      sTreeOpQueueMaxLength = NS_HTML5_TREE_OP_EXECUTOR_MIN_QUEUE_LENGTH;
    }
#ifdef DEBUG_hsivonen
    printf("QUEUE MAX LENGTH: %d\n", sTreeOpQueueMaxLength);
#endif
  }
  mFlushTimer->InitWithFuncCallback(TimerCallbackFunc, static_cast<void*> (this), NS_HTML5_TREE_OP_EXECUTOR_MAX_TIME_WITHOUT_FLUSH, nsITimer::TYPE_ONE_SHOT);
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

nsresult
nsHtml5TreeOpExecutor::MaybePerformCharsetSwitch()
{
  if (!mNeedsCharsetSwitch) {
    return NS_ERROR_HTMLPARSER_CONTINUE;
  }
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
    mNeedsCharsetSwitch = PR_FALSE;
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
nsHtml5TreeOpExecutor::ExecuteScript()
{
  NS_PRECONDITION(mScriptElement, "Trying to run a script without having one!");
  Flush();
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
    mParser->BlockParser();
  } else {
    // This may have already happened if the script executed, but in case
    // it didn't then remove the element so that it doesn't get stuck forever.
    htmlDocument->ScriptExecuted(sele);
  }
  mScriptElement = nsnull;
}

nsresult
nsHtml5TreeOpExecutor::Init(nsIDocument* aDoc,
                            nsIURI* aURI,
                            nsISupports* aContainer,
                            nsIChannel* aChannel)
{
  nsresult rv = nsContentSink::Init(aDoc, aURI, aContainer, aChannel);
  NS_ENSURE_SUCCESS(rv, rv);
  aDoc->AddObserver(this);
  return rv;
}

void
nsHtml5TreeOpExecutor::Start()
{
  mNeedsFlush = PR_FALSE;
  mNeedsCharsetSwitch = PR_FALSE;
  mPendingCharset.Truncate();
  mScriptElement = nsnull;
}

void
nsHtml5TreeOpExecutor::End()
{
  mFlushTimer->Cancel();
}

void
nsHtml5TreeOpExecutor::NeedsCharsetSwitchTo(const nsACString& aEncoding)
{
  mNeedsCharsetSwitch = PR_TRUE;
  mPendingCharset.Assign(aEncoding);
}

nsHtml5Tokenizer*
nsHtml5TreeOpExecutor::GetTokenizer()
{
  return (static_cast<nsHtml5Parser*> (mParser.get()))->GetTokenizer();
}

void
nsHtml5TreeOpExecutor::MaybeSuspend() {
  if (!mNeedsFlush) {
    mNeedsFlush = !!(mTreeBuilder->GetOpQueueLength() >= sTreeOpQueueMaxLength);
  }
  if (DidProcessATokenImpl() == NS_ERROR_HTMLPARSER_INTERRUPTED || mNeedsFlush) {
    // We've been in the parser for too long and/or the op queue is becoming too
    // long to flush in one go it it grows further.
    static_cast<nsHtml5Parser*>(mParser.get())->Suspend();
    mTreeBuilder->ReqSuspension();
  }
}

void
nsHtml5TreeOpExecutor::MaybeExecuteScript() {
  if (mScriptElement) {
    // mUninterruptibleDocWrite = PR_FALSE;
    ExecuteScript();
    if (mStreamParser) {
      mStreamParser->Suspend();
    }
  }
}

void
nsHtml5TreeOpExecutor::DeferredTimerFlush() {
  if (mTreeBuilder->GetOpQueueLength() > 0) {
    mNeedsFlush = PR_TRUE;
  }
}

void
nsHtml5TreeOpExecutor::FillQueue() {
  mTreeBuilder->SwapQueue(mOpQueue);
}

void
nsHtml5TreeOpExecutor::Reset() {
  mSuppressEOF = PR_FALSE;
  mHasProcessedBase = PR_FALSE;
  mNeedsFlush = PR_FALSE;
  mOpQueue.Clear();
  mPendingCharset.Truncate();
  mNeedsCharsetSwitch = PR_FALSE;
  mLifeCycle = NOT_STARTED;
  mScriptElement = nsnull;
}

PRUint32 nsHtml5TreeOpExecutor::sTreeOpQueueMaxLength = NS_HTML5_TREE_OP_EXECUTOR_DEFAULT_QUEUE_LENGTH;
#ifdef DEBUG_hsivonen
PRUint32 nsHtml5TreeOpExecutor::sInsertionBatchMaxLength = 0;
PRUint32 nsHtml5TreeOpExecutor::sAppendBatchMaxSize = 0;
PRUint32 nsHtml5TreeOpExecutor::sAppendBatchSlotsExamined = 0;
PRUint32 nsHtml5TreeOpExecutor::sAppendBatchExaminations = 0;
#endif
