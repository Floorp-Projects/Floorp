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

#include "nsTextAreaWidget.h"
#include "nsString.h"
#include "nslog.h"

NS_IMPL_LOG(nsTextAreaWidgetLog)
#define PRINTF NS_LOG_PRINTF(nsTextAreaWidgetLog)
#define FLUSH  NS_LOG_FLUSH(nsTextAreaWidgetLog)

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

  PRINTF("Parent 0x%x\n", (unsigned int)parentWidget);

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

  return nsWindow::QueryInterface(aIID, aInstancePtr);
}
