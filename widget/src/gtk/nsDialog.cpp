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

#include "nsDialog.h"
#include "nsIDialog.h"
#include "nsToolkit.h"
#include "nsColor.h"
#include "nsGUIEvent.h"
#include "nsString.h"
#include "nsStringUtil.h"

#include <gtk/gtk.h>

NS_IMPL_ADDREF(nsDialog)
NS_IMPL_RELEASE(nsDialog)

//-------------------------------------------------------------------------
//
// nsDialog constructor
//
//-------------------------------------------------------------------------
nsDialog::nsDialog() : nsWidget(), nsIDialog()
{
  NS_INIT_REFCNT();
}

//-------------------------------------------------------------------------
NS_METHOD nsDialog::Create(nsIWidget *aParent,
                      const nsRect &aRect,
                      EVENT_CALLBACK aHandleEventFunction,
                      nsIDeviceContext *aContext,
                      nsIAppShell *aAppShell,
                      nsIToolkit *aToolkit,
                      nsWidgetInitData *aInitData)
{
  aParent->AddChild(this);
  GtkWidget *parentWidget = nsnull;

  if (aParent) {
    parentWidget = (GtkWidget *) aParent->GetNativeData(NS_NATIVE_WIDGET);
  } else if (aAppShell) {
    parentWidget = (GtkWidget *) aAppShell->GetNativeData(NS_NATIVE_SHELL);
  }

  InitToolkit(aToolkit, aParent);
  InitDeviceContext(aContext, parentWidget);

  mShell = gtk_dialog_new();
/*
  mShell = ::XtVaCreateManagedWidget("Dialog",
                                    xmDialogShellWidgetClass,
                                    parentWidget,
                                    XmNwidth, aRect.width,
                                    XmNheight, aRect.height,
                                    XmNrecomputeSize, False,
                                    XmNhighlightOnEnter, False,
		                    XmNx, aRect.x,
		                    XmNy, aRect.y,
                                    nsnull);
*/
  // Must use newManageClass instead of xmDrawingAreaWidgetClass
  // Drawing area will spontaneously resize to fit it's contents
  // which is undesirable.

  /*mWidget = mShell->vbox; */
  /*
  mWidget = ::XtVaCreateManagedWidget("drawingArea",
                                    newManageClass,
                                    mShell,
                                    XmNwidth, aRect.width,
                                    XmNheight, aRect.height,
                                    XmNmarginHeight, 0,
                                    XmNmarginWidth, 0,
                                    XmNrecomputeSize, False,
                                    XmNuserData, this,
                                    nsnull);
*/
  // save the event callback function
  mEventCallback = aHandleEventFunction;

  InitCallbacks("nsDialog");
  return NS_OK;

}

NS_METHOD nsDialog::Create(nsNativeWidget aParent,
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
// nsDialog destructor
//
//-------------------------------------------------------------------------
nsDialog::~nsDialog()
{
}

//-------------------------------------------------------------------------
//
// Query interface implementation
//
//-------------------------------------------------------------------------
nsresult nsDialog::QueryInterface(const nsIID& aIID, void** aInstancePtr)
{
  nsresult result = nsWidget::QueryInterface(aIID, aInstancePtr);

  static NS_DEFINE_IID(kInsDialogIID, NS_IDIALOG_IID);
  if (result == NS_NOINTERFACE && aIID.Equals(kInsDialogIID)) {
      *aInstancePtr = (void*) ((nsIDialog*)this);
      AddRef();
      result = NS_OK;
  }

  return result;
}


//-------------------------------------------------------------------------
//
// Set this button label
//
//-------------------------------------------------------------------------
NS_METHOD nsDialog::SetLabel(const nsString& aText)
{
// Why are we setting a label in a dialog??
/*
  NS_ALLOC_STR_BUF(label, aText, 256);
  XmString str;
  // XXX What is up with this, why does label work better than str?
  str = XmStringCreate(label, XmFONTLIST_DEFAULT_TAG);
  XtVaSetValues(mShell, XmNtitle, label, nsnull);
  NS_FREE_STR_BUF(label);
  XmStringFree(str);
*/
  return NS_OK;
}

//-------------------------------------------------------------------------
//
// Get this button label
//
//-------------------------------------------------------------------------
NS_METHOD nsDialog::GetLabel(nsString& aBuffer)
{
// Why are we using a label in a dialog??
/*
  XmString str;
  XtVaGetValues(mShell, XmNtitle, &str, nsnull);
  char * text;
  if (XmStringGetLtoR(str, XmFONTLIST_DEFAULT_TAG, &text)) {
    aBuffer.SetLength(0);
    aBuffer.Append(text);
    XtFree(text);
  }

  XmStringFree(str);
*/
  return NS_OK;

}

//-------------------------------------------------------------------------
//
// paint message. Don't send the paint out
//
//-------------------------------------------------------------------------
PRBool nsDialog::OnPaint(nsPaintEvent &aEvent)
{
  return PR_FALSE;
}

PRBool nsDialog::OnResize(nsSizeEvent &aEvent)
{
    return PR_FALSE;
}
