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

/* The nsMemCacheObject class holds the actual nsCacheObject and a pointer
 * to the next nsMemCacheObject. Pretty simple solution for the time being. 
 * Should be replaced once the memory hash table stuff gets up and running. 
 * 
 * -Gagan Saksena 09/15/98.
 */

#ifndef _nsMemCacheObject_h_
#define _nsMemCacheObject_h_

#include "prtypes.h"
#include "prlog.h"
#include "nsICacheObject.h"
#include "nsCacheObject.h"

class nsMemCacheObject
{
public:

    nsMemCacheObject(void);
    nsMemCacheObject(nsICacheObject* io_pObject);
    nsMemCacheObject(const char* i_url); 
    ~nsMemCacheObject();

    void                Next(nsMemCacheObject* pObject);
    void                Next(nsICacheObject* io_pObject);
    
    nsMemCacheObject*   Next(void) const;
    
    nsICacheObject*      ThisObject(void) const;
    
private:
    nsICacheObject*  m_pObject;
    nsMemCacheObject* m_pNextObject;

    nsMemCacheObject& operator=(const nsMemCacheObject& mco);
    nsMemCacheObject(const nsMemCacheObject&);

};

inline nsMemCacheObject::nsMemCacheObject(void):
    m_pObject((nsICacheObject *) new nsCacheObject()), 
    m_pNextObject(0)
{
}

inline nsMemCacheObject::nsMemCacheObject(nsICacheObject* io_pObject):
    m_pObject(io_pObject),
    m_pNextObject(0)
{
}

inline nsMemCacheObject::nsMemCacheObject(const char* i_url):
    m_pObject((nsICacheObject *) new nsCacheObject(i_url)), 
    m_pNextObject(0)
{
}

inline nsMemCacheObject* nsMemCacheObject::Next(void) const
{
    return m_pNextObject;
}

inline void nsMemCacheObject::Next(nsMemCacheObject* pObject)
{
    PR_ASSERT(0==m_pNextObject);
    m_pNextObject = pObject;
}

inline void nsMemCacheObject::Next(nsICacheObject* pObject)
{
    PR_ASSERT(0==m_pNextObject);
    m_pNextObject = new nsMemCacheObject(pObject);
}

inline nsICacheObject* nsMemCacheObject::ThisObject(void) const
{
   return m_pObject;
}

#endif //_nsMemCacheObject_h_
