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
