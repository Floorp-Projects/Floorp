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

#include "nsScrollbar.h"
#include "nsToolkit.h"
#include "nsGUIEvent.h"
#include <windows.h>
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
  mPositionFlag  = (aIsVertical) ? SBS_VERT : SBS_HORZ;
  mScaleFactor   = 1.0f;
  mLineIncrement = 0;
  mBackground    = ::GetSysColor(COLOR_SCROLLBAR);

  //prevent resource leaks..
  if (mBrush)
    ::DeleteObject(mBrush);

  mBrush         = ::CreateSolidBrush(NSRGB_2_COLOREF(mBackground));
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
NS_METHOD nsScrollbar::SetMaxRange(PRUint32 aEndRange)
{
    if (aEndRange > 32767)
        mScaleFactor = aEndRange / 32767.0f;
    if (mWnd) {
        VERIFY(::SetScrollRange(mWnd, SB_CTL, 0, NSToIntRound(aEndRange / mScaleFactor), TRUE));
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
  int startRange, endRange;
  if (mWnd) {
      VERIFY(::GetScrollRange(mWnd, SB_CTL, &startRange, &endRange));
  }
  aRange = (PRUint32)NSToIntRound(endRange * mScaleFactor);
  return NS_OK;
}


//-------------------------------------------------------------------------
//
// Set the thumb position
//
//-------------------------------------------------------------------------
NS_METHOD nsScrollbar::SetPosition(PRUint32 aPos)
{
  ::SetScrollPos(mWnd, SB_CTL, NSToIntRound(aPos / mScaleFactor), TRUE);
  return NS_OK;
}


//-------------------------------------------------------------------------
//
// Get the current thumb position.
//
//-------------------------------------------------------------------------
PRUint32 nsScrollbar::GetPosition(PRUint32& aPosition)
{
  aPosition = (PRUint32)NSToIntRound(::GetScrollPos(mWnd, SB_CTL) * mScaleFactor);
  return NS_OK;
}


//-------------------------------------------------------------------------
//
// Set the thumb size
//
//-------------------------------------------------------------------------
NS_METHOD nsScrollbar::SetThumbSize(PRUint32 aSize)
{
  if (mWnd) {
    SCROLLINFO si;
    si.cbSize = sizeof(SCROLLINFO);
    si.fMask = SIF_PAGE;
    si.nPage = NSToIntRound(aSize / mScaleFactor);
    ::SetScrollInfo(mWnd, SB_CTL, &si, TRUE);
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
  if (mWnd) {
    SCROLLINFO si;
    si.cbSize = sizeof(SCROLLINFO);
    si.fMask = SIF_PAGE;
    VERIFY(::GetScrollInfo(mWnd, SB_CTL, &si));
    aSize = (PRUint32)NSToIntRound(si.nPage * mScaleFactor);
  }
  else
  {
    aSize = 0;
  }
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
  if (aMaxRange > 32767)
      mScaleFactor = aMaxRange / 32767.0f;

  if (mWnd) {
      SCROLLINFO si;
      si.cbSize = sizeof(SCROLLINFO);
      si.fMask = SIF_PAGE | SIF_POS | SIF_RANGE;
      si.nPage = NSToIntRound(aThumbSize / mScaleFactor);
      si.nPos = NSToIntRound(aPosition / mScaleFactor);
      si.nMin = 0;
      si.nMax = NSToIntRound(aMaxRange / mScaleFactor);
      ::SetScrollInfo(mWnd, SB_CTL, &si, TRUE);
  }

  mLineIncrement = NSToIntRound(aLineIncrement / mScaleFactor);
  return NS_OK;
}


//-------------------------------------------------------------------------
//
// paint message. Don't send the paint out
//
//-------------------------------------------------------------------------
PRBool nsScrollbar::OnPaint()
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
PRBool nsScrollbar::OnScroll(UINT scrollCode, int cPos)
{
    PRBool result = PR_TRUE;
    int newPosition;

    switch (scrollCode) {

        // scroll one line right or down
        // SB_LINERIGHT and SB_LINEDOWN are actually the same value
        //case SB_LINERIGHT: 
        case SB_LINEDOWN: 
        {
            newPosition = ::GetScrollPos(mWnd, SB_CTL) + mLineIncrement;
            
            PRUint32 range;
            PRUint32 size;
            GetMaxRange(range);
            GetThumbSize(size);
            PRUint32 max = range - size;

            if (newPosition > (int)max) 
                newPosition = (int)max;

            // if an event callback is registered, give it the chance
            // to change the increment
            if (mEventCallback) {
                nsScrollbarEvent event;
                event.message = NS_SCROLLBAR_LINE_NEXT;
                event.widget = (nsWindow*)this;
                DWORD pos = ::GetMessagePos();
                POINT cpos;
                cpos.x = LOWORD(pos);
                cpos.y = HIWORD(pos);
                ::ScreenToClient(mWnd, &cpos);
                event.point.x = cpos.x;
                event.point.y = cpos.y;
                event.time = ::GetMessageTime();
                event.position = (PRUint32)NSToIntRound(newPosition * mScaleFactor);

                result = ConvertStatus((*mEventCallback)(&event));
                newPosition = NSToIntRound(event.position / mScaleFactor);
            }

            ::SetScrollPos(mWnd, SB_CTL, newPosition, TRUE);

            break;
        }


        // scroll one line left or up
        //case SB_LINELEFT:
        case SB_LINEUP: 
        {
            newPosition = ::GetScrollPos(mWnd, SB_CTL) - mLineIncrement;
            if (newPosition < 0) 
                newPosition = 0;

            // if an event callback is registered, give it the chance
            // to change the decrement
            if (mEventCallback) {
                nsScrollbarEvent event;
                event.message = NS_SCROLLBAR_LINE_PREV;
                event.widget = (nsWindow*)this;
                DWORD pos = ::GetMessagePos();
                POINT cpos;
                cpos.x = LOWORD(pos);
                cpos.y = HIWORD(pos);
                ::ScreenToClient(mWnd, &cpos);
                event.point.x = cpos.x;
                event.point.y = cpos.y;
                event.time = ::GetMessageTime();
                event.position = (PRUint32)NSToIntRound(newPosition * mScaleFactor);

                result = ConvertStatus((*mEventCallback)(&event));
                newPosition = NSToIntRound(event.position / mScaleFactor);
            }

            ::SetScrollPos(mWnd, SB_CTL, newPosition, TRUE);

            break;
        }

        // Scrolls one page right or down
        // case SB_PAGERIGHT:
        case SB_PAGEDOWN: 
        {
            SCROLLINFO si;
            si.cbSize = sizeof(SCROLLINFO);
            si.fMask = SIF_PAGE;
            VERIFY(::GetScrollInfo(mWnd, SB_CTL, &si));

            newPosition = ::GetScrollPos(mWnd, SB_CTL)  + si.nPage;

            PRUint32 range;
            PRUint32 size;
            GetMaxRange(range);
            GetThumbSize(size);
            PRUint32 max = range - size;
            
                     
            if (newPosition > (int)max) 
                newPosition = (int)max;

            // if an event callback is registered, give it the chance
            // to change the increment
            if (mEventCallback) {
                nsScrollbarEvent event;
                event.message = NS_SCROLLBAR_PAGE_NEXT;
                event.widget = (nsWindow*)this;
                DWORD pos = ::GetMessagePos();
                POINT cpos;
                cpos.x = LOWORD(pos);
                cpos.y = HIWORD(pos);
                ::ScreenToClient(mWnd, &cpos);
                event.point.x = cpos.x;
                event.point.y = cpos.y;
                event.time = ::GetMessageTime();
                event.position = (PRUint32)NSToIntRound(newPosition * mScaleFactor);;


                result = ConvertStatus((*mEventCallback)(&event));
                newPosition = NSToIntRound(event.position / mScaleFactor);
            }

            ::SetScrollPos(mWnd, SB_CTL, newPosition, TRUE);

            break;
        }

        // Scrolls one page left or up.
        //case SB_PAGELEFT:
        case SB_PAGEUP: 
        {
            SCROLLINFO si;
            si.cbSize = sizeof(SCROLLINFO);
            si.fMask = SIF_PAGE;
            VERIFY(::GetScrollInfo(mWnd, SB_CTL, &si));

            newPosition = ::GetScrollPos(mWnd, SB_CTL)  - si.nPage;
            if (newPosition < 0) 
                newPosition = 0;

            // if an event callback is registered, give it the chance
            // to change the increment
            if (mEventCallback) {
                nsScrollbarEvent event;
                event.message = NS_SCROLLBAR_PAGE_PREV;
                event.widget = (nsWindow*)this;
                DWORD pos = ::GetMessagePos();
                POINT cpos;
                cpos.x = LOWORD(pos);
                cpos.y = HIWORD(pos);
                ::ScreenToClient(mWnd, &cpos);
                event.point.x = cpos.x;
                event.point.y = cpos.y;
                event.time = ::GetMessageTime();
                event.position = (PRUint32)NSToIntRound(newPosition * mScaleFactor);

                result = ConvertStatus((*mEventCallback)(&event));
                newPosition = NSToIntRound(event.position / mScaleFactor);
            }

            ::SetScrollPos(mWnd, SB_CTL, newPosition - 10, TRUE);

            break;
        }

        // Scrolls to the absolute position. The current position is specified by 
        // the cPos parameter.
        case SB_THUMBPOSITION: 
        case SB_THUMBTRACK: 
        {
            newPosition = cPos;

            // if an event callback is registered, give it the chance
            // to change the increment
            if (mEventCallback) {
                nsScrollbarEvent event;
                event.message = NS_SCROLLBAR_POS;
                event.widget = (nsWindow*)this;
                DWORD pos = ::GetMessagePos();
                POINT cpos;
                cpos.x = LOWORD(pos);
                cpos.y = HIWORD(pos);
                ::ScreenToClient(mWnd, &cpos);
                event.point.x = cpos.x;
                event.point.y = cpos.y;
                event.time = ::GetMessageTime();
                event.position = (PRUint32)NSToIntRound(newPosition * mScaleFactor);

                result = ConvertStatus((*mEventCallback)(&event));
                newPosition = NSToIntRound(event.position / mScaleFactor);
            }

            ::SetScrollPos(mWnd, SB_CTL, newPosition, TRUE);

            break;
        }
    }

    return result;
}


//-------------------------------------------------------------------------
//
// return the window class name and initialize the class if needed
//
//-------------------------------------------------------------------------
LPCTSTR nsScrollbar::WindowClass()
{
    return "SCROLLBAR";
}


//-------------------------------------------------------------------------
//
// return window styles
//
//-------------------------------------------------------------------------
DWORD nsScrollbar::WindowStyle()
{
    return mPositionFlag | WS_CHILD | WS_CLIPSIBLINGS;
}


//-------------------------------------------------------------------------
//
// return window extended styles
//
//-------------------------------------------------------------------------
DWORD nsScrollbar::WindowExStyle()
{
    return 0;
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


