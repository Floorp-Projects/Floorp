/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is the Mozilla OS/2 libraries.
 *
 * The Initial Developer of the Original Code is John Fairhurst,
 * <john_fairhurst@iname.com>.  Portions created by John Fairhurst are
 * Copyright (C) 1999 John Fairhurst. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *   Pierre Phaneuf <pp@ludusdesign.com>
 */

#include "nsScrollbar.h"
#include "nsToolkit.h"
#include "nsGUIEvent.h"
#include "nsUnitConversion.h"


NS_IMPL_ADDREF(nsScrollbar)
NS_IMPL_RELEASE(nsScrollbar)

//-------------------------------------------------------------------------
//
// nsScrollbar constructor
//
//-------------------------------------------------------------------------
nsScrollbar::nsScrollbar(PRBool aIsVertical) : nsWindow(), nsIScrollbar()
{
  NS_INIT_REFCNT();
  mPositionFlag = aIsVertical ? SBS_VERT : SBS_HORZ;
  mScaleFactor   = 1.0f;
  mLineIncrement = 0;
  mThumbSize = 0;
  mMaxRange = 0;
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
//
// Query interface implementation
//
//-------------------------------------------------------------------------
nsresult nsScrollbar::QueryInterface(const nsIID& aIID, void** aInstancePtr)
{
    nsresult result = nsWindow::QueryInterface(aIID, aInstancePtr);

    static NS_DEFINE_IID(kInsScrollbarIID, NS_ISCROLLBAR_IID);
    if (result == NS_NOINTERFACE && aIID.Equals(kInsScrollbarIID)) {
        *aInstancePtr = (void*) ((nsIScrollbar*)this);
        NS_ADDREF_THIS();
        result = NS_OK;
    }

    return result;
}


//-------------------------------------------------------------------------
//
// Define the range settings 
//
//-------------------------------------------------------------------------
#define ROUND_D(u) NSToIntRound( (u) / mScaleFactor)
#define ROUND_U(u) (PRUint32) NSToIntRound( (u) * mScaleFactor)

void nsScrollbar::SetThumbRange( PRUint32 aEndRange, PRUint32 aSize)
{
  // get current position

  PRUint32 pos;
  GetPosition( pos);

  // set scale factor dependent on max range
  if( aEndRange > 32000)
    mScaleFactor = aEndRange / 32000.0f;

  // set new sizes
  mMaxRange = aEndRange;
  mThumbSize = aSize;

  // set range & pos
  mOS2Toolkit->SendMsg( mWnd, SBM_SETSCROLLBAR,
                        MPFROMSHORT( ROUND_D(pos)),
                        MPFROM2SHORT( 0, ROUND_D(mMaxRange-mThumbSize)));

  // set thumb
  mOS2Toolkit->SendMsg( mWnd, SBM_SETTHUMBSIZE,
                        MPFROM2SHORT( ROUND_D(mThumbSize),
                                      ROUND_D(mMaxRange))); 
}

NS_METHOD nsScrollbar::SetMaxRange(PRUint32 aEndRange)
{
    SetThumbRange( aEndRange, mThumbSize);
    return NS_OK;
}


//-------------------------------------------------------------------------
//
// Return the range settings 
//
//-------------------------------------------------------------------------
PRUint32 nsScrollbar::GetMaxRange(PRUint32& aRange)
{
  aRange = ROUND_U(mMaxRange);
  return NS_OK;
}


//-------------------------------------------------------------------------
//
// Set the thumb position
//
//-------------------------------------------------------------------------
NS_METHOD nsScrollbar::SetPosition(PRUint32 aPos)
{
  mOS2Toolkit->SendMsg( mWnd, SBM_SETPOS, MPFROMSHORT(ROUND_D( aPos)));
  return NS_OK;
}


//-------------------------------------------------------------------------
//
// Get the current thumb position.
//
//-------------------------------------------------------------------------
PRUint32 nsScrollbar::GetPosition(PRUint32& aPosition)
{
  MRESULT rc = mOS2Toolkit->SendMsg( mWnd, SBM_QUERYPOS);
  aPosition = ROUND_U( SHORT1FROMMR(rc));
  return NS_OK;
}


//-------------------------------------------------------------------------
//
// Set the thumb size
//
//-------------------------------------------------------------------------
NS_METHOD nsScrollbar::SetThumbSize(PRUint32 aSize)
{
  SetThumbRange( mMaxRange, aSize);
  return NS_OK;
}


//-------------------------------------------------------------------------
//
// Get the thumb size
//
//-------------------------------------------------------------------------
NS_METHOD nsScrollbar::GetThumbSize(PRUint32& aSize)
{
  aSize = ROUND_U(mThumbSize);
  return NS_OK;
}


//-------------------------------------------------------------------------
//
// Set the line increment for this scrollbar
//
//-------------------------------------------------------------------------
NS_METHOD nsScrollbar::SetLineIncrement(PRUint32 aSize)
{
  mLineIncrement = NSToIntRound(aSize / mScaleFactor);
  return NS_OK;
}


//-------------------------------------------------------------------------
//
// Get the line increment for this scrollbar
//
//-------------------------------------------------------------------------
NS_METHOD nsScrollbar::GetLineIncrement(PRUint32& aSize)
{
  aSize = (PRUint32)NSToIntRound(mLineIncrement * mScaleFactor);
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
  SetThumbRange( aMaxRange, aThumbSize);
  SetPosition( aPosition);
  SetLineIncrement( aLineIncrement);
  return NS_OK;
}


//-------------------------------------------------------------------------
//
// Deal with scrollbar messages (actually implemented only in nsScrollbar)
//
//-------------------------------------------------------------------------
PRBool nsScrollbar::OnScroll( ULONG msgid, MPARAM mp1, MPARAM mp2)
{
    PRBool result = PR_TRUE;

    USHORT   scrollCode = SHORT2FROMMP(mp2);
    int      msg = 0;

    BOOL hasEvent = mEventListener != nsnull || mEventCallback != nsnull;

    PRUint32 newpos, li, ts;

    GetPosition( newpos);
    GetLineIncrement( li);
    GetThumbSize( ts);

    switch (scrollCode) {

        // scroll one line right or down
        // SB_LINERIGHT and SB_LINEDOWN are actually the same value
        //case SB_LINERIGHT: 
        case SB_LINEDOWN: 
        {
            newpos += li;
            msg = NS_SCROLLBAR_LINE_NEXT;
            break;
        }


        // scroll one line left or up
        //case SB_LINELEFT:
        case SB_LINEUP: 
        {
            if( newpos > li) newpos -= li;
            else newpos = 0;
            msg = NS_SCROLLBAR_LINE_PREV;
            break;
        }

        // Scrolls one page right or down
        // case SB_PAGERIGHT:
        case SB_PAGEDOWN: 
        {
            newpos += (ts - li);
            msg = NS_SCROLLBAR_PAGE_NEXT;
            break;
        }

        // Scrolls one page left or up.
        //case SB_PAGELEFT:
        case SB_PAGEUP: 
        {
            PRUint32 delta;
            delta = ts - li;
            if( newpos > delta) newpos -= delta;
            else newpos = 0;
            msg = NS_SCROLLBAR_PAGE_PREV;
            break;
        }

        case SB_SLIDERTRACK:
        {
            newpos = ROUND_U( SHORT1FROMMP(mp2));
            msg = NS_SCROLLBAR_POS;
            break;
        }
    }

    if( msg != 0)
    {
        // Ensure newpos is sensible
        if( newpos > (mMaxRange - mThumbSize))
            newpos = mMaxRange - mThumbSize;
    
        // if there are listeners, give them a chance to alter newpos
        if( mEventListener != nsnull || mEventCallback != nsnull)
        {
            nsScrollbarEvent event;
            InitEvent( event, msg);
            event.eventStructType = NS_SCROLLBAR_EVENT;
            event.position = newpos;
            DispatchWindowEvent( &event);
            NS_RELEASE( event.widget);

            // Ensure position is still sensible
            newpos = event.position;
            if( newpos > (mMaxRange - mThumbSize))
                newpos = mMaxRange - mThumbSize;
        }
    
        // Now move the scrollbar
        SetPosition( newpos);
    }

  return result;
}


//-------------------------------------------------------------------------
//
// return the window class name and initialize the class if needed
//
//-------------------------------------------------------------------------
PCSZ nsScrollbar::WindowClass()
{
    /* Can't return (PSZ)WC_SCROLLBAR since we need to return a real string
     * as some string manipulation might be done on the returned value.
     * The return from WinQueryClassName for a WC_SCROLLBAR is "#8", so that is
     * what we'll return.  Value makes since since WC_SCROLLBAR is 0xFFFF0008.
     */
    return WC_SCROLLBAR_STRING;
}


//-------------------------------------------------------------------------
//
// return window styles
//
//-------------------------------------------------------------------------
ULONG nsScrollbar::WindowStyle()
{
    return mPositionFlag | SBS_THUMBSIZE | SBS_AUTOTRACK | (BASE_CONTROL_STYLE & (~WS_TABSTOP));
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

PRBool nsScrollbar::OnPaint()
{
   return PR_FALSE;
}
