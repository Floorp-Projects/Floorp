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
#include "nsAgg.h"

// Standard nsISupport method implementations.

#ifdef SUPPORT_AGGREGATION

SupportsMixin::SupportsMixin(void* instance, const InterfaceInfo interfaces[], UInt32 interfaceCount, nsISupports* outer)
	: mInstance(instance), mRefCount(0), mInterfaces(interfaces), mInterfaceCount(interfaceCount), mOuter(outer)
{
	if (mOuter != NULL)
		mInner = new Inner(this);
}

SupportsMixin::~SupportsMixin()
{
	if (mRefCount > 0) {
		::DebugStr("\pmRefCount > 0!");
	}
	if (mInner != NULL)
		delete mInner;
}

/**
 * The uppercase versions QueryInterface, AddRef, and Release are meant to be called by subclass
 * implementations. They take aggregation into account.
 */
nsresult SupportsMixin::OuterQueryInterface(const nsIID& aIID, void** aInstancePtr)
{
	// first, see if we really implement this interface.
	nsresult result = queryInterface(aIID, aInstancePtr);
	// if not, then delegate to the outer object, if any.
	if (result != NS_OK && mOuter != NULL)
		return mOuter->QueryInterface(aIID, aInstancePtr);
	else
		return result;
}

nsrefcnt SupportsMixin::OuterAddRef()
{
	nsrefcnt result = addRef();
	if (mOuter != NULL)
		return mOuter->AddRef();
	return result;
}

nsrefcnt SupportsMixin::OuterRelease()
{
	if (mOuter != NULL) {
		nsIOuter* outer = NULL;
		nsISupports* supports = mOuter;
		static NS_DEFINE_IID(kIOuterIID, NS_IOUTER_IID);
		if (mRefCount == 1 && supports->QueryInterface(kIOuterIID, &outer) == NS_OK) {
			outer->ReleaseInner(mInner);
			outer->Release();
		} else
			release();
		return supports->Release();
	} else {
		return release();
	}
}

#else /* !SUPPORT_AGGREGATION */

SupportsMixin::SupportsMixin(void* instance, const InterfaceInfo interfaces[], UInt32 interfaceCount, nsISupports* /* outer */)
	: mInstance(instance), mRefCount(0), mInterfaces(interfaces), mInterfaceCount(interfaceCount)
{
}

SupportsMixin::~SupportsMixin()
{
	if (mRefCount > 0) {
		::DebugStr("\pmRefCount > 0!");
	}
}

#endif /* !SUPPORT_AGGREGATION */

/**
 * The lowercase implementations of queryInterface, addRef, and release all act locally
 * on the current object, regardless of aggregation. They are meant to be called by
 * aggregating outer objects.
 */
NS_IMETHODIMP SupportsMixin::queryInterface(const nsIID& aIID, void** aInstancePtr)
{
	if (aInstancePtr == NULL) {
		return NS_ERROR_NULL_POINTER;
	}
	// first check to see if it's one of our known interfaces.
	// need to solve the non-left inheritance graph case.
	const InterfaceInfo* interfaces = mInterfaces;
	UInt32 count = mInterfaceCount;
	for (UInt32 i = 0; i < count; i++) {
		if (aIID.Equals(interfaces[i].mIID)) {
			*aInstancePtr = (void*) (UInt32(mInstance) + interfaces[i].mOffset);
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

NS_IMETHODIMP_(nsrefcnt) SupportsMixin::addRef()
{
	return ++mRefCount;
}

NS_IMETHODIMP_(nsrefcnt) SupportsMixin::release()
{
	if (--mRefCount == 0) {
		delete this;
		return 0;
	}
	return mRefCount;
}
