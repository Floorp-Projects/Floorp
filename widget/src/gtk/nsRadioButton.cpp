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

#include <gtk/gtk.h>

#include "nsRadioButton.h"
#include "nsString.h"

#include "nsGtkEventHandler.h"

NS_IMPL_ADDREF_INHERITED(nsRadioButton, nsWidget)
NS_IMPL_RELEASE_INHERITED(nsRadioButton, nsWidget)

//-------------------------------------------------------------------------
//
// nsRadioButton constructor
//
//-------------------------------------------------------------------------
nsRadioButton::nsRadioButton() : nsWidget(), nsIRadioButton()
{
  NS_INIT_REFCNT();
  mLabel = nsnull;
  mRadioButton = nsnull;
}


//-------------------------------------------------------------------------
//
// nsRadioButton destructor
//
//-------------------------------------------------------------------------
nsRadioButton::~nsRadioButton()
{
#if 0
  if (mLabel)
    gtk_widget_destroy(mLabel);
#endif
}

//-------------------------------------------------------------------------
//
// Query interface implementation
//
//-------------------------------------------------------------------------
nsresult nsRadioButton::QueryInterface(const nsIID& aIID, void** aInstancePtr)
{
  nsresult result = nsWidget::QueryInterface(aIID, aInstancePtr);

  if (result == NS_NOINTERFACE && aIID.Equals(NS_GET_IID(nsIRadioButton))) {
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
NS_METHOD  nsRadioButton::CreateNative(GtkObject *parentWindow)
{
  mWidget = gtk_event_box_new();
  mRadioButton = gtk_radio_button_new(nsnull);

  gtk_container_add(GTK_CONTAINER(mWidget), mRadioButton);

  gtk_widget_show(mRadioButton);

  gtk_widget_set_name(mWidget, "nsRadioButton");

  gtk_radio_button_set_group(GTK_RADIO_BUTTON(mRadioButton), nsnull);

  gtk_signal_connect(GTK_OBJECT(mRadioButton),
                     "destroy",
                     GTK_SIGNAL_FUNC(DestroySignal),
                     this);

  return NS_OK;
}

void
nsRadioButton::OnDestroySignal(GtkWidget* aGtkWidget)
{
  if (aGtkWidget == mLabel) {
    mLabel = nsnull;
  }
  else if (aGtkWidget == mRadioButton) {
    mRadioButton = nsnull;
  }
  else {
    nsWidget::OnDestroySignal(aGtkWidget);
  }
}

void nsRadioButton::InitCallbacks(char * aName)
{
  InstallButtonPressSignal(mRadioButton);
  InstallButtonReleaseSignal(mRadioButton);

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
NS_METHOD nsRadioButton::SetState(const PRBool aState)
{
  if (mWidget) {
    GtkToggleButton * item = GTK_TOGGLE_BUTTON(mRadioButton);
    item->active = (gboolean) aState;
    gtk_widget_queue_draw(GTK_WIDGET(item));
  }
  return NS_OK;
}

//-------------------------------------------------------------------------
//
// Set this button state
//-------------------------------------------------------------------------
NS_METHOD nsRadioButton::GetState(PRBool& aState)
{
  if (mWidget) {
    aState = (PRBool) GTK_TOGGLE_BUTTON(mRadioButton)->active;
  }
  else {
    aState = PR_TRUE;
  }
  return NS_OK;
}

//-------------------------------------------------------------------------
//
// Set this button label
//
//-------------------------------------------------------------------------
NS_METHOD nsRadioButton::SetLabel(const nsString& aText)
{
  if (mWidget) {
    NS_ALLOC_STR_BUF(label, aText, 256);
    g_print("nsRadioButton::SetLabel(%s)\n",label);
    if (mLabel) {
      gtk_label_set(GTK_LABEL(mLabel), label);
    } else {
      mLabel = gtk_label_new(label);
      gtk_misc_set_alignment (GTK_MISC (mLabel), 0.0, 0.5);
      gtk_container_add(GTK_CONTAINER(mRadioButton), mLabel);
      gtk_widget_show(mLabel); /* XXX */
      gtk_signal_connect(GTK_OBJECT(mLabel),
                         "destroy",
                         GTK_SIGNAL_FUNC(DestroySignal),
                         this);
    }
    NS_FREE_STR_BUF(label);
  }
  return NS_OK;
}


//-------------------------------------------------------------------------
//
// Get this button label
//
//-------------------------------------------------------------------------
NS_METHOD nsRadioButton::GetLabel(nsString& aBuffer)
{
  aBuffer.Truncate();
  if (mWidget) {
    if (mLabel) {
      char* text;
      gtk_label_get(GTK_LABEL(mLabel), &text);
      aBuffer.AppendWithConversion(text);
    }
  }
  return NS_OK;
}

//////////////////////////////////////////////////////////////////////
// SetBackgroundColor for RadioButton
/*virtual*/
void nsRadioButton::SetBackgroundColorNative(GdkColor *aColorNor,
                                        GdkColor *aColorBri,
                                        GdkColor *aColorDark)
{
  // use same style copy as SetFont
  GtkStyle *style = gtk_style_copy(GTK_WIDGET (g_list_nth_data(gtk_container_children(GTK_CONTAINER (mWidget)),0))->style);
  
  style->bg[GTK_STATE_NORMAL]=*aColorNor;
  
  // Mouse over button
  style->bg[GTK_STATE_PRELIGHT]=*aColorBri;

  // Button is down
  style->bg[GTK_STATE_ACTIVE]=*aColorDark;

  // other states too? (GTK_STATE_ACTIVE, GTK_STATE_PRELIGHT,
  //               GTK_STATE_SELECTED, GTK_STATE_INSENSITIVE)
  gtk_widget_set_style(GTK_WIDGET (g_list_nth_data(gtk_container_children(GTK_CONTAINER (mWidget)),0)), style);
  // set style for eventbox too
  gtk_widget_set_style(mWidget, style);

  gtk_style_unref(style);
}

