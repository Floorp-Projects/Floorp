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

#define DBG 1

//-------------------------------------------------------------------------
//
// nsScrollbar constructor
//
//-------------------------------------------------------------------------
nsScrollbar::nsScrollbar(nsISupports *aOuter, PRBool aIsVertical) : nsWindow(aOuter)
{
    strcpy(gInstanceClassName, "nsScrollbar");
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
  strcpy(gInstanceClassName, "nsScrollbar");

  if (aParent) {
    parentWidget = (Widget) aParent->GetNativeData(NS_NATIVE_WIDGET);
  } else {
    parentWidget = (Widget) aInitData ;
  }

  int procDir = mOrientation == XmVERTICAL? XmMAX_ON_BOTTOM:XmMAX_ON_RIGHT;

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

  /*XtAddCallback(mWidget,
                XmNresizeCallback,
                nsXtWidget_Resize_Callback,
                this);*/

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
nsresult nsScrollbar::QueryObject(const nsIID& aIID, void** aInstancePtr)
{
  static NS_DEFINE_IID(kInsScrollbarIID, NS_ISCROLLBAR_IID);

  if (aIID.Equals(kInsScrollbarIID)) {
    AddRef();
    *aInstancePtr = (void**) &mAggWidget;
    return NS_OK;
  }
  return nsWindow::QueryObject(aIID, aInstancePtr);

}


//-------------------------------------------------------------------------
//
// Define the range settings 
//
//-------------------------------------------------------------------------
void nsScrollbar::SetMaxRange(PRUint32 aEndRange)
{
    int max = aEndRange;
    XtVaGetValues(mWidget, XmNmaximum, &max, nsnull);
    if (DBG) printf("SetMaxRange %d\n", max);

}


//-------------------------------------------------------------------------
//
// Return the range settings 
//
//-------------------------------------------------------------------------
PRUint32 nsScrollbar::GetMaxRange()
{
    int maxRange = 0;
    XtVaGetValues(mWidget, XmNmaximum, &maxRange, nsnull);
    return (PRUint32)maxRange;
}


//-------------------------------------------------------------------------
//
// Set the thumb position
//
//-------------------------------------------------------------------------
void nsScrollbar::SetPosition(PRUint32 aPos)
{
    int pos = aPos;
    XtVaSetValues(mWidget, XmNvalue, pos, nsnull);
    if (DBG) printf("SetPosition %d\n", pos);
}


//-------------------------------------------------------------------------
//
// Get the current thumb position.
//
//-------------------------------------------------------------------------
PRUint32 nsScrollbar::GetPosition()
{
    int pagePos = 0;
    XtVaGetValues(mWidget, XmNvalue, &pagePos, nsnull);

    return (PRUint32)pagePos;
}


//-------------------------------------------------------------------------
//
// Set the thumb size
//
//-------------------------------------------------------------------------
void nsScrollbar::SetThumbSize(PRUint32 aSize)
{
    XtVaSetValues(mWidget, XmNpageIncrement, (int)aSize, nsnull);
    if (DBG) printf("SetThumbSize %d\n", aSize);
}


//-------------------------------------------------------------------------
//
// Get the thumb size
//
//-------------------------------------------------------------------------
PRUint32 nsScrollbar::GetThumbSize()
{
    int pageSize = 0;
    XtVaGetValues(mWidget, XmNpageIncrement, &pageSize, nsnull);

    return (PRUint32)pageSize;
}


//-------------------------------------------------------------------------
//
// Set the line increment for this scrollbar
//
//-------------------------------------------------------------------------
void nsScrollbar::SetLineIncrement(PRUint32 aLineIncrement)
{
    mLineIncrement = aLineIncrement;
    XtVaSetValues(mWidget, XmNincrement, aLineIncrement, nsnull);

    if (DBG) printf("SetLineIncrement %d\n", aLineIncrement);
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
                           XmNsliderSize, aThumbSize,
                           XmNvalue,     aPosition, 
                           nsnull);

    mLineIncrement = aLineIncrement;
    if (DBG) printf("SetParameters %d %d %d %d \n", aMaxRange, aThumbSize, 
                    aPosition, aLineIncrement);
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


PRBool nsScrollbar::OnResize(nsSizeEvent &aEvent)
{
  if (DBG) printf("*&*&*&*&*&*&*()()()()(((( nsScrollbar::OnResize\n");
  return nsWindow::OnResize(aEvent);
  //return PR_FALSE;
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

    switch (aEvent.message) {

        // scroll one line right or down
        case NS_SCROLLBAR_LINE_NEXT: 
        {
            XtVaGetValues(mWidget, XmNvalue, &newPosition, nsnull);
            newPosition += mLineIncrement;
            PRUint32 max = GetMaxRange() - GetThumbSize();
            if (newPosition > (int)max) 
                newPosition = (int)max;

            // if an event callback is registered, give it the chance
            // to change the increment
            if (mEventCallback) {
                aEvent.position = newPosition;
                result = ConvertStatus((*mEventCallback)(&aEvent));
                newPosition = aEvent.position;
            }
            XtVaSetValues(mWidget, XmNvalue, newPosition, nsnull);
            break;
        }


        // scroll one line left or up
        case NS_SCROLLBAR_LINE_PREV: 
        {
            XtVaGetValues(mWidget, XmNvalue, &newPosition, nsnull);
            
            newPosition -= mLineIncrement;
            if (newPosition < 0) 
                newPosition = 0;

            // if an event callback is registered, give it the chance
            // to change the decrement
            if (mEventCallback) {
                aEvent.position = newPosition;

                result = ConvertStatus((*mEventCallback)(&aEvent));
                newPosition = aEvent.position;
            }

            XtVaSetValues(mWidget, XmNvalue, newPosition, nsnull);

            break;
        }

        // Scrolls one page right or down
        case NS_SCROLLBAR_PAGE_NEXT: 
        {
            XtVaGetValues(mWidget, XmNvalue, &newPosition, nsnull);
            PRUint32 max = GetMaxRange() - GetThumbSize();
            if (newPosition > (int)max) 
                newPosition = (int)max;

            // if an event callback is registered, give it the chance
            // to change the increment
            if (mEventCallback) {
                aEvent.position = newPosition;
                result = ConvertStatus((*mEventCallback)(&aEvent));
                newPosition = aEvent.position;
            }
            XtVaSetValues(mWidget, XmNvalue, newPosition + 10, nsnull);
            break;
        }

        // Scrolls one page left or up.
        case NS_SCROLLBAR_PAGE_PREV: 
        {
            XtVaGetValues(mWidget, XmNvalue, &newPosition, nsnull);
            if (newPosition < 0) 
                newPosition = 0;

            // if an event callback is registered, give it the chance
            // to change the increment
            if (mEventCallback) {
                aEvent.position = newPosition;
                result = ConvertStatus((*mEventCallback)(&aEvent));
                newPosition = aEvent.position;
            }

            XtVaSetValues(mWidget, XmNvalue, newPosition - 10, nsnull);
            break;
        }


        // Scrolls to the absolute position. The current position is specified by 
        // the cPos parameter.
        case NS_SCROLLBAR_POS: 
        {
            newPosition = cPos;

            // if an event callback is registered, give it the chance
            // to change the increment
            if (mEventCallback) {
                aEvent.position = newPosition;
                result = ConvertStatus((*mEventCallback)(&aEvent));
                newPosition = aEvent.position;
            }

            XtVaSetValues(mWidget, XmNvalue, newPosition, nsnull);

            break;
        }
    }
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

