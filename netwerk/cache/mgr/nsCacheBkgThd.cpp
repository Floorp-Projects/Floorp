/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#include "prlog.h"
#include "nsCacheBkgThd.h"
#include "nsIServiceManager.h"
#include "nsCacheManager.h"
//#include "nsCacheModule.h"
#include "nsCachePref.h"

static NS_DEFINE_IID(kICachePrefIID, NS_ICACHEPREF_IID) ;
static NS_DEFINE_IID(kCachePrefCID, NS_CACHEPREF_CID) ;

nsCacheBkgThd::nsCacheBkgThd(PRIntervalTime iSleepTime):
    nsBkgThread(iSleepTime)
{
}

nsCacheBkgThd::~nsCacheBkgThd()
{
}

/*
nsrefcnt nsCacheBkgThd::AddRef(void)
{
    return ++m_RefCnt;
}
nsrefcnt nsCacheBkgThd::Release(void)
{
    if (--m_RefCnt == 0)
    {
        delete this;
        return 0;
    }
    return m_RefCnt;
}

nsresult nsCacheBkgThd::QueryInterface(const nsIID& aIID,
                                        void** aInstancePtrResult)
{
    return NS_OK;
}
*/

void nsCacheBkgThd::Run(void)
{
    /* Step thru all the modules and call cleanup on each */
    nsCacheManager* pCM  = nsCacheManager::GetInstance();

	nsICachePref* pref ;

	nsServiceManager::GetService(kCachePrefCID,
	                             kICachePrefIID,
								 (nsISupports **)&pref) ;

	PRBool offline ;

    PR_ASSERT(pCM);
    pCM->IsOffline(&offline) ;
    if (offline)
        return; /* Dont update entries if offline */
    PRInt16 i ;
	pCM->Entries(&i);
    while (i>0)
    {
        nsICacheModule* pModule ;
		pCM->GetModule(--i, &pModule);
        PR_ASSERT(pModule);

        /* TODO change this based on if it supports garbage cleaning */

		PRBool ro, revalidate ;
		pModule->IsReadOnly(&ro) ;
        if (!ro)
        {
            pModule->GarbageCollect();

            pref->RevalidateInBkg(&revalidate) ;

            if (revalidate)
            {
                pModule->Revalidate();
            }
        }
    }
}

