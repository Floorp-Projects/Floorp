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
	MRJMonitor.h
	
	Provides a C++ interface to Java monitors.
	
	by Patrick C. Beard.
 */

#include "Monitor.h"

#ifndef JNI_H
#include <jni.h>
#endif

class MRJSession;

class MRJMonitor : public Monitor {
public:
	MRJMonitor(MRJSession* session, jobject monitor = NULL);
	~MRJMonitor();
	
	virtual void enter();
	virtual void exit();

	virtual void wait();
	virtual void wait(long long millis);
	virtual void notify();
	virtual void notifyAll();
	
	virtual jobject getObject();

private:
	MRJSession* mSession;
	jobject mMonitor;
	jmethodID mWaitMethod;
	jmethodID mTimedWaitMethod;
	jmethodID mNotifyMethod;
	jmethodID mNotifyAllMethod;
};
