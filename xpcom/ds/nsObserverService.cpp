/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 *   Pierre Phaneuf <pp@ludusdesign.com>
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

#define NS_IMPL_IDS
#include "prlock.h"
#include "nsIFactory.h"
#include "nsIServiceManager.h"
#include "nsIComponentManager.h"
#include "nsIObserverService.h"
#include "nsObserverService.h"
#include "nsObserverList.h"
#include "nsHashtable.h"
#include "nsIWeakReference.h"

#define NS_WEAK_OBSERVERS

static NS_DEFINE_CID(kObserverServiceCID, NS_OBSERVERSERVICE_CID);

////////////////////////////////////////////////////////////////////////////////

static nsObserverService* gObserverService = nsnull; // The one-and-only ObserverService

////////////////////////////////////////////////////////////////////////////////
// nsObserverService Implementation


NS_IMPL_THREADSAFE_ISUPPORTS1(nsObserverService, nsIObserverService)

NS_COM nsresult NS_NewObserverService(nsIObserverService** anObserverService)
{
    return nsObserverService::GetObserverService(anObserverService);
}

nsObserverService::nsObserverService()
    : mObserverTopicTable(nsnull)
{
    NS_INIT_REFCNT();
    mObserverTopicTable = nsnull;
}

nsObserverService::~nsObserverService(void)
{
    if(mObserverTopicTable)
        delete mObserverTopicTable;
    gObserverService = nsnull;
}

NS_METHOD
nsObserverService::Create(nsISupports* outer, const nsIID& aIID, void* *aInstancePtr)
{
    nsresult rv;
    nsObserverService* os = new nsObserverService();
    if (os == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(os);
    rv = os->QueryInterface(aIID, aInstancePtr);
    NS_RELEASE(os);
    return rv;
}

nsresult nsObserverService::GetObserverService(nsIObserverService** anObserverService)
{
    if (! gObserverService) {
        nsObserverService* it = new nsObserverService();
        if (! it)
            return NS_ERROR_OUT_OF_MEMORY;
        gObserverService = it;
    }

    NS_ADDREF(gObserverService);
    *anObserverService = gObserverService;
    return NS_OK;
}

static PRBool PR_CALLBACK 
ReleaseObserverList(nsHashKey *aKey, void *aData, void* closure)
{
    nsObserverList* observerList = NS_STATIC_CAST(nsObserverList*, aData);
    delete(observerList);
    return PR_TRUE;
}

nsresult nsObserverService::GetObserverList(const char* aTopic, nsObserverList** anObserverList)
{
    if (anObserverList == nsnull)
        return NS_ERROR_NULL_POINTER;
	
	if(mObserverTopicTable == nsnull) 
    {
        mObserverTopicTable = new nsObjectHashtable(nsnull, 
                                                    nsnull,   // should never be cloned
                                                    ReleaseObserverList, 
                                                    nsnull,
                                                    256, 
                                                    PR_TRUE);
        if (mObserverTopicTable == nsnull)
            return NS_ERROR_OUT_OF_MEMORY;
    }
	

	nsCStringKey key(aTopic);

    nsObserverList *topicObservers;
    topicObservers = (nsObserverList *) mObserverTopicTable->Get(&key);

    if (topicObservers) 
    {
        *anObserverList = topicObservers;	
        return NS_OK;
    }

    topicObservers = new nsObserverList();
    if (!topicObservers)
        return NS_ERROR_OUT_OF_MEMORY;
    
    *anObserverList = topicObservers;
    mObserverTopicTable->Put(&key, topicObservers);
    
	return NS_OK;
}

NS_IMETHODIMP nsObserverService::AddObserver(nsIObserver* anObserver, const char* aTopic, PRBool ownsWeak)
{
	nsObserverList* anObserverList;
	nsresult rv;

    if (anObserver == nsnull || aTopic == nsnull)
        return NS_ERROR_NULL_POINTER;

	rv = GetObserverList(aTopic, &anObserverList);
	if (NS_FAILED(rv)) return rv;

    return anObserverList->AddObserver(anObserver, ownsWeak);
}

NS_IMETHODIMP nsObserverService::RemoveObserver(nsIObserver* anObserver, const char* aTopic)
{
	nsObserverList* anObserverList;
	nsresult rv;

    if (anObserver == nsnull || aTopic == nsnull)
        return NS_ERROR_NULL_POINTER;

	rv = GetObserverList(aTopic, &anObserverList);
	if (NS_FAILED(rv)) return rv;

    return anObserverList->RemoveObserver(anObserver);
}

NS_IMETHODIMP nsObserverService::EnumerateObservers(const char* aTopic, nsISimpleEnumerator** anEnumerator)
{
	nsObserverList* anObserverList;
	nsresult rv;

    if (anEnumerator == nsnull || aTopic == nsnull)
        return NS_ERROR_NULL_POINTER;

	rv = GetObserverList(aTopic, &anObserverList);
	if (NS_FAILED(rv)) return rv;

    return anObserverList->GetObserverList(anEnumerator);
}

// Enumerate observers of aTopic and call Observe on each.
NS_IMETHODIMP nsObserverService::NotifyObservers( nsISupports *aSubject,
                                                  const char *aTopic,
                                                  const PRUnichar *someData ) {
    nsresult rv = NS_OK;
    nsCOMPtr<nsISimpleEnumerator> observers;
    nsCOMPtr<nsISupports> observerRef;

    rv = EnumerateObservers( aTopic, getter_AddRefs(observers) );
    if ( NS_FAILED( rv ) )
        return rv;
    PRBool loop = PR_TRUE;
    while( NS_SUCCEEDED(observers->HasMoreElements(&loop)) && loop) 
    {
        observers->GetNext(getter_AddRefs(observerRef));
        nsCOMPtr<nsIObserver> observer = do_QueryInterface(observerRef);
        if ( observer ) 
            observer->Observe( aSubject, aTopic, someData );
#ifdef NS_WEAK_OBSERVERS
        else
        {  // check for weak reference.
             nsCOMPtr<nsIWeakReference> weakRef = do_QueryInterface(observerRef);     
             if ( weakRef )                                                              
                weakRef->QueryReferent(NS_GET_IID(nsIObserver), getter_AddRefs(observer));

             if ( observer ) 
                observer->Observe( aSubject, aTopic, someData );
        }
#endif
    }
    return NS_OK;
}
////////////////////////////////////////////////////////////////////////////////

