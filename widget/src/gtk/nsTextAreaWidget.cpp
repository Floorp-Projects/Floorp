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

#include "nsTextAreaWidget.h"
#include "nsToolkit.h"
#include "nsColor.h"
#include "nsGUIEvent.h"
#include "nsString.h"
#include "nsGtkEventHandler.h"

#define DBG 0

NS_IMPL_ADDREF(nsTextAreaWidget)
NS_IMPL_RELEASE(nsTextAreaWidget)

//-------------------------------------------------------------------------
//
// nsTextAreaWidget constructor
//
//-------------------------------------------------------------------------
nsTextAreaWidget::nsTextAreaWidget()
{
  mMakeReadOnly = PR_FALSE;
  mBackground = NS_RGB(124, 124, 124);
}

//-------------------------------------------------------------------------
//
// nsTextAreaWidget destructor
//
//-------------------------------------------------------------------------
nsTextAreaWidget::~nsTextAreaWidget()
{
}

//-------------------------------------------------------------------------
NS_METHOD nsTextAreaWidget::Create(nsIWidget *aParent,
                      const nsRect &aRect,
                      EVENT_CALLBACK aHandleEventFunction,
                      nsIDeviceContext *aContext,
                      nsIAppShell *aAppShell,
                      nsIToolkit *aToolkit,
                      nsWidgetInitData *aInitData)
{
#if 0
  aParent->AddChild(this);
  Widget parentWidget = nsnull;

  if (DBG) fprintf(stderr, "aParent 0x%x\n", aParent);

  if (aParent) {
    parentWidget = (Widget) aParent->GetNativeData(NS_NATIVE_WIDGET);
  } else {
    parentWidget = (Widget) aAppShell->GetNativeData(NS_NATIVE_SHELL);
  }

  InitToolkit(aToolkit, aParent);
  InitDeviceContext(aContext, parentWidget);

  if (DBG) fprintf(stderr, "Parent 0x%x\n", parentWidget);

  mWidget = ::XtVaCreateManagedWidget("button",
                                    xmTextWidgetClass,
                                    parentWidget,
                                    XmNwidth, aRect.width,
                                    XmNheight, aRect.height,
                                    XmNrecomputeSize, False,
                                    XmNhighlightOnEnter, False,
                                    XmNeditMode, XmMULTI_LINE_EDIT,
                                    XmNeditable, mMakeReadOnly?False:True,
		                    XmNx, aRect.x,
		                    XmNy, aRect.y,
                                    nsnull);

  // save the event callback function
  mEventCallback = aHandleEventFunction;

  InitCallbacks("nsTextAreaWidget");

  if (mMakeReadOnly) {
    PRBool oldReadOnly;
    SetReadOnly(PR_TRUE, oldReadOnly);
  }
#endif
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsTextAreaWidget::Create(nsNativeWidget aParent,
                      const nsRect &aRect,
                      EVENT_CALLBACK aHandleEventFunction,
                      nsIDeviceContext *aContext,
                      nsIAppShell *aAppShell,
                      nsIToolkit *aToolkit,
                      nsWidgetInitData *aInitData)
{
  return NS_OK;
}


nsresult nsTextAreaWidget::QueryInterface(const nsIID& aIID, void** aInstancePtr)
{

  static NS_DEFINE_IID(kITextAreaWidgetIID, NS_ITEXTAREAWIDGET_IID);
  static NS_DEFINE_IID(kIWidgetIID, NS_IWIDGET_IID);


  if (aIID.Equals(kITextAreaWidgetIID)) {
      nsITextAreaWidget* textArea = this;
      *aInstancePtr = (void*) (textArea);
      AddRef();
      return NS_OK;
  }
  else if (aIID.Equals(kIWidgetIID))
  {
      nsIWidget* widget = this;
      *aInstancePtr = (void*) (widget);
      AddRef();
      return NS_OK;
  }

  return nsWidget::QueryInterface(aIID, aInstancePtr);
}

//-------------------------------------------------------------------------
//
// paint, resizes message - ignore
//
//-------------------------------------------------------------------------
PRBool nsTextAreaWidget::OnPaint(nsPaintEvent & aEvent)
{
  return PR_FALSE;
}


//--------------------------------------------------------------
PRBool nsTextAreaWidget::OnResize(nsSizeEvent &aEvent)
{
  return PR_FALSE;
}
