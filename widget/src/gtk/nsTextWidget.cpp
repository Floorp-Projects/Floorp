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
nsTextWidget::nsTextWidget() : nsTextHelper()
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
//
// Create the native Entry widget
//
//-------------------------------------------------------------------------
NS_METHOD nsTextWidget::CreateNative(GtkWidget *parentWindow)
{
  mWidget = gtk_entry_new();
  gtk_widget_set_name(mWidget, "nsTextWidget");
  gtk_signal_connect(GTK_OBJECT(mWidget),
                     "key_release_event",
                     GTK_SIGNAL_FUNC(nsGtkWidget_Text_Callback),
                     this);
  gtk_widget_show(mWidget);
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
