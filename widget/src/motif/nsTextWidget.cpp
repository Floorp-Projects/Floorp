/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#include <Xm/Text.h>

#include "nsTextWidget.h"
#include "nsToolkit.h"
#include "nsColor.h"
#include "nsGUIEvent.h"
#include "xlibrgb.h"
#include "nsString.h"
#include "nsXtEventHandler.h"
#include "nslog.h"

NS_IMPL_LOG(nsTextWidgetLog)
#define PRINTF NS_LOG_PRINTF(nsTextWidgetLog)
#define FLUSH  NS_LOG_FLUSH(nsTextWidgetLog)

extern int mIsPasswordCallBacksInstalled;

NS_IMPL_ADDREF(nsTextWidget)
NS_IMPL_RELEASE(nsTextWidget)

//-------------------------------------------------------------------------
//
// nsTextWidget constructor
//
//-------------------------------------------------------------------------
nsTextWidget::nsTextWidget() : nsTextHelper(),
  mIsPasswordCallBacksInstalled(PR_FALSE),
  mMakeReadOnly(PR_FALSE),
  mMakePassword(PR_FALSE)
{
  mBackground = NS_RGB(127, 127, 127);
  mForeground = NS_RGB(255, 255, 255);
  mBackgroundPixel = xlib_rgb_xpixel_from_rgb(mBackground);
  mForegroundPixel = xlib_rgb_xpixel_from_rgb(mForeground);
}

//-------------------------------------------------------------------------
//
// nsTextWidget destructor
//
//-------------------------------------------------------------------------
nsTextWidget::~nsTextWidget()
{
}

//-------------------------------------------------------------------------
NS_METHOD nsTextWidget::Create(nsIWidget *aParent,
                               const nsRect &aRect,
                               EVENT_CALLBACK aHandleEventFunction,
                               nsIDeviceContext *aContext,
                               nsIAppShell *aAppShell,
                               nsIToolkit *aToolkit,
                               nsWidgetInitData *aInitData)
{
  PRINTF("nsTextWidget::Create called\n");
  aParent->AddChild(this);
  Widget parentWidget = nsnull;

  PRINTF("aParent 0x%x\n", (unsigned int)aParent);

  if (aParent) {
    parentWidget = (Widget) aParent->GetNativeData(NS_NATIVE_WIDGET);
  } else {
    parentWidget = (Widget) aAppShell->GetNativeData(NS_NATIVE_SHELL);
  }

  InitToolkit(aToolkit, aParent);
  InitDeviceContext(aContext, parentWidget);

  mWidget = ::XtVaCreateManagedWidget("nsTextWidget",
                                      xmTextWidgetClass, 
                                      parentWidget,
                                      XmNwidth, aRect.width,
                                      XmNheight, aRect.height,
                                      XmNrecomputeSize, False,
                                      XmNbackground, mBackgroundPixel,
                                      XmNforeground, mForegroundPixel,
                                      XmNhighlightOnEnter, False,
                                      XmNeditable, mMakeReadOnly?False:True,
                                      XmNx, aRect.x,
                                      XmNy, aRect.y, 
                                      nsnull);

  // save the event callback function
  mEventCallback = aHandleEventFunction;

  InitCallbacks("nsTextWidget");

  XtAddCallback(mWidget,
                XmNfocusCallback,
                nsXtWidget_Focus_Callback,
                this);

  XtAddCallback(mWidget,
                XmNlosingFocusCallback,
                nsXtWidget_Focus_Callback,
                this);

  if (mMakeReadOnly) {
    PRBool oldReadOnly;
    SetReadOnly(PR_TRUE, oldReadOnly);
  }
  if (mMakePassword) {
    SetPassword(PR_TRUE);
    PasswordData * data = new PasswordData();
    if (data) {
      data->mPassword = "";
    }
    else return NS_ERROR_OUT_OF_MEMORY;
    XtVaSetValues(mWidget, XmNuserData, data, NULL);
  }
  return NS_OK;
}

//-------------------------------------------------------------------------
//
// Query interface implementation
//
//-------------------------------------------------------------------------
nsresult nsTextWidget::QueryInterface(const nsIID& aIID, void** aInstancePtr)
{
    nsresult result = nsWindow::QueryInterface(aIID, aInstancePtr);

    static NS_DEFINE_IID(kInsTextWidgetIID, NS_ITEXTWIDGET_IID);
    if (result == NS_NOINTERFACE && aIID.Equals(kInsTextWidgetIID)) {
        *aInstancePtr = (void*) ((nsITextWidget*)this);
        AddRef();
        result = NS_OK;
    }

    return result;
}

//--------------------------------------------------------------
NS_METHOD nsTextWidget::SetPassword(PRBool aIsPassword)
{
  if (mWidget == nsnull && aIsPassword) {
    mMakePassword = PR_TRUE;
    return NS_OK;
  }

  if (aIsPassword) {
    if (!mIsPasswordCallBacksInstalled) {
      XtAddCallback(mWidget, XmNmodifyVerifyCallback, nsXtWidget_Text_Callback, NULL);
      XtAddCallback(mWidget, XmNactivateCallback,     nsXtWidget_Text_Callback, NULL);
      mIsPasswordCallBacksInstalled = PR_TRUE;
    }
  } else {
    if (mIsPasswordCallBacksInstalled) {
      XtRemoveCallback(mWidget, XmNmodifyVerifyCallback, nsXtWidget_Text_Callback, NULL);
      XtRemoveCallback(mWidget, XmNactivateCallback,     nsXtWidget_Text_Callback, NULL);
      mIsPasswordCallBacksInstalled = PR_FALSE;
    }
  }
  nsTextHelper::SetPassword(aIsPassword);
  return NS_OK;
}
