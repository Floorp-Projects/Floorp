/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
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
 * The purpose of this file is to help phase out XP_ library
 * from the image library. In general, XP_ data structures and
 * functions will be replaced with the PR_ or PL_ equivalents.
 * In cases where the PR_ or PL_ equivalents don't yet exist,
 * this file (and its header equivalent) will play the role 
 * of the XP_ library.
 */
#include "xpcompat.h"
#if TARGET_CARBON
# include <time.h>
#endif
#include <stdlib.h>
#include "prlog.h"
#include "prmem.h"
#include "plstr.h"
#include "ilISystemServices.h"
#include "nsCRT.h"

extern ilISystemServices *il_ss;

char *XP_GetString(int i)
{
  return ("XP_GetString replacement needed");
}

#ifdef XP_MAC

#ifndef UNIXMINUSMACTIME
#define UNIXMINUSMACTIME 2082844800UL
#endif

#include <OSUtils.h>

static void MyReadLocation(MachineLocation * loc)
{
	static MachineLocation storedLoc;	// InsideMac, OSUtilities, page 4-20
	static Boolean didReadLocation = FALSE;
	if (!didReadLocation)
	{	
		ReadLocation(&storedLoc);
		didReadLocation = TRUE;
	}
	*loc = storedLoc;
}

// current local time = GMTDelta() + GMT
// GMT = local time - GMTDelta()
static long GMTDelta()
{
	MachineLocation loc;
	long gmtDelta;
	
	MyReadLocation(&loc);
	gmtDelta = loc.u.gmtDelta & 0x00FFFFFF;
	if ((gmtDelta & 0x00800000) != 0)
		gmtDelta |= 0xFF000000;
	return gmtDelta;
}

// This routine simulates stdclib time(), time in seconds since 1.1.1970
// The time is in GMT
time_t GetTimeMac()
{
	unsigned long maclocal;
	// Get Mac local time
	GetDateTime(&maclocal); 
	// Get Mac GMT	
	maclocal -= GMTDelta();
	// return unix GMT
	return (maclocal - UNIXMINUSMACTIME);
}

// Returns the GMT times
time_t Mactime(time_t *timer)
{
	time_t t = GetTimeMac();
	if (timer != NULL)
		*timer = t;
	return t;
}
#endif /* XP_MAC */



NS_EXPORT void*
IL_SetTimeout(TimeoutCallbackFunction func, void * closure, PRUint32 msecs)
{
    return il_ss->SetTimeout((ilTimeoutCallbackFunction)func,
                             closure, msecs);
}

NS_EXPORT void
IL_ClearTimeout(void *timer_id)
{
    il_ss->ClearTimeout(timer_id);
}


