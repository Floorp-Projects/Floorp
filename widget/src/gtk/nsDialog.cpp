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

//-------------------------------------------------------------------------
//
// nsDialog constructor
//
//-------------------------------------------------------------------------
nsDialog::nsDialog() : nsWindow(), nsIDialog()
{
  NS_INIT_REFCNT();
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
// nsDialog addref, release
//
//-------------------------------------------------------------------------
nsrefcnt
nsDialog::AddRef()
{
  return nsWindow::AddRef();
}

nsrefcnt
nsDialog::Release()
{
  return nsWindow::Release();
}

//-------------------------------------------------------------------------
//
// Create the native GtkDialog widget
//
//-------------------------------------------------------------------------

NS_METHOD nsDialog::CreateNative(GtkWidget *parentWindow)
{
  mShell = gtk_window_new(GTK_WINDOW_DIALOG);
  gtk_window_set_default_size(GTK_WINDOW(mShell),
                              mBounds.width,
                              mBounds.height);
  gtk_widget_set_name(mShell, "nsDialog");
  gtk_widget_show(mShell);
  mWidget = gtk_layout_new(PR_FALSE, PR_FALSE);
  gtk_container_add(GTK_CONTAINER(mShell), mWidget);
  gtk_widget_set_app_paintable(mWidget, PR_TRUE);

  return NS_OK;
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
