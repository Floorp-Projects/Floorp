/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *  Simon Fraser <sfraser@netscape.com>
 */


#include "nsWindowUtils.h"



long CountWindowsOfKind(TWindowKind windowKind)
{
	// еее write me
	return 0;
}


WindowPtr GetNamedOrFrontmostWindow(TWindowKind windowKind, const char* windowName)
{
	// еее write me
	return NULL;
}


WindowPtr GetIndexedWindowOfKind(TWindowKind windowKind, TAEListIndex index)
{
	// еее write me
	return NULL;
}


TAEListIndex GetWindowIndex(TWindowKind windowKind, WindowPtr theWindow)
{
	// еее write me
	return 0;
}


void GetCleanedWindowName(WindowPtr wind, char* outName, long maxLen)
{
	// еее write me
	*outName = '\0';
}


//---------------------------------------------------------
inline void GetWindowPortRect(WindowPtr wind, Rect *outRect)
{
#if OPAQUE_TOOLBOX_STRUCTS
    ::GetPortBounds(GetWindowPort(wind), outRect);
#else
	*outRect = wind->portRect;
#endif
}

/*----------------------------------------------------------------------------
	LocalToGlobalRect 
	
	Convert a rectangle from local to global coordinates.
			
	Entry:	r = pointer to rectangle.
----------------------------------------------------------------------------*/

static void LocalToGlobalRect (Rect *r)
{
	LocalToGlobal((Point*)&r->top);
	LocalToGlobal((Point*)&r->bottom);
}

void GetWindowGlobalBounds(WindowPtr wind, Rect* outBounds)
{
	GrafPtr	curPort;
	GetWindowPortRect(wind, outBounds);
	GetPort(&curPort);
	SetPortWindowPort(wind);
	LocalToGlobalRect(outBounds);
	SetPort(curPort);
}


#pragma mark -

/*----------------------------------------------------------------------------
	WindowIsResizeable 

----------------------------------------------------------------------------*/

Boolean WindowIsResizeable(WindowPtr wind)
{
	OSStatus		status;
	UInt32		features;
	status = GetWindowFeatures(wind, &features);
	return ((status == noErr) && ((features & kWindowCanGrow) != 0));
}

/*----------------------------------------------------------------------------
	WindowIsZoomable 

----------------------------------------------------------------------------*/
Boolean WindowIsZoomable(WindowPtr wind)
{
	OSStatus		status;
	UInt32		features;
	status = GetWindowFeatures(wind, &features);
	return ((status == noErr) && ((features & kWindowCanZoom) != 0));
}

/*----------------------------------------------------------------------------
	WindowIsZoomed 

----------------------------------------------------------------------------*/
Boolean WindowIsZoomed(WindowPtr wind)
{
	Rect			r, userRect;
	GetWindowUserState(wind, &userRect);
	GetWindowPortRect(wind, &r);
	OffsetRect(&userRect, -userRect.left, -userRect.top);
	return EqualRect(&userRect, &r);
}

/*----------------------------------------------------------------------------
	WindowHasTitleBar 
	
	This stuff only works in 8.0 and later (Appearance Mgr)
----------------------------------------------------------------------------*/
Boolean WindowHasTitleBar(WindowPtr wind)
{
	OSStatus		status;
	UInt32		features;
	status = GetWindowFeatures(wind, &features);
	return ((status == noErr) && ((features & kWindowHasTitleBar) != 0));
}

/*----------------------------------------------------------------------------
	WindowIsCloseable 
	
	This stuff only works in 8.5 and later (Appearance Mgr)
----------------------------------------------------------------------------*/
Boolean WindowIsCloseable(WindowPtr wind)
{
	if ((long)GetWindowAttributes != kUnresolvedCFragSymbolAddress)
	{
		OSStatus		status;
		UInt32		attributes;
		
		status = GetWindowAttributes(wind, &attributes);
		return ((status == noErr) && ((attributes & kWindowCloseBoxAttribute) != 0));
	}
	
	return true;
}

/*----------------------------------------------------------------------------
	WindowIsModal 
	
	This stuff only works in 8.0 and later (Appearance Mgr)
----------------------------------------------------------------------------*/
Boolean WindowIsModal(WindowPtr wind)
{
	OSStatus		status;
	UInt32		features;
	status = GetWindowFeatures(wind, &features);
	return ((status == noErr) && ((features & kWindowIsModal) != 0));
}

/*----------------------------------------------------------------------------
	WindowIsFloating 
	
----------------------------------------------------------------------------*/
Boolean WindowIsFloating(WindowPtr wind)
{
	WindowClass 	windClass;
	if (GetWindowClass(wind, &windClass) == noErr)
	{
		return (windClass == kFloatingWindowClass);
	}

	return false;
}

/*----------------------------------------------------------------------------
	WindowIsModified 
	
----------------------------------------------------------------------------*/
Boolean WindowIsModified(WindowPtr wind)
{
	// еее write me
	return false;
}
