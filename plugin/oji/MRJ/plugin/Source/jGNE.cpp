/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
