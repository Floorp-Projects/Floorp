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

#include "nsISelectionMgr.h"

#include <glib.h>               // for gpointer type
// Forward class declarations don't seem to work for the next two:
#include <gtk/gtkwidget.h>      // for GtkWidget
#include <gtk/gtkselection.h>   // for GtkSelectionData

class ostrstream;

/**
 * Selection Manager for X11.
 * Owns the copied text, listens for selection request events.
 */

class nsSelectionMgr : public nsISelectionMgr
{
public:
  NS_DECL_ISUPPORTS

  nsSelectionMgr();
  virtual ~nsSelectionMgr();

  //
  // nsISelectionMgr methods:
  //
  NS_IMETHOD GetCopyOStream(ostream** aStream);

  NS_IMETHOD CopyToClipboard();

  // Other methods specific to X:
  static void SetTopLevelWidget(GtkWidget* w);

private:
  ostrstream* mCopyStream;

  static GtkWidget* sWidget;    // the app's top level widget, set by nsWindow

  void SelectionClear( GtkWidget *w,
                       GdkEventSelection *event );
  void SelectionRequestor( GtkWidget *w,
                           GtkSelectionData *selection_data );

  static void SelectionRequestCB( GtkWidget *widget, 
                                  GtkSelectionData *selection_data,
                                  guint info,
                                  guint time,
                                  gpointer data );
  static void SelectionClearCB( GtkWidget *widget, 
                                GdkEventSelection *event,
                                gpointer data );
};

nsresult NS_NewSelectionMgr(nsISelectionMgr** aInstancePtrResult);

