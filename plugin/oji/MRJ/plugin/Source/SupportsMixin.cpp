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
	SupportsMixin.cpp
	
	Experimental way to implement nsISupports interface.
	
	by Patrick C. Beard.
 */

#include "SupportsMixin.h"

// Standard ISupported Method Implementations.

SupportsMixin::SupportsMixin(nsISupports* instance, nsID* ids, UInt32 idCount)
	: mInstance(instance), mRefCount(0), mIDs(ids), mIDCount(idCount)
{
}

SupportsMixin::~SupportsMixin()
{
	if (mRefCount > 0) {
		::DebugStr("\pmRefCount > 0!");
	}
}

nsresult SupportsMixin::queryInterface(const nsIID& aIID, void** aInstancePtr)
{
	if (aInstancePtr == NULL) {
		return NS_ERROR_NULL_POINTER;
	}
	// first check to see if it's one of our known interfaces.
	// need to solve the non-left inheritance graph case.
	nsID* ids = mIDs;
	UInt32 count = mIDCount;
	for (UInt32 i = 0; i < count; i++) {
		if (aIID.Equals(ids[i])) {
			*aInstancePtr = (void*) mInstance;
			addRef();
			return NS_OK;
		}
	}
	// finally, does the interface match nsISupports?
	static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
	if (aIID.Equals(kISupportsIID)) {
		*aInstancePtr = (void*) mInstance;
		addRef();
		return NS_OK;
	}
	return NS_NOINTERFACE;
}

nsrefcnt SupportsMixin::addRef()
{
	return ++mRefCount;
}

nsrefcnt SupportsMixin::release()
{
	if (--mRefCount == 0) {
		delete this;
		return 0;
	}
	return mRefCount;
}
