/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

#include "nsChildWindow.h"
#include "nsCOMPtr.h"
#include "nsRegionMac.h"

//-------------------------------------------------------------------------
//
//-------------------------------------------------------------------------

nsChildWindow::nsChildWindow() : nsWindow()
{
	strcpy(gInstanceClassName, "nsChildWindow");
	mClipChildren = PR_FALSE;
	mClipSiblings = PR_FALSE;
}


nsChildWindow::~nsChildWindow()
{
}

//-------------------------------------------------------------------------
//
//
//-------------------------------------------------------------------------

nsresult nsChildWindow::StandardCreate(nsIWidget *aParent,
                      const nsRect &aRect,
                      EVENT_CALLBACK aHandleEventFunction,
                      nsIDeviceContext *aContext,
                      nsIAppShell *aAppShell,
                      nsIToolkit *aToolkit,
                      nsWidgetInitData *aInitData,
                      nsNativeWidget aNativeParent)	// should always be nil here
{
	if (aInitData)
	{
		mClipChildren = aInitData->clipChildren;
		mClipSiblings = aInitData->clipSiblings;
	}

	return Inherited::StandardCreate(aParent,
                      aRect,
                      aHandleEventFunction,
                      aContext,
                      aAppShell,
                      aToolkit,
                      aInitData,
                      aNativeParent);
}

//-------------------------------------------------------------------------
//
//
//-------------------------------------------------------------------------

void nsChildWindow::CalcWindowRegions()
{
	Inherited::CalcWindowRegions();

	// clip the siblings out of the window region and visRegion 
	if (mClipSiblings && mParent) {
		// need to walk the siblings backwards, to get clipping right.
		nsCOMPtr<nsIBidirectionalEnumerator> siblings = getter_AddRefs((nsIBidirectionalEnumerator*)mParent->GetChildren());
		if (siblings && NS_SUCCEEDED(siblings->Last())) {
			StRegionFromPool siblingRgn;
			if (siblingRgn == nsnull)
				return;
			do {
				// when we reach ourself, stop clipping.
				nsCOMPtr<nsISupports> item;
				if (NS_FAILED(siblings->CurrentItem(getter_AddRefs(item))) || item == this)
					break;
				
				nsCOMPtr<nsIWidget> sibling(do_QueryInterface(item));
				PRBool visible;
				sibling->IsVisible(visible);
				if (visible) {	// don't clip if not visible.
					// get sibling's bounds in parent's coordinate system.
					nsRect siblingRect;
					sibling->GetBounds(siblingRect);
					
					// transform from parent's coordinate system to widget coordinates.
					siblingRect.MoveBy(-mBounds.x, -mBounds.y);

					Rect macRect;
					::SetRect(&macRect, siblingRect.x, siblingRect.y, siblingRect.XMost(), siblingRect.YMost());
					::RectRgn(siblingRgn, &macRect);
					::DiffRgn(mWindowRegion, siblingRgn, mWindowRegion);
					::DiffRgn(mVisRegion, siblingRgn, mVisRegion);
				}
			} while (NS_SUCCEEDED(siblings->Prev()));
		}
	}
}
