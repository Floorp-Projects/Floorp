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

#ifndef _nsMemCacheObject_h_
#define _nsMemCacheObject_h_

#include <prtypes.h>

#include "nsCacheObject.h"

class nsMemCacheObject
{
public:

    nsMemCacheObject(void);
    nsMemCacheObject(nsCacheObject* io_pObject);
	nsMemCacheObject(const char* i_url); 
	~nsMemCacheObject();

    void*               Data(void) const;
    
    void                Next(nsMemCacheObject* pObject);
    void                Next(nsCacheObject* io_pObject);
	
    nsMemCacheObject*   Next(void) const;
    
    nsCacheObject*      ThisObject(void) const;
	
private:
	nsMemCacheObject* m_pNextObject;
    nsCacheObject*  m_pObject;
	void* m_pData;

	nsMemCacheObject& operator=(const nsMemCacheObject& mco);	
	nsMemCacheObject(const nsMemCacheObject&);

};

inline nsMemCacheObject::nsMemCacheObject(void):
	m_pObject(new nsCacheObject()), 
	m_pNextObject(0),
    m_pData(0)
{
}

inline nsMemCacheObject::nsMemCacheObject(nsCacheObject* io_pObject):
	m_pObject(io_pObject),
	m_pNextObject(0),
    m_pData(0)
{
}

inline nsMemCacheObject::nsMemCacheObject(const char* i_url):
	m_pObject(new nsCacheObject(i_url)), 
	m_pNextObject(0),
    m_pData(0)
{
}

inline void* nsMemCacheObject::Data(void) const
{
//    PR_ASSERT(m_pData);
    return m_pData;
}

inline nsMemCacheObject* nsMemCacheObject::Next(void) const
{
    return m_pNextObject;
}

inline void nsMemCacheObject::Next(nsMemCacheObject* pObject)
{
    m_pNextObject = pObject;
}

inline void nsMemCacheObject::Next(nsCacheObject* pObject)
{
    m_pNextObject = new nsMemCacheObject(pObject);
}

inline nsCacheObject* nsMemCacheObject::ThisObject(void) const
{
   return m_pObject;
}

#endif //_nsMemCacheObject_h_
