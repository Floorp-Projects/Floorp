/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:set ts=4 sw=4 sts=4 ci et: */
/* ***** BEGIN LICENSE BLOCK *****
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
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef nsIJVMThreadManager_h___
#define nsIJVMThreadManager_h___

#include "nsISupports.h"
#include "nsIRunnable.h"
#include "nspr.h"

////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// Thread Manager
// This interface provides thread primitives.

#define NS_IJVMTHREADMANAGER_IID                        \
{ /* 97bb54c0-6846-11d2-801f-00805f71101c */            \
	0x97bb54c0,											\
	0x6846,												\
	0x11d2,												\
	{0x80, 0x1f, 0x00, 0x80, 0x5f, 0x71, 0x10, 0x1c}	\
}

class nsIRunnable;

class nsIJVMThreadManager : public nsISupports {
public:
	NS_DECLARE_STATIC_IID_ACCESSOR(NS_IJVMTHREADMANAGER_IID)
	
	/**
	 * Returns a unique identifier for the "current" system thread.
	 */
	NS_IMETHOD
	GetCurrentThread(PRThread* *threadID) = 0;

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
	CreateThread(PRThread **thread, nsIRunnable* runnable) = 0;
	
	/**
	 * Posts an event to specified thread, calling the runnable from that thread.
	 * @param threadID thread to call runnable from
	 * @param runnable object to invoke from thread
	 * @param async if true, won't block current thread waiting for result
	 */
	NS_IMETHOD
	PostEvent(PRThread* thread, nsIRunnable* runnable, PRBool async) = 0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIJVMThreadManager, NS_IJVMTHREADMANAGER_IID)

#ifndef NS_OJI_IMPL
// For backwards compatibility:
typedef nsIJVMThreadManager nsIThreadManager;
#define NS_ITHREADMANAGER_IID NS_IJVMTHREADMANAGER_IID
#endif

#endif /* nsIJVMThreadManager_h___ */
