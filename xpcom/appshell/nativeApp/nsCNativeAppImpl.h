/* -*- Mode: IDL; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is the Mozilla browser.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications, Inc.  Portions created by Netscape are
 * Copyright (C) 1999, Mozilla.  All Rights Reserved.
 * 
 * Contributor(s):
 *   Travis Bogard <travis@netscape.com>
 */

#ifndef nsCNativeAppImpl_h__
#define nsCNativeAppImpl_h__

#include "prthread.h"
#include "nsWeakPtr.h"
#include "nsVoidArray.h"
#include "nsString.h"
#include "nsCNativeApp.h"
#include "nsCEventLoop.h"

class nsCLoopInfo
{
public:
	nsCLoopInfo(const PRUnichar* loopName, PRThread* thread, 
		nsIEventLoop* eventLoop, nsEventLoopType type);

	void GetEventLoop(nsIEventLoop** eventLoop);
	PRBool CompareName(const PRUnichar* loopName);

	// These hand back a raw pointer without refcounting
	PRThread* Thread();
	nsEventLoopType Type();

protected:
	PRThread* m_Thread;
	nsWeakPtr m_pEventLoop;
	nsEventLoopType m_Type;
	nsAutoString m_LoopName;
};

class nsCNativeAppImpl : public nsINativeApp
{
public:
	NS_DECL_ISUPPORTS

	NS_DECL_NSINATIVEAPP

	static NS_METHOD Create(nsISupports* aOuter, const nsIID& aIID, void** ppv);

protected:
	nsCNativeAppImpl();
	virtual ~nsCNativeAppImpl();

	// Event Loop list management
	nsCLoopInfo* AddLoop(const PRUnichar* loopName, PRThread* thread, 
		nsIEventLoop* eventLoop, nsEventLoopType type);
	void RemoveLoop(nsCLoopInfo* loopInfo);
	PRBool VerifyLoop(nsCLoopInfo* loopInfo);
	nsCLoopInfo* GetLoop(const PRUnichar* loopName);
	nsCLoopInfo* GetLoop(PRThread* thread);
	nsCLoopInfo* GetLoop(nsEventLoopType type);

protected:
	nsVoidArray		m_EventLoopList;
};

#endif /* nsCNativeAppImpl_h__ */
