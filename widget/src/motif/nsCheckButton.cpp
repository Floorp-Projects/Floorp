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

NS_IMPL_ADDREF(nsCheckButton)
NS_IMPL_RELEASE(nsCheckButton)

//-------------------------------------------------------------------------
//
// nsCheckButton constructor
//
//-------------------------------------------------------------------------
nsCheckButton::nsCheckButton() : nsWindow() , nsICheckButton()
{
  NS_INIT_REFCNT();
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
NS_METHOD nsCheckButton::Create(nsIWidget *aParent,
                      const nsRect &aRect,
                      EVENT_CALLBACK aHandleEventFunction,
                      nsIDeviceContext *aContext,
                      nsIAppShell *aAppShell,
                      nsIToolkit *aToolkit,
                      nsWidgetInitData *aInitData)
{
  aParent->AddChild(this);
  Widget parentWidget = nsnull;

  if (aParent) {
    parentWidget = (Widget) aParent->GetNativeData(NS_NATIVE_WIDGET);
  } else {
    parentWidget = (Widget) aAppShell->GetNativeData(NS_NATIVE_SHELL);
  }

  InitToolkit(aToolkit, aParent);
  InitDeviceContext(aContext, parentWidget);

  mWidget = ::XtVaCreateManagedWidget("",
                                    xmToggleButtonWidgetClass,
                                    parentWidget,
                                    XmNwidth, aRect.width,
                                    XmNheight, aRect.height,
                                    XmNrecomputeSize, False,
                                    XmNhighlightOnEnter, False,
                                    XmNx, aRect.x,
                                    XmNy, aRect.y,
                                    XmNresizeHeight, False,
                                    XmNresizeWidth, False,
                                    XmNmarginHeight, 0,
                                    XmNmarginWidth, 0,
                                    XmNadjustMargin, False,
                                    XmNspacing, 0,
                                    XmNisAligned, False,
                                    XmNentryBorder, 0,
                                    XmNborderWidth, 0,
                                    0);

  // save the event callback function
  mEventCallback = aHandleEventFunction;

  InitCallbacks();

  XtAddCallback(mWidget,
                XmNarmCallback,
                nsXtWidget_Toggle_ArmCallback,
                this);

  XtAddCallback(mWidget,
                XmNdisarmCallback,
                nsXtWidget_Toggle_DisArmCallback,
                this);


  return NS_OK;
}

//-------------------------------------------------------------------------
//
// nsCheckButton Creator
//
//-------------------------------------------------------------------------
NS_METHOD nsCheckButton::Create(nsNativeWidget aParent,
                      const nsRect &aRect,
                      EVENT_CALLBACK aHandleEventFunction,
                      nsIDeviceContext *aContext,
                      nsIAppShell *aAppShell,
                      nsIToolkit *aToolkit,
                      nsWidgetInitData *aInitData)
{
  return NS_ERROR_FAILURE;
}

/**
 * Implement the standard QueryInterface for NS_IWIDGET_IID and NS_ISUPPORTS_IID
 * @modify gpk 8/4/98
 * @param aIID The name of the class implementing the method
 * @param _classiiddef The name of the #define symbol that defines the IID
 * for the class (e.g. NS_ISUPPORTS_IID)
 * 
*/ 
nsresult nsCheckButton::QueryInterface(const nsIID& aIID, void** aInstancePtr)
{
    if (NULL == aInstancePtr) {
        return NS_ERROR_NULL_POINTER;
    }

    static NS_DEFINE_IID(kICheckButtonIID, NS_ICHECKBUTTON_IID);
    if (aIID.Equals(kICheckButtonIID)) {
        *aInstancePtr = (void*) ((nsICheckButton*)this);
        AddRef();
        return NS_OK;
    }
    return nsWindow::QueryInterface(aIID,aInstancePtr);
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
}

//-------------------------------------------------------------------------
//
// DisArmed
//
//-------------------------------------------------------------------------
void nsCheckButton::DisArmed() 
{
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
NS_METHOD nsCheckButton::SetState(const PRBool aState) 
{
  if (mIsArmed) {
    mNewValue    = aState;
    mValueWasSet = PR_TRUE;
  }
  XmToggleButtonSetState(mWidget, aState, TRUE);
  return NS_OK;
}

//-------------------------------------------------------------------------
//
// Set this button label
//
//-------------------------------------------------------------------------
NS_METHOD nsCheckButton::GetState(PRBool& aState)
{
  int state = XmToggleButtonGetState(mWidget);
  if (mIsArmed) {
    if (mValueWasSet) {
      aState = mNewValue;
    } else {
      aState = state;
    }
  } else {
    aState = state;
  }
  return NS_OK;
}

//-------------------------------------------------------------------------
//
// Set this Checkbox label
//
//-------------------------------------------------------------------------
NS_METHOD nsCheckButton::SetLabel(const nsString& aText)
{
  NS_ALLOC_STR_BUF(label, aText, 256);
  XmString str;
  str = XmStringCreate(label, XmFONTLIST_DEFAULT_TAG);
  XtVaSetValues(mWidget, XmNlabelString, str, nsnull);
  NS_FREE_STR_BUF(label);
  XmStringFree(str);
  return NS_OK;
}


//-------------------------------------------------------------------------
//
// Get this button label
//
//-------------------------------------------------------------------------
NS_METHOD nsCheckButton::GetLabel(nsString& aBuffer)
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
  return NS_OK;
}
