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

#include "nsScrollbar.h"
#include "nsToolkit.h"
#include "nsGUIEvent.h"
#include "nsUnitConversion.h"
#include <Xm/ScrollBar.h>

#include "nsXtEventHandler.h"

//-------------------------------------------------------------------------
//
// nsScrollbar constructor
//
//-------------------------------------------------------------------------
nsScrollbar::nsScrollbar(nsISupports *aOuter, PRBool aIsVertical) : nsWindow(aOuter)
{
    mOrientation  = (aIsVertical) ? XmVERTICAL : XmHORIZONTAL;
    //mScaleFactor   = 1.0;
    mLineIncrement = 0;
    //mBackground    = ::GetSysColor(COLOR_SCROLLBAR);
    //mBrush         = ::CreateSolidBrush(NSRGB_2_COLOREF(mBackground));

}
//-------------------------------------------------------------------------
//
// Create
//
//-------------------------------------------------------------------------
void nsScrollbar::Create(nsIWidget *aParent,
                      const nsRect &aRect,
                      EVENT_CALLBACK aHandleEventFunction,
                      nsIDeviceContext *aContext,
                      nsIToolkit *aToolkit,
                      nsWidgetInitData *aInitData)
{
  Widget parentWidget = nsnull;

//fprintf(stderr, "aParent 0x%x\n", aParent);

  if (aParent) {
    parentWidget = (Widget) aParent->GetNativeData(NS_NATIVE_WIDGET);
  } else {
    parentWidget = (Widget) aInitData ;
  }

fprintf(stderr, "Parent 0x%x\n", parentWidget);

  int procDir = mOrientation == XmVERTICAL? XmMAX_ON_BOTTOM:XmMAX_ON_RIGHT;

fprintf(stderr, "mOrient %d %d %d %d\n", mOrientation, XmVERTICAL, XmMAX_ON_BOTTOM, procDir);
  mWidget = ::XtVaCreateManagedWidget("scrollbar",
                                    xmScrollBarWidgetClass,
                                    parentWidget,
                                    XmNorientation, mOrientation,
                                    XmNprocessingDirection, procDir,
                                    XmNwidth, aRect.width,
                                    XmNheight, aRect.height,
                                    XmNrecomputeSize, False,
                                    XmNhighlightOnEnter, False,
                                    XmNminimum, 0,
                                    XmNmaximum, 100,
                                    XmNx, aRect.x,
                                    XmNy, aRect.y,
                                    nsnull);

fprintf(stderr, "Scrollbar 0x%x  this 0x%x\n", mWidget, this);

  // save the event callback function
  mEventCallback = aHandleEventFunction;

  //InitCallbacks();
  XtAddCallback(mWidget,
                XmNdragCallback,
                nsXtWidget_Scrollbar_Callback,
                this);

  XtAddCallback(mWidget,
                XmNdecrementCallback,
                nsXtWidget_Scrollbar_Callback,
                this);

  XtAddCallback(mWidget,
                XmNincrementCallback,
                nsXtWidget_Scrollbar_Callback,
                this);

  XtAddCallback(mWidget,
                XmNvalueChangedCallback,
                nsXtWidget_Scrollbar_Callback,
                this);


}

//-------------------------------------------------------------------------
//
// Create
//
//-------------------------------------------------------------------------
void nsScrollbar::Create(nsNativeWindow aParent,
                      const nsRect &aRect,
                      EVENT_CALLBACK aHandleEventFunction,
                      nsIDeviceContext *aContext,
                      nsIToolkit *aToolkit,
                      nsWidgetInitData *aInitData)
{
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
  static NS_DEFINE_IID(kInsScrollbarIID, NS_ISCROLLBAR_IID);

  if (aIID.Equals(kInsScrollbarIID)) {
    AddRef();
    *aInstancePtr = (void**) &mAggWidget;
    return NS_OK;
  }
  return nsWindow::QueryInterface(aIID, aInstancePtr);

}


//-------------------------------------------------------------------------
//
// Define the range settings 
//
//-------------------------------------------------------------------------
void nsScrollbar::SetMaxRange(PRUint32 aEndRange)
{
    fprintf(stderr, "Setting Max Range to %d 0x%x\n", aEndRange, mWidget);
    int max = aEndRange;
    XtVaGetValues(mWidget, XmNmaximum, &max);
    fprintf(stderr, "Max is %d\n", max);

    max = aEndRange;
    XtVaSetValues(mWidget, XmNmaximum, max);
}


//-------------------------------------------------------------------------
//
// Return the range settings 
//
//-------------------------------------------------------------------------
PRUint32 nsScrollbar::GetMaxRange()
{
    int maxRange = 0;
    XtVaGetValues(mWidget, XmNmaximum, &maxRange);

    return (PRUint32)maxRange;
}


//-------------------------------------------------------------------------
//
// Set the thumb position
//
//-------------------------------------------------------------------------
void nsScrollbar::SetPosition(PRUint32 aPos)
{
    fprintf(stderr, "Setting SetPosition to %d 0x%x\n", aPos, mWidget);
    int pos = aPos;
    XtVaGetValues(mWidget, XmNvalue, &pos);
    fprintf(stderr, "pos is %d\n", pos);

    pos = aPos;
    XtVaSetValues(mWidget, XmNvalue, pos);
}


//-------------------------------------------------------------------------
//
// Get the current thumb position.
//
//-------------------------------------------------------------------------
PRUint32 nsScrollbar::GetPosition()
{
    int pagePos = 0;
    XtVaGetValues(mWidget, XmNvalue, &pagePos);

    return (PRUint32)pagePos;
}


//-------------------------------------------------------------------------
//
// Set the thumb size
//
//-------------------------------------------------------------------------
void nsScrollbar::SetThumbSize(PRUint32 aSize)
{
    XtVaSetValues(mWidget, XmNpageIncrement, (int)aSize);
}


//-------------------------------------------------------------------------
//
// Get the thumb size
//
//-------------------------------------------------------------------------
PRUint32 nsScrollbar::GetThumbSize()
{
    int pageSize = 0;
    XtVaGetValues(mWidget, XmNpageIncrement, &pageSize);

    return (PRUint32)pageSize;
}


//-------------------------------------------------------------------------
//
// Set the line increment for this scrollbar
//
//-------------------------------------------------------------------------
void nsScrollbar::SetLineIncrement(PRUint32 aSize)
{
    mLineIncrement = aSize;
}


//-------------------------------------------------------------------------
//
// Get the line increment for this scrollbar
//
//-------------------------------------------------------------------------
PRUint32 nsScrollbar::GetLineIncrement()
{
    return mLineIncrement;
}


//-------------------------------------------------------------------------
//
// Set all scrolling parameters
//
//-------------------------------------------------------------------------
void nsScrollbar::SetParameters(PRUint32 aMaxRange, PRUint32 aThumbSize,
                                PRUint32 aPosition, PRUint32 aLineIncrement)
{

    XtVaSetValues(mWidget, XmNincrement, aLineIncrement,
                           XmNmaximum,   aMaxRange,
                           XmNminimum,   0,
                           XmNpageIncrement, aThumbSize,
                           XmNvalue,     aPosition);

    mLineIncrement = aLineIncrement;
}


//-------------------------------------------------------------------------
//
// paint message. Don't send the paint out
//
//-------------------------------------------------------------------------
PRBool nsScrollbar::OnPaint(nsPaintEvent & aEvent)
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
PRBool nsScrollbar::OnScroll(nsScrollbarEvent & aEvent, PRUint32 cPos)
{
    PRBool result = PR_TRUE;
    int newPosition;
    fprintf(stderr, "Scrollbar %d pos %d\n", aEvent.message, cPos);
#if 0
    switch (scrollCode) {

        // scroll one line right or down
        // SB_LINERIGHT and SB_LINEDOWN are actually the same value
        //case SB_LINERIGHT: 
        case SB_LINEDOWN: 
        {
            XtVaGetValues(mWidget, XmNvalue, &newPosition);
            newPosition += mLineIncrement;
            PRUint32 max = GetMaxRange() - GetThumbSize();
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
                event.position = (PRUint32)NS_TO_INT_ROUND(newPosition * mScaleFactor);

                result = ConvertStatus((*mEventCallback)(&event));
                newPosition = NS_TO_INT_ROUND(event.position / mScaleFactor);
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
                event.position = (PRUint32)NS_TO_INT_ROUND(newPosition * mScaleFactor);

                result = ConvertStatus((*mEventCallback)(&event));
                newPosition = NS_TO_INT_ROUND(event.position / mScaleFactor);
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
            PRUint32 max = GetMaxRange() - GetThumbSize();
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
                event.position = (PRUint32)NS_TO_INT_ROUND(newPosition * mScaleFactor);;


                result = ConvertStatus((*mEventCallback)(&event));
                newPosition = NS_TO_INT_ROUND(event.position / mScaleFactor);
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
                event.position = (PRUint32)NS_TO_INT_ROUND(newPosition * mScaleFactor);

                result = ConvertStatus((*mEventCallback)(&event));
                newPosition = NS_TO_INT_ROUND(event.position / mScaleFactor);
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
                event.position = (PRUint32)NS_TO_INT_ROUND(newPosition * mScaleFactor);

                result = ConvertStatus((*mEventCallback)(&event));
                newPosition = NS_TO_INT_ROUND(event.position * mScaleFactor);
            }

            ::SetScrollPos(mWnd, SB_CTL, newPosition, TRUE);

            break;
        }
    }
#endif
    return result;
}

//----------------------------------------------------------------------
//----------------------------------------------------------------------
//----------------------------------------------------------------------
//----------------------------------------------------------------------
#define GET_OUTER() ((nsScrollbar*) ((char*)this - nsScrollbar::GetOuterOffset()))

// nsIScrollbar part
void nsScrollbar::AggScrollbar::SetMaxRange(PRUint32 aEndRange)
{
  GET_OUTER()->SetMaxRange(aEndRange);
}

PRUint32 nsScrollbar::AggScrollbar::GetMaxRange()
{
  return GET_OUTER()->GetMaxRange();
}

void nsScrollbar::AggScrollbar::SetPosition(PRUint32 aPos)
{
  GET_OUTER()->SetPosition(aPos);
}

PRUint32 nsScrollbar::AggScrollbar::GetPosition()
{
  return GET_OUTER()->GetPosition();
}

void nsScrollbar::AggScrollbar::SetThumbSize(PRUint32 aSize)
{
  GET_OUTER()->SetThumbSize(aSize);
}

PRUint32 nsScrollbar::AggScrollbar::GetThumbSize()
{
  return GET_OUTER()->GetThumbSize();
}

void nsScrollbar::AggScrollbar::SetLineIncrement(PRUint32 aSize)
{
  GET_OUTER()->SetLineIncrement(aSize);
}

PRUint32 nsScrollbar::AggScrollbar::GetLineIncrement()
{
  return GET_OUTER()->GetLineIncrement();
}

void nsScrollbar::AggScrollbar::SetParameters(PRUint32 aMaxRange, 
                                              PRUint32 aThumbSize,
                                              PRUint32 aPosition, 
                                              PRUint32 aLineIncrement)
{
  GET_OUTER()->SetParameters(aMaxRange, aThumbSize, aPosition, aLineIncrement);
}

//----------------------------------------------------------------------

BASE_IWIDGET_IMPL(nsScrollbar, AggScrollbar);

