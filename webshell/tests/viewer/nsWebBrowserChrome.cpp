/* -*- Mode: C++; tab-width: 3; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is the Mozilla browser.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications, Inc.  Portions created by Netscape are
 * Copyright (C) 1999, Mozilla.  All Rights Reserved.
 * 
 * Contributor(s):
 *   Travis Bogard <travis@netscape.com>
 *   Brian Ryner <bryner@netscape.com>
 */

// Local Includes
#include "nsWebBrowserChrome.h"
#include "nsBrowserWindow.h"
#include "nsWebCrawler.h"
#include "nsViewerApp.h"

// Helper Classes
#include "nsIGenericFactory.h"
#include "nsString.h"
#include "nsXPIDLString.h"

// Interfaces needed to be included
#include "nsIDocShellTreeItem.h"
#include "nsIWebProgress.h"
#include "nsIChannel.h"
#include "nsIURI.h"
#include "nsIDOMWindow.h"

struct JSContext;
#include "nsIJSContextStack.h"

// CIDs
#include "nsWidgetsCID.h"
static NS_DEFINE_CID(kAppShellCID, NS_APPSHELL_CID);

//*****************************************************************************
//***    nsWebBrowserChrome: Object Management
//*****************************************************************************

nsWebBrowserChrome::nsWebBrowserChrome() : mBrowserWindow(nsnull), mTimerSet(PR_FALSE)

{
	NS_INIT_REFCNT();

   mActiveDocuments = 0;
   mChromeFlags = 0;
   mSizeSet = PR_FALSE;
   mContinueModalLoop = PR_FALSE;
   mModalStatus = NS_OK;
   mChromeLoaded = PR_FALSE;
}

nsWebBrowserChrome::~nsWebBrowserChrome()
{
}

//*****************************************************************************
// nsWebBrowserChrome::nsISupports
//*****************************************************************************   

NS_IMPL_ADDREF(nsWebBrowserChrome)
NS_IMPL_RELEASE(nsWebBrowserChrome)

NS_INTERFACE_MAP_BEGIN(nsWebBrowserChrome)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIWebBrowserChrome)
  NS_INTERFACE_MAP_ENTRY(nsIInterfaceRequestor)
  NS_INTERFACE_MAP_ENTRY(nsIWebBrowserChrome)
  NS_INTERFACE_MAP_ENTRY(nsIWebBrowserChromeFocus)
  NS_INTERFACE_MAP_ENTRY(nsIEmbeddingSiteWindow)
  NS_INTERFACE_MAP_ENTRY(nsIWebProgressListener)
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
NS_INTERFACE_MAP_END

//*****************************************************************************
// nsWebBrowserChrome::nsIInterfaceRequestor
//*****************************************************************************   

NS_IMETHODIMP nsWebBrowserChrome::GetInterface(const nsIID &aIID, void** aInstancePtr)
{
   return QueryInterface(aIID, aInstancePtr);
}

//*****************************************************************************
// nsWebBrowserChrome::nsIWebBrowserChrome
//*****************************************************************************   
NS_IMETHODIMP nsWebBrowserChrome::SetStatus(PRUint32 aStatusType, const PRUnichar* aStatus)
{
   NS_ENSURE_STATE(mBrowserWindow->mStatus);

   switch (aStatusType)
     {
     case STATUS_SCRIPT:
     case STATUS_LINK:
       {
         NS_ENSURE_STATE(mBrowserWindow->mStatus);
         PRUint32 size;
         mBrowserWindow->mStatus->SetText(nsAutoString(aStatus), size);
       }
       break;
     }
   return NS_OK;
}

NS_IMETHODIMP nsWebBrowserChrome::SetWebBrowser(nsIWebBrowser* aWebBrowser)
{
   NS_ERROR("Haven't Implemented this yet");
   return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsWebBrowserChrome::GetWebBrowser(nsIWebBrowser** aWebBrowser)
{
   NS_ERROR("Haven't Implemented this yet");
   return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsWebBrowserChrome::SetChromeFlags(PRUint32 aChromeFlags)
{
   mChromeFlags = aChromeFlags;
   return NS_OK;
}

NS_IMETHODIMP nsWebBrowserChrome::GetChromeFlags(PRUint32* aChromeFlags)
{
   *aChromeFlags = mChromeFlags;
   return NS_OK;
}

NS_IMETHODIMP nsWebBrowserChrome::CreateBrowserWindow(PRUint32 aChromeMask,
   PRInt32 aX, PRInt32 aY, PRInt32 aCX, PRInt32 aCY, nsIWebBrowser** aWebBrowser)
{
   if(mBrowserWindow->mWebCrawler && (mBrowserWindow->mWebCrawler->Crawling() || 
      mBrowserWindow->mWebCrawler->LoadingURLList()))
      {
      // Do not fly javascript popups when we are crawling
      *aWebBrowser = nsnull;
      return NS_ERROR_NOT_IMPLEMENTED;
      }

   nsBrowserWindow* browser = nsnull;
   mBrowserWindow->mApp->OpenWindow(aChromeMask, browser);

   NS_ENSURE_TRUE(browser, NS_ERROR_FAILURE);
   browser->mWebBrowserChrome->SetChromeFlags(aChromeMask);

   *aWebBrowser = browser->mWebBrowser;
   NS_IF_ADDREF(*aWebBrowser);
   return NS_OK;
}

NS_IMETHODIMP nsWebBrowserChrome::DestroyBrowserWindow()
{
   ExitModalEventLoop(NS_OK);
   return mBrowserWindow->Destroy();
}

#if 0
/* Just commenting out for now because it looks like somebody went to
   a lot of work here. This method has been removed from nsIWebBrowserChrome
   per the 5 Feb 01 API review, to be handled one level further down
   in nsDocShellTreeOwner.
*/
NS_IMETHODIMP nsWebBrowserChrome::FindNamedBrowserItem(const PRUnichar* aName,
   nsIDocShellTreeItem** aBrowserItem)
{
   NS_ENSURE_ARG_POINTER(aBrowserItem);
   *aBrowserItem = nsnull;

   PRInt32 i = 0;
   PRInt32 n = mBrowserWindow->gBrowsers.Count();

   nsString aNameStr(aName);

   for (i = 0; i < n; i++)
      {
      nsBrowserWindow* bw = (nsBrowserWindow*)mBrowserWindow->gBrowsers.ElementAt(i);
      nsCOMPtr<nsIDocShellTreeItem> docShellAsItem(do_QueryInterface(bw->mWebBrowser));
      NS_ENSURE_TRUE(docShellAsItem, NS_ERROR_FAILURE);

      docShellAsItem->FindItemWithName(aName, NS_STATIC_CAST(nsIWebBrowserChrome*, this), aBrowserItem);

      if(!*aBrowserItem)
         return NS_OK;
      }
   return NS_OK;
}
#endif

NS_IMETHODIMP nsWebBrowserChrome::SizeBrowserTo(PRInt32 aCX, PRInt32 aCY)
{
   mSizeSet = PR_TRUE;
   mBrowserWindow->mWindow->Resize(aCX, aCY, PR_FALSE);
   mBrowserWindow->Layout(aCX, aCY);

   return NS_OK;
}

NS_IMETHODIMP nsWebBrowserChrome::ShowAsModal()
{
  /* Copied from nsXULWindow */
  nsCOMPtr<nsIAppShell> appShell(do_CreateInstance(kAppShellCID));
  if (!appShell)
    return NS_ERROR_FAILURE;

  appShell->Create(0, nsnull);
  appShell->Spinup();
  // Store locally so it doesn't die on us
  nsCOMPtr<nsIWidget> window = mBrowserWindow->mWindow;

  window->SetModal(PR_TRUE);
  mContinueModalLoop = PR_TRUE;
  EnableParent(PR_FALSE);

  nsCOMPtr<nsIJSContextStack> stack(do_GetService("@mozilla.org/js/xpc/ContextStack;1"));
  nsresult rv = NS_OK;
  if (stack && NS_SUCCEEDED(stack->Push(nsnull))) {
    while (NS_SUCCEEDED(rv) && mContinueModalLoop) {
      void* data;
      PRBool isRealEvent;
      PRBool processEvent;

      rv = appShell->GetNativeEvent(isRealEvent, data);
      if (NS_SUCCEEDED(rv)) {
        window->ModalEventFilter(isRealEvent, data, &processEvent);
        if (processEvent)
          appShell->DispatchNativeEvent(isRealEvent, data);
      }
    }

    JSContext* cx;
    stack->Pop(&cx);
    NS_ASSERTION(!cx, "JSContextStack mismatch");
  } else
    rv = NS_ERROR_FAILURE;

  mContinueModalLoop = PR_FALSE;
  window->SetModal(PR_FALSE);
  appShell->Spindown();

  return mModalStatus;
}

NS_IMETHODIMP nsWebBrowserChrome::IsWindowModal(PRBool *_retval)
{
  *_retval = mContinueModalLoop;
  return NS_OK;
}

NS_IMETHODIMP nsWebBrowserChrome::ExitModalEventLoop(nsresult aStatus)
{
  if (mContinueModalLoop)
    EnableParent(PR_TRUE);
  mContinueModalLoop = PR_FALSE;
  mModalStatus = aStatus;
  return NS_OK;
}

//*****************************************************************************
// nsIWebBrowserChromeFocus
//*****************************************************************************

NS_IMETHODIMP
nsWebBrowserChrome::FocusNextElement()
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsWebBrowserChrome::FocusPrevElement()
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

//*****************************************************************************
// nsWebBrowserChrome::nsIEmbeddingSiteWindow
//*****************************************************************************   

NS_IMETHODIMP nsWebBrowserChrome::SetDimensions(PRUint32 aFlags, PRInt32 aX, PRInt32 aY, PRInt32 aCX, PRInt32 aCY)
{
  PRInt32 x, y, cx, cy;
  mBrowserWindow->GetPositionAndSize(&x, &y, &cx, &cy);
  if (aFlags & DIM_FLAGS_POSITION)
  {
    x = aX;
    y = aY;
  }
  if (aFlags & DIM_FLAGS_SIZE_INNER || aFlags & DIM_FLAGS_SIZE_OUTER)
  {
    cx = aCX;
    cy = aCY;
  }
  return mBrowserWindow->SetPositionAndSize(aX, aY, aCX, aCY, PR_TRUE);
}

NS_IMETHODIMP nsWebBrowserChrome::GetDimensions(PRUint32 aFlags, PRInt32 *aX, PRInt32 *aY, PRInt32 *aCX, PRInt32 *aCY)
{
  PRInt32 x, y, cx, cy;
  mBrowserWindow->GetPositionAndSize(&x, &y, &cx, &cy);
  if (aFlags & DIM_FLAGS_POSITION)
  {
    *aX = x;
    *aY = y;
  }
  if (aFlags & DIM_FLAGS_SIZE_INNER || aFlags & DIM_FLAGS_SIZE_OUTER)
  {
    *aCX = cx;
    *aCY = cy;
  }
  return NS_OK;
}

NS_IMETHODIMP nsWebBrowserChrome::GetSiteWindow(void ** aParentNativeWindow)
{
   NS_ENSURE_ARG_POINTER(aParentNativeWindow);

   //XXX First Check In
   NS_ASSERTION(PR_FALSE, "Not Yet Implemented");
   return NS_OK;
}

NS_IMETHODIMP nsWebBrowserChrome::GetVisibility(PRBool *aVisibility)
{
  return mBrowserWindow->GetVisibility(aVisibility);
}

NS_IMETHODIMP nsWebBrowserChrome::SetVisibility(PRBool aVisibility)
{
  if ((mChromeFlags & nsIWebBrowserChrome::CHROME_OPENAS_CHROME) && !mChromeLoaded) {
    // suppress showing the window until the chrome has completely loaded
    return NS_OK;
  } else
    return mBrowserWindow->SetVisibility(aVisibility);
}

NS_IMETHODIMP nsWebBrowserChrome::SetFocus()
{  
   return mBrowserWindow->SetFocus();
}

NS_IMETHODIMP nsWebBrowserChrome::GetTitle(PRUnichar** aTitle)
{
   return mBrowserWindow->GetTitle(aTitle);
}

NS_IMETHODIMP nsWebBrowserChrome::SetTitle(const PRUnichar* aTitle)
{
   mBrowserWindow->mTitle = aTitle;

   nsAutoString newTitle(aTitle);

   newTitle.AppendWithConversion(" - Raptor");
   
   mBrowserWindow->SetTitle(newTitle.get());
   return NS_OK;
}

//*****************************************************************************
// nsWebBrowserChrome::nsIWebProgressListener
//*****************************************************************************   

NS_IMETHODIMP
nsWebBrowserChrome::OnProgressChange(nsIWebProgress* aProgress,
                                      nsIRequest* aRequest,
                                      PRInt32 aCurSelfProgress,
                                      PRInt32 aMaxSelfProgress, 
                                      PRInt32 aCurTotalProgress,
                                      PRInt32 aMaxTotalProgress)
{
  mProgress = aCurTotalProgress;
  mMaxProgress = aMaxTotalProgress;
  if(mBrowserWindow->mStatus) {
    nsAutoString buf;
    PRUint32 size;

    buf.AppendWithConversion("Loaded ");
    buf.AppendInt(mCurrent);
    buf.AppendWithConversion(" of ");
    buf.AppendInt(mTotal);
    buf.AppendWithConversion(" items.  (");
    buf.AppendInt(mProgress);
    buf.AppendWithConversion(" bytes of ");
    buf.AppendInt(mMaxProgress);
    buf.AppendWithConversion(" bytes)");

    mBrowserWindow->mStatus->SetText(buf,size);
  }
   return NS_OK;
}

NS_IMETHODIMP
nsWebBrowserChrome::OnStateChange(nsIWebProgress* aProgress,
                                  nsIRequest* aRequest,
                                  PRInt32 aProgressStateFlags,
                                  nsresult aStatus)
{
  if (aProgressStateFlags & STATE_START) {
    if (aProgressStateFlags & STATE_IS_NETWORK) {
      OnWindowActivityStart();
      OnLoadStart(aRequest);
    }
    if (aProgressStateFlags & STATE_IS_REQUEST) {
      mTotal += 1;
    }
  }

  if (aProgressStateFlags & STATE_STOP) {
    if (aProgressStateFlags & STATE_IS_REQUEST) {
      mCurrent += 1;

      if(mBrowserWindow->mStatus) {
        nsAutoString buf;
        PRUint32 size;

        buf.AppendWithConversion("Loaded ");
        buf.AppendInt(mCurrent);
        buf.AppendWithConversion(" of ");
        buf.AppendInt(mTotal);
        buf.AppendWithConversion(" items.  (");
        buf.AppendInt(mProgress);
        buf.AppendWithConversion(" bytes of ");
        buf.AppendInt(mMaxProgress);
        buf.AppendWithConversion(" bytes)");

        mBrowserWindow->mStatus->SetText(buf,size);
      }
    }

    if (aProgressStateFlags & STATE_IS_NETWORK) {
      OnLoadFinished(aRequest, aProgressStateFlags);
      OnWindowActivityFinished();
    }
  }

  if (aProgressStateFlags & STATE_TRANSFERRING) {
    OnStatusTransferring(aRequest);
  }

  return NS_OK;
}

NS_IMETHODIMP nsWebBrowserChrome::OnLocationChange(nsIWebProgress* aWebProgress,
                                                   nsIRequest* aRequest,
                                                   nsIURI* aURI)
{
   nsXPIDLCString spec;

   if(aURI)
      aURI->GetSpec(getter_Copies(spec));

   PRBool isSubFrameLoad = PR_FALSE; // Is this a subframe load
   if (aWebProgress) {
     nsCOMPtr<nsIDOMWindow>  domWindow;
     nsCOMPtr<nsIDOMWindow>  topDomWindow;
     aWebProgress->GetDOMWindow(getter_AddRefs(domWindow));
     if (domWindow) { // Get root domWindow
       domWindow->GetTop(getter_AddRefs(topDomWindow));
     }
     if (domWindow != topDomWindow)
       isSubFrameLoad = PR_TRUE;
   }

   if(mBrowserWindow->mLocation && !isSubFrameLoad)
      {
      PRUint32 size;
      nsAutoString tmp; tmp.AssignWithConversion(spec);

      mBrowserWindow->mLocation->SetText(tmp,size);
      }

   return NS_OK;
}

NS_IMETHODIMP 
nsWebBrowserChrome::OnStatusChange(nsIWebProgress* aWebProgress,
                                   nsIRequest* aRequest,
                                   nsresult aStatus,
                                   const PRUnichar* aMessage)
{
    if (mBrowserWindow && mBrowserWindow->mStatus) {
        PRUint32 size;
        mBrowserWindow->mStatus->SetText(nsAutoString(aMessage), size);
    }
    return NS_OK;
}

NS_IMETHODIMP 
nsWebBrowserChrome::OnSecurityChange(nsIWebProgress *aWebProgress, 
                                     nsIRequest *aRequest, 
                                     PRInt32 state)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

//*****************************************************************************
// nsWebBrowserChrome: Helpers
//*****************************************************************************   

//*****************************************************************************
// nsWebBrowserChrome: Accessors
//*****************************************************************************   

void nsWebBrowserChrome::BrowserWindow(nsBrowserWindow* aBrowserWindow)
{
   mBrowserWindow = aBrowserWindow;
}

nsBrowserWindow* nsWebBrowserChrome::BrowserWindow()
{
   return mBrowserWindow;
}

//*****************************************************************************
// nsWebBrowserChrome: Status Change Handling
//*****************************************************************************   

void nsWebBrowserChrome::OnLoadStart(nsIRequest* aRequest)
{
mCurrent=mTotal=mProgress=mMaxProgress=0;

  mBrowserWindow->mLoadStartTime = PR_Now();

  if (aRequest) {
    nsresult rv;
    nsCOMPtr<nsIChannel> channel;
    nsCOMPtr<nsIURI> uri;

    channel = do_QueryInterface(aRequest, &rv);
    if (NS_SUCCEEDED(rv)) {
      channel->GetURI(getter_AddRefs(uri));
     
#ifdef MOZ_PERF_METRICS
      if (PR_FALSE == mTimerSet) {
        char* url;
        nsresult rv = NS_OK;
        rv = uri->GetSpec(&url);
        if (NS_SUCCEEDED(rv)) {
          MOZ_TIMER_LOG(("*** Timing layout processes on url: '%s', WebBrowserChrome: %p\n", url, this));
          delete [] url;
        }
        MOZ_TIMER_DEBUGLOG(("Reset and start: nsWebBrowserChrome::OnLoadStart(), this=%p\n", this));
        MOZ_TIMER_RESET(mTotalTime);
        MOZ_TIMER_START(mTotalTime);
        mTimerSet = PR_TRUE;
      }
#endif

      if(mBrowserWindow->mStatus) {
        nsXPIDLCString uriString;

        uri->GetSpec(getter_Copies(uriString));

        nsAutoString url; url.AssignWithConversion(uriString);
        url.AppendWithConversion(": start");
        PRUint32 size;
        mBrowserWindow->mStatus->SetText(url,size);
      }
    }
  } // if (aChannel)
}

void nsWebBrowserChrome::OnLoadFinished(nsIRequest* aRequest,
                                        PRInt32 aProgressStatusFlags)
{
#ifdef MOZ_PERF_METRICS
  if ( /*(aProgressStatusFlags & nsIWebProgress::flag_win_stop) && */mTimerSet ) {
    MOZ_TIMER_DEBUGLOG(("Stop: nsWebShell::OnEndDocumentLoad(), this=%p\n", this));
    MOZ_TIMER_STOP(mTotalTime);
    MOZ_TIMER_LOG(("Total (Layout + Page Load) Time (webBrowserChrome=%p): ", this));
    MOZ_TIMER_PRINT(mTotalTime);
    mTimerSet = PR_FALSE;
  }
#endif

  nsXPIDLCString uriString;
  if(aRequest) {
    nsresult rv;
    nsCOMPtr<nsIChannel> channel;
    nsCOMPtr<nsIURI> uri;

    channel = do_QueryInterface(channel, &rv);
    if (NS_SUCCEEDED(rv)) {
      channel->GetURI(getter_AddRefs(uri));

      uri->GetSpec(getter_Copies(uriString));
    }
  }  
  nsAutoString msg; msg.AssignWithConversion(uriString);

  PRTime endLoadTime = PR_Now();
  if(mBrowserWindow->mShowLoadTimes)
     {
     printf("Loading ");
     fputs(NS_LossyConvertUCS2toASCII(msg).get(), stdout);
     PRTime delta;
     LL_SUB(delta, endLoadTime, mBrowserWindow->mLoadStartTime);
     double usecs;
     LL_L2D(usecs, delta);
     printf(" took %g milliseconds\n", usecs / 1000.0);
     }

  if(mBrowserWindow->mStatus)
     {
//     PRUint32 size;

     msg.AppendWithConversion(" done.");

///      mBrowserWindow->mStatus->SetText(msg, size);
      }
}

void nsWebBrowserChrome::OnStatusDNS(nsIChannel* aChannel)
{
}

void nsWebBrowserChrome::OnStatusConnecting(nsIChannel* aChannel)
{
   nsXPIDLCString uriString;
   if(aChannel)
      {
      nsCOMPtr<nsIURI> uri;
      aChannel->GetURI(getter_AddRefs(uri));

      uri->GetSpec(getter_Copies(uriString));
      }
   
   nsAutoString msg; msg.AssignWithConversion("Connecting to ");
   msg.AppendWithConversion(uriString);
      
   PRUint32 size;
   mBrowserWindow->mStatus->SetText(msg,size);
}

void nsWebBrowserChrome::OnStatusRedirecting(nsIChannel* aChannel)
{
}

void nsWebBrowserChrome::OnStatusNegotiating(nsIChannel* aChannel)
{
}

void nsWebBrowserChrome::OnStatusTransferring(nsIRequest* aRequest)
{
}

void nsWebBrowserChrome::OnWindowActivityStart()
{
   if(mBrowserWindow->mThrobber)
      mBrowserWindow->mThrobber->Start();

}

void nsWebBrowserChrome::OnWindowActivityFinished()
{
   if(mBrowserWindow->mThrobber)
      mBrowserWindow->mThrobber->Stop();

   if (!mSizeSet && (mChromeFlags & nsIWebBrowserChrome::CHROME_OPENAS_CHROME)) {
     nsCOMPtr<nsIDOMWindow> contentWin;
     mBrowserWindow->mWebBrowser->GetContentDOMWindow(getter_AddRefs(contentWin));
     if (contentWin)
       contentWin->SizeToContent();
     mBrowserWindow->SetVisibility(PR_TRUE);
     mChromeLoaded = PR_TRUE;
   }
}

void nsWebBrowserChrome::EnableParent(PRBool aEnable)
{
  nsCOMPtr<nsIWidget> parentWidget = mBrowserWindow->mWindow;
  if (parentWidget)
    parentWidget->Enable(aEnable);
}
