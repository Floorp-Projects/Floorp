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

#ifndef _CacheManager_H_
#define _CacheManager_H_
/* 
 * nsCacheManager
 *
 * Gagan Saksena 02/02/98
 * 
 */

#if 0
#include "nsISupports.h"
#endif

#include "nsCacheModule.h"
#include "nsCacheObject.h"

class nsCacheManager //: public nsISupports
{

public:
	nsCacheManager();
	~nsCacheManager();

	PRInt32		    AddModule(nsCacheModule* i_cacheModule);
    PRBool          Contains(const char* i_url) const;
	PRInt32		    Entries() const;
	/* Singleton */
	static nsCacheManager* 
				    GetInstance();
    nsCacheObject*  GetObject(const char* i_url) const;
	nsCacheModule*  GetModule(PRInt32 i_index) const;
	const char*     Trace() const;
    /* Performance measure- microseconds */
    PRUint32         WorstCaseTime() const;
protected:
	nsCacheModule*
				LastModule() const;


private:
	nsCacheModule* m_pFirstModule;

	nsCacheManager(const nsCacheManager& cm);
	nsCacheManager& operator=(const nsCacheManager& cm);	
};

#endif
