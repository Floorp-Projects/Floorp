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
#include "nsString.h"
#include "nsGtkEventHandler.h"

extern int mIsPasswordCallBacksInstalled;


NS_IMPL_ADDREF_INHERITED(nsTextWidget, nsWidget)
NS_IMPL_RELEASE_INHERITED(nsTextWidget, nsWidget)
NS_IMPL_QUERY_INTERFACE2(nsTextWidget, nsITextWidget, nsIWidget)


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
  // avoid freeing this twice in other destructors
  mTextWidget = nsnull;
}

//-------------------------------------------------------------------------
//
// Create the native Entry widget
//
//-------------------------------------------------------------------------
NS_IMETHODIMP nsTextWidget::CreateNative(GtkWidget *parentWindow)
{
  PRBool oldIsReadOnly;
  mWidget = gtk_entry_new();

  // used by nsTextHelper because nsTextArea needs a scrolled_window
  mTextWidget = mWidget;

  gtk_widget_set_name(mWidget, "nsTextWidget");

  /*
   * GTK's text widget does XIM for us, so we don't want to use the default key handler
   * which does XIM, so we connect to a non-XIM key event for the text widget
   */
  gtk_signal_connect_after(GTK_OBJECT(mWidget),
                     "key_press_event",
#ifdef USE_XIM
                     GTK_SIGNAL_FUNC(handle_key_press_event_for_text),
#else
                     GTK_SIGNAL_FUNC(handle_key_press_event),
#endif
                     this);
  gtk_signal_connect(GTK_OBJECT(mWidget),
                     "key_release_event",
#ifdef USE_XIM
                     GTK_SIGNAL_FUNC(handle_key_release_event_for_text),
#else
                     GTK_SIGNAL_FUNC(handle_key_release_event),
#endif
                     this);
  SetPassword(mIsPassword);
  SetReadOnly(mIsReadOnly, oldIsReadOnly);
  gtk_widget_show(mWidget);

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

  return NS_OK;
}

#ifdef USE_XIM
PRBool nsTextWidget::OnKey(nsKeyEvent &aEvent)
{
  if (mEventCallback) {
    return DispatchWindowEvent(&aEvent);
  }
  return PR_FALSE;
}
#endif
