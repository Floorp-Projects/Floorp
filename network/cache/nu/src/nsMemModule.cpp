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

#include <prtypes.h>
#include <plstr.h>
#include <prlog.h>

#include "nsMemModule.h"
#include "nsMemCacheObject.h"
#include "nsCacheManager.h"

/* 
 * nsMemModule
 *
 * Gagan Saksena 02/02/98
 * 
 */

//NS_DEFINE_IID(kMemModuleIID, NS_MEMMODULE_IID);

nsMemModule::nsMemModule(const PRUint32 size): 
    m_pFirstObject(0),
    nsCacheModule(size)
{
    Size(size);
}

nsMemModule::~nsMemModule()
{
    if (m_pFirstObject) {
        delete m_pFirstObject;
        m_pFirstObject = 0;
    }
}

PRBool nsMemModule::AddObject(nsCacheObject* io_pObject)
{

#if 0
    if (io_pObject)
    {
        m_ht.Put(io_pObject->Address(), io_pObject);
    }
    return PR_FALSE;
#endif

    if (io_pObject)
	{
		if (m_pFirstObject) 
		{
			LastObject()->Next(new nsMemCacheObject(io_pObject)); 
		}
		else
		{
			m_pFirstObject = new nsMemCacheObject(io_pObject);
		}
		m_Entries++;

        io_pObject->Module(nsCacheManager::MEM);

        return PR_TRUE;
	}
	return PR_FALSE;
}

PRBool nsMemModule::Contains(const char* i_url) const
{
    if (m_pFirstObject && i_url && *i_url)
    {
        nsMemCacheObject* pObj = m_pFirstObject;
        PRUint32 inlen = PL_strlen(i_url);
        do
        {
            if (0 == PL_strncasecmp(pObj->ThisObject()->Address(), i_url, inlen))
                return PR_TRUE;
            pObj = pObj->Next();
        }
        while (pObj);
    }
    return PR_FALSE;
}

PRBool nsMemModule::Contains(nsCacheObject* i_pObject) const
{
    if (i_pObject && *i_pObject->Address())
    {
        return this->Contains(i_pObject->Address());
    }
    return 0;
}

nsCacheObject* nsMemModule::GetObject(const PRUint32 i_index) const
{
	nsMemCacheObject* pNth = 0;
	if (m_pFirstObject)
	{
		PRUint32 index = 0;
		pNth = m_pFirstObject;
		while (pNth->Next() && (index++ != i_index ))
		{
			pNth = pNth->Next();
		}
	}
	return pNth->ThisObject();
}

nsCacheObject* nsMemModule::GetObject(const char* i_url) const
{
	if (m_pFirstObject && i_url && *i_url)
	{
		nsMemCacheObject* pObj = m_pFirstObject;
		int inlen = PL_strlen(i_url);
		do
		{
			if (0 == PL_strncasecmp(pObj->ThisObject()->Address(), i_url, inlen))
				return pObj->ThisObject();
			pObj = pObj->Next();
		}
		while (pObj);
	}
	return 0;
}

nsMemCacheObject* nsMemModule::LastObject(void) const
{
	nsMemCacheObject* pLast = 0;
	if (m_pFirstObject)
	{
		pLast = m_pFirstObject;
		while (pLast->Next())
			pLast = pLast->Next();
	}
	return pLast;
}

PRBool nsMemModule::Remove(const char* i_url)
{
    return PR_FALSE;
}

PRBool nsMemModule::Remove(const PRUint32 i_index)
{
    return PR_FALSE;
}

void nsMemModule::GarbageCollect(void)
{
    if (m_Entries > 0)
    {
        PRUint32 index = 0;
        while (index < m_Entries)
        {
            //TODO change the iteration 
            nsCacheObject* pObj = GetObject(index);
            if (pObj->IsExpired())
            {
                PRBool status = Remove(index);
                PR_ASSERT(status == PR_TRUE);
            }
            --index;
        }
    }
}

/*
NS_IMETHOD nsMemModule::QueryInterface(const nsIID& aIID, void** aInstancePtr)
{

}
NS_IMETHOD_(nsrefcnt) nsMemModule::AddRef(void)
{

}

NS_IMETHOD_(nsrefcnt) nsMemModule::Release(void)
{

}
*/

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
