/*
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

/*
	SupportsMixin.h
	
	Experimental way to implement nsISupports interface.
	
	by Patrick C. Beard.
 */

#pragma once

#include "nsISupports.h"

class SupportsMixin {
public:
	SupportsMixin(nsISupports* instance, nsID* ids, UInt32 idCount);
	virtual ~SupportsMixin();

	nsresult queryInterface(const nsIID& aIID, void** aInstancePtr);
	nsrefcnt addRef(void);
	nsrefcnt release(void);

protected:
	nsISupports* mInstance;
	nsrefcnt mRefCount;
	nsID* mIDs;
	UInt32 mIDCount;
};
