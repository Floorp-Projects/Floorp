/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
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
 * Portions created by the Initial Developer are Copyright (C) 1999-2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Stuart Parmenter <pavlov@netscape.com>
 *   Mike Pinkerton   <pinkerton@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef nsClipboard_h__
#define nsClipboard_h__

#include <gtk/gtk.h>
#include <gtk/gtkinvisible.h>

#include "nsIClipboard.h"
#include "nsITransferable.h"
#include "nsIClipboardOwner.h"
#include <nsCOMPtr.h>

class nsITransferable;
class nsIClipboardOwner;
class nsIWidget;

/**
 * Native Gtk Clipboard wrapper
 */

class nsClipboard : public nsIClipboard
{

public:
  nsClipboard();
  virtual ~nsClipboard();

  //nsISupports
  NS_DECL_ISUPPORTS

  // nsIClipboard
  NS_DECL_NSICLIPBOARD

protected:
  void Init(void);

  NS_IMETHOD SetNativeClipboardData(PRInt32 aWhichClipboard);
  NS_IMETHOD GetNativeClipboardData(nsITransferable * aTransferable, 
                                    PRInt32 aWhichClipboard );

  inline nsITransferable *GetTransferable(PRInt32 aWhichClipboard);


  PRBool  mIgnoreEmptyNotification;

private:
  inline GdkAtom GetSelectionAtom(PRInt32 aWhichClipboard);
  inline void AddTarget(GdkAtom aAtom, GdkAtom aSelectionAtom);
  void RegisterFormat(const char *aMimeStr, GdkAtom aSelectionAtom);
  PRBool DoRealConvert(GdkAtom type, GdkAtom aSelectionAtom);
  PRBool DoConvert(const char *aMimeStr, GdkAtom aSelectionAtom);
  PRBool GetTargets(GdkAtom aSelectionAtom);

  nsCOMPtr<nsIClipboardOwner> mSelectionOwner;
  nsCOMPtr<nsIClipboardOwner> mGlobalOwner;
  nsCOMPtr<nsITransferable>   mSelectionTransferable;
  nsCOMPtr<nsITransferable>   mGlobalTransferable;

  // Used for communicating pasted data
  // from the asynchronous X routines back to a blocking paste:
  GtkSelectionData mSelectionData;
  PRBool mBlocking;

  // invisible widget.  also used by dragndrop
  static GtkWidget *sWidget;

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
  

  /* functions to do blocking when trying to get info from another client */
  PRBool FindSelectionNotifyEvent();


  void SetCutBuffer();
};

#endif // nsClipboard_h__
