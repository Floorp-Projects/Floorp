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
#include "nsStyleLinkElement.h"
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
      mExecutor->RunFlushLoop();
      return NS_OK;
    }
};

nsHtml5TreeOpExecutor::nsHtml5TreeOpExecutor()
{
  mPreloadedURLs.Init(23); // Mean # of preloadable resources per page on dmoz
  // zeroing operator new for everything else
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
    // This is needed to avoid unblocking loads too many times on one hand
    // and on the other hand to avoid destroying the frame constructor from
    // within an update batch. See bug 537683.
    EndDocUpdate();
    
    // If the above caused a call to nsIParser::Terminate(), let that call
    // win.
    if (!mParser) {
      return NS_OK;
    }
  }
  
  static_cast<nsHtml5Parser*> (mParser.get())->DropStreamParser();

  // This comes from nsXMLContentSink and nsHTMLContentSink
  DidBuildModelImpl(aTerminated);

  if (!mLayoutStarted) {
    // We never saw the body, and layout never got started. Force
    // layout *now*, to get an initial reflow.

    // NOTE: only force the layout if we are NOT destroying the
    // docshell. If we are destroying it, then starting layout will
    // likely cause us to crash, or at best waste a lot of time as we
    // are just going to tear it down anyway.
    PRBool destroying = PR_TRUE;
    if (mDocShell) {
      mDocShell->IsBeingDestroyed(&destroying);
    }

    if (!destroying) {
      nsContentSink::StartLayout(PR_FALSE);
    }
  }

  ScrollToRef();
  mDocument->ScriptLoader()->RemoveObserver(this);
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
  NS_NOTREACHED("Don't call. For interface compat only.");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsHtml5TreeOpExecutor::WillResume()
{
  NS_NOTREACHED("Don't call. For interface compat only.");
  return NS_ERROR_NOT_IMPLEMENTED;
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
  if (aType >= Flush_InterruptibleLayout) {
    // Bug 577508 / 253951
    nsContentSink::StartLayout(PR_FALSE);
  }
}

void
nsHtml5TreeOpExecutor::SetDocumentCharsetAndSource(nsACString& aCharset, PRInt32 aCharsetSource)
{
  if (mDocument) {
    mDocument->SetDocumentCharacterSetSource(aCharsetSource);
    mDocument->SetDocumentCharacterSet(aCharset);
  }
  if (mDocShell) {
    // the following logic to get muCV is copied from
    // nsHTMLDocument::StartDocumentLoad
    // We need to call muCV->SetPrevDocCharacterSet here in case
    // the charset is detected by parser DetectMetaTag
    nsCOMPtr<nsIMarkupDocumentViewer> mucv;
    nsCOMPtr<nsIContentViewer> cv;
    mDocShell->GetContentViewer(getter_AddRefs(cv));
    if (cv) {
      mucv = do_QueryInterface(cv);
    } else {
      // in this block of code, if we get an error result, we return
      // it but if we get a null pointer, that's perfectly legal for
      // parent and parentContentViewer
      nsCOMPtr<nsIDocShellTreeItem> docShellAsItem =
        do_QueryInterface(mDocShell);
      if (!docShellAsItem) {
    	  return;
      }
      nsCOMPtr<nsIDocShellTreeItem> parentAsItem;
      docShellAsItem->GetSameTypeParent(getter_AddRefs(parentAsItem));
      nsCOMPtr<nsIDocShell> parent(do_QueryInterface(parentAsItem));
      if (parent) {
        nsCOMPtr<nsIContentViewer> parentContentViewer;
        nsresult rv =
          parent->GetContentViewer(getter_AddRefs(parentContentViewer));
        if (NS_SUCCEEDED(rv) && parentContentViewer) {
          mucv = do_QueryInterface(parentContentViewer);
        }
      }
    }
    if (mucv) {
      mucv->SetPrevDocCharacterSet(aCharset);
    }
  }
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
nsHtml5TreeOpExecutor::ContinueInterruptedParsingAsync()
{
  nsCOMPtr<nsIRunnable> flusher = new nsHtml5ExecutorReflusher(this);  
  if (NS_FAILED(NS_DispatchToMainThread(flusher))) {
    NS_WARNING("failed to dispatch executor flush event");
  }          
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

  if (aElement->IsHTML() && aElement->Tag() == nsGkAtoms::link) {
    // look for <link rel="next" href="url">
    nsAutoString relVal;
    aElement->GetAttr(kNameSpaceID_None, nsGkAtoms::rel, relVal);
    if (!relVal.IsEmpty()) {
      // XXX seems overkill to generate this string array
      nsAutoTArray<nsString, 4> linkTypes;
      nsStyleLinkElement::ParseLinkTypes(relVal, linkTypes);
      PRBool hasPrefetch = linkTypes.Contains(NS_LITERAL_STRING("prefetch"));
      if (hasPrefetch || linkTypes.Contains(NS_LITERAL_STRING("next"))) {
        nsAutoString hrefVal;
        aElement->GetAttr(kNameSpaceID_None, nsGkAtoms::href, hrefVal);
        if (!hrefVal.IsEmpty()) {
          PrefetchHref(hrefVal, aElement, hasPrefetch);
        }
      }
      if (linkTypes.Contains(NS_LITERAL_STRING("dns-prefetch"))) {
        nsAutoString hrefVal;
        aElement->GetAttr(kNameSpaceID_None, nsGkAtoms::href, hrefVal);
        if (!hrefVal.IsEmpty()) {
          PrefetchDNS(hrefVal);
        }
      }
    }
  }

  // Re-open update
  BeginDocUpdate();
}

void
nsHtml5TreeOpExecutor::FlushSpeculativeLoads()
{
  if (NS_UNLIKELY(!mParser)) {
    return;
  }
  nsTArray<nsHtml5SpeculativeLoad> speculativeLoadQueue;
  mStage.MoveSpeculativeLoadsTo(speculativeLoadQueue);
  const nsHtml5SpeculativeLoad* start = speculativeLoadQueue.Elements();
  const nsHtml5SpeculativeLoad* end = start + speculativeLoadQueue.Length();
  for (nsHtml5SpeculativeLoad* iter = (nsHtml5SpeculativeLoad*)start;
       iter < end; 
       ++iter) {
    iter->Perform(this);
  }
}

class nsHtml5FlushLoopGuard
{
  private:
    nsRefPtr<nsHtml5TreeOpExecutor> mExecutor;
    #ifdef DEBUG_NS_HTML5_TREE_OP_EXECUTOR_FLUSH
    PRUint32 mStartTime;
    #endif
  public:
    nsHtml5FlushLoopGuard(nsHtml5TreeOpExecutor* aExecutor)
      : mExecutor(aExecutor)
    #ifdef DEBUG_NS_HTML5_TREE_OP_EXECUTOR_FLUSH
      , mStartTime(PR_IntervalToMilliseconds(PR_IntervalNow()))
    #endif
    {
      mExecutor->mRunFlushLoopOnStack = PR_TRUE;
    }
    ~nsHtml5FlushLoopGuard()
    {
      #ifdef DEBUG_NS_HTML5_TREE_OP_EXECUTOR_FLUSH
        PRUint32 timeOffTheEventLoop = 
          PR_IntervalToMilliseconds(PR_IntervalNow()) - mStartTime;
        if (timeOffTheEventLoop > 
            nsHtml5TreeOpExecutor::sLongestTimeOffTheEventLoop) {
          nsHtml5TreeOpExecutor::sLongestTimeOffTheEventLoop = 
            timeOffTheEventLoop;
        }
        printf("Longest time off the event loop: %d\n", 
          nsHtml5TreeOpExecutor::sLongestTimeOffTheEventLoop);
      #endif

      mExecutor->mRunFlushLoopOnStack = PR_FALSE;
    }
};

/**
 * The purpose of the loop here is to avoid returning to the main event loop
 */
void
nsHtml5TreeOpExecutor::RunFlushLoop()
{
  if (mRunFlushLoopOnStack) {
    // There's already a RunFlushLoop() on the call stack.
    return;
  }
  
  nsHtml5FlushLoopGuard guard(this); // this is also the self-kungfu!
  
  nsCOMPtr<nsIParser> parserKungFuDeathGrip(mParser);

  // Remember the entry time
  (void) nsContentSink::WillParseImpl();

  for (;;) {
    if (!mParser) {
      // Parse has terminated.
      mOpQueue.Clear(); // clear in order to be able to assert in destructor
      return;
    }

    if (!mParser->IsParserEnabled()) {
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
      mStage.MoveOpsAndSpeculativeLoadsTo(mOpQueue, speculativeLoadQueue);
      // Make sure speculative loads never start after the corresponding
      // normal loads for the same URLs.
      const nsHtml5SpeculativeLoad* start = speculativeLoadQueue.Elements();
      const nsHtml5SpeculativeLoad* end = start + speculativeLoadQueue.Length();
      for (nsHtml5SpeculativeLoad* iter = (nsHtml5SpeculativeLoad*)start;
           iter < end;
           ++iter) {
        iter->Perform(this);
      }
    } else {
      FlushSpeculativeLoads(); // Make sure speculative loads never start after
                               // the corresponding normal loads for the same
                               // URLs.
      // Not sure if this grip is still needed, but previously, the code
      // gripped before calling ParseUntilBlocked();
      nsRefPtr<nsHtml5StreamParser> streamKungFuDeathGrip = 
        static_cast<nsHtml5Parser*> (mParser.get())->GetStreamParser();
      // Now parse content left in the document.write() buffer queue if any.
      // This may generate tree ops on its own or dequeue a speculation.
      static_cast<nsHtml5Parser*> (mParser.get())->ParseUntilBlocked();
    }

    if (mOpQueue.IsEmpty()) {
      // Avoid bothering the rest of the engine with a doc update if there's 
      // nothing to do.
      return;
    }

    mFlushState = eInFlush;

    nsIContent* scriptElement = nsnull;
    
    BeginDocUpdate();

    PRUint32 numberOfOpsToFlush = mOpQueue.Length();

    mElementsSeenInThisAppendBatch.SetCapacity(numberOfOpsToFlush * 2);

    const nsHtml5TreeOperation* first = mOpQueue.Elements();
    const nsHtml5TreeOperation* last = first + numberOfOpsToFlush - 1;
    for (nsHtml5TreeOperation* iter = (nsHtml5TreeOperation*)first;;) {
      if (NS_UNLIKELY(!mParser)) {
        // The previous tree op caused a call to nsIParser::Terminate().
        break;
      }
      NS_ASSERTION(mFlushState == eInDocUpdate, 
        "Tried to perform tree op outside update batch.");
      iter->Perform(this, &scriptElement);

      // Be sure not to check the deadline if the last op was just performed.
      if (NS_UNLIKELY(iter == last)) {
        break;
      } else if (NS_UNLIKELY(nsContentSink::DidProcessATokenImpl() == 
                             NS_ERROR_HTMLPARSER_INTERRUPTED)) {
        mOpQueue.RemoveElementsAt(0, (iter - first) + 1);
        
        EndDocUpdate();

        mFlushState = eNotFlushing;

        #ifdef DEBUG_NS_HTML5_TREE_OP_EXECUTOR_FLUSH
          printf("REFLUSH SCHEDULED (executing ops): %d\n", 
            ++sTimesFlushLoopInterrupted);
        #endif
        nsHtml5TreeOpExecutor::ContinueInterruptedParsingAsync();
        return;
      }
      ++iter;
    }
    
    mOpQueue.Clear();
    
    EndDocUpdate();

    mFlushState = eNotFlushing;

    if (NS_UNLIKELY(!mParser)) {
      // The parse ended already.
      return;
    }

    if (scriptElement) {
      // must be tail call when mFlushState is eNotFlushing
      RunScript(scriptElement);
      
      // The script execution machinery makes sure this doesn't get deflected.
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

void
nsHtml5TreeOpExecutor::FlushDocumentWrite()
{
  if (!mParser) {
    // The parse has ended.
    mOpQueue.Clear(); // clear in order to be able to assert in destructor
    return;
  }
  
  FlushSpeculativeLoads(); // Make sure speculative loads never start after the 
                // corresponding normal loads for the same URLs.

  if (mFlushState != eNotFlushing) {
    // XXX Can this happen? In case it can, let's avoid crashing.
    return;
  }

  mFlushState = eInFlush;

  // avoid crashing near EOF
  nsRefPtr<nsHtml5TreeOpExecutor> kungFuDeathGrip(this);
  nsCOMPtr<nsIParser> parserKungFuDeathGrip(mParser);

  NS_ASSERTION(!mReadingFromStage,
    "Got doc write flush when reading from stage");

#ifdef DEBUG
  mStage.AssertEmpty();
#endif
  
  nsIContent* scriptElement = nsnull;
  
  BeginDocUpdate();

  PRUint32 numberOfOpsToFlush = mOpQueue.Length();

  mElementsSeenInThisAppendBatch.SetCapacity(numberOfOpsToFlush * 2);

  const nsHtml5TreeOperation* start = mOpQueue.Elements();
  const nsHtml5TreeOperation* end = start + numberOfOpsToFlush;
  for (nsHtml5TreeOperation* iter = (nsHtml5TreeOperation*)start;
       iter < end; 
       ++iter) {
    if (NS_UNLIKELY(!mParser)) {
      // The previous tree op caused a call to nsIParser::Terminate().
      break;
    }
    NS_ASSERTION(mFlushState == eInDocUpdate, 
      "Tried to perform tree op outside update batch.");
    iter->Perform(this, &scriptElement);
  }

  mOpQueue.Clear();
  
  EndDocUpdate();

  mFlushState = eNotFlushing;

  if (NS_UNLIKELY(!mParser)) {
    // Ending the doc update caused a call to nsIParser::Terminate().
    return;
  }

  if (scriptElement) {
    // must be tail call when mFlushState is eNotFlushing
    RunScript(scriptElement);
  }
}

// copied from HTML content sink
PRBool
nsHtml5TreeOpExecutor::IsScriptEnabled()
{
  if (!mDocument || !mDocShell)
    return PR_TRUE;
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
nsHtml5TreeOpExecutor::SetDocumentMode(nsHtml5DocumentMode m)
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

void
nsHtml5TreeOpExecutor::StartLayout() {
  if (mLayoutStarted || !mDocument) {
    return;
  }

  EndDocUpdate();

  if(NS_UNLIKELY(!mParser)) {
    // got terminate
    return;
  }

  nsContentSink::StartLayout(PR_FALSE);

  BeginDocUpdate();
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
    
    // mParser may have been nulled out by now, but the flusher deals

    // If this event isn't needed, it doesn't do anything. It is sometimes
    // necessary for the parse to continue after complex situations.
    nsHtml5TreeOpExecutor::ContinueInterruptedParsingAsync();
  }
}

nsresult
nsHtml5TreeOpExecutor::Init(nsIDocument* aDoc,
                            nsIURI* aURI,
                            nsISupports* aContainer,
                            nsIChannel* aChannel)
{
  return nsContentSink::Init(aDoc, aURI, aContainer, aChannel);
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
  
  nsCOMPtr<nsIWebShellServices> wss = do_QueryInterface(mDocShell);
  if (!wss) {
    return;
  }

  // ask the webshellservice to load the URL
  if (NS_SUCCEEDED(wss->StopDocumentLoad())) {
    wss->ReloadDocument(aEncoding, kCharsetFromMetaTag);
  }
  // if the charset switch was accepted, wss has called Terminate() on the
  // parser by now

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
  DropHeldElements();
  mReadingFromStage = PR_FALSE;
  mOpQueue.Clear();
  mStarted = PR_FALSE;
  mFlushState = eNotFlushing;
  mRunFlushLoopOnStack = PR_FALSE;
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

// Speculative loading

already_AddRefed<nsIURI>
nsHtml5TreeOpExecutor::ConvertIfNotPreloadedYet(const nsAString& aURL)
{
  // The URL of the document without <base>
  nsIURI* documentURI = mDocument->GetDocumentURI();
  // The URL of the document with non-speculative <base>
  nsIURI* documentBaseURI = mDocument->GetDocBaseURI();

  // If the two above are different, use documentBaseURI. If they are the
  // same, the document object isn't aware of a <base>, so attempt to use the
  // mSpeculationBaseURI or, failing, that, documentURI.
  nsIURI* base = (documentURI == documentBaseURI) ?
                  (mSpeculationBaseURI ?
                   mSpeculationBaseURI.get() : documentURI)
                 : documentBaseURI;
  const nsCString& charset = mDocument->GetDocumentCharacterSet();
  nsCOMPtr<nsIURI> uri;
  nsresult rv = NS_NewURI(getter_AddRefs(uri), aURL, charset.get(), base);
  if (NS_FAILED(rv)) {
    NS_WARNING("Failed to create a URI");
    return nsnull;
  }
  nsCAutoString spec;
  uri->GetSpec(spec);
  if (mPreloadedURLs.Contains(spec)) {
    return nsnull;
  }
  mPreloadedURLs.Put(spec);
  nsIURI* retURI = uri;
  NS_ADDREF(retURI);
  return retURI;
}

void
nsHtml5TreeOpExecutor::PreloadScript(const nsAString& aURL,
                                     const nsAString& aCharset,
                                     const nsAString& aType)
{
  nsCOMPtr<nsIURI> uri = ConvertIfNotPreloadedYet(aURL);
  if (!uri) {
    return;
  }
  mDocument->ScriptLoader()->PreloadURI(uri, aCharset, aType);
}

void
nsHtml5TreeOpExecutor::PreloadStyle(const nsAString& aURL,
                                    const nsAString& aCharset)
{
  nsCOMPtr<nsIURI> uri = ConvertIfNotPreloadedYet(aURL);
  if (!uri) {
    return;
  }
  mDocument->PreloadStyle(uri, aCharset);
}

void
nsHtml5TreeOpExecutor::PreloadImage(const nsAString& aURL)
{
  nsCOMPtr<nsIURI> uri = ConvertIfNotPreloadedYet(aURL);
  if (!uri) {
    return;
  }
  mDocument->MaybePreLoadImage(uri);
}

void
nsHtml5TreeOpExecutor::SetSpeculationBase(const nsAString& aURL)
{
  if (mSpeculationBaseURI) {
    // the first one wins
    return;
  }
  const nsCString& charset = mDocument->GetDocumentCharacterSet();
  nsresult rv = NS_NewURI(getter_AddRefs(mSpeculationBaseURI), aURL,
      charset.get(), mDocument->GetDocumentURI());
  if (NS_FAILED(rv)) {
    NS_WARNING("Failed to create a URI");
  }
}

#ifdef DEBUG_NS_HTML5_TREE_OP_EXECUTOR_FLUSH
PRUint32 nsHtml5TreeOpExecutor::sAppendBatchMaxSize = 0;
PRUint32 nsHtml5TreeOpExecutor::sAppendBatchSlotsExamined = 0;
PRUint32 nsHtml5TreeOpExecutor::sAppendBatchExaminations = 0;
PRUint32 nsHtml5TreeOpExecutor::sLongestTimeOffTheEventLoop = 0;
PRUint32 nsHtml5TreeOpExecutor::sTimesFlushLoopInterrupted = 0;
#endif
