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

#include "nsCacheObject.h"

class nsMemCacheObject : public nsCacheObject
{
public:
	nsMemCacheObject(void);
	nsMemCacheObject(const nsCacheObject&);
	nsMemCacheObject(const char* i_url); 
	~nsMemCacheObject();

	void Next(nsCacheObject* pObject);
	nsMemCacheObject* Next(void) const;

private:
	nsMemCacheObject* m_pNextObject;
	/* The actual data of this cache object */
	char* m_pData;

	nsMemCacheObject& operator=(const nsMemCacheObject& lco);	

};

inline nsMemCacheObject::nsMemCacheObject(void):
	nsCacheObject(), 
	m_pNextObject(0) 
{
}

inline nsMemCacheObject::nsMemCacheObject(const nsCacheObject& another):
	nsCacheObject(another),
	m_pNextObject(0)
{
}

inline nsMemCacheObject::nsMemCacheObject(const char* i_url):
	nsCacheObject(i_url), 
	m_pNextObject(0) 
{
}

#endif //_nsMemCacheObject_h_
