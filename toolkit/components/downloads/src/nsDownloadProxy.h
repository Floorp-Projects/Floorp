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
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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
#include "nsIFileURL.h"

#define PREF_BDM_SHOWWHENSTARTING "browser.download.manager.showWhenStarting"
#define PREF_BDM_USEWINDOW "browser.download.manager.useWindow"

class nsDownloadProxy : public nsIDownload
{
public:

  nsDownloadProxy() { }
  virtual ~nsDownloadProxy() { }

  NS_DECL_ISUPPORTS

  NS_IMETHODIMP Init(nsIURI* aSource,
                     nsIURI* aTarget,
                     const nsAString& aDisplayName,
                     nsIMIMEInfo *aMIMEInfo,
                     PRTime aStartTime,
                     nsILocalFile* aTempFile,
                     nsICancelable* aCancelable) {
    nsresult rv;
    nsCOMPtr<nsIDownloadManager> dm = do_GetService("@mozilla.org/download-manager;1", &rv);
    if (NS_FAILED(rv))
      return rv;
    
    rv = dm->AddDownload(nsIDownloadManager::DOWNLOAD_TYPE_DOWNLOAD, aSource,
                         aTarget, aDisplayName, EmptyString(), aMIMEInfo,
                         aStartTime, aTempFile, aCancelable,
                         getter_AddRefs(mInner));
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIPrefService> prefs = do_GetService("@mozilla.org/preferences-service;1", &rv);
    if (NS_FAILED(rv)) return rv;
    nsCOMPtr<nsIPrefBranch> branch = do_QueryInterface(prefs);

    PRBool showDM = PR_TRUE;
    branch->GetBoolPref(PREF_BDM_SHOWWHENSTARTING , &showDM);

    PRBool useWindow = PR_TRUE;
    branch->GetBoolPref(PREF_BDM_USEWINDOW, &useWindow);
    if (showDM && useWindow) {
      nsAutoString path;

      nsCOMPtr<nsIFileURL> fileURL = do_QueryInterface(aTarget, &rv);
      if (NS_FAILED(rv)) return rv;

      nsCOMPtr<nsIFile> file;
      rv = fileURL->GetFile(getter_AddRefs(file));
      if (NS_FAILED(rv)) return rv;

      rv = file->GetPath(path);
      if (NS_FAILED(rv)) return rv;

      return dm->Open(nsnull, path.get());
    }
    return rv;
  }

 
  NS_IMETHODIMP GetDisplayName(PRUnichar** aDisplayName)
  {
    return mInner->GetDisplayName(aDisplayName);
  }
  
  NS_IMETHODIMP GetMIMEInfo(nsIMIMEInfo** aMIMEInfo)
  {
    return mInner->GetMIMEInfo(aMIMEInfo);
  }
  
  NS_IMETHODIMP GetSource(nsIURI** aSource)
  {
    return mInner->GetSource(aSource);
  }
  
  NS_IMETHODIMP GetTarget(nsIURI** aTarget)
  {
    return mInner->GetTarget(aTarget);
  }
  
  NS_IMETHODIMP GetStartTime(PRInt64* aStartTime)
  {
    return mInner->GetStartTime(aStartTime);
  }

  NS_IMETHODIMP GetPercentComplete(PRInt32* aPercentComplete)
  {
    return mInner->GetPercentComplete(aPercentComplete);
  }

  NS_IMETHODIMP GetAmountTransferred(PRUint64* aAmountTransferred)
  {
    return mInner->GetAmountTransferred(aAmountTransferred);
  }
  
  NS_IMETHODIMP GetSize(PRUint64* aSize)
  {
    return mInner->GetSize(aSize);
  }

  NS_IMETHODIMP GetCancelable(nsICancelable** aCancelable)
  {
    return mInner->GetCancelable(aCancelable);
  }

  NS_IMETHODIMP GetTargetFile(nsILocalFile** aTargetFile)
  {
    return mInner->GetTargetFile(aTargetFile);
  }

  NS_IMETHODIMP GetSpeed(double* aSpeed)
  {
    return mInner->GetSpeed(aSpeed);
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
      return listener->OnProgressChange(aWebProgress, aRequest,
                                        aCurSelfProgress,
                                        aMaxSelfProgress,
                                        aCurTotalProgress,
                                        aMaxTotalProgress);
    return NS_OK;
  }

  NS_IMETHODIMP OnProgressChange64(nsIWebProgress *aWebProgress,
                                   nsIRequest *aRequest,
                                   PRInt64 aCurSelfProgress,
                                   PRInt64 aMaxSelfProgress,
                                   PRInt64 aCurTotalProgress,
                                   PRInt64 aMaxTotalProgress)
  {
    nsCOMPtr<nsIWebProgressListener2> listener = do_QueryInterface(mInner);
    if (listener)
      return listener->OnProgressChange64(aWebProgress, aRequest,
                                          aCurSelfProgress,
                                          aMaxSelfProgress,
                                          aCurTotalProgress,
                                          aMaxTotalProgress);
    return NS_OK;
  }

  NS_IMETHODIMP OnRefreshAttempted(nsIWebProgress *aWebProgress,
                                   nsIURI *aUri,
                                   PRInt32 aDelay,
                                   PRBool aSameUri,
                                   PRBool *allowRefresh)
  {
    *allowRefresh = PR_TRUE;
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

NS_IMPL_ISUPPORTS4(nsDownloadProxy, nsIDownload, nsITransfer,
                   nsIWebProgressListener, nsIWebProgressListener2)

#endif
