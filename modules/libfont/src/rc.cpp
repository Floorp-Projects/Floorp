/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
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
 * rc.cpp (RenderingContextObject)
 *
 * C++ implementation of the (rc) RenderingContextObject
 *
 * dp Suresh <dp@netscape.com>
 */


#include "rc.h"

// Constructor and Destructor
RenderingContextObject::
RenderingContextObject(jint majorType, jint minorType, void **arg, jsize nargs)
{
	rcData.majorType = NF_RC_INVALID;
	rcData.minorType = 0;
	rcData.displayer_data = NULL;
	if (majorType == NF_RC_DIRECT)
	{
		rcData.majorType = majorType;
		rcData.minorType = minorType;
#if defined(XP_UNIX)
		rcData.t.directRc.display = NULL;
		rcData.t.directRc.d = NULL;
		rcData.t.directRc.gc = NULL;
		rcData.t.directRc.mask = 0;
		if (nargs >= 1) rcData.t.directRc.display = arg[0];
		if (nargs >= 2) rcData.t.directRc.d = arg[1];
		if (nargs >= 3) rcData.t.directRc.gc = arg[2];
		if (nargs >= 4) rcData.t.directRc.mask = (jint)arg[3];
#elif defined(XP_WIN) || defined(XP_OS2)
		rcData.t.directRc.dc = NULL;
		if (nargs >= 1) rcData.t.directRc.dc = arg[0];
#elif defined(XP_MAC)
		rcData.t.directRc.port = NULL;
		if (nargs >= 1) rcData.t.directRc.port = arg[0];
#endif
	}
}


RenderingContextObject::~RenderingContextObject()
{
}

jint
RenderingContextObject::GetMajorType(void)
{
	return (rcData.majorType);
}

jint
RenderingContextObject::GetMinorType(void)
{
	return (rcData.minorType);
}

jint
RenderingContextObject::IsEquivalent(jint majorType, jint minorType)
{
	return ((minorType != NF_RC_ALWAYS_DIFFERENT) &&
			(majorType == rcData.majorType) &&
			(minorType == rcData.minorType));
}

//
// This will be used only by the Displayer & Consumers
//
struct rc_data
RenderingContextObject::GetPlatformData(void)
{
	return (rcData);
}

jint
RenderingContextObject::SetPlatformData(struct rc_data *newRcData)
{
	jint ret = -1;
	if (newRcData)
	{
		rcData = *newRcData;
		ret = 0;
	}
	return (ret);
}
