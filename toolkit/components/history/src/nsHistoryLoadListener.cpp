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


#include "nsIDOMWindow.h"
#include "nsIDOMHTMLDocument.h"
#include "nsIDocument.h"
#include "nsIURI.h"

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
                                                      this),
                                       nsIWebProgress::NOTIFY_STATE_DOCUMENT);
    printf("\tSuccess: %8.8X\n", rv);
    
    return NS_OK;
}

NS_IMPL_ISUPPORTS2(nsHistoryLoadListener,
                   nsIWebProgressListener,
                   nsISupportsWeakReference)

/* void onStateChange (in nsIWebProgress aWebProgress, in nsIRequest aRequest, in long aStateFlags, in nsresult aStatus); */
NS_IMETHODIMP
nsHistoryLoadListener::OnStateChange(nsIWebProgress *aWebProgress,
                                     nsIRequest *aRequest,
                                     PRUint32 aStateFlags, nsresult aStatus)
{
    // we only care about finishing up documents
    if (! (aStateFlags & nsIWebProgressListener::STATE_STOP))
        return NS_OK;

    nsresult rv;
    
    nsCOMPtr<nsIDOMWindow> window;
    rv = aWebProgress->GetDOMWindow(getter_AddRefs(window));
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIDOMDocument> domDoc;

    rv = window->GetDocument(getter_AddRefs(domDoc));
    nsCOMPtr<nsIDocument> doc = do_QueryInterface(domDoc);
    if (!doc) return NS_OK;

    nsCOMPtr<nsIURI> uri;
    rv = doc->GetDocumentURL(getter_AddRefs(uri));
    if (NS_FAILED(rv)) return rv;
        
    nsXPIDLCString urlString;
    uri->GetSpec(getter_Copies(urlString));
    
    if (aStatus == NS_BINDING_REDIRECTED) {
        
        mHistory->HidePage(urlString);
        
    } else {
        nsCOMPtr<nsIDOMHTMLDocument> htmlDoc = do_QueryInterface(doc);

        // there should be a better way to handle non-html docs
        if (!htmlDoc) return NS_OK;
        // somehow get the title, and store it in history
        nsAutoString title;
        htmlDoc->GetTitle(title);

        mHistory->SetPageTitle(urlString, title.get());

#if 0
        nsAutoString referrer;
        htmlDoc->GetReferrer(referrer);
        mHistory->SetReferrer(urlString, referrer.get());
#endif
    }
    printf("nsHistoryLoadListener::OnStateChange(w,r, %8.8X, %d)\n", aStateFlags, aStatus);
    return NS_OK;
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
    NS_NOTREACHED("notification excluded in AddProgressListener(...)");
    return NS_OK;
}

/* void onLocationChange (in nsIWebProgress aWebProgress, in nsIRequest aRequest, in nsIURI location); */
NS_IMETHODIMP nsHistoryLoadListener::OnLocationChange(nsIWebProgress *aWebProgress, nsIRequest *aRequest, nsIURI *location)
{
    NS_NOTREACHED("notification excluded in AddProgressListener(...)");
    return NS_OK;
}

/* void onStatusChange (in nsIWebProgress aWebProgress, in nsIRequest aRequest, in nsresult aStatus, in wstring aMessage); */
NS_IMETHODIMP nsHistoryLoadListener::OnStatusChange(nsIWebProgress *aWebProgress, nsIRequest *aRequest, nsresult aStatus, const PRUnichar *aMessage)
{
    NS_NOTREACHED("notification excluded in AddProgressListener(...)");
    return NS_OK;
}

/* void onSecurityChange (in nsIWebProgress aWebProgress, in nsIRequest aRequest, in unsigned long state); */
NS_IMETHODIMP nsHistoryLoadListener::OnSecurityChange(nsIWebProgress *aWebProgress, nsIRequest *aRequest, PRUint32 state)
{
    NS_NOTREACHED("notification excluded in AddProgressListener(...)");
    return NS_OK;
}

