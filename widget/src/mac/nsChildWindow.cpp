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

#if 0	//¥REVISIT: this code can probably be removed: the children 
		// are already clipped out in nsWindow::CalcWindowRegions()
	// clip the children out of the visRgn
	if (mClipChildren)
	{
		RgnHandle childRgn = ::NewRgn();
		if (!childRgn) return;

		nsCOMPtr<nsIEnumerator> children(dont_AddRef(GetChildren()));
		if (children)
		{
			children->First();
			do
			{
				nsISupports* child;
				if (NS_SUCCEEDED(children->CurrentItem(&child)))
				{
					nsWindow* childWindow = static_cast<nsWindow*>(child);
					NS_RELEASE(child);

					nsRect childRect;
					childWindow->GetBounds(childRect);

					Rect macRect;
					::SetRect(&macRect, childRect.x, childRect.y, childRect.XMost(), childRect.YMost());
					::RectRgn(childRgn, &macRect);
					::DiffRgn(mVisRegion, childRgn, mVisRegion);
				}
			} while (NS_SUCCEEDED(children->Next()));
		}
		::DisposeRgn(childRgn);
	}
#endif

	// clip the siblings out of the window region and visRegion 
	if (mClipSiblings && mParent)
	{
		nsCOMPtr<nsIEnumerator> children(dont_AddRef(mParent->GetChildren()));
		if (children)
		{
			StRegionFromPool siblingRgn;
			if (siblingRgn != nsnull) {
				children->First();
				do
				{
					nsISupports* child;
					if (NS_SUCCEEDED(children->CurrentItem(&child)))
					{
						nsWindow* childWindow = static_cast<nsWindow*>(child);
						NS_RELEASE(child);
						
						PRBool visible;
						childWindow->IsVisible(visible);

						if (visible && childWindow != this)	// don't clip myself
						{
							nsRect childRect;
							childWindow->GetBounds(childRect);

							Rect macRect;
							::SetRect(&macRect, childRect.x, childRect.y, childRect.XMost(), childRect.YMost());
							::RectRgn(siblingRgn, &macRect);
							::DiffRgn(mWindowRegion, siblingRgn, mWindowRegion);
							::DiffRgn(mVisRegion, siblingRgn, mVisRegion);
						}
					}
				} while (NS_SUCCEEDED(children->Next()));
			}
		}
	}
}

