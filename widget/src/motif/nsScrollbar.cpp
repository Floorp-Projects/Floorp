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

#include <Xm/ScrollBar.h>

#include "nsScrollbar.h"
#include "nsGUIEvent.h"

#include "nsXtEventHandler.h"

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

  strcpy(gInstanceClassName, "nsScrollbar");
  mOrientation  = (aIsVertical) ? XmVERTICAL : XmHORIZONTAL;
  mLineIncrement = 0;
}

//-------------------------------------------------------------------------
//
// Create
//
//-------------------------------------------------------------------------
NS_METHOD nsScrollbar::Create(nsNativeWidget aParent,
                      const nsRect &aRect,
                      EVENT_CALLBACK aHandleEventFunction,
                      nsIDeviceContext *aContext,
                      nsIAppShell *aAppShell,
                      nsIToolkit *aToolkit,
                      nsWidgetInitData *aInitData)
{
  Widget parentWidget = (Widget)aParent;
  strcpy(gInstanceClassName, "nsScrollbar");

  int procDir = mOrientation == XmVERTICAL? XmMAX_ON_BOTTOM:XmMAX_ON_RIGHT;

  mWidget = ::XtVaCreateManagedWidget("nsScrollbar",
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

  return NS_OK;
}


NS_METHOD nsScrollbar::Create(nsIWidget *aParent,
                      const nsRect &aRect,
                      EVENT_CALLBACK aHandleEventFunction,
                      nsIDeviceContext *aContext,
                      nsIAppShell *aAppShell,
                      nsIToolkit *aToolkit,
                      nsWidgetInitData *aInitData)
{
  Widget parentWidget;

  if (aParent) {
    parentWidget = (Widget) aParent->GetNativeData(NS_NATIVE_WIDGET);
  } else {
    parentWidget = (Widget) aAppShell->GetNativeData(NS_NATIVE_SHELL);
  }

  Create((nsNativeWidget)parentWidget, aRect, aHandleEventFunction, aContext, aAppShell, aToolkit, aInitData);
  return NS_OK;
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
        AddRef();
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
  int max = aEndRange;
  XtVaGetValues(mWidget, XmNmaximum, &max, nsnull);
  return NS_OK;
}


//-------------------------------------------------------------------------
//
// Return the range settings 
//
//-------------------------------------------------------------------------
NS_METHOD nsScrollbar::GetMaxRange(PRUint32 & aMaxRange)
{
  int maxRange = 0;
  XtVaGetValues(mWidget, XmNmaximum, &maxRange, nsnull);
  aMaxRange = (PRUint32)maxRange;
  return NS_OK;
}


//-------------------------------------------------------------------------
//
// Set the thumb position
//
//-------------------------------------------------------------------------
NS_METHOD nsScrollbar::SetPosition(PRUint32 aPos)
{
  int pos = (int)aPos;
  XtVaSetValues(mWidget, XmNvalue, pos, nsnull);

  return NS_OK;
}


//-------------------------------------------------------------------------
//
// Get the current thumb position.
//
//-------------------------------------------------------------------------
NS_METHOD nsScrollbar::GetPosition(PRUint32 & aPos)
{
  int pagePos = 0;
  XtVaGetValues(mWidget, XmNvalue, &pagePos, nsnull);

  aPos = (PRUint32)pagePos;
  return NS_OK;
}


//-------------------------------------------------------------------------
//
// Set the thumb size
//
//-------------------------------------------------------------------------
NS_METHOD nsScrollbar::SetThumbSize(PRUint32 aSize)
{
  if (aSize > 0) {
    XtVaSetValues(mWidget, XmNpageIncrement, (int)aSize, nsnull);
    XtVaSetValues(mWidget, XmNsliderSize, (int)aSize, nsnull);
  }
  return NS_OK;
}


//-------------------------------------------------------------------------
//
// Get the thumb size
//
//-------------------------------------------------------------------------
NS_METHOD nsScrollbar::GetThumbSize(PRUint32 & aThumbSize)
{
  int pageSize = 0;
  XtVaGetValues(mWidget, XmNpageIncrement, &pageSize, nsnull);

  aThumbSize = (PRUint32)pageSize;
  return NS_OK;
}


//-------------------------------------------------------------------------
//
// Set the line increment for this scrollbar
//
//-------------------------------------------------------------------------
NS_METHOD nsScrollbar::SetLineIncrement(PRUint32 aLineIncrement)
{
  if (aLineIncrement > 0) {
    mLineIncrement = aLineIncrement;
    XtVaSetValues(mWidget, XmNincrement, aLineIncrement, nsnull);
  }

  return NS_OK;
}


//-------------------------------------------------------------------------
//
// Get the line increment for this scrollbar
//
//-------------------------------------------------------------------------
NS_METHOD nsScrollbar::GetLineIncrement(PRUint32 & aLineInc)
{
  aLineInc =  mLineIncrement;
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

    int thumbSize = (((int)aThumbSize) > 0?aThumbSize:1);
    int maxRange  = (((int)aMaxRange) > 0?aMaxRange:10);
    mLineIncrement = (((int)aLineIncrement) > 0?aLineIncrement:1);

    int maxPos = maxRange - thumbSize;
    int pos    = ((int)aPosition) > maxPos ? maxPos-1 : ((int)aPosition);

    XtVaSetValues(mWidget, 
                  XmNincrement,     mLineIncrement,
                  XmNminimum,       0,
                  XmNmaximum,       maxRange,
                  XmNsliderSize,    thumbSize,
                  XmNpageIncrement, thumbSize,
                  XmNvalue,         pos,
                  nsnull);

  return NS_OK;
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
  return nsWindow::OnResize(aEvent);
}

//-------------------------------------------------------------------------
int nsScrollbar::AdjustScrollBarPosition(int aPosition) 
{
  int maxRange;
  int sliderSize;

  XtVaGetValues(mWidget, XmNmaximum, &maxRange, 
                         XmNsliderSize, &sliderSize, 
                         nsnull);
  int cap = maxRange - sliderSize;
  return aPosition > cap ? cap : aPosition;
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
            PRUint32 thumbSize;
            PRUint32 maxRange;
            GetThumbSize(thumbSize);
            GetMaxRange(maxRange);
            PRUint32 max = maxRange - thumbSize;
            if (newPosition > (int)max) 
                newPosition = (int)max;

            // if an event callback is registered, give it the chance
            // to change the increment
            if (mEventCallback) {
                aEvent.position = newPosition;
                result = ConvertStatus((*mEventCallback)(&aEvent));
                newPosition = aEvent.position;
            }
            
            XtVaSetValues(mWidget, XmNvalue, 
                          AdjustScrollBarPosition(newPosition), nsnull);
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
            PRUint32 thumbSize;
            GetThumbSize(thumbSize);
            PRUint32 maxRange;
            GetThumbSize(thumbSize);
            GetMaxRange(maxRange);
            PRUint32 max = maxRange - thumbSize;
            if (newPosition > (int)max) 
                newPosition = (int)max;

            // if an event callback is registered, give it the chance
            // to change the increment
            if (mEventCallback) {
                aEvent.position = newPosition;
                result = ConvertStatus((*mEventCallback)(&aEvent));
                newPosition = aEvent.position;
            }
            XtVaSetValues(mWidget, XmNvalue, 
                          AdjustScrollBarPosition(newPosition+10), nsnull);
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

            XtVaSetValues(mWidget, XmNvalue, newPosition-10, nsnull);
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

            XtVaSetValues(mWidget, XmNvalue, 
                          AdjustScrollBarPosition(newPosition), nsnull);

            break;
        }
    }
    return result;
}
