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
#include <gtk/gtkinvisible.h>

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

  // nsIClipboard  
  NS_IMETHOD ForceDataToClipboard();


  // invisible widget.  also used by dragndrop
  static GtkWidget *sWidget;

protected:
  NS_IMETHOD SetNativeClipboardData();
  NS_IMETHOD GetNativeClipboardData(nsITransferable * aTransferable);

  PRBool  mIgnoreEmptyNotification;

  void AddTarget(GdkAtom aAtom);

  gint GetFormat(const char* aMimeStr);
  void RegisterFormat(gint format);


  PRBool DoRealConvert(GdkAtom type);
  PRBool DoConvert(gint format);

  void Init(void);

  // Used for communicating pasted data
  // from the asynchronous X routines back to a blocking paste:
  GtkSelectionData mSelectionData;
  PRBool mBlocking;

  void SelectionReceiver(GtkWidget *aWidget,
                         GtkSelectionData *aSD);

  /**
   * This is the callback which is called when another app
   * requests the selection.
   *
   * @param  widget The widget
   * @param  aSelectionData Selection data
   * @param  info Value passed in from the callback init
   * @param  time Time when the selection request came in
   */
  static void SelectionGetCB(GtkWidget *aWidget, 
                             GtkSelectionData *aSelectionData,
                             guint      /*info*/,
                             guint      /*time*/);

  /**
   * Called when another app requests selection ownership
   *
   * @param  aWidget the widget
   * @param  aEvent the GdkEvent for the selection
   * @param  aData value passed in from the callback init
   */
  static void SelectionClearCB(GtkWidget *aWidget, 
                               GdkEventSelection *aEvent,
                               gpointer aData);

  /**
   * The routine called when another app asks for the content of the selection
   *
   * @param  aWidget the widget
   * @param  aSelectionData gtk selection stuff
   * @param  aData value passed in from the callback init
   */
  static void SelectionRequestCB(GtkWidget *aWidget,
                                 GtkSelectionData *aSelectionData,
                                 gpointer data);
  
  /**
   * Called when the data from a paste comes in
   *
   * @param  aWidget the widget
   * @param  aSelectionData gtk selection stuff
   * @param  aTime time the selection was requested
   */
  static void SelectionReceivedCB(GtkWidget *aWidget,
                                  GtkSelectionData *aSelectionData,
                                  guint aTime);


  static void SelectionNotifyCB(GtkWidget *aWidget,
                                GtkSelectionData *aSelectionData,
                                gpointer aData);
  

};

#endif // nsClipboard_h__
