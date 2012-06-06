/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
 
#ifndef downloadproxy___h___
#define downloadproxy___h___

#include "nsIDownloadManager.h"
#include "nsIPrefBranch.h"
#include "nsIPrefService.h"
#include "nsIMIMEInfo.h"
#include "nsIFileURL.h"
#include "nsIDownloadManagerUI.h"

#define PREF_BDM_SHOWWHENSTARTING "browser.download.manager.showWhenStarting"
#define PREF_BDM_FOCUSWHENSTARTING "browser.download.manager.focusWhenStarting"

class nsDownloadProxy : public nsITransfer
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
                     nsIFile* aTempFile,
                     nsICancelable* aCancelable) {
    nsresult rv;
    nsCOMPtr<nsIDownloadManager> dm = do_GetService("@mozilla.org/download-manager;1", &rv);
    NS_ENSURE_SUCCESS(rv, rv);
    
    rv = dm->AddDownload(nsIDownloadManager::DOWNLOAD_TYPE_DOWNLOAD, aSource,
                         aTarget, aDisplayName, aMIMEInfo, aStartTime,
                         aTempFile, aCancelable, getter_AddRefs(mInner));
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIPrefService> prefs = do_GetService("@mozilla.org/preferences-service;1", &rv);
    NS_ENSURE_SUCCESS(rv, rv);
    nsCOMPtr<nsIPrefBranch> branch = do_QueryInterface(prefs);

    bool showDM = true;
    if (branch)
      branch->GetBoolPref(PREF_BDM_SHOWWHENSTARTING, &showDM);

    if (showDM) {
      PRUint32 id;
      mInner->GetId(&id);

      nsCOMPtr<nsIDownloadManagerUI> dmui =
        do_GetService("@mozilla.org/download-manager-ui;1", &rv);
      NS_ENSURE_SUCCESS(rv, rv);

      bool visible;
      rv = dmui->GetVisible(&visible);
      NS_ENSURE_SUCCESS(rv, rv);

      bool focusWhenStarting = true;
      if (branch)
        (void)branch->GetBoolPref(PREF_BDM_FOCUSWHENSTARTING, &focusWhenStarting);

      if (visible && !focusWhenStarting)
        return NS_OK;

      return dmui->Show(nsnull, id, nsIDownloadManagerUI::REASON_NEW_DOWNLOAD);
    }
    return rv;
  }

  NS_IMETHODIMP OnStateChange(nsIWebProgress* aWebProgress,
                              nsIRequest* aRequest, PRUint32 aStateFlags,
                              PRUint32 aStatus)
  {
    NS_ENSURE_TRUE(mInner, NS_ERROR_NOT_INITIALIZED);
    return mInner->OnStateChange(aWebProgress, aRequest, aStateFlags, aStatus);
  }
  
  NS_IMETHODIMP OnStatusChange(nsIWebProgress *aWebProgress,
                               nsIRequest *aRequest, nsresult aStatus,
                               const PRUnichar *aMessage)
  {
    NS_ENSURE_TRUE(mInner, NS_ERROR_NOT_INITIALIZED);
    return mInner->OnStatusChange(aWebProgress, aRequest, aStatus, aMessage);
  }

  NS_IMETHODIMP OnLocationChange(nsIWebProgress *aWebProgress,
                                 nsIRequest *aRequest, nsIURI *aLocation,
                                 PRUint32 aFlags)
  {
    NS_ENSURE_TRUE(mInner, NS_ERROR_NOT_INITIALIZED);
    return mInner->OnLocationChange(aWebProgress, aRequest, aLocation, aFlags);
  }
  
  NS_IMETHODIMP OnProgressChange(nsIWebProgress *aWebProgress,
                                 nsIRequest *aRequest,
                                 PRInt32 aCurSelfProgress,
                                 PRInt32 aMaxSelfProgress,
                                 PRInt32 aCurTotalProgress,
                                 PRInt32 aMaxTotalProgress)
  {
    NS_ENSURE_TRUE(mInner, NS_ERROR_NOT_INITIALIZED);
    return mInner->OnProgressChange(aWebProgress, aRequest,
                                    aCurSelfProgress,
                                    aMaxSelfProgress,
                                    aCurTotalProgress,
                                    aMaxTotalProgress);
  }

  NS_IMETHODIMP OnProgressChange64(nsIWebProgress *aWebProgress,
                                   nsIRequest *aRequest,
                                   PRInt64 aCurSelfProgress,
                                   PRInt64 aMaxSelfProgress,
                                   PRInt64 aCurTotalProgress,
                                   PRInt64 aMaxTotalProgress)
  {
    NS_ENSURE_TRUE(mInner, NS_ERROR_NOT_INITIALIZED);
    return mInner->OnProgressChange64(aWebProgress, aRequest,
                                      aCurSelfProgress,
                                      aMaxSelfProgress,
                                      aCurTotalProgress,
                                      aMaxTotalProgress);
  }

  NS_IMETHODIMP OnRefreshAttempted(nsIWebProgress *aWebProgress,
                                   nsIURI *aUri,
                                   PRInt32 aDelay,
                                   bool aSameUri,
                                   bool *allowRefresh)
  {
    *allowRefresh = true;
    return NS_OK;
  }

  NS_IMETHODIMP OnSecurityChange(nsIWebProgress *aWebProgress,
                                 nsIRequest *aRequest, PRUint32 aState)
  {
    NS_ENSURE_TRUE(mInner, NS_ERROR_NOT_INITIALIZED);
    return mInner->OnSecurityChange(aWebProgress, aRequest, aState);
  }

private:
  nsCOMPtr<nsIDownload> mInner;
};

NS_IMPL_ISUPPORTS3(nsDownloadProxy, nsITransfer,
                   nsIWebProgressListener, nsIWebProgressListener2)

#endif
