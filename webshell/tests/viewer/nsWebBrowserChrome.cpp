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

// CIDs

//*****************************************************************************
//***    nsWebBrowserChrome: Object Management
//*****************************************************************************

nsWebBrowserChrome::nsWebBrowserChrome() : mBrowserWindow(nsnull), mTimerSet(PR_FALSE)

{
	NS_INIT_REFCNT();

    mActiveDocuments = 0;
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
   NS_INTERFACE_MAP_ENTRY(nsIWebProgressListener)
   NS_INTERFACE_MAP_ENTRY(nsIBaseWindow)
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
   NS_ERROR("Haven't Implemented this yet");
   return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsWebBrowserChrome::GetChromeFlags(PRUint32* aChromeFlags)
{
   NS_ERROR("Haven't Implemented this yet");
   return NS_ERROR_FAILURE;
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

   *aWebBrowser = browser->mWebBrowser;
   NS_IF_ADDREF(*aWebBrowser);
   return NS_OK;
}

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

NS_IMETHODIMP nsWebBrowserChrome::SizeBrowserTo(PRInt32 aCX, PRInt32 aCY)
{
   mBrowserWindow->mWindow->Resize(aCX, aCY, PR_FALSE);
   mBrowserWindow->Layout(aCX, aCY);

   return NS_OK;
}

NS_IMETHODIMP nsWebBrowserChrome::ShowAsModal()
{
   NS_ERROR("Haven't Implemented this yet");
   return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsWebBrowserChrome::ExitModalEventLoop(nsresult aStatus)
{
   NS_ERROR("Haven't Implemented this yet");
   return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsWebBrowserChrome::SetPersistence(PRBool aPersistX, PRBool aPersistY,
                                   PRBool aPersistCX, PRBool aPersistCY,
                                   PRBool aPersistSizeMode)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsWebBrowserChrome::GetPersistence(PRBool* aPersistX, PRBool* aPersistY,
                                   PRBool* aPersistCX, PRBool* aPersistCY,
                                   PRBool* aPersistSizeMode)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

//*****************************************************************************
// nsWebBrowserChrome::nsIBaseWindow
//*****************************************************************************   

NS_IMETHODIMP nsWebBrowserChrome::InitWindow(nativeWindow aParentNativeWindow,
   nsIWidget* parentWidget, PRInt32 x, PRInt32 y, PRInt32 cx, PRInt32 cy)   
{
   // Ignore wigdet parents for now.  Don't think those are a vaild thing to call.
   NS_ENSURE_SUCCESS(SetPositionAndSize(x, y, cx, cy, PR_FALSE), NS_ERROR_FAILURE);

   return NS_OK;
}

NS_IMETHODIMP nsWebBrowserChrome::Create()
{
   NS_ASSERTION(PR_FALSE, "You can't call this");
   return NS_ERROR_UNEXPECTED;
}

NS_IMETHODIMP nsWebBrowserChrome::Destroy()
{
   return mBrowserWindow->Destroy();
}

NS_IMETHODIMP nsWebBrowserChrome::SetPosition(PRInt32 aX, PRInt32 aY)
{
   PRInt32 cx=0;
   PRInt32 cy=0;

   NS_ENSURE_SUCCESS(GetSize(&cx, &cy), NS_ERROR_FAILURE);
   NS_ENSURE_SUCCESS(SetPositionAndSize(aX, aY, cx, cy, PR_FALSE), 
      NS_ERROR_FAILURE);
   return NS_OK;
}

NS_IMETHODIMP nsWebBrowserChrome::GetPosition(PRInt32* aX, PRInt32* aY)
{
   return GetPositionAndSize(aX, aY, nsnull, nsnull);
}

NS_IMETHODIMP nsWebBrowserChrome::SetSize(PRInt32 aCX, PRInt32 aCY, PRBool aRepaint)
{
   PRInt32 x=0;
   PRInt32 y=0;

   NS_ENSURE_SUCCESS(GetPosition(&x, &y), NS_ERROR_FAILURE);
   NS_ENSURE_SUCCESS(SetPositionAndSize(x, y, aCX, aCY, aRepaint), 
      NS_ERROR_FAILURE);

   return NS_OK;
}

NS_IMETHODIMP nsWebBrowserChrome::GetSize(PRInt32* aCX, PRInt32* aCY)
{
   return GetPositionAndSize(nsnull, nsnull, aCX, aCY);
}

NS_IMETHODIMP nsWebBrowserChrome::SetPositionAndSize(PRInt32 aX, PRInt32 aY, 
   PRInt32 aCX, PRInt32 aCY, PRBool aRepaint)
{
   return mBrowserWindow->SetPositionAndSize(aX, aY, aCX, aCY, aRepaint);
}

NS_IMETHODIMP nsWebBrowserChrome::GetPositionAndSize(PRInt32* aX, PRInt32* aY, 
   PRInt32* aCX, PRInt32* aCY)
{
   return mBrowserWindow->GetPositionAndSize(aX, aY, aCX, aCY);
}

NS_IMETHODIMP nsWebBrowserChrome::Repaint(PRBool aForce)
{
   return mBrowserWindow->Repaint(aForce);
}

NS_IMETHODIMP nsWebBrowserChrome::GetParentWidget(nsIWidget** aParentWidget)
{
   NS_ENSURE_ARG_POINTER(aParentWidget);
   //XXX First Check In
   NS_ASSERTION(PR_FALSE, "Not Yet Implemented");
   return NS_OK;
}

NS_IMETHODIMP nsWebBrowserChrome::SetParentWidget(nsIWidget* aParentWidget)
{
   NS_ASSERTION(PR_FALSE, "You can't call this");
   return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsWebBrowserChrome::GetParentNativeWindow(nativeWindow* aParentNativeWindow)
{
   NS_ENSURE_ARG_POINTER(aParentNativeWindow);

   //XXX First Check In
   NS_ASSERTION(PR_FALSE, "Not Yet Implemented");
   return NS_OK;
}

NS_IMETHODIMP nsWebBrowserChrome::SetParentNativeWindow(nativeWindow aParentNativeWindow)
{
   NS_ASSERTION(PR_FALSE, "You can't call this");
   return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsWebBrowserChrome::GetVisibility(PRBool* aVisibility)
{
   return mBrowserWindow->GetVisibility(aVisibility);
}

NS_IMETHODIMP nsWebBrowserChrome::SetVisibility(PRBool aVisibility)
{
   return mBrowserWindow->SetVisibility(aVisibility);
}

NS_IMETHODIMP nsWebBrowserChrome::GetMainWidget(nsIWidget** aMainWidget)
{
   NS_ENSURE_ARG_POINTER(aMainWidget);

   //XXX First Check In
   NS_ASSERTION(PR_FALSE, "Not Yet Implemented");
   return NS_OK;
}

NS_IMETHODIMP nsWebBrowserChrome::SetFocus()
{  
   return mBrowserWindow->SetFocus();
}

NS_IMETHODIMP nsWebBrowserChrome::FocusAvailable(nsIBaseWindow* aCurrentFocus, 
   PRBool* aTookFocus)
{
   return mBrowserWindow->FocusAvailable(aCurrentFocus, aTookFocus);
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
   
   mBrowserWindow->SetTitle(newTitle.GetUnicode());
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

   if(mBrowserWindow->mLocation)
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
    if (mBrowserWindow->mStatus) {
        PRUint32 size;
        mBrowserWindow->mStatus->SetText(nsAutoString(aMessage), size);
    }
    return NS_OK;
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
     fputs(msg, stdout);
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

}
