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

#include "nsMemModule.h"
#include "nsMemCacheObject.h"
#include <string.h>

/* 
 * nsMemModule
 *
 * Gagan Saksena 02/02/98
 * 
 */


const long DEFAULT_SIZE = 5*1024*1024;

nsMemModule::nsMemModule():m_pFirstObject(0)
{
	//Size(DEFAULT_SIZE);
}

nsMemModule::nsMemModule(const long size):m_pFirstObject(0)
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

int nsMemModule::AddObject(nsCacheObject* i_pObject)
{
	if (i_pObject)
	{
		if (m_pFirstObject) 
		{
			LastObject()->Next(new nsMemCacheObject(*i_pObject)); 
		}
		else
		{
			m_pFirstObject = new nsMemCacheObject(*i_pObject);
		}
		m_Entries++;
		return 1;
	}
	return 0;
}

int nsMemModule::Contains(const char* i_url) const
{
	if (m_pFirstObject && i_url && *i_url)
	{
		nsMemCacheObject* pObj = m_pFirstObject;
		int inlen = strlen(i_url);
		do
		{
			if (0 == _strnicmp(pObj->Address(), i_url, inlen))
				return 1;
			pObj = pObj->Next();
		}
		while (pObj);
	}
	return 0;
}

int nsMemModule::Contains(nsCacheObject* i_pObject) const
{
    if (i_pObject && i_pObject->Address())
    {
        return this->Contains(i_pObject->Address());
    }
    return 0;
    /* //XXX Think!
	if (m_pFirstObject && i_pObject)
	{
		nsMemCacheObject* pNext = m_pFirstObject;
		do
		{
            if (pNext == i_pObject) // Equality operator ?
                return 1;
			pNext = pNext->Next();
		}
		while (pNext);
	}
	return 0;
    */
}

nsCacheObject* nsMemModule::GetObject(long i_index) const
{
	nsMemCacheObject* pNth = 0;
	if (m_pFirstObject)
	{
		int index = 0;
		pNth = m_pFirstObject;
		while (pNth->Next() && (index++ != i_index ))
		{
			pNth = pNth->Next();
		}
	}
	return pNth;
}

nsCacheObject* nsMemModule::GetObject(const char* i_url) const
{
	if (m_pFirstObject && i_url && *i_url)
	{
		nsMemCacheObject* pObj = m_pFirstObject;
		int inlen = strlen(i_url);
		do
		{
			if (0 == _strnicmp(pObj->Address(), i_url, inlen))
				return pObj;
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