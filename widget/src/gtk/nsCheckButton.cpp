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

NS_IMPL_ADDREF_INHERITED(nsCheckButton, nsWidget)
NS_IMPL_RELEASE_INHERITED(nsCheckButton, nsWidget)

//-------------------------------------------------------------------------
//
// nsCheckButton constructor
//
//-------------------------------------------------------------------------
nsCheckButton::nsCheckButton() : nsWidget() , nsICheckButton()
{
  NS_INIT_REFCNT();
  mLabel = nsnull;
  mCheckButton = nsnull;
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
// Create the native CheckButton widget
//
//-------------------------------------------------------------------------
NS_METHOD  nsCheckButton::CreateNative(GtkWidget *parentWindow)
{
  mWidget = gtk_event_box_new();
  mCheckButton = gtk_check_button_new();

  gtk_container_add(GTK_CONTAINER(mWidget), mCheckButton);

  gtk_widget_show(mCheckButton);

  gtk_widget_set_name(mCheckButton, "nsCheckButton");

  return NS_OK;
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
    return nsWidget::QueryInterface(aIID,aInstancePtr);
}

//-------------------------------------------------------------------------
//
// Set this button label
//
//-------------------------------------------------------------------------
NS_METHOD nsCheckButton::SetState(const PRBool aState)
{
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(mCheckButton), aState);
  return NS_OK;
}

//-------------------------------------------------------------------------
//
// Set this button label
//
//-------------------------------------------------------------------------
NS_METHOD nsCheckButton::GetState(PRBool& aState)
{
  gint state = GTK_TOGGLE_BUTTON(mCheckButton)->active;
  aState = state;
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
  if (mLabel) {
    gtk_label_set(GTK_LABEL(mLabel), label);
  } else {
    mLabel = gtk_label_new(label);
    gtk_misc_set_alignment (GTK_MISC (mLabel), 0.0, 0.5);
    gtk_container_add(GTK_CONTAINER(mCheckButton), mLabel);
    gtk_widget_show(mLabel);
  }
  NS_FREE_STR_BUF(label);
  return NS_OK;
}


//-------------------------------------------------------------------------
//
// Get this button label
//
//-------------------------------------------------------------------------
NS_METHOD nsCheckButton::GetLabel(nsString& aBuffer)
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
PRBool nsCheckButton::OnMove(PRInt32, PRInt32)
{
  return PR_FALSE;
}

PRBool nsCheckButton::OnPaint(nsPaintEvent &aEvent)
{
  return PR_FALSE;
}

PRBool nsCheckButton::OnResize(nsSizeEvent &aEvent)
{
  return PR_FALSE;
}
