/* ----- BEGIN LICENSE BLOCK -----
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the MRJ Carbon OJI Plugin.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corp.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):  Patrick C. Beard <beard@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ----- END LICENSE BLOCK ----- */

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
