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

#if 0 //¥¥¥Êwill be enabled later - pierre

#include "nsChildWindow.h"
#include "nsCOMPtr.h"




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

void nsChildWindow::CalcWindowRegions(nsIWidget* aParent, nsRect& aBounds)
{
	Inherited::CalcWindowRegions(aParent, aBounds);

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
			::DisposeRgn(childRgn);
		}
	}

	// clip the siblings out of the visRgn
	if (mClipSiblings && mParent)
	{
		RgnHandle siblingRgn = ::NewRgn();
		if (!siblingRgn) return;

		nsCOMPtr<nsIEnumerator> children(dont_AddRef(mParent->GetChildren()));
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

					if (childWindow != this)	// don't clip myself
					{
						nsRect childRect;
						childWindow->GetBounds(childRect);

						Rect macRect;
						::SetRect(&macRect, childRect.x, childRect.y, childRect.XMost(), childRect.YMost());
						::RectRgn(siblingRgn, &macRect);
						::DiffRgn(mVisRegion, siblingRgn, mVisRegion);
					}
				}
			} while (NS_SUCCEEDED(children->Next()));
			::DisposeRgn(siblingRgn);
		}
	}
}

#endif //¥¥¥Êwill be enabled later - pierre

