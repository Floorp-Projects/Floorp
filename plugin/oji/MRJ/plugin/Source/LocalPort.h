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
