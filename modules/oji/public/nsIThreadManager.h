/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#ifndef nsIThreadManager_h___
#define nsIThreadManager_h___

#include "nsISupports.h"

////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// Thread Manager
// This interface provides thread primitives.

#define NS_ITHREADMANAGER_IID                           \
{ /* 97bb54c0-6846-11d2-801f-00805f71101c */            \
	0x97bb54c0,											\
	0x6846,												\
	0x11d2,												\
	{0x80, 0x1f, 0x00, 0x80, 0x5f, 0x71, 0x10, 0x1c}	\
}

class nsIRunnable;

typedef struct _nsPluginThread nsPluginThread;

class nsIThreadManager : public nsISupports {
public:
	NS_DEFINE_STATIC_IID_ACCESSOR(NS_ITHREADMANAGER_IID)
	
	/**
	 * Returns a unique identifier for the "current" system thread.
	 */
	NS_IMETHOD
	GetCurrentThread(nsPluginThread* *threadID) = 0;

	/**
	 * Pauses the current thread for the specified number of milliseconds.
	 * If milli is zero, then merely yields the CPU if another thread of
	 * greater or equal priority.
	 */
	NS_IMETHOD
	Sleep(PRUint32 milli = 0) = 0;
	
	/**
	 * Creates a unique monitor for the specified address, and makes the
	 * current system thread the owner of the monitor.
	 */
	NS_IMETHOD
	EnterMonitor(void* address) = 0;
	
	/**
	 * Exits the monitor associated with the address.
	 */
	NS_IMETHOD
	ExitMonitor(void* address) = 0;
	
	/**
	 * Waits on the monitor associated with the address (must be entered already).
	 * If milli is 0, wait indefinitely.
	 */
	NS_IMETHOD
	Wait(void* address, PRUint32 milli = 0) = 0;

	/**
	 * Notifies a single thread waiting on the monitor associated with the address (must be entered already).
	 */
	NS_IMETHOD
	Notify(void* address) = 0;

	/**
	 * Notifies all threads waiting on the monitor associated with the address (must be entered already).
	 */
	NS_IMETHOD
	NotifyAll(void* address) = 0;

	/**
	 * Creates a new thread, calling the specified runnable's Run method (a la Java).
	 */
	NS_IMETHOD
	CreateThread(PRUint32* threadID, nsIRunnable* runnable) = 0;
	
	/**
	 * Posts an event to specified thread, calling the runnable from that thread.
	 * @param threadID thread to call runnable from
	 * @param runnable object to invoke from thread
	 * @param async if true, won't block current thread waiting for result
	 */
	NS_IMETHOD
	PostEvent(PRUint32 threadID, nsIRunnable* runnable, PRBool async) = 0;
};

////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// Runnable
// This interface represents the invocation of a new thread.

#define NS_IRUNNABLE_IID								\
{ /* 930f3d70-6849-11d2-801f-00805f71101c */			\
	0x930f3d70,											\
	0x6849,												\
	0x11d2,												\
	{0x80, 0x1f, 0x00, 0x80, 0x5f, 0x71, 0x10, 0x1c}	\
}

class nsIRunnable : public nsISupports {
public:
	NS_DEFINE_STATIC_IID_ACCESSOR(NS_IRUNNABLE_IID)

	/**
	 * Defines an entry point for a newly created thread.
	 */
	NS_IMETHOD
	Run() = 0;
};

#endif /* nsIThreadManager_h___ */
