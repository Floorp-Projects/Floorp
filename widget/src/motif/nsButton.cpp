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

#include "nsButton.h"
#include "nsIButton.h"
#include "nsToolkit.h"
#include "nsColor.h"
#include "nsGUIEvent.h"
#include "nsString.h"
#include "nsStringUtil.h"

#include "nsXtEventHandler.h"

#include <Xm/PushB.h>

#define DBG 0
//-------------------------------------------------------------------------
//
// nsButton constructor
//
//-------------------------------------------------------------------------
nsButton::nsButton(nsISupports *aOuter) : nsWindow(aOuter)
{
}

void nsButton::Create(nsIWidget *aParent,
                      const nsRect &aRect,
                      EVENT_CALLBACK aHandleEventFunction,
                      nsIDeviceContext *aContext,
                      nsIToolkit *aToolkit,
                      nsWidgetInitData *aInitData) 
{
  Widget parentWidget = nsnull;

  if (DBG) fprintf(stderr, "aParent 0x%x\n", aParent);

  if (aParent) {
    parentWidget = (Widget) aParent->GetNativeData(NS_NATIVE_WIDGET);
  } else {
    parentWidget = (Widget) aInitData ;
  }

  if (DBG) fprintf(stderr, "Parent 0x%x\n", parentWidget);

  mWidget = ::XtVaCreateManagedWidget("button",
                                    xmPushButtonWidgetClass, 
                                    parentWidget,
                                    XmNwidth, aRect.width,
                                    XmNheight, aRect.height,
                                    XmNrecomputeSize, False,
                                    XmNhighlightOnEnter, False,
		                    XmNx, aRect.x,
		                    XmNy, aRect.y, 
                                    nsnull);

  if (DBG) fprintf(stderr, "Button 0x%x  this 0x%x\n", mWidget, this);

  // save the event callback function
  mEventCallback = aHandleEventFunction;

  InitCallbacks("nsButton");

}

void nsButton::Create(nsNativeWidget aParent,
                      const nsRect &aRect,
                      EVENT_CALLBACK aHandleEventFunction,
                      nsIDeviceContext *aContext,
                      nsIToolkit *aToolkit,
                      nsWidgetInitData *aInitData)
{
}

//-------------------------------------------------------------------------
//
// nsButton destructor
//
//-------------------------------------------------------------------------
nsButton::~nsButton()
{
}

//-------------------------------------------------------------------------
//
// Query interface implementation
//
//-------------------------------------------------------------------------
nsresult nsButton::QueryObject(REFNSIID aIID, void** aInstancePtr)
{
  static NS_DEFINE_IID(kIButtonIID,    NS_IBUTTON_IID);

  if (aIID.Equals(kIButtonIID)) {
    AddRef();
    *aInstancePtr = (void**) &mAggWidget;
    return NS_OK;
  }
  return nsWindow::QueryObject(aIID, aInstancePtr);
}


//-------------------------------------------------------------------------
//
// Set this button label
//
//-------------------------------------------------------------------------
void nsButton::SetLabel(const nsString& aText)
{
  NS_ALLOC_STR_BUF(label, aText, 256);
  XmString str;
  str = XmStringCreate(label, XmFONTLIST_DEFAULT_TAG);
  XtVaSetValues(mWidget, XmNlabelString, str, nsnull);
  NS_FREE_STR_BUF(label);
  XmStringFree(str);

}

//-------------------------------------------------------------------------
//
// Get this button label
//
//-------------------------------------------------------------------------
void nsButton::GetLabel(nsString& aBuffer)
{
  XmString str;
  XtVaGetValues(mWidget, XmNlabelString, &str, nsnull);
  char * text;
  if (XmStringGetLtoR(str, XmFONTLIST_DEFAULT_TAG, &text)) {
    aBuffer.SetLength(0);
    aBuffer.Append(text);
    XtFree(text);
  }

  XmStringFree(str);

}

//-------------------------------------------------------------------------
//
// paint message. Don't send the paint out
//
//-------------------------------------------------------------------------
PRBool nsButton::OnPaint(nsPaintEvent &aEvent)
{
  //printf("** nsButton::OnPaint **\n");
  return PR_FALSE;
}

PRBool nsButton::OnResize(nsSizeEvent &aEvent)
{
    return PR_FALSE;
}


#define GET_OUTER() ((nsButton*) ((char*)this - nsButton::GetOuterOffset()))

void nsButton::AggButton::GetLabel(nsString& aBuffer)
{
  GET_OUTER()->GetLabel(aBuffer);
}

void nsButton::AggButton::SetLabel(const nsString& aText)
{
  GET_OUTER()->SetLabel(aText);
}

//----------------------------------------------------------------------

BASE_IWIDGET_IMPL(nsButton, AggButton);


