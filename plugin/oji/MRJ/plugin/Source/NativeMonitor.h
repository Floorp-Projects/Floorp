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
	NativeMonitor.h
	
	Provides a C++ interface to native monitors.
	
	by Patrick C. Beard.
 */

#include "Monitor.h"

class MRJSession;
class nsIThreadManager;

class NativeMonitor : public Monitor {
public:
	NativeMonitor(MRJSession* session, nsIThreadManager* manager, void* address = NULL);
	virtual ~NativeMonitor();

	virtual void enter();
	virtual void exit();
	
	virtual void wait();
	virtual void wait(long long millis);
	virtual void notify();
	virtual void notifyAll();

private:
	MRJSession* mSession;
	nsIThreadManager* mManager;
	void* mAddress;
};
