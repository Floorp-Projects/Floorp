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
#include <Memory.h>
#include <LowMem.h>
#include <TextUtils.h>

/**
 * A 68K jump vector.
 */
#pragma options align=mac68k

struct Jump {
	unsigned short		jmp;
	UniversalProcPtr	addr;
};
 
#pragma options align=reset

static void GNEFilter(EventRecord *event, Boolean* result);

static RoutineDescriptor theGNEFilterDescriptor = BUILD_ROUTINE_DESCRIPTOR(uppGetNextEventFilterProcInfo, GNEFilter);
static Jump* theGNEFilterJump;
static GetNextEventFilterUPP theOldGNEFilterUPP = NULL;
static EventFilterProcPtr  theEventFilter = NULL;

static Str63 theAppName;

OSStatus InstallEventFilter(EventFilterProcPtr filter)
{
	if (theEventFilter == NULL) {
		theEventFilter = filter;

		// record the current application's name.
		StringPtr currentAppName = LMGetCurApName();
		::BlockMoveData(currentAppName, theAppName, 1 + currentAppName[0]);

		// allocate a jump vector in the System heap, so it will be retained after termination.
		if (theGNEFilterJump == NULL) {
			theGNEFilterJump = (Jump*) NewPtrSys(sizeof(Jump));
			if (theGNEFilterJump == NULL)
				return MemError();
			
			theGNEFilterJump->jmp = 0x4EF9;
			theGNEFilterJump->addr = &theGNEFilterDescriptor;
			
			// get previous event filter routine.
			theOldGNEFilterUPP = LMGetGNEFilter();
			LMSetGNEFilter(GetNextEventFilterUPP(theGNEFilterJump));
		} else {
			// our previously allocated Jump is still installed, use it.
			theOldGNEFilterUPP = theGNEFilterJump->addr;
			theGNEFilterJump->addr = &theGNEFilterDescriptor;
		}
		
		return noErr;
	}
	return paramErr;
}

OSStatus RemoveEventFilter()
{
	if (theEventFilter != NULL) {
		// It's only truly safe to remove our filter, if nobody else installed one after us.
		if (LMGetGNEFilter() == GetNextEventFilterUPP(theGNEFilterJump)) {
			// can safely restore the old filter.
			LMSetGNEFilter(theOldGNEFilterUPP);
			DisposePtr(Ptr(theGNEFilterJump));
			theGNEFilterJump = NULL;
		} else {
			// modify the jump instruction to point to the previous filter.
			theGNEFilterJump->addr = theOldGNEFilterUPP;
		}
		theOldGNEFilterUPP = NULL;
		theEventFilter = NULL;
		return noErr;
	}
	return paramErr;
}

static void GNEFilter(EventRecord *event, Boolean* result)
{
	// call next filter in chain first.
	if (theOldGNEFilterUPP != NULL)
		CallGetNextEventFilterProc(theOldGNEFilterUPP, event, result);

	// now, let the filter proc have a crack at the event.
	if (*result) {
		// only call the filter if called in the current application's context.
		/* if (::EqualString(theAppName, LMGetCurApName(), true, true)) */
		{
			// prevent recursive calls to the filter.
			static Boolean inFilter = false;
			if (! inFilter) {
				inFilter = true;
				Boolean filteredEvent = theEventFilter(event);
				if (filteredEvent) {
					// consume the event by making it a nullEvent.
					event->what = nullEvent;
					*result = false;
				}
				inFilter = false;
			}
		}
	}
}
