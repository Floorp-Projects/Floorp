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

#include "nsGtkEventHandler.h"

#include "nsButton.h"
#include "nsString.h"

NS_IMPL_ADDREF_INHERITED(nsButton, nsWidget)
NS_IMPL_RELEASE_INHERITED(nsButton, nsWidget)
NS_IMPL_QUERY_INTERFACE2(nsButton, nsIButton, nsIWidget)

//-------------------------------------------------------------------------
//
// nsButton constructor
//
//-------------------------------------------------------------------------
nsButton::nsButton() : nsWidget() , nsIButton()
{
}

//-------------------------------------------------------------------------
//
// Create the native Button widget
//
//-------------------------------------------------------------------------
NS_IMETHODIMP nsButton::CreateNative(GtkWidget *parentWindow)
{
  mWidget = gtk_button_new_with_label("");
  gtk_widget_set_name(mWidget, "nsButton");
  return NS_OK;
}

//-------------------------------------------------------------------------
//
// nsButton destructor
//
//-------------------------------------------------------------------------
nsButton::~nsButton()
{
}

void nsButton::InitCallbacks(char * aName)
{
  InstallButtonPressSignal(mWidget);
  InstallButtonReleaseSignal(mWidget);

  InstallEnterNotifySignal(mWidget);
  InstallLeaveNotifySignal(mWidget);

  // These are needed so that the events will go to us and not our parent.
  AddToEventMask(mWidget,
                 GDK_BUTTON_PRESS_MASK |
                 GDK_BUTTON_RELEASE_MASK |
                 GDK_ENTER_NOTIFY_MASK |
                 GDK_EXPOSURE_MASK |
                 GDK_FOCUS_CHANGE_MASK |
                 GDK_KEY_PRESS_MASK |
                 GDK_KEY_RELEASE_MASK |
                 GDK_LEAVE_NOTIFY_MASK |
                 GDK_POINTER_MOTION_MASK);
}

//-------------------------------------------------------------------------
//
// Set this button label
//
//-------------------------------------------------------------------------
NS_IMETHODIMP nsButton::SetLabel(const nsString& aText)
{
  NS_ALLOC_STR_BUF(label, aText, 256);

  gtk_label_set(GTK_LABEL(GTK_BIN (mWidget)->child), label);

  NS_FREE_STR_BUF(label);
  return (NS_OK);
}

//-------------------------------------------------------------------------
//
// Get this button label
//
//-------------------------------------------------------------------------
NS_IMETHODIMP nsButton::GetLabel(nsString& aBuffer)
{
  char * text;

  gtk_label_get(GTK_LABEL(GTK_BIN (mWidget)->child), &text);
  aBuffer.SetLength(0);
  aBuffer.Append(text);

  return (NS_OK);

}

//-------------------------------------------------------------------------
//
// set font for button
//
//-------------------------------------------------------------------------
/* virtual */
void nsButton::SetFontNative(GdkFont *aFont)
{
  GtkStyle *style = gtk_style_copy(GTK_BIN (mWidget)->child->style);
  // gtk_style_copy ups the ref count of the font
  gdk_font_unref (style->font);
  
  style->font = aFont;
  gdk_font_ref(style->font);
  
  gtk_widget_set_style(GTK_BIN (mWidget)->child, style);
  
  gtk_style_unref(style);
}
