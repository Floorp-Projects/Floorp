/* ----- BEGIN LICENSE BLOCK -----
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the MRJ Carbon OJI Plugin.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corp.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):  Patrick C. Beard <beard@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ----- END LICENSE BLOCK ----- */

/*
	EventFilter.cpp
	
	Provides a generalized event filtering service.
	
	Patches WaitNextEvent for events, and MenuSelect for menus.
	
	by Patrick C. Beard.
 */

#include "EventFilter.h"

#include <MixedMode.h>
#include <Memory.h>
#include <LowMem.h>
#include <Menus.h>
#include <Patches.h>
#include <Traps.h>

enum {
	uppWaitNextEventProcInfo = kPascalStackBased
		| RESULT_SIZE(SIZE_CODE(sizeof(Boolean)))
		| STACK_ROUTINE_PARAMETER(1, SIZE_CODE(sizeof(EventMask)))
		| STACK_ROUTINE_PARAMETER(2, SIZE_CODE(sizeof(EventRecord*)))
		| STACK_ROUTINE_PARAMETER(3, SIZE_CODE(sizeof(UInt32)))
		| STACK_ROUTINE_PARAMETER(4, SIZE_CODE(sizeof(RgnHandle))),
	uppMenuSelectProcInfo = kPascalStackBased
		| RESULT_SIZE(SIZE_CODE(sizeof(long)))
		| STACK_ROUTINE_PARAMETER(1, SIZE_CODE(sizeof(Point)))
};

static pascal Boolean NewWaitNextEvent(EventMask eventMask, EventRecord* event, UInt32 sleep, RgnHandle mouseRgn);
static pascal long NewMenuSelect(Point where);

enum {
	kIsToolboxTrap = (1 << 11)
};

inline TrapType getTrapType(UInt16 trapNum)
{
	return ((trapNum & kIsToolboxTrap) != 0 ? kToolboxTrapType : kOSTrapType);
}

static UniversalProcPtr SwapTrapAddress(UInt16 trapNum, UniversalProcPtr newTrapAddress)
{
	TrapType type = getTrapType(trapNum);
	UniversalProcPtr oldTrapAddress = NGetTrapAddress(trapNum, type);
	NSetTrapAddress(newTrapAddress, trapNum, type);
	return oldTrapAddress;
}

struct Patch {
	UInt16 trap;
	RoutineDescriptor descriptor;
	UniversalProcPtr original;

	void Install() { original = SwapTrapAddress(trap, &descriptor); }
	void Remove() { SwapTrapAddress(trap, original); original = NULL; }
};

static Patch WaitNextEventPatch = {
	_WaitNextEvent,
	BUILD_ROUTINE_DESCRIPTOR(uppWaitNextEventProcInfo, NewWaitNextEvent),
	NULL
};

static Patch MenuSelectPatch = {
	_MenuSelect,
	BUILD_ROUTINE_DESCRIPTOR(uppMenuSelectProcInfo, NewMenuSelect),
	NULL
};

static EventFilterProcPtr theEventFilter;
static MenuFilterProcPtr theMenuFilter;

OSStatus InstallEventFilters(EventFilterProcPtr eventFilter, MenuFilterProcPtr menuFilter)
{
	if (theEventFilter == NULL) {
		theEventFilter = eventFilter;
		theMenuFilter = menuFilter;

		// Patch WNE, which will be used to filter events.
		WaitNextEventPatch.Install();
		
		// Patch MenuSelect, which will be used to filter menu selections.
		MenuSelectPatch.Install();

		return noErr;
	}
	return paramErr;
}

OSStatus RemoveEventFilters()
{
	if (theEventFilter != NULL) {
		WaitNextEventPatch.Remove();
		MenuSelectPatch.Remove();
		
		theEventFilter = NULL;
		theMenuFilter = NULL;
		
		return noErr;
	}
	return paramErr;
}

static pascal Boolean NewWaitNextEvent(EventMask eventMask, EventRecord* event, UInt32 sleep, RgnHandle mouseRgn)
{
	Boolean gotEvent = CALL_FOUR_PARAMETER_UPP(WaitNextEventPatch.original, uppWaitNextEventProcInfo, eventMask, event, sleep, mouseRgn);
	if (true) {
		// prevent recursive calls to the filter.
		static Boolean inFilter = false;
		if (! inFilter) {
			inFilter = true;
			Boolean filteredEvent = theEventFilter(event);
			if (filteredEvent) {
				// consume the event by making it a nullEvent.
				event->what = nullEvent;
				gotEvent = false;
			}
			inFilter = false;
		}
	}
	return gotEvent;
}

static pascal long NewMenuSelect(Point where)
{
	long menuSelection = CALL_ONE_PARAMETER_UPP(MenuSelectPatch.original, uppMenuSelectProcInfo, where);
	if (menuSelection != 0) {
		Boolean filteredEvent = theMenuFilter(menuSelection);
		if (filteredEvent) {
			// consume the menu selection by zeroing it out.
			menuSelection = 0;
		}
	}
	return menuSelection;
}
