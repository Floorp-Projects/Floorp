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

#include "nsXtEventHandler.h"

#include <Xm/DialogS.h>

#define DBG 0
//-------------------------------------------------------------------------
//
// nsDialog constructor
//
//-------------------------------------------------------------------------
nsDialog::nsDialog(nsISupports *aOuter) : nsWindow(aOuter)
{
}

void nsDialog::Create(nsIWidget *aParent,
                      const nsRect &aRect,
                      EVENT_CALLBACK aHandleEventFunction,
                      nsIDeviceContext *aContext,
                      nsIAppShell *aAppShell,
                      nsIToolkit *aToolkit,
                      nsWidgetInitData *aInitData) 
{
  aParent->AddChild(this);
  Widget parentWidget = nsnull;

  if (DBG) fprintf(stderr, "aParent 0x%x\n", aParent);

  if (aParent) {
    parentWidget = (Widget) aParent->GetNativeData(NS_NATIVE_WIDGET);
  } else if (aAppShell) {
    parentWidget = (Widget) aAppShell->GetNativeData(NS_NATIVE_SHELL);
  }

  InitToolkit(aToolkit, aParent);
  InitDeviceContext(aContext, parentWidget);

  if (DBG) fprintf(stderr, "Parent 0x%x\n", parentWidget);


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

  // Initially used xmDrawingAreaWidgetClass instead of
  // newManageClass. Drawing area will spontaneously resize
  // to fit it's contents.

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

  if (DBG) fprintf(stderr, "Dialog 0x%x  this 0x%x\n", mWidget, this);

  // save the event callback function
  mEventCallback = aHandleEventFunction;

  InitCallbacks("nsDialog");

}

void nsDialog::Create(nsNativeWidget aParent,
                      const nsRect &aRect,
                      EVENT_CALLBACK aHandleEventFunction,
                      nsIDeviceContext *aContext,
                      nsIAppShell *aAppShell,
                      nsIToolkit *aToolkit,
                      nsWidgetInitData *aInitData)
{
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
nsresult nsDialog::QueryObject(REFNSIID aIID, void** aInstancePtr)
{
  static NS_DEFINE_IID(kIDialogIID,    NS_IDIALOG_IID);

  if (aIID.Equals(kIDialogIID)) {
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
void nsDialog::SetLabel(const nsString& aText)
{
  NS_ALLOC_STR_BUF(label, aText, 256);
  XmString str;
  // XXX What is up with this, why does label work better than str?
  str = XmStringCreate(label, XmFONTLIST_DEFAULT_TAG);
  XtVaSetValues(mShell, XmNtitle, label, nsnull);
  NS_FREE_STR_BUF(label);
  XmStringFree(str);

}

//-------------------------------------------------------------------------
//
// Get this button label
//
//-------------------------------------------------------------------------
void nsDialog::GetLabel(nsString& aBuffer)
{
  XmString str;
  XtVaGetValues(mShell, XmNtitle, &str, nsnull);
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
PRBool nsDialog::OnPaint(nsPaintEvent &aEvent)
{
  //printf("** nsDialog::OnPaint **\n");
  return PR_FALSE;
}

PRBool nsDialog::OnResize(nsSizeEvent &aEvent)
{
    return PR_FALSE;
}


#define GET_OUTER() ((nsDialog*) ((char*)this - nsDialog::GetOuterOffset()))

void nsDialog::AggDialog::GetLabel(nsString& aBuffer)
{
  GET_OUTER()->GetLabel(aBuffer);
}

void nsDialog::AggDialog::SetLabel(const nsString& aText)
{
  GET_OUTER()->SetLabel(aText);
}

//----------------------------------------------------------------------

BASE_IWIDGET_IMPL(nsDialog, AggDialog);


