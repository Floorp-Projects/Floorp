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
	LocalPort.h
	
	Simple utility class to put a Quickdraw GrafPort into a known state.
	
	by Patrick C. Beard.
 */

#pragma once

#include <Quickdraw.h>

class LocalPort {
public:
	LocalPort(GrafPtr port)
	{
		fPort = port;
		fOrigin.h = fOrigin.v = 0;
	}
	
	LocalPort(GrafPtr port, Point origin)
	{
		fPort = port;
		fOrigin.h = origin.h;
		fOrigin.v = origin.v;
	}

	void Enter();
	void Exit();

private:
	GrafPtr fPort;
	Point fOrigin;
	GrafPtr fOldPort;
	Point fOldOrigin;
};
