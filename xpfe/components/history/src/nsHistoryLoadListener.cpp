/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Original Author(s):
 *   Alec Flett <alecf@netscape.com>
 *
 * Contributor(s): 
 */

#include "nsHistoryLoadListener.h"
#include "nsIGlobalHistory.h"

#include "nsIServiceManager.h"
#include "nsICategoryManager.h"
#include "nsString.h"
#include "nsXPIDLString.h"

#include "nsIDocumentLoader.h"
#include "nsIWebProgress.h"
#include "nsIRequestObserver.h"
#include "nsCURILoader.h"


static NS_DEFINE_IID(kDocLoaderServiceCID, NS_DOCUMENTLOADER_SERVICE_CID);

nsHistoryLoadListener::nsHistoryLoadListener(nsIBrowserHistory *aHistory)
{
    NS_INIT_ISUPPORTS();
    mHistory = aHistory;
    printf("Creating nsHistoryLoadListener\n");
}

nsHistoryLoadListener::~nsHistoryLoadListener()
{

}

nsresult
nsHistoryLoadListener::Init()
{
    nsresult rv;

    printf("Creating history load listener..\n");
    mHistory = do_GetService(NS_GLOBALHISTORY_CONTRACTID, &rv);
    if (NS_FAILED(rv)) return rv;

    printf("Have global history\n");
    
    return NS_OK;
}

NS_IMPL_ISUPPORTS3(nsHistoryLoadListener,
                   nsIWebProgressListener,
                   nsIObserver,
                   nsISupportsWeakReference)

/* void onStateChange (in nsIWebProgress aWebProgress, in nsIRequest aRequest, in long aStateFlags, in unsigned long aStatus); */
NS_IMETHODIMP
nsHistoryLoadListener::OnStateChange(nsIWebProgress *aWebProgress,
                                     nsIRequest *aRequest,
                                     PRInt32 aStateFlags, PRUint32 aStatus)
{
    // we only care about finishing up documents
    if (! (aStateFlags & nsIWebProgressListener::STATE_STOP))
        return NS_OK;

    {
        if (aStatus == NS_BINDING_REDIRECTED) {
            // now the question is, is this already in history, or do
            // we have to prevent it from existing there in the first place?
        }

        else {
            // somehow get the title, and store it in history

        }
    }
    printf("nsHistoryLoadListener::OnStateChange(w,r, %8.8X, %d)\n", aStateFlags, aStatus);
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void onProgressChange (in nsIWebProgress aWebProgress, in nsIRequest aRequest, in long aCurSelfProgress, in long aMaxSelfProgress, in long aCurTotalProgress, in long aMaxTotalProgress); */
NS_IMETHODIMP
nsHistoryLoadListener::OnProgressChange(nsIWebProgress *aWebProgress,
                                        nsIRequest *aRequest,
                                        PRInt32 aCurSelfProgress,
                                        PRInt32 aMaxSelfProgress,
                                        PRInt32 aCurTotalProgress,
                                        PRInt32 aMaxTotalProgress)
{
    printf("nsHistoryLoadListener::OnProgressChange(w,r, %d, %d, %d, %d)\n",
           aCurSelfProgress, aMaxSelfProgress,
           aCurTotalProgress, aMaxTotalProgress );
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void onLocationChange (in nsIWebProgress aWebProgress, in nsIRequest aRequest, in nsIURI location); */
NS_IMETHODIMP nsHistoryLoadListener::OnLocationChange(nsIWebProgress *aWebProgress, nsIRequest *aRequest, nsIURI *location)
{
    printf("nsHistoryLoadListener::OnLocationChange(w,r,l)\n");
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void onStatusChange (in nsIWebProgress aWebProgress, in nsIRequest aRequest, in nsresult aStatus, in wstring aMessage); */
NS_IMETHODIMP nsHistoryLoadListener::OnStatusChange(nsIWebProgress *aWebProgress, nsIRequest *aRequest, nsresult aStatus, const PRUnichar *aMessage)
{
    printf("nsHistoryLoadListener::OnStatusChange(w,r, %8.8X, %s)\n",
           aStatus, NS_ConvertUCS2toUTF8(aMessage).get());

    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void onSecurityChange (in nsIWebProgress aWebProgress, in nsIRequest aRequest, in long state); */
NS_IMETHODIMP nsHistoryLoadListener::OnSecurityChange(nsIWebProgress *aWebProgress, nsIRequest *aRequest, PRInt32 state)
{
    printf("nsHistoryLoadListener::OnSecurityChange(w,r, %d)\n", state);
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsHistoryLoadListener::Observe(nsISupports *,
                               const char *aTopic,
                               const PRUnichar *)
{
    if (nsCRT::strcmp("app-startup",aTopic) != 0)
        return NS_OK;

    nsresult rv;
    
    // the global docloader
    nsCOMPtr<nsIDocumentLoader> docLoaderService =
        do_GetService(kDocLoaderServiceCID, &rv);
    if (NS_FAILED(rv)) return rv;

    printf("Have docloader\n");
    
    nsCOMPtr<nsIWebProgress> progress =
        do_QueryInterface(docLoaderService, &rv);
    if (NS_FAILED(rv)) return rv;

    printf("have web progress\n");

    NS_ADDREF_THIS();
    rv = progress->AddProgressListener(NS_STATIC_CAST(nsIWebProgressListener*,
                                                      this));
    printf("\tSuccess: %8.8X\n", rv);
    return NS_OK;
}

nsresult
nsHistoryLoadListener::registerSelf(nsIComponentManager*, nsIFile*,
                                    const char *, const char *,
                                    const nsModuleComponentInfo* info)
{
    nsresult rv;

    printf("registering nsHistoryLoadListener\n");
    nsCOMPtr<nsICategoryManager> catman =
        do_GetService("@mozilla.org/categorymanager;1", &rv);
    if (NS_FAILED(rv)) return rv;

    nsCAutoString contractId("service,");
    contractId.Append(info->mContractID);
    
    nsXPIDLCString prevEntry;
    rv = catman->AddCategoryEntry("app-startup",
                                  info->mDescription,
                                  contractId.get(),
                                  PR_TRUE, PR_TRUE, getter_Copies(prevEntry));
    
    return rv;
}

nsresult
nsHistoryLoadListener::unregisterSelf(nsIComponentManager*, nsIFile*,
                                      const char*,
                                      const nsModuleComponentInfo* info)
{
    nsresult rv;
    
    nsCOMPtr<nsICategoryManager> catman =
        do_GetService("@mozilla.org/categorymanager;1", &rv);
    if (NS_FAILED(rv)) return rv;

    rv = catman->DeleteCategoryEntry("app-startup",
                                     info->mDescription, PR_TRUE);
    
    return rv;
}

