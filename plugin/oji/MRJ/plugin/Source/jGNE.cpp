/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
	jGNE.cpp
	
	Provides a generalized jGNE filtering service.
	
	by Patrick C. Beard.
 */

#include "jGNE.h"

#include <MixedMode.h>
#include <LowMem.h>

static void GNEFilter(EventRecord *event, Boolean* result);

static RoutineDescriptor theGNEFilterUPP = BUILD_ROUTINE_DESCRIPTOR(uppGetNextEventFilterProcInfo, GNEFilter);
static GetNextEventFilterUPP theGetNextEventFilterUPP = NULL;
static EventFilterProcPtr  theEventFilter = NULL;

OSStatus InstallEventFilter(EventFilterProcPtr filter)
{
	theEventFilter = filter;

	// get previous event filter routine.
	theGetNextEventFilterUPP = LMGetGNEFilter();
	LMSetGNEFilter(&theGNEFilterUPP);
	
	return noErr;
}

OSStatus RemoveEventFilter()
{
	// should really check to see if somebody else has installed a filter proc as well.
	LMSetGNEFilter(theGetNextEventFilterUPP);
	theGetNextEventFilterUPP = NULL;
	theEventFilter = NULL;
	return noErr;
}

static void GNEFilter(EventRecord *event, Boolean* result)
{
	// call next filter in chain first.
	if (theGetNextEventFilterUPP)
		CallGetNextEventFilterProc(theGetNextEventFilterUPP, event, result);

	// now, let the filter proc have a crack at the event.
	if (*result)
		*result = !theEventFilter(event);
}
