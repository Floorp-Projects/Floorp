/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL. You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation. Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All Rights
 * Reserved.
 */

#define NS_IMPL_IDS
#include "nsISupports.h"
#include "nsRepository.h"
#include "nsIObserverService.h"
#include "nsIObserver.h"
#include "nsString.h"

static NS_DEFINE_IID(kIObserverServiceIID, NS_IOBSERVERSERVICE_IID);
static NS_DEFINE_IID(kObserverServiceCID, NS_OBSERVERSERVICE_CID);
static NS_DEFINE_IID(kIObserverIID, NS_IOBSERVER_IID);
static NS_DEFINE_IID(kObserverCID, NS_OBSERVER_CID);

int main(int argc, char *argv[])
{

    nsIObserverService *anObserverService = NULL;
    nsresult rv;

    rv = nsComponentManager::AutoRegister(nsIComponentManager::NS_Startup,
                                         "components");
    if (NS_FAILED(rv)) return rv;

    nsresult res = nsComponentManager::CreateInstance(kObserverServiceCID,
                                                NULL,
                                                 kIObserverServiceIID,
                                                (void **) &anObserverService);
	
    if (res == NS_OK) {

        nsString  aTopic("htmlparser");

        nsIObserver *anObserver;
        nsIObserver *aObserver = nsnull;
        nsIObserver *bObserver = nsnull;
            
        nsresult res = nsRepository::CreateInstance(kObserverCID,
                                                    NULL,
                                                    kIObserverIID,
                                                    (void **) &anObserver);

        rv = NS_NewObserver(&aObserver);

        if (NS_FAILED(rv)) return rv;

        rv = anObserverService->AddObserver(&aObserver, &aTopic);
        if (NS_FAILED(rv)) return rv;
 
        rv = NS_NewObserver(&bObserver);
        if (NS_FAILED(rv)) return rv;

        rv = anObserverService->AddObserver(&bObserver, &aTopic);
        if (NS_FAILED(rv)) return rv;

        nsIEnumerator* e;
        rv = anObserverService->EnumerateObserverList(&e, &aTopic);
        if (NS_FAILED(rv)) return rv;
        nsISupports *inst;

        for (e->First(); e->IsDone() != NS_OK; e->Next()) {
            rv = e->CurrentItem(&inst);
            if (NS_SUCCEEDED(rv)) {
              rv = inst->QueryInterface(nsIObserver::GetIID(),(void**)&anObserver);
            }
            rv = anObserver->Notify(nsnull);
        }

        rv = anObserverService->RemoveObserver(&aObserver, &aTopic);
		    if (NS_FAILED(rv)) return rv;


        rv = anObserverService->RemoveObserver(&bObserver, &aTopic);
		    if (NS_FAILED(rv)) return rv;
       
    }
    return NS_OK;
}
