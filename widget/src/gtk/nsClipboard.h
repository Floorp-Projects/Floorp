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

#ifndef nsClipboard_h__
#define nsClipboard_h__

#include "nsBaseClipboard.h"
#include <gtk/gtk.h>

class nsITransferable;
class nsIClipboardOwner;
class nsIWidget;

/**
 * Native Gtk Clipboard wrapper
 */

class nsClipboard : public nsBaseClipboard
{

public:
  nsClipboard();
  virtual ~nsClipboard();

  //nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // nsIClipboard  
  NS_IMETHOD ForceDataToClipboard();

  static void SetTopLevelWidget(GtkWidget* w);


protected:
  NS_IMETHOD SetNativeClipboardData();
  NS_IMETHOD GetNativeClipboardData(nsITransferable * aTransferable);

  PRBool            mIgnoreEmptyNotification;

  nsIWidget         *mWindow;
  static GtkWidget  *sWidget;

  static void SelectionGetCB(GtkWidget *widget, 
                             GtkSelectionData *selection_data,
                             gpointer data);

  static void SelectionClearCB(GtkWidget *widget, 
                               GdkEventSelection *event,
                               gpointer data );

  static void SelectionRequestCB(GtkWidget *aWidget,
                             GtkSelectionData *aSelectionData,
                             gpointer aData);
  
  static void SelectionReceivedCB(GtkWidget *aWidget,
                                  GtkSelectionData *aSelectionData,
                                  gpointer aData);

  static void SelectionNotifyCB(GtkWidget *aWidget,
                                GtkSelectionData *aSelectionData,
                                gpointer aData);
  

};

#endif // nsClipboard_h__
