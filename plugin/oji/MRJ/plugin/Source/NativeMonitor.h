/*
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
