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
