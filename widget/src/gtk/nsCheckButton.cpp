/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsCheckButton.h"
#include "nsString.h"

NS_IMPL_ADDREF_INHERITED(nsCheckButton, nsWidget)
NS_IMPL_RELEASE_INHERITED(nsCheckButton, nsWidget)
NS_IMPL_QUERY_INTERFACE2(nsCheckButton, nsICheckButton, nsIWidget)

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
  mState = PR_FALSE;
}

//-------------------------------------------------------------------------
//
// nsCheckButton destructor
//
//-------------------------------------------------------------------------
nsCheckButton::~nsCheckButton()
{
}

void
nsCheckButton::OnDestroySignal(GtkWidget* aGtkWidget)
{
  if (aGtkWidget == mCheckButton) {
    mCheckButton = nsnull;
  }
  else if (aGtkWidget == mLabel) {
    mLabel = nsnull;
  }
  else {
    nsWidget::OnDestroySignal(aGtkWidget);
  }
}

//-------------------------------------------------------------------------
//
// Create the native CheckButton widget
//
//-------------------------------------------------------------------------
NS_METHOD  nsCheckButton::CreateNative(GtkObject *parentWindow)
{
  mWidget = gtk_event_box_new();
  mCheckButton = gtk_check_button_new();

  gtk_container_add(GTK_CONTAINER(mWidget), mCheckButton);

  gtk_widget_show(mCheckButton);

  gtk_widget_set_name(mWidget, "nsCheckButton");

  return NS_OK;
}

void nsCheckButton::InitCallbacks(char * aName)
{
  InstallButtonPressSignal(mCheckButton);
  InstallButtonReleaseSignal(mCheckButton);

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

  // Add in destroy callback
  gtk_signal_connect(GTK_OBJECT(mCheckButton),
                     "destroy",
                     GTK_SIGNAL_FUNC(DestroySignal),
                     this);

  InstallSignal((GtkWidget *)mCheckButton,
                (gchar *)"toggled",
                GTK_SIGNAL_FUNC(nsCheckButton::ToggledSignal));
}

//-------------------------------------------------------------------------
//
// Set this button label
//
//-------------------------------------------------------------------------
NS_METHOD nsCheckButton::SetState(const PRBool aState)
{
  mState = aState;

  if (mWidget && mCheckButton) 
  {
    GtkToggleButton * item = GTK_TOGGLE_BUTTON(mCheckButton);

    item->active = (gboolean) mState;

    gtk_widget_queue_draw(GTK_WIDGET(item));
  }

  return NS_OK;
}

//-------------------------------------------------------------------------
//
// Set this button label
//
//-------------------------------------------------------------------------
NS_METHOD nsCheckButton::GetState(PRBool& aState)
{
  aState = mState;
    
  return NS_OK;
}

//-------------------------------------------------------------------------
//
// Set this Checkbox label
//
//-------------------------------------------------------------------------
NS_METHOD nsCheckButton::SetLabel(const nsString& aText)
{
  if (mWidget) {
    NS_ALLOC_STR_BUF(label, aText, 256);
    if (mLabel) {
      gtk_label_set(GTK_LABEL(mLabel), label);
    } else {
      mLabel = gtk_label_new(label);
      gtk_misc_set_alignment (GTK_MISC (mLabel), 0.0, 0.5);
      gtk_container_add(GTK_CONTAINER(mCheckButton), mLabel);
      gtk_widget_show(mLabel);
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
NS_METHOD nsCheckButton::GetLabel(nsString& aBuffer)
{
  aBuffer.SetLength(0);
  if (mWidget) {
    char * text;
    if (mLabel) {
      gtk_label_get(GTK_LABEL(mLabel), &text);
      aBuffer.AppendWithConversion(text);
    }
  }
  return NS_OK;
}

/* virtual */ void
nsCheckButton::OnToggledSignal(const gboolean aState)
{
  // Untoggle the sonofabitch
  if (mWidget && mCheckButton) 
  {
    GtkToggleButton * item = GTK_TOGGLE_BUTTON(mCheckButton);

    item->active = !item->active;

    gtk_widget_queue_draw(GTK_WIDGET(item));
  }
}
//////////////////////////////////////////////////////////////////////

/* static */ gint 
nsCheckButton::ToggledSignal(GtkWidget *      aWidget, 
                             gpointer         aData)
{
  NS_ASSERTION( nsnull != aWidget, "widget is null");

  nsCheckButton * button = (nsCheckButton *) aData;

  NS_ASSERTION( nsnull != button, "instance pointer is null");

  button->OnToggledSignal(GTK_TOGGLE_BUTTON(aWidget)->active);

  return PR_TRUE;
}

//////////////////////////////////////////////////////////////////////
// SetBackgroundColor for CheckButton
/*virtual*/
void nsCheckButton::SetBackgroundColorNative(GdkColor *aColorNor,
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

