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

#include <gtk/gtk.h>

#include "nsTextWidget.h"
#include "nsToolkit.h"
#include "nsColor.h"
#include "nsGUIEvent.h"
#include "nsString.h"
#include "nsGtkEventHandler.h"

#define DBG 0

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
  aParent->AddChild(this);
  GtkWidget *parentWidget = nsnull;

  if (DBG) fprintf(stderr, "aParent 0x%x\n", aParent);

  if (aParent) {
    parentWidget = GTK_WIDGET(aParent->GetNativeData(NS_NATIVE_WIDGET));
  } else {
    parentWidget = GTK_WIDGET(aAppShell->GetNativeData(NS_NATIVE_SHELL));
  }

  InitToolkit(aToolkit, aParent);
  InitDeviceContext(aContext, parentWidget);

  mWidget = gtk_entry_new();
  gtk_entry_set_editable(GTK_ENTRY(mWidget), mMakeReadOnly?PR_FALSE:PR_TRUE);
  
  gtk_layout_put(GTK_LAYOUT(parentWidget), mWidget, aRect.x, aRect.y);
  gtk_widget_set_usize(mWidget, aRect.width, aRect.height);

  gtk_widget_show(mWidget);
/*
  mWidget = ::XtVaCreateManagedWidget("button",
                                    xmTextWidgetClass,
                                    parentWidget,
                                    XmNwidth, aRect.width,
                                    XmNheight, aRect.height,
                                    XmNrecomputeSize, PR_FALSE,
                                    XmNhighlightOnEnter, PR_FALSE,
                                    XmNeditable, mMakeReadOnly?PR_FALSE:PR_TRUE,
		                    XmNx, aRect.x,
		                    XmNy, aRect.y,
                                    nsnull);
*/
  // save the event callback function
  mEventCallback = aHandleEventFunction;
/*
  InitCallbacks("nsTextWidget");

  XtAddCallback(mWidget,
                XmNfocusCallback,
                nsXtWidget_Focus_Callback,
                this);

  XtAddCallback(mWidget,
                XmNlosingFocusCallback,
                nsXtWidget_Focus_Callback,
                this);
*/
  if (mMakeReadOnly) {
    PRUint32 oldReadOnly;
    SetReadOnly(PR_TRUE, oldReadOnly);
  }
  if (mMakePassword) {
    SetPassword(PR_TRUE);
    /*
    PasswordData * data = new PasswordData();
    data->mPassword = "";
    XtVaSetValues(mWidget, XmNuserData, data, NULL);
    */
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
    nsresult result = nsWidget::QueryInterface(aIID, aInstancePtr);

    static NS_DEFINE_IID(kInsTextWidgetIID, NS_ITEXTWIDGET_IID);
    if (result == NS_NOINTERFACE && aIID.Equals(kInsTextWidgetIID)) {
        *aInstancePtr = (void*) ((nsITextWidget*)this);
        AddRef();
        result = NS_OK;
    }

    return result;
}


//-------------------------------------------------------------------------
NS_METHOD nsTextWidget::Create(nsNativeWidget aParent,
                      const nsRect &aRect,
                      EVENT_CALLBACK aHandleEventFunction,
                      nsIDeviceContext *aContext,
                      nsIAppShell *aAppShell,
                      nsIToolkit *aToolkit,
                      nsWidgetInitData *aInitData)
{
  return NS_ERROR_FAILURE;
}


//-------------------------------------------------------------------------
//
// paint, resizes message - ignore
//
//-------------------------------------------------------------------------
PRBool nsTextWidget::OnPaint(nsPaintEvent & aEvent)
{
  return PR_FALSE;
}


//--------------------------------------------------------------
PRBool nsTextWidget::OnResize(nsSizeEvent &aEvent)
{
  return PR_FALSE;
}

//--------------------------------------------------------------
NS_METHOD nsTextWidget::SetPassword(PRBool aIsPassword)
{
  if (mWidget == nsnull && aIsPassword) {
    mMakePassword = PR_TRUE;
    return NS_OK;
  }
  gtk_entry_set_visibility(GTK_ENTRY(mWidget), aIsPassword);
#if 0
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
#endif
  nsTextHelper::SetPassword(aIsPassword);
  return NS_OK;
}
