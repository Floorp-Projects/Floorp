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

#include "prtypes.h"
#include "prmem.h"
#include "plstr.h"
#include "prclist.h"
#include "prio.h"
#include "prsystem.h"
#include "prlog.h"

#include "nsMemModule.h"
#include "nsMemCacheObject.h"
#include "nsCacheManager.h"
#include "nsICacheManager.h"
#include "nsICachePref.h"
#include "nsMemStream.h"

/* 
 * nsMemModule
 *
 * Gagan Saksena 02/02/98
 * 
 */

static NS_DEFINE_IID(kICacheManagerIID, NS_ICACHEMANAGER_IID);
static NS_DEFINE_IID(kCacheManagerCID, NS_CACHEMANAGER_CID);

nsMemModule::nsMemModule(): 
    m_pFirstObject(0),
    m_SizeInUse(0),
    m_pEnumeration(0),
    m_pNext(0)
{
    nsICacheManager* pICacheMgr;
    nsresult rv = nsServiceManager::GetService (kCacheManagerCID,
						kICacheManagerIID,
						(nsISupports **) &pICacheMgr);
    PR_ASSERT (rv == NS_OK);
   
    nsICachePref* pPref;
    rv = pICacheMgr->GetPrefs (&pPref);

    PR_ASSERT (rv == NS_OK);

    pPref->GetMemCacheSize(&m_Size);
    
}

nsMemModule::nsMemModule(const PRUint32 size): 
    m_pFirstObject(0),
    m_Size (size),
    m_SizeInUse(0),
    m_pEnumeration(0),
    m_pNext(0)
{
}

nsMemModule::~nsMemModule()
{
    if (m_pFirstObject) {
        delete m_pFirstObject;
        m_pFirstObject = 0;
    }
}


NS_IMETHODIMP 
nsMemModule::AddObject(nsICacheObject* io_pObject)
{

#if 0
    if (io_pObject)
    {
        m_ht.Put(io_pObject->Address(), io_pObject);
    }
    return NS_ERROR_FAILURE;
#endif

    if (io_pObject)
    {
        MonitorLocker ml((nsMonitorable*)this);
		nsStream* pStream = (nsStream *) nsnull;
		io_pObject->GetStream(&pStream);

		// I removed this assert. I don't think it should be here. yixiong
		// pStream would be set later by GetStreamFor( ) in nsCacheobject
        // PR_ASSERT(pStream); // A valid stream does exist for this 

        if (m_pFirstObject) 
        {
            LastObject()->Next(new nsMemCacheObject(io_pObject)); 
        }
        else
        {
            m_pFirstObject = new nsMemCacheObject(io_pObject);
        }
        m_Entries++;

        io_pObject->SetModuleIndex(nsCacheManager::MEM);

        return NS_OK;
    }
	// Todo:  Better error handling
    return NS_ERROR_FAILURE;
}



NS_IMETHODIMP 
nsMemModule::ContainsURL (const char* i_url, PRBool* o_bContain)
{
    MonitorLocker ml((nsMonitorable*)this);

    *o_bContain = PR_FALSE;
    if (m_pFirstObject && i_url && *i_url)
    {
        nsMemCacheObject* pObj = m_pFirstObject;
        do
        {
			char *thisURL = nsnull;
			pObj->ThisObject()->GetAddress (&thisURL);
            if (0 == PL_strcasecmp(thisURL, i_url))
			{
				*o_bContain = PR_TRUE;
                break;
			}
            pObj = pObj->Next();
        }
        while (pObj);
    }
    return NS_OK;
}


NS_IMETHODIMP
nsMemModule::ContainsCacheObj(nsICacheObject* i_pObject, PRBool* o_bContain)
{
    MonitorLocker ml((nsMonitorable*)this);

	*o_bContain = PR_FALSE;
	char *url = (char *) nsnull;
	if (i_pObject)
    {
        i_pObject->GetAddress (&url);
	}
    if (url && *url)
    {
        this->ContainsURL(url, o_bContain);
    }
    return NS_OK;
}


NS_IMETHODIMP
nsMemModule::GarbageCollect(void)
{
    MonitorLocker ml((nsMonitorable*) this);

    if (m_Entries > 0)
    {
        nsEnumeration* pEnum;
		GetEnumerator(&pEnum);
        PRUint32 idx = 0;
        while (pEnum->HasMoreElements())
        {
            nsICacheObject* pObj = (nsICacheObject*) pEnum->NextElement();
			PRBool bExpired = PR_FALSE;
            if (pObj)
			{
				pObj->IsExpired (&bExpired);
           	}
            if (bExpired == PR_TRUE)
            {
                nsresult r = RemoveByIndex (idx);
                PR_ASSERT(r == NS_OK);
            }
            ++idx;
        }
    }
    return NS_OK;
}

NS_IMETHODIMP
nsMemModule::GetObjectByIndex (const PRUint32 i_index, 
								nsICacheObject** ppICacheObject)
{
    MonitorLocker ml((nsMonitorable*)this);
    nsMemCacheObject* pNth = 0;
    if (m_pFirstObject)
    {
        PRUint32 idx = 0;
        pNth = m_pFirstObject;
        while (pNth->Next() && (idx++ != i_index ))
        {
            pNth = pNth->Next();
        }
    }
    if (pNth)
		*ppICacheObject = pNth->ThisObject();
	else
		*ppICacheObject = (nsICacheObject *) nsnull;
    return NS_OK;
}


NS_IMETHODIMP
nsMemModule::GetObjectByURL(const char* i_url, nsICacheObject** ppICacheObject)
{
    MonitorLocker ml((nsMonitorable*)this);
    *ppICacheObject = (nsICacheObject *) nsnull;

    if (m_pFirstObject && i_url && *i_url)
    {
        nsMemCacheObject* pObj = m_pFirstObject;
        do
        {
            char *thisURL;
			pObj->ThisObject()->GetAddress (&thisURL);
            if (0 == PL_strcasecmp(thisURL, i_url))
			{
				*ppICacheObject = pObj->ThisObject();
                break;
			}
            pObj = pObj->Next();
        }
        while (pObj);
    }
    return NS_OK;
}

NS_IMETHODIMP
nsMemModule::GetStreamFor(const nsICacheObject* i_pObject, nsStream** o_stream)
{
    MonitorLocker ml(this);

	*o_stream = nsnull;
    if (i_pObject)
    {
        PRBool bContain = PR_FALSE;
        ContainsCacheObj((nsICacheObject*) i_pObject, &bContain);
        if (bContain)
        {
            nsStream* pStream = (nsStream *) nsnull;
            i_pObject->GetStream(&pStream);
            if (pStream)
			{
				*o_stream = pStream;
				return NS_OK;
			}
        }
        // Set up a new stream for this object
        *o_stream = new nsMemStream();
    }
    return NS_OK;
}


nsMemCacheObject* nsMemModule::LastObject(void) const
{
    MonitorLocker ml((nsMonitorable*)this);
    
    nsMemCacheObject* pLast = 0;
    if (m_pFirstObject)
    {
        pLast = m_pFirstObject;
        while (pLast->Next())
            pLast = pLast->Next();
    }
    return pLast;
}

NS_IMETHODIMP
nsMemModule::ReduceSizeTo(const PRUint32 i_NewSize)
{
    //TODO
    return PR_TRUE;
}


NS_IMETHODIMP
nsMemModule::RemoveByURL (const char* i_url)
{
    //TODO
    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
nsMemModule::RemoveByIndex(const PRUint32 i_index)
{
    //TODO
    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
nsMemModule::RemoveByObject(nsICacheObject* i_pObj)
{
    //TODO
    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
nsMemModule::RemoveAll()
{
	//TODO
    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
nsMemModule::SetSize(const PRUint32 i_Size)
{
	//TODO
    return NS_ERROR_NOT_IMPLEMENTED;
}

//////////////////////////////////////////////////////////////////////////////
NS_IMETHODIMP
nsMemModule::QueryInterface(const nsIID& aIID, void** aInstancePtr)
{
    NS_ASSERTION(aInstancePtr, "no instance pointer");

    *aInstancePtr = 0;

    if (aIID.Equals(nsCOMTypeInfo<nsISupports>::GetIID()) ||
        aIID.Equals(nsCOMTypeInfo<nsICacheModule>::GetIID()))
    {
        *aInstancePtr = NS_STATIC_CAST (nsICacheModule*, this);
        NS_ADDREF_THIS();
        return NS_OK;
    }
    return NS_NOINTERFACE;
}


NS_IMPL_ADDREF(nsMemModule);
NS_IMPL_RELEASE(nsMemModule);

NS_METHOD
nsMemModule::Create (nsISupports *aOuter, REFNSIID aIID, void** aResult)
{
	if (aOuter)
		return NS_ERROR_NO_AGGREGATION;

	nsMemModule* pm = new nsMemModule();
	if (pm == nsnull)
		return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF (pm);
    nsresult rv = pm->QueryInterface (aIID, aResult);
    NS_RELEASE (pm);
    return rv;
}

//////////////////////////////////////////////////////////////////////////////

NS_IMETHODIMP
nsMemModule::Enable(PRBool i_bEnable)
{
    m_Enabled = i_bEnable;
    return NS_OK;
}


NS_IMETHODIMP 
nsMemModule::GetNumOfEntries(PRUint32* o_nEntries)
{
    *o_nEntries = m_Entries;
    return NS_OK;
}


NS_IMETHODIMP
nsMemModule::GetEnumerator(nsEnumeration** o_enum)
{
    MonitorLocker ml((nsMonitorable*)this);
    if (!m_pEnumeration)
    {
        PR_ASSERT(m_pIterator);
        ((nsMemModule*)this)->m_pEnumeration = 
									new nsEnumeration((nsIterator*)m_pIterator);
    }
    else
        ((nsMemModule*)this)->m_pEnumeration->Reset();
    *o_enum = m_pEnumeration;
    return NS_OK;
}


NS_IMETHODIMP
nsMemModule::IsEnabled(PRBool* o_bEnabled)
{
	*o_bEnabled = m_Enabled;
    return NS_OK;
}


NS_IMETHODIMP
nsMemModule::IsReadOnly (PRBool* o_bReadOnly)
{
	*o_bReadOnly  = PR_FALSE;		// for Now
    return NS_OK;
}


NS_IMETHODIMP
nsMemModule::GetNextModule(nsICacheModule** o_pICacheModule)
{
    if (m_pNext) {
      *o_pICacheModule = m_pNext;
      return NS_OK;
	}
	return NS_ERROR_FAILURE ;
}


NS_IMETHODIMP
nsMemModule::SetNextModule(nsICacheModule* pNext) 
{
    /* No overwriting */
    PR_ASSERT(m_pNext == 0);
    if (m_pNext)
    {
        /* ERROR */
        delete m_pNext; //Worst case. 
        m_pNext = 0;
    }
    m_pNext = pNext;
	return NS_OK;
}


NS_IMETHODIMP
nsMemModule::GetSize(PRUint32* o_size)
{
	*o_size = m_Size;
    return NS_OK;
}


NS_IMETHODIMP
nsMemModule::GetSizeInUse(PRUint32* o_size)
{
	*o_size = m_SizeInUse;
    return NS_OK;
}

/*
PRUint32 nsMemModule::nsMemKey::HashValue()
{
    return 0;
}
PRBool nsMemModule::nsMemKey::Equals(nsHashKey *aKey)
{
    return PR_FALSE;
}

nsHashKey* nsMemModule::nsMemKey::Clone()
{
    return new nsMemModule::nsMemKey();
}

nsMemModule::nsMemKey::neMemKey()
{
}

  */
