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
	Monitor.h
	
	Abstract class representing monitors.
	
	by Patrick C. Beard.
 */

#pragma once

class Monitor {
public:
	virtual ~Monitor() {}
	
	virtual void enter() = 0;
	virtual void exit() = 0;
	
	virtual void wait() = 0;
	virtual void wait(long long millis) = 0;
	virtual void notify() = 0;
	virtual void notifyAll() = 0;
};
