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

#include "nsCheckButton.h"
#include "nsColor.h"
#include "nsGUIEvent.h"
#include "nsString.h"
#include "nsStringUtil.h"

#include "nsXtEventHandler.h"
#include <Xm/ToggleB.h>

#define DBG 0
//-------------------------------------------------------------------------
//
// nsCheckButton constructor
//
//-------------------------------------------------------------------------
nsCheckButton::nsCheckButton(nsISupports *aOuter) : 
  nsWindow(aOuter),
  mIsArmed(PR_FALSE)
{
}

//-------------------------------------------------------------------------
//
// nsCheckButton destructor
//
//-------------------------------------------------------------------------
nsCheckButton::~nsCheckButton()
{
}

//-------------------------------------------------------------------------
//
// nsCheckButton Creator
//
//-------------------------------------------------------------------------
void nsCheckButton::Create(nsIWidget *aParent,
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

  mWidget = ::XtVaCreateManagedWidget("",
                                    xmToggleButtonWidgetClass,
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

  InitCallbacks();

  /*XtAddCallback(mWidget,
                XmNvalueChangedCallback,
                nsXtWidget_Toggle_Callback,
                this);*/

  XtAddCallback(mWidget,
                XmNarmCallback,
                nsXtWidget_Toggle_ArmCallback,
                this);

  XtAddCallback(mWidget,
                XmNdisarmCallback,
                nsXtWidget_Toggle_DisArmCallback,
                this);



}

//-------------------------------------------------------------------------
//
// nsCheckButton Creator
//
//-------------------------------------------------------------------------
void nsCheckButton::Create(nsNativeWindow aParent,
                      const nsRect &aRect,
                      EVENT_CALLBACK aHandleEventFunction,
                      nsIDeviceContext *aContext,
                      nsIToolkit *aToolkit,
                      nsWidgetInitData *aInitData)
{
}

//-------------------------------------------------------------------------
//
// Query interface implementation
//
//-------------------------------------------------------------------------
nsresult nsCheckButton::QueryObject(REFNSIID aIID, void** aInstancePtr)
{
  static NS_DEFINE_IID(kICheckButtonIID,    NS_ICHECKBUTTON_IID);

  if (aIID.Equals(kICheckButtonIID)) {
    AddRef();
    *aInstancePtr = (void**) &mAggWidget;
    return NS_OK;
  }
  return nsWindow::QueryObject(aIID, aInstancePtr);
}

//-------------------------------------------------------------------------
//
// Armed
//
//-------------------------------------------------------------------------
void nsCheckButton::Armed() 
{
  mIsArmed      = PR_TRUE;
  mValueWasSet  = PR_FALSE;
  mInitialState = XmToggleButtonGetState(mWidget);
  if (DBG) printf("Arm: InitialValue: %d\n", mInitialState);
}

//-------------------------------------------------------------------------
//
// DisArmed
//
//-------------------------------------------------------------------------
void nsCheckButton::DisArmed() 
{
  if (DBG) printf("DisArm: InitialValue: %d\n", mInitialState);
  if (DBG) printf("DisArm: ActualValue:  %d\n", XmToggleButtonGetState(mWidget));
  if (DBG) printf("DisArm: mValueWasSet  %d\n", mValueWasSet);
  if (DBG) printf("DisArm: mNewValue     %d\n", mNewValue);

  if (mValueWasSet) {
    XmToggleButtonSetState(mWidget, mNewValue, TRUE);
  } else {
    XmToggleButtonSetState(mWidget, mInitialState, TRUE);
  }
  mIsArmed = PR_FALSE;
}

//-------------------------------------------------------------------------
//
// Set this button label
//
//-------------------------------------------------------------------------
void nsCheckButton::SetState(PRBool aState) 
{
  int state = aState;
  if (mIsArmed) {
    mNewValue    = aState;
    mValueWasSet = PR_TRUE;
  }
  XmToggleButtonSetState(mWidget, aState, TRUE);
}

//-------------------------------------------------------------------------
//
// Set this button label
//
//-------------------------------------------------------------------------
PRBool nsCheckButton::GetState()
{
  int state = XmToggleButtonGetState(mWidget);
  if (mIsArmed) {
    if (mValueWasSet) {
      return mNewValue;
    } else {
      return state;
    }
  } else {
    return state;
  }
}

//-------------------------------------------------------------------------
//
// Set this button label
//
//-------------------------------------------------------------------------
void nsCheckButton::SetLabel(const nsString& aText)
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
void nsCheckButton::GetLabel(nsString& aBuffer)
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
// move, paint, resizes message - ignore
//
//-------------------------------------------------------------------------
PRBool nsCheckButton::OnMove(PRInt32, PRInt32)
{
  return PR_FALSE;
}

PRBool nsCheckButton::OnPaint(nsPaintEvent &aEvent)
{
  return PR_FALSE;
}

PRBool nsCheckButton::OnResize(nsSizeEvent &aEvent)
{
    return PR_FALSE;
}

//----------------------------------------------------------------------
//----------------------------------------------------------------------
//----------------------------------------------------------------------

#define GET_OUTER() \
  ((nsCheckButton*) ((char*)this - nsCheckButton::GetOuterOffset()))

PRBool nsCheckButton::AggCheckButton::GetState()
{
  return GET_OUTER()->GetState();
}

void nsCheckButton::AggCheckButton::SetState(PRBool aState)
{
  GET_OUTER()->SetState(aState);
}

void nsCheckButton::AggCheckButton::SetLabel(const nsString& aText)
{
  GET_OUTER()->SetLabel(aText);
}

void nsCheckButton::AggCheckButton::GetLabel(nsString& aText)
{
  GET_OUTER()->GetLabel(aText);
}



//----------------------------------------------------------------------

BASE_IWIDGET_IMPL(nsCheckButton, AggCheckButton);

