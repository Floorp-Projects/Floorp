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

#include "nsRadioButton.h"
#include "nsColor.h"
#include "nsGUIEvent.h"
#include "nsString.h"
#include "nsStringUtil.h"

#include "nsGtkEventHandler.h"

NS_IMPL_ADDREF(nsRadioButton)
NS_IMPL_RELEASE(nsRadioButton)

//-------------------------------------------------------------------------
//
// nsRadioButton constructor
//
//-------------------------------------------------------------------------
nsRadioButton::nsRadioButton() : nsWidget(), nsIRadioButton()
{
  NS_INIT_REFCNT();
}


//-------------------------------------------------------------------------
//
// nsRadioButton destructor
//
//-------------------------------------------------------------------------
nsRadioButton::~nsRadioButton()
{
}


//-------------------------------------------------------------------------
//
// Query interface implementation
//
//-------------------------------------------------------------------------
nsresult nsRadioButton::QueryInterface(const nsIID& aIID, void** aInstancePtr)
{
  nsresult result = nsWidget::QueryInterface(aIID, aInstancePtr);

  static NS_DEFINE_IID(kIRadioButtonIID, NS_IRADIOBUTTON_IID);
  if (result == NS_NOINTERFACE && aIID.Equals(kIRadioButtonIID)) {
      *aInstancePtr = (void*) ((nsIRadioButton*)this);
      AddRef();
      result = NS_OK;
  }
  return result;
}


//-------------------------------------------------------------------------
//
// Create the native RadioButton widget
//
//-------------------------------------------------------------------------
NS_METHOD  nsRadioButton::CreateNative(GtkWidget *parentWindow)
{
  mWidget = gtk_radio_button_new(nsnull);
  gtk_widget_set_name(mWidget, "nsRadioButton");

  return NS_OK;
}

//-------------------------------------------------------------------------
//
// Armed
//
//-------------------------------------------------------------------------
void nsRadioButton::Armed()
{
  mIsArmed      = PR_TRUE;
  mValueWasSet  = PR_FALSE;
  mInitialState = GTK_TOGGLE_BUTTON(mWidget)->active;
}

//-------------------------------------------------------------------------
//
// DisArmed
//
//-------------------------------------------------------------------------
void nsRadioButton::DisArmed()
{
  if (mValueWasSet) {
    gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(mWidget), PR_TRUE);
//    XmToggleButtonSetState(mRadioBtn, mNewValue, PR_TRUE);
  } else {
    gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(mWidget), PR_TRUE);
//    XmToggleButtonSetState(mRadioBtn, mInitialState, PR_TRUE);
  }
  mIsArmed = PR_FALSE;
}

//-------------------------------------------------------------------------
//
// Set this button label
//
//-------------------------------------------------------------------------
NS_METHOD nsRadioButton::SetState(const PRBool aState)
{
  int state = aState;
  if (mIsArmed) {
    mNewValue    = aState;
    mValueWasSet = PR_TRUE;
  }
//  XmToggleButtonSetState(mRadioBtn, aState, PR_TRUE);
  gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(mWidget), aState);
  return NS_OK;
}

//-------------------------------------------------------------------------
//
// Set this button state
//-------------------------------------------------------------------------
NS_METHOD nsRadioButton::GetState(PRBool& aState)
{
//  int state = XmToggleButtonGetState(mRadioBtn);
  int state = GTK_TOGGLE_BUTTON(mWidget)->active;
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
NS_METHOD nsRadioButton::SetLabel(const nsString& aText)
{
  NS_ALLOC_STR_BUF(label, aText, 256);
  if (mLabel) {
    gtk_label_set(GTK_LABEL(mLabel), label);
  } else {
    mLabel = gtk_label_new(label);
    gtk_misc_set_alignment (GTK_MISC (mLabel), 0.0, 0.5);
    gtk_container_add(GTK_CONTAINER(mWidget), mLabel);
    gtk_widget_show(mLabel); /* XXX */
  }
  NS_FREE_STR_BUF(label);
  return NS_OK;
}


//-------------------------------------------------------------------------
//
// Get this button label
//
//-------------------------------------------------------------------------
NS_METHOD nsRadioButton::GetLabel(nsString& aBuffer)
{
  char * text;
  if (mLabel) {
    gtk_label_get(GTK_LABEL(mLabel), &text);
    aBuffer.SetLength(0);
    aBuffer.Append(text);
  }
  return NS_OK;
}

//-------------------------------------------------------------------------
//
// move, paint, resizes message - ignore
//
//-------------------------------------------------------------------------
PRBool nsRadioButton::OnMove(PRInt32, PRInt32)
{
  return PR_FALSE;
}

PRBool nsRadioButton::OnPaint(nsPaintEvent &aEvent)
{
  return PR_FALSE;
}

PRBool nsRadioButton::OnResize(nsSizeEvent &aEvent)
{
    return PR_FALSE;
}
