/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 *   Blake Ross <blaker@netscape.com> (Original Author)
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
 
#ifndef downloadproxy___h___
#define downloadproxy___h___

#include "nsIDownload.h"
#include "nsIDownloadManager.h"
#include "nsIPrefBranch.h"
#include "nsIPrefService.h"
#include "nsIMIMEInfo.h"

#define DOWNLOAD_MANAGER_BEHAVIOR_PREF "browser.downloadmanager.behavior"

class nsDownloadProxy : public nsIDownload,
                        public nsIWebProgressListener
{
public:

  nsDownloadProxy() { }
  virtual ~nsDownloadProxy() { }

  NS_DECL_ISUPPORTS

  NS_IMETHODIMP Init(nsIURI* aSource,
                     nsIURI* aTarget,
                     const PRUnichar* aDisplayName,
                     nsIMIMEInfo *aMIMEInfo,
                     PRInt64 aStartTime,
                     nsIWebBrowserPersist* aPersist) {
    nsresult rv;
    nsCOMPtr<nsIDownloadManager> dm = do_GetService("@mozilla.org/download-manager;1", &rv);
    if (NS_FAILED(rv))
      return rv;
    
    rv = dm->AddDownload(aSource, aTarget, aDisplayName, aMIMEInfo, aStartTime, aPersist, getter_AddRefs(mInner));
    if (NS_FAILED(rv))
      return rv;

    PRInt32 behavior;
    nsCOMPtr<nsIPrefBranch> branch = do_GetService("@mozilla.org/preferences-service;1", &rv);
    if (NS_SUCCEEDED(rv))
      rv = branch->GetIntPref(DOWNLOAD_MANAGER_BEHAVIOR_PREF, &behavior);
    if (NS_FAILED(rv))
      behavior = 0;

    if (behavior == 0)
      rv = dm->Open(nsnull, this);
    else if (behavior == 1)
      rv = dm->OpenProgressDialogFor(mInner, nsnull, PR_TRUE);
    return rv;
  }

 
  NS_IMETHODIMP GetDisplayName(PRUnichar** aDisplayName)
  {
    if (!mInner)
      return NS_ERROR_NOT_INITIALIZED;
    return mInner->GetDisplayName(aDisplayName);
  }
  
  NS_IMETHODIMP SetDisplayName(const PRUnichar* aDisplayName)
  {
    if (!mInner)
      return NS_ERROR_NOT_INITIALIZED;
    return mInner->SetDisplayName(aDisplayName);
  }
  
  NS_IMETHODIMP GetMIMEInfo(nsIMIMEInfo** aMIMEInfo)
  {
    if (!mInner)
      return NS_ERROR_NOT_INITIALIZED;
    return mInner->GetMIMEInfo(aMIMEInfo);
  }
  
  NS_IMETHODIMP GetSource(nsIURI** aSource)
  {
    if (!mInner)
      return NS_ERROR_NOT_INITIALIZED;
    return mInner->GetSource(aSource);
  }
  
  NS_IMETHODIMP GetTarget(nsIURI** aTarget)
  {
    if (!mInner)
      return NS_ERROR_NOT_INITIALIZED;
    return mInner->GetTarget(aTarget);
  }
  
  NS_IMETHODIMP GetStartTime(PRInt64* aStartTime)
  {
    if (!mInner)
      return NS_ERROR_NOT_INITIALIZED;
    return mInner->GetStartTime(aStartTime);
  }

  NS_IMETHODIMP GetPercentComplete(PRInt32* aPercentComplete)
  {
    if (!mInner)
      return NS_ERROR_NOT_INITIALIZED;
    return mInner->GetPercentComplete(aPercentComplete);
  }
   
  NS_IMETHODIMP GetListener(nsIWebProgressListener** aListener)
  {
    if (!mInner)
      return NS_ERROR_NOT_INITIALIZED;
    return mInner->GetListener(aListener);
  }

  NS_IMETHODIMP SetListener(nsIWebProgressListener* aListener)
  {
    if (!mInner)
      return NS_ERROR_NOT_INITIALIZED;
    return mInner->SetListener(aListener);
  }
  
  NS_IMETHODIMP GetObserver(nsIObserver** aObserver)
  {
    if (!mInner)
      return NS_ERROR_NOT_INITIALIZED;
    return mInner->GetObserver(aObserver);
  }

  NS_IMETHODIMP SetObserver(nsIObserver* aObserver)
  {
    if (!mInner)
      return NS_ERROR_NOT_INITIALIZED;
    return mInner->SetObserver(aObserver);
  }
   
  NS_IMETHODIMP GetPersist(nsIWebBrowserPersist** aPersist)
  {
    if (!mInner)
      return NS_ERROR_NOT_INITIALIZED;
    return mInner->GetPersist(aPersist);
  }

  NS_IMETHODIMP GetTargetFile(nsILocalFile** aTargetFile)
  {
    if (!mInner)
      return NS_ERROR_NOT_INITIALIZED;
    return mInner->GetTargetFile(aTargetFile);
  }

  NS_IMETHODIMP OnStateChange(nsIWebProgress* aWebProgress,
                              nsIRequest* aRequest, PRUint32 aStateFlags,
                              PRUint32 aStatus)
  {
    nsCOMPtr<nsIWebProgressListener> listener = do_QueryInterface(mInner);
    if (listener)
      return listener->OnStateChange(aWebProgress, aRequest, aStateFlags, aStatus);
    return NS_OK;
  }
  
  NS_IMETHODIMP OnStatusChange(nsIWebProgress *aWebProgress,
                               nsIRequest *aRequest, nsresult aStatus,
                               const PRUnichar *aMessage)
  {
    nsCOMPtr<nsIWebProgressListener> listener = do_QueryInterface(mInner);
    if (listener)
      return listener->OnStatusChange(aWebProgress, aRequest, aStatus, aMessage);
    return NS_OK;
  }

  NS_IMETHODIMP OnLocationChange(nsIWebProgress *aWebProgress,
                                 nsIRequest *aRequest, nsIURI *aLocation)
  {
    nsCOMPtr<nsIWebProgressListener> listener = do_QueryInterface(mInner);
    if (listener)
      return listener->OnLocationChange(aWebProgress, aRequest, aLocation);
    return NS_OK;
  }
  
  NS_IMETHODIMP OnProgressChange(nsIWebProgress *aWebProgress,
                                 nsIRequest *aRequest,
                                 PRInt32 aCurSelfProgress,
                                 PRInt32 aMaxSelfProgress,
                                 PRInt32 aCurTotalProgress,
                                 PRInt32 aMaxTotalProgress)
  {
    nsCOMPtr<nsIWebProgressListener> listener = do_QueryInterface(mInner);
    if (listener)
      return listener->OnProgressChange(aWebProgress, aRequest, aCurSelfProgress,
                                        aMaxSelfProgress, aCurTotalProgress, aMaxTotalProgress);
    return NS_OK;
  }

  NS_IMETHODIMP OnSecurityChange(nsIWebProgress *aWebProgress,
                                 nsIRequest *aRequest, PRUint32 aState)
  {
    nsCOMPtr<nsIWebProgressListener> listener = do_QueryInterface(mInner);
    if (listener)
      return listener->OnSecurityChange(aWebProgress, aRequest, aState);
    return NS_OK;
  }

private:
  nsCOMPtr<nsIDownload> mInner;
};

NS_IMPL_ISUPPORTS3(nsDownloadProxy, nsIDownload, nsITransfer, nsIWebProgressListener)  

#endif
