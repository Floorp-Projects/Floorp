/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
