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
	NativeMonitor.cpp
	
	Provides a C++ interface to native monitors.
	
	by Patrick C. Beard.
 */

#include "NativeMonitor.h"
#include "MRJSession.h"
#include "nsIThreadManager.h"

NativeMonitor::NativeMonitor(MRJSession* session, nsIThreadManager* manager, void* address)
	:	mSession(session), mManager(manager), mAddress(address)
{
	if (address == NULL)
		mAddress = this;
}

NativeMonitor::~NativeMonitor() {}

void NativeMonitor::enter()
{
	mManager->EnterMonitor(mAddress);
}

void NativeMonitor::exit()
{
	mManager->ExitMonitor(mAddress);
}

void NativeMonitor::wait()
{
	// this is weird hackery, but we don't want to let the VM be reentered while we wait on a native monitor.
	Boolean inJavaThread = (mSession->getMainEnv() != mSession->getCurrentEnv());
	if (inJavaThread)
		mSession->lock();

	if (mManager->EnterMonitor(mAddress) == NS_OK) {
		mManager->Wait(mAddress);
		mManager->ExitMonitor(mAddress);
	}

	if (inJavaThread)
		mSession->unlock();
}

void NativeMonitor::wait(long long millis)
{
	if (mManager->EnterMonitor(mAddress) == NS_OK) {
		mManager->Wait(mAddress, PRUint32(millis));
		mManager->ExitMonitor(mAddress);
	}
}

void NativeMonitor::notify()
{
	if (mManager->EnterMonitor(mAddress) == NS_OK) {
		mManager->Notify(mAddress);
		mManager->ExitMonitor(mAddress);
	}
}

void NativeMonitor::notifyAll()
{
	if (mManager->EnterMonitor(mAddress) == NS_OK) {
		mManager->NotifyAll(mAddress);
		mManager->ExitMonitor(mAddress);
	}
}
