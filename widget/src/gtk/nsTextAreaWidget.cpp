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
#include "nsString.h"

NS_IMPL_ADDREF_INHERITED(nsTextAreaWidget, nsWidget)
NS_IMPL_RELEASE_INHERITED(nsTextAreaWidget, nsWidget)
NS_IMPL_QUERY_INTERFACE2(nsTextAreaWidget, nsITextAreaWidget, nsIWidget)

//-------------------------------------------------------------------------
//
// nsTextAreaWidget constructor
//
//-------------------------------------------------------------------------
nsTextAreaWidget::nsTextAreaWidget()
{
  mBackground = NS_RGB(124, 124, 124);
}

//-------------------------------------------------------------------------
//
// nsTextAreaWidget destructor
//
//-------------------------------------------------------------------------
nsTextAreaWidget::~nsTextAreaWidget()
{
  gtk_widget_destroy(mTextWidget);
  mTextWidget = nsnull;
}

//-------------------------------------------------------------------------
//
// Create the native Text widget
//
//-------------------------------------------------------------------------
NS_METHOD nsTextAreaWidget::CreateNative(GtkWidget *parentWindow)
{
  PRBool oldIsReadOnly;
  mWidget = gtk_scrolled_window_new(nsnull, nsnull);
  gtk_container_set_border_width(GTK_CONTAINER(mWidget), 0);
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(mWidget),
                                 GTK_POLICY_NEVER,
                                 GTK_POLICY_ALWAYS);
  
  mTextWidget = gtk_text_new(nsnull, nsnull);
  gtk_text_set_word_wrap(GTK_TEXT(mTextWidget), PR_TRUE);
  gtk_widget_set_name(mTextWidget, "nsTextAreaWidget");
  gtk_widget_show(mTextWidget);
  SetPassword(mIsPassword);
  SetReadOnly(mIsReadOnly, oldIsReadOnly);

  gtk_container_add(GTK_CONTAINER(mWidget), mTextWidget);

  return NS_OK;
}

//-------------------------------------------------------------------------
//
// set font for textarea
//
//-------------------------------------------------------------------------
/* virtual */
void nsTextAreaWidget::SetFontNative(GdkFont *aFont)
{
  GtkStyle *style = gtk_style_copy(GTK_WIDGET (g_list_nth_data(gtk_container_children(GTK_CONTAINER (mWidget)),0))->style);
  // gtk_style_copy ups the ref count of the font
  gdk_font_unref (style->font);
  
  style->font = aFont;
  gdk_font_ref(style->font);
  
  gtk_widget_set_style(GTK_WIDGET (g_list_nth_data(gtk_container_children(GTK_CONTAINER (mWidget)),0)), style);
  
  gtk_style_unref(style);
}
