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

#ifndef nsClipboard_h__
#define nsClipboard_h__

#include "nsBaseClipboard.h"
#include <Clipboard.h>

class nsITransferable;
class nsIClipboardOwner;
class nsIWidget;

/**
 * Native BeOS Clipboard wrapper
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

  static void SetTopLevelView(BView *v);


protected:
  NS_IMETHOD SetNativeClipboardData( PRInt32 aWhichClipboard );
  NS_IMETHOD GetNativeClipboardData(nsITransferable * aTransferable, PRInt32 aWhichClipboard);

  PRBool            mIgnoreEmptyNotification;

  static BView  *sView;

//  // Used for communicating pasted data
//  // from the asynchronous X routines back to a blocking paste:
//  GtkSelectionData mSelectionData;
//  PRBool mBlocking;
//
//  void SelectionReceiver(GtkWidget *aWidget,
//                         GtkSelectionData *aSelectionData);
//
//  static void SelectionGetCB(GtkWidget *widget, 
//                             GtkSelectionData *selection_data,
//                             guint      /*info*/,
//                             guint      /*time*/,
//                             gpointer data);
//
//  static void SelectionClearCB(GtkWidget *widget, 
//                               GdkEventSelection *event,
//                               gpointer data );
//
//  static void SelectionRequestCB(GtkWidget *aWidget,
//                             GtkSelectionData *aSelectionData,
//                             gpointer aData);
//  
//  static void SelectionReceivedCB(GtkWidget *aWidget,
//                                  GtkSelectionData *aSelectionData,
//                                  gpointer aData);
//
//  static void SelectionNotifyCB(GtkWidget *aWidget,
//                                GtkSelectionData *aSelectionData,
//                                gpointer aData);
};

#endif // nsClipboard_h__
