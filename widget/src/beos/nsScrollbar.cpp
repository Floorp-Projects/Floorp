/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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

#include "nsISupportsUtils.h"
#include "nsWidgetsCID.h"
#include "nsScrollbar.h"
#include "nsToolkit.h"
#include "nsGUIEvent.h"
#include "nsUnitConversion.h"


//-------------------------------------------------------------------------
//
// nsISupports 
//
//-------------------------------------------------------------------------
NS_IMPL_ADDREF(nsScrollbar)
NS_IMPL_RELEASE(nsScrollbar)
NS_IMPL_QUERY_INTERFACE2(nsScrollbar, nsIScrollbar, nsWindow)

//-------------------------------------------------------------------------
//
// nsScrollbar constructor
//
//-------------------------------------------------------------------------
nsScrollbar::nsScrollbar(PRBool aIsVertical) : nsWindow(), nsIScrollbar()
{
  NS_INIT_REFCNT();
  mOrientation  = (aIsVertical) ? B_VERTICAL : B_HORIZONTAL;
  mLineIncrement = 0;
  mScrollbar = 0;
  thumb = 0;
}


//-------------------------------------------------------------------------
//
// nsScrollbar destructor
//
//-------------------------------------------------------------------------
nsScrollbar::~nsScrollbar()
{
}


//-------------------------------------------------------------------------
//  Handle Destroy() here so that we properly Release() the instance
//  and mScrollbar is destroyed by nsWindow::Destroy even though
//  mScrollbar is non-zero after the call
//-------------------------------------------------------------------------
NS_METHOD nsScrollbar::Destroy() {
	nsWindow::Destroy();
	NS_RELEASE_THIS();
	return NS_OK;
}

//-------------------------------------------------------------------------
//
// Define the range settings 
//
//-------------------------------------------------------------------------
NS_METHOD nsScrollbar::SetMaxRange(PRUint32 aEndRange)
{
	float min, max;
	if(mScrollbar && mScrollbar->LockLooper())
	{
		mScrollbar->GetRange(&min, &max);
		mScrollbar->SetRange(min, float(aEndRange - thumb));
		mScrollbar->UnlockLooper();
	}
	return NS_OK;
}


//-------------------------------------------------------------------------
//
// Return the range settings 
//
//-------------------------------------------------------------------------
PRUint32 nsScrollbar::GetMaxRange(PRUint32& aRange)
{
	float startRange, endRange;
	if(mScrollbar && mScrollbar->LockLooper())
	{
		mScrollbar->GetRange(&startRange, &endRange);
		aRange = endRange + thumb;
		mScrollbar->UnlockLooper();
	}
	return NS_OK;
}


//-------------------------------------------------------------------------
//
// Set the thumb position
//
//-------------------------------------------------------------------------
NS_METHOD nsScrollbar::SetPosition(PRUint32 aPos)
{
	if(mScrollbar && mScrollbar->LockLooper())
	{
		mScrollbar->SetValue(aPos);
		mScrollbar->UnlockLooper();
	}
	return NS_OK;
}


//-------------------------------------------------------------------------
//
// Get the current thumb position.
//
//-------------------------------------------------------------------------
PRUint32 nsScrollbar::GetPosition(PRUint32& aPosition)
{
	if(mScrollbar && mScrollbar->LockLooper())
	{
		aPosition = mScrollbar->Value();
		mScrollbar->UnlockLooper();
	}
	return NS_OK;
}


//-------------------------------------------------------------------------
//
// Set the thumb size
//
//-------------------------------------------------------------------------
NS_METHOD nsScrollbar::SetThumbSize(PRUint32 aSize)
{
	float min, max, smallStep, bigStep;
	if(mScrollbar && mScrollbar->LockLooper())
	{
		mScrollbar->GetRange(&min, &max);
		max = max + thumb - aSize;
//		thumb = aSize;
		mScrollbar->SetRange(min, max);
		mScrollbar->SetProportion(max == min ? 1 : (aSize / (max > min ? max - min : min - max)));
		mScrollbar->GetSteps(&smallStep, &bigStep);
		mScrollbar->SetSteps(smallStep, aSize);
		mScrollbar->UnlockLooper();
	}
	return NS_OK;
}


//-------------------------------------------------------------------------
//
// Get the thumb size
//
//-------------------------------------------------------------------------
NS_METHOD nsScrollbar::GetThumbSize(PRUint32& aSize)
{
  aSize = thumb;
  return NS_OK;
}


//-------------------------------------------------------------------------
//
// Set the line increment for this scrollbar
//
//-------------------------------------------------------------------------
NS_METHOD nsScrollbar::SetLineIncrement(PRUint32 aSize)
{
	float smallStep, bigStep;
	if(mScrollbar && mScrollbar->LockLooper())
	{
		mScrollbar->GetSteps(&smallStep, &bigStep);
		mScrollbar->SetSteps(aSize, bigStep); 
		mScrollbar->UnlockLooper();
	}
	return NS_OK;
}


//-------------------------------------------------------------------------
//
// Get the line increment for this scrollbar
//
//-------------------------------------------------------------------------
NS_METHOD nsScrollbar::GetLineIncrement(PRUint32& aSize)
{
	float smallStep, bigStep;
	aSize = 0;
	if(mScrollbar && mScrollbar->LockLooper())
	{
		mScrollbar->GetSteps(&smallStep, &bigStep);
		aSize = uint32(smallStep);
		mScrollbar->UnlockLooper();
	}
	return NS_OK;
}


//-------------------------------------------------------------------------
//
// Set all scrolling parameters
//
//-------------------------------------------------------------------------
NS_METHOD nsScrollbar::SetParameters(PRUint32 aMaxRange, PRUint32 aThumbSize,
                                PRUint32 aPosition, PRUint32 aLineIncrement)
{
	SetMaxRange(aMaxRange);
	SetThumbSize(aThumbSize);
	SetPosition(aPosition);
	SetLineIncrement(aLineIncrement);
	return NS_OK;
}

//-------------------------------------------------------------------------
//
// paint message. Don't send the paint out
//
//-------------------------------------------------------------------------
PRBool nsScrollbar::OnPaint(nsRect &r)
{
    return PR_FALSE;
}


PRBool nsScrollbar::OnResize(nsRect &aWindowRect)
{
    return PR_FALSE;
}


//-------------------------------------------------------------------------
//
// Deal with scrollbar messages (actually implemented only in nsScrollbar)
//
//-------------------------------------------------------------------------
PRBool nsScrollbar::OnScroll()
{
    PRBool result = PR_TRUE;
	int32 newpos;
	if(mEventCallback)
	{
		if(mScrollbar->LockLooper())
		{
			if(mScrollbar->GetPosition(newpos))
			{
				nsScrollbarEvent event;
				InitEvent(event, NS_SCROLLBAR_POS);
				event.widget = (nsWindow *)this;
				event.position = newpos;
				result = ConvertStatus((*mEventCallback)(&event));
		
//				if(newpos != event.position)
//					SetPosition(newpos = event.position);
			}
			mScrollbar->UnlockLooper();
		}
	}

	return result;
}


//-------------------------------------------------------------------------
//
// get position/dimensions
//
//-------------------------------------------------------------------------

NS_METHOD nsScrollbar::GetBounds(nsRect &aRect)
{
  return nsWindow::GetBounds(aRect);
}

BView *nsScrollbar::CreateBeOSView()
{
	BRect	temp;

	// work around stupit BeOS bug
	if(mOrientation == B_VERTICAL)
		temp.Set(0, 0, B_V_SCROLL_BAR_WIDTH, B_H_SCROLL_BAR_HEIGHT * 2);
	else
		temp.Set(0, 0, B_V_SCROLL_BAR_WIDTH * 2, B_H_SCROLL_BAR_HEIGHT);

	mScrollbar = new nsScrollbarBeOS(this, temp, "", NULL, 0, 0, mOrientation);

	return mScrollbar;
}

//-------------------------------------------------------------------------
// Sub-class of BeOS Scrollbar
//-------------------------------------------------------------------------
nsScrollbarBeOS::nsScrollbarBeOS( nsIWidget *aWidgetWindow, BRect aFrame, 
    const char *aName, BView *aTarget, float aMin, float aMax, 
    orientation aOrientation )
  : BScrollBar( aFrame, aName, aTarget, aMin, aMax, aOrientation ),
    nsIWidgetStore( aWidgetWindow ), first(true), sbposchanged(false)
{
}

void nsScrollbarBeOS::ValueChanged(float newValue)
{
	if(first)
	{
		first = false;
		return;
	}

	sbpos = newValue;
	sbposchanged = true;

	nsWindow	*w = (nsWindow *)GetMozillaWidget();
	nsToolkit	*t;
	if(w && (t = w->GetToolkit()) != 0)
	{
		MethodInfo *info = new MethodInfo(w, w, nsWindow::ONSCROLL);
		t->CallMethodAsync(info);
		NS_RELEASE(t);
	}
}

bool nsScrollbarBeOS::GetPosition(int32 &p)
{
	if(! sbposchanged)
		return false;
	p = (int32)sbpos;
	sbposchanged = false;
	return true;
}
