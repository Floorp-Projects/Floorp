/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
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
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsChildWindow.h"
#include "nsCOMPtr.h"
#include "nsRegionPool.h"

//-------------------------------------------------------------------------
//
//-------------------------------------------------------------------------

nsChildWindow::nsChildWindow() : nsWindow()
{
	WIDGET_SET_CLASSNAME("nsChildWindow");
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
	if (mClipSiblings && mParent && !mIsTopWidgetWindow) {
		// need to walk the siblings backwards, to get clipping right.
		nsCOMPtr<nsIBidirectionalEnumerator> siblings = getter_AddRefs((nsIBidirectionalEnumerator*)mParent->GetChildren());
		if (siblings && NS_SUCCEEDED(siblings->Last())) {
			StRegionFromPool siblingRgn;
			if (siblingRgn == nsnull)
				return;
			do {
				// when we reach ourself, stop clipping.
				nsCOMPtr<nsISupports> item;
				if (NS_FAILED(siblings->CurrentItem(getter_AddRefs(item))) ||
				    item == NS_STATIC_CAST(nsIWidget*, this))
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
