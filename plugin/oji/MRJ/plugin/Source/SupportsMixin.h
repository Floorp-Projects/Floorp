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
	
	Uses table-driven approach for fast lookup.
	
	Aggregation support isn't quite here yet, it requires a helper object that is referenced only by the outer object,
	whose QueryInterace, AddRef, and Release act only locally (i.e. they call queryInterface, addRef, and release).
	However, if we're not using aggregation (hardly ever are) it seems wasteful to define the helper object.
	
	by Patrick C. Beard.
 */

#pragma once

#include "nsISupports.h"

// #define SUPPORT_AGGREGATION

struct InterfaceInfo {
	nsID mIID;			// the IID of this interface.
	UInt32 mOffset;		// the offset of this interface.
};

class SupportsMixin {
public:
	// These act locally on the current object, and are meant to be called by sub-classes.
	nsresult queryInterface(const nsIID& aIID, void** aInstancePtr);
	nsrefcnt addRef(void);
	nsrefcnt release(void);

protected:
	SupportsMixin(void* instance, const InterfaceInfo interfaces[], UInt32 interfaceCount, nsISupports* outer = NULL);
	virtual ~SupportsMixin();

#ifdef SUPPORT_AGGREGATION
	NS_METHOD OuterQueryInterface(REFNSIID aIID, void** aInstancePtr);
	NS_METHOD_(nsrefcnt) OuterAddRef(void);
	NS_METHOD_(nsrefcnt) OuterRelease(void);
#endif

private:
	void* mInstance;
	nsrefcnt mRefCount;
	const InterfaceInfo* mInterfaces;
	UInt32 mInterfaceCount;

#ifdef SUPPORT_AGGREGATION
	nsISupports* mOuter;
	
	class Inner : public nsISupports {
	public:
		NS_IMETHOD QueryInterface(REFNSIID aIID, void** aInstancePtr) { return mSupports->queryInterface(aIID, aInstancePtr); }
		NS_IMETHOD_(nsrefcnt) AddRef(void) { return mSupports->addRef(); }
		NS_IMETHOD_(nsrefcnt) Release(void) { return mSupports->release(); }
		
		Inner(SupportsMixin* supports) : mSupports(supports) {}
		
	private:
		SupportsMixin* mSupports;
	};

	Inner* mInner;
#endif
};

#ifdef SUPPORT_AGGREGATION

#define DECL_SUPPORTS_MIXIN \
	NS_IMETHOD QueryInterface(REFNSIID aIID, void** aInstancePtr) { return OuterQueryInterface(aIID, aInstancePtr); } \
	NS_IMETHOD_(nsrefcnt) AddRef(void) { return OuterAddRef(); } \
	NS_IMETHOD_(nsrefcnt) Release(void) { return OuterRelease(); }

#else

#define DECL_SUPPORTS_MIXIN \
	NS_IMETHOD QueryInterface(REFNSIID aIID, void** aInstancePtr) { return queryInterface(aIID, aInstancePtr); } \
	NS_IMETHOD_(nsrefcnt) AddRef(void) { return addRef(); } \
	NS_IMETHOD_(nsrefcnt) Release(void) { return release(); }

#endif

#define INTERFACE_OFFSET(leafType, interfaceType) \
	UInt32((interfaceType*) ((leafType*)0))
