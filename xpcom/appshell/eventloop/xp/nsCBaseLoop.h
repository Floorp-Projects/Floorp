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

#ifndef nsCBaseLoop_h__
#define nsCBaseLoop_h__

#include "nsWeakReference.h"
#include "nsString.h"

#include "nsCEventLoop.h"
#include "nsPIEventLoop.h"

class nsCBaseLoop : public nsIEventLoop, public nsPIEventLoop, 
	public nsSupportsWeakReference
{
public:
	NS_DECL_ISUPPORTS

	//nsIEventLoop
	NS_DECL_NSIEVENTLOOP

	//nsPIEventLoop
	NS_DECL_NSPIEVENTLOOP

protected:
	nsCBaseLoop(nsEventLoopType type);
	virtual ~nsCBaseLoop();

	// Internal Helpers
	void* GetPlatformEventData(nsIEvent* event);
	void* GetPlatformFilterData(nsIEventFilter* filter);

	// Virtuals Specific Base Loop must override.
	virtual nsresult RunWithNoListener(nsIEvent* event, nsIEventFilter* filter)=0;
	virtual nsresult RunWithTranslateListener(nsIEvent* event, 
		nsIEventFilter* filter, nsITranslateListener* translateListener)=0;
	virtual nsresult RunWithDispatchListener(nsIEvent* event, 
		nsIEventFilter* filter, nsIDispatchListener* dispatchListener)=0;
	virtual nsresult RunWithTranslateAndDispatchListener(nsIEvent* event,
		nsIEventFilter* filter,	nsITranslateListener* translateListener, 
		nsIDispatchListener* dispatchListener)=0;

	// Virtuals Platform Loop must override
		// Internal Platform Implementations of nsIEventLoop 
	// (Error checking is ensured by caller)
	virtual nsresult PlatformExit(PRInt32 exitCode)=0;
	virtual nsresult PlatformGetNextEvent(void* platformFilterData, 
		void* platformEventData)=0;
	virtual nsresult PlatformPeekNextEvent(void* platformFilterData, 
		void* platformEventData, PRBool fRemoveEvent)=0;
	virtual nsresult PlatformTranslateEvent(void* platformEventData)=0;
	virtual nsresult PlatformDispatchEvent(void* platformEventData)=0;
	virtual nsresult PlatformSendLoopEvent(void* platformEventData, 
		PRInt32* result)=0;
	virtual nsresult PlatformPostLoopEvent(void* platformEventData)=0;  

	virtual nsNativeEventDataType PlatformGetEventType()=0;
	virtual nsNativeFilterDataType PlatformGetFilterType()=0;
	virtual PRInt32 PlatformGetReturnCode(void* platformEventData)=0; 

protected:
	nsAutoString	m_LoopName;
	nsEventLoopType	m_Type;
};

#endif /* nsCBaseLoop_h__ */
