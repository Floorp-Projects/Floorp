/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
 
#ifndef downloadproxy___h___
#define downloadproxy___h___

#include "nsIDownload.h"
#include "nsIDownloadManager.h"

class nsDownloadProxy : nsIDownload
{
public:

  nsDownloadProxy() { NS_INIT_ISUPPORTS(); }
  virtual ~nsDownloadProxy() { };

  NS_DECL_ISUPPORTS

  NS_IMETHODIMP Init(nsIURI* aSource,
                     nsILocalFile* aTarget,
                     const PRUnichar* aDisplayName,
                     nsIWebBrowserPersist* aPersist) {
    nsresult rv;
    nsCOMPtr<nsIDownloadManager> dm = do_GetService("@mozilla.org/download-manager;1", &rv);
    if (NS_FAILED(rv)) return rv;
    
    rv = dm->AddDownload(aSource, aTarget, aDisplayName, aPersist, getter_AddRefs(mInner));
    if (NS_FAILED(rv)) return rv;
    
    char* persistentDescriptor;
    aTarget->GetPersistentDescriptor(&persistentDescriptor);
    return dm->OpenProgressDialogFor(persistentDescriptor, nsnull);
  }
 
  NS_IMETHODIMP GetDisplayName(PRUnichar** aDisplayName)
  {
    return mInner->GetDisplayName(aDisplayName);
  }
  
  NS_IMETHODIMP SetDisplayName(const PRUnichar* aDisplayName)
  {
    return mInner->SetDisplayName(aDisplayName);
  }
  
  NS_IMETHODIMP GetOpeningWith(PRUnichar** aOpeningWith)
  {
    return mInner->GetOpeningWith(aOpeningWith);
  }
  
  NS_IMETHODIMP SetOpeningWith(const PRUnichar* aOpeningWith)
  {
    return mInner->SetOpeningWith(aOpeningWith);
  }
  
  NS_IMETHODIMP GetSource(nsIURI** aSource)
  {
    return mInner->GetSource(aSource);
  }
  
  NS_IMETHODIMP GetTarget(nsILocalFile** aTarget)
  {
    return mInner->GetTarget(aTarget);
  }
  
  NS_IMETHODIMP GetStartTime(PRInt64* aStartTime)
  {
    return mInner->GetStartTime(aStartTime);
  }

  NS_IMETHODIMP SetStartTime(PRInt64 aStartTime)
  {
    return mInner->SetStartTime(aStartTime);
  }

  NS_IMETHODIMP GetPercentComplete(PRInt32* aPercentComplete)
  {
    return mInner->GetPercentComplete(aPercentComplete);
  }
   
  NS_IMETHODIMP GetListener(nsIWebProgressListener** aListener)
  {
    return mInner->GetListener(aListener);
  }

  NS_IMETHODIMP SetListener(nsIWebProgressListener* aListener)
  {
    return mInner->SetListener(aListener);
  }
  
  NS_IMETHODIMP GetObserver(nsIObserver** aObserver)
  {
    return mInner->GetObserver(aObserver);
  }

  NS_IMETHODIMP SetObserver(nsIObserver* aObserver)
  {
    return mInner->SetObserver(aObserver);
  }
   
  NS_IMETHODIMP GetPersist(nsIWebBrowserPersist** aPersist)
  {
    return mInner->GetPersist(aPersist);
  }
private:
  nsCOMPtr<nsIDownload> mInner;
};

NS_IMPL_ISUPPORTS1(nsDownloadProxy, nsIDownload)  

#endif
  