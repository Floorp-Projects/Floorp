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
#ifndef _nsMemModule_H_
#define _nsMemModule_H_
/* 
* nsMemModule
*
* Gagan Saksena
* 02/03/98
* 
*/

#include "nsCacheModule.h"
#include "nsMemCacheObject.h"

class nsMemModule : public nsCacheModule
{

public:
	nsMemModule();
	nsMemModule(const PRUint32 size);
	~nsMemModule();

	PRBool          AddObject(nsCacheObject* i_pObject);
	PRBool          Contains(nsCacheObject* i_pObject) const;
    PRBool          Contains(const char* i_url) const;
	nsCacheObject*	GetObject(PRUint32 i_index) const;
	nsCacheObject*	GetObject(const char* i_url) const;

	// Start of nsMemModule specific stuff...
	// Here is a sample implementation using linked list
protected:
	nsMemCacheObject* LastObject(void) const;

private:
	nsMemCacheObject* m_pFirstObject;

	nsMemModule(const nsMemModule& mm);
	nsMemModule& operator=(const nsMemModule& mm);
};

#endif
