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

//
// gtk/nsSelectionMgr: the class which handles X selection for DoCopy.
//
// Xheads: One thing we might want to change later:
// Currently it's necessary to do edit->copy (or keyboard equivalent)
// to get selection info to this class.
// The PresShell does a DoCopy, which calls the parser and creates a
// content stream with the contents of the then-current selection.
// X users may be more comfortable with having this happen every time
// the selection is changed.  This could be done in two ways:
//
// 1. Have the mozilla selection class (nsRangeList) call nsSelectionMgr
//    explicitly every time the selection changes.  We have to make sure
//    that this isn't expensive, since selection changes happen frequently,
//    and that it doesn't happen by default on the other platforms, since
//    nsRangeList is XP code and the other two platforms wouldn't like
//    this behavior.
//
// 2. Have nsSelectionMgr::SelectionRequestor query for the current
//    selection each time it's called, and throw away the stream passed in
//    (or, better, change the interface not to pass a stream in in the
//    first place).
//    This requires that nsSelectionMgr have access to the current selection,
//    which probably means that when the PresShell is creating nsSelectionMgr,
//    it pass a pointer to itself along, and the SelectionMgr just queries
//    the PresShell whenever it wants the selection.
//    This seems like a much cleaner solution, but unfortunately does
//    require an interface change on all three platforms.
//
// #2 will probably happen soon.  Talk to me (akkana@netscape.com) if
// you're reading this and have an opinion about this.
//

#include "nsSelectionMgr.h"

#include <strstream.h>

#include <gtk/gtksignal.h>

#include <stdio.h>

NS_IMPL_ADDREF(nsSelectionMgr)

NS_IMPL_RELEASE(nsSelectionMgr)

GtkWidget* nsSelectionMgr::sWidget = 0;

nsresult nsSelectionMgr::QueryInterface(const nsIID& aIID,
                                        void** aInstancePtrResult)
{
  NS_PRECONDITION(aInstancePtrResult, "null pointer");
  if (!aInstancePtrResult) 
  {
    return NS_ERROR_NULL_POINTER;
  }
  if (aIID.Equals(nsISupports::IID())) 
  {
    *aInstancePtrResult = (void*)(nsISupports*)this;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(nsISelectionMgr::IID())) 
  {
    *aInstancePtrResult = (void*)(nsISelectionMgr*)this;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  return NS_ERROR_FAILURE;
}

nsSelectionMgr::nsSelectionMgr()
{
  NS_INIT_REFCNT();

  mCopyStream = 0;
  sWidget = 0;
}

nsSelectionMgr::~nsSelectionMgr()
{
  // Remove all our event handlers:
  if (sWidget &&
      gdk_selection_owner_get (GDK_SELECTION_PRIMARY) == sWidget->window)
    gtk_selection_remove_all(sWidget);
  if (mCopyStream)
    delete mCopyStream;
}

void nsSelectionMgr::SetTopLevelWidget(GtkWidget* w)
{
  // Don't set up any more event handlers if we're being called twice
  // for the same toplevel widget
  if (sWidget == w)
    return;

  sWidget = w;
}

nsresult nsSelectionMgr::GetCopyOStream(ostream** aStream)
{
  if (mCopyStream)
    delete mCopyStream;
  mCopyStream = new ostrstream;
  *aStream = mCopyStream;
  return NS_OK;
}

// Called when another app requests the selection:
void nsSelectionMgr::SelectionClearCB( GtkWidget *widget,
                                       GdkEventSelection *event,
                                       gpointer data)
{
  if (data)
    ((nsSelectionMgr*)data)->SelectionClearor(widget, event);
}

void nsSelectionMgr::SelectionClearor( GtkWidget *w,
                                       GdkEventSelection *event )
{
  // Delete the copy stream, since we don't need it any more:
  if (mCopyStream)
    delete mCopyStream;
  mCopyStream = 0;
}

//
// Here follows a bunch of code which came from GTK's gtktestselection.c:
//

// XXX Scary -- this list doesn't seem to be in an include file anywhere!
// Apparently every app which uses gtk selection is expected to copy
// this list verbatim, and hope that it doesn't get out of sync!  Oy.
typedef enum {
  SEL_TYPE_NONE,
  APPLE_PICT,
  ATOM,
  ATOM_PAIR,
  BITMAP,
  C_STRING,
  COLORMAP,
  COMPOUND_TEXT,
  DRAWABLE,
  INTEGER,
  PIXEL,
  PIXMAP,
  SPAN,
  STRING,
  TEXT,
  WINDOW,
  LAST_SEL_TYPE
} SelType;

//
// The types we support
//
static GtkTargetEntry targetlist[] = {
  { "STRING",        0, STRING },
  { "TEXT",          0, TEXT },
  { "COMPOUND_TEXT", 0, COMPOUND_TEXT }
  // Probably need to add entries for XIF and HTML
};
static gint ntargets = sizeof(targetlist) / sizeof(targetlist[0]);

//
// The event handler to handle selection requests:
//
void nsSelectionMgr::SelectionRequestCB( GtkWidget        *widget, 
                                         GtkSelectionData *selection_data,
                                         guint      /*info*/,
                                         guint      /*time*/,
                                         gpointer   data)
{
  if (data)
    ((nsSelectionMgr*)data)->SelectionRequestor(widget, selection_data);
}

//
// SelectionRequestor:
// This is the routine which gets called when another app
// requests the selection
//
void nsSelectionMgr::SelectionRequestor( GtkWidget        *widget, 
                                         GtkSelectionData *selection_data )
{
  if (!mCopyStream)
    return;

  guchar* str = (guchar*)(mCopyStream->str());

  gtk_selection_data_set (selection_data, GDK_SELECTION_TYPE_STRING,
                          8, str, strlen((char*)str));
  // the format arg, "8", indicates string data with no endianness
}

//
// CopyToClipboard:
// This is the routine which gets called when the user selects edit->copy.
//
nsresult nsSelectionMgr::CopyToClipboard()
{
  // we'd better already have a stream and a widget ...
  if (!mCopyStream || !sWidget)
      return NS_ERROR_NOT_INITIALIZED;

  // If we're already the selection owner, don't need to do anything,
  // we're already handling the events:
  if (gdk_selection_owner_get (GDK_SELECTION_PRIMARY) == sWidget->window)
    return NS_OK;

  // register as the selection owner:
  gint have_selection = gtk_selection_owner_set(sWidget,
                                                GDK_SELECTION_PRIMARY,
                                                GDK_CURRENT_TIME);
  if (!have_selection)
  {
#ifdef NS_DEBUG
    printf("Couldn't claim primary selection\n");
#endif
    return NS_ERROR_FAILURE;
  }

  // Set up all the event handlers
  gtk_selection_add_targets (sWidget, GDK_SELECTION_PRIMARY,
                             targetlist, ntargets);
  // Respond to requests for the selection:
  gtk_signal_connect(GTK_OBJECT(sWidget),
                     "selection_get",
                     GTK_SIGNAL_FUNC(nsSelectionMgr::SelectionRequestCB),
                     this);
  gtk_signal_connect(GTK_OBJECT(sWidget), "selection_clear_event",
                     GTK_SIGNAL_FUNC (nsSelectionMgr::SelectionClearCB),
                     this);

  // Other signals we should handle in some fashion:
  //"selection_received"  (when we've requested a paste and it's come in --
  //   this may need to be handled by a different class, e.g. nsIEditor.
  //"selection_request_event" (dunno what this one is, maybe we don't need it)
  //"selection_notify_event" (don't know what this is either)

  return NS_OK;
}

nsresult NS_NewSelectionMgr(nsISelectionMgr** aInstancePtrResult)
{
  nsSelectionMgr* sm = new nsSelectionMgr;
  static nsIID iid = NS_ISELECTIONMGR_IID;
  return sm->QueryInterface(iid, (void**) aInstancePtrResult);
}


