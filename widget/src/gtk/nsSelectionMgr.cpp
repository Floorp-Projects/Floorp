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
#include <gtk/gtk.h>
#include "nsString.h"
#include <stdio.h>

// XXX BWEEP BWEEP This is ONLY TEMPORARY until the service manager
// has a way of registering instances
// (see http://bugzilla.mozilla.org/show_bug.cgi?id=3509 ).
nsISelectionMgr* theSelectionMgr = 0;

extern "C" NS_EXPORT nsISelectionMgr*
GetSelectionMgr()
{
  return theSelectionMgr;
}
// BWEEP BWEEP

//
// nsISelectionMgr interface
//
NS_IMPL_ADDREF(nsSelectionMgr)
NS_IMPL_RELEASE(nsSelectionMgr)

// The class statics:
GtkWidget* nsSelectionMgr::sWidget = 0;

nsresult nsSelectionMgr::QueryInterface(const nsIID& aIID,
                                        void** aInstancePtrResult)
{
  NS_PRECONDITION(aInstancePtrResult, "null pointer");
  if (!aInstancePtrResult) 
  {
    return NS_ERROR_NULL_POINTER;
  }
  if (aIID.Equals(nsISupports::GetIID())) 
  {
    *aInstancePtrResult = (void*)(nsISupports*)this;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(nsISelectionMgr::GetIID())) 
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
  mBlocking = PR_FALSE;

  // XXX BWEEP BWEEP see comment above
  theSelectionMgr = this;
}

nsSelectionMgr::~nsSelectionMgr()
{
  // Remove all our event handlers:
  if (sWidget &&
      gdk_selection_owner_get (GDK_SELECTION_PRIMARY) == sWidget->window)
    gtk_selection_remove_all(sWidget);
  if (mCopyStream)
    delete mCopyStream;
  mCopyStream = 0;

  // XXX BWEEP BWEEP see comment above
  if (theSelectionMgr == this)
    theSelectionMgr = 0;
}

nsresult nsSelectionMgr::GetCopyOStream(ostream** aStream)
{
  if (mCopyStream)
    delete mCopyStream;
  mCopyStream = new ostrstream;
  *aStream = mCopyStream;
  return NS_OK;
}

nsresult NS_NewSelectionMgr(nsISelectionMgr** aInstancePtrResult)
{
#ifndef NEW_CLIPBOARD_SUPPORT
  nsSelectionMgr* sm = new nsSelectionMgr;
  static nsIID iid = NS_ISELECTIONMGR_IID;
  return sm->QueryInterface(iid, (void**) aInstancePtrResult);
#else
  // Need to return something here to not break the build on Solaris CC.
  return NS_OK;
#endif
}

//
// End of nsISelectionMgr interface
//

//
// X/gtk specific stuff:
//

void nsSelectionMgr::SetTopLevelWidget(GtkWidget* w)
{
  // Don't set up any more event handlers if we're being called twice
  // for the same toplevel widget
  if (sWidget == w)
    return;

  sWidget = w;

  // Respond to requests for the selection:
  gtk_signal_connect(GTK_OBJECT(sWidget),
                     "selection_get",
                     GTK_SIGNAL_FUNC(nsSelectionMgr::SelectionRequestCB),
                     theSelectionMgr);

  // When someone else takes the selection away:
  gtk_signal_connect(GTK_OBJECT(sWidget), "selection_clear_event",
                     GTK_SIGNAL_FUNC(nsSelectionMgr::SelectionClearCB),
                     theSelectionMgr);

  // Set up the paste handler:
  gtk_signal_connect(GTK_OBJECT(sWidget), "selection_received",
                     GTK_SIGNAL_FUNC(nsSelectionMgr::SelectionReceivedCB),
                     theSelectionMgr);

  // Hmm, sometimes we need this, sometimes not.  I'm not clear why:
  // Register all the target types we handle:
  gtk_selection_add_target(sWidget, 
                           GDK_SELECTION_PRIMARY,
                           GDK_SELECTION_TYPE_STRING,
                           GDK_SELECTION_TYPE_STRING);
  // Need to add entries for whatever it is that emacs uses
  // Need to add entries for XIF and HTML
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
void nsSelectionMgr::SelectionRequestor(GtkWidget *widget, 
					GtkSelectionData *selection_data)
{
  if (!mCopyStream)
    return;

  guchar* str = (guchar*)(mCopyStream->str());

  // Currently we only offer the data in GDK_SELECTION_TYPE_STRING format.
  if(str) {
    gtk_selection_data_set(selection_data,
			   GDK_SELECTION_TYPE_STRING,
			   8, str, mCopyStream->pcount());
    // the format arg, "8", indicates string data with no endianness
  }
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
  // we'll already get the events:
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

  return NS_OK;
}

nsresult nsSelectionMgr::PasteTextBlocking(nsString *aPastedText)
{
  mBlocking = PR_TRUE;
  gtk_selection_convert(sWidget,
			GDK_SELECTION_PRIMARY,
			GDK_SELECTION_TYPE_STRING,
			GDK_CURRENT_TIME);
#if 0
  // Tried to use straight Xlib call but this would need more work:
  XConvertSelection(GDK_WINDOW_XDISPLAY(sWidget->window),
                    XA_PRIMARY, XA_STRING, gdk_selection_property, 
                    GDK_WINDOW_XWINDOW(sWidget->window), GDK_CURRENT_TIME);
#endif

  // Now we need to wait until the callback comes in ...
  // i is in case we get a runaway (yuck).
  for (int i=0; mBlocking == PR_TRUE && i < 10000; ++i)
  {
    gtk_main_iteration_do(TRUE);
  }

  mBlocking = PR_FALSE;

  if (!mSelectionData.data)
    return NS_ERROR_NOT_AVAILABLE;

  *aPastedText = (char*)(mSelectionData.data);
  g_free(mSelectionData.data);
  mSelectionData.data = nsnull;
  return NS_OK;
}

void
nsSelectionMgr::SelectionReceivedCB (GtkWidget *aWidget,
                                     GtkSelectionData *aSelectionData,
                                     gpointer aData)
{
  // ARGHH!  GTK doesn't pass the arg to the callback, so we can't
  // get "this" back!  Until we solve this, use the global:
  ((nsSelectionMgr*)theSelectionMgr)->SelectionReceiver(aWidget,
                                                        aSelectionData);
}

void
nsSelectionMgr::SelectionReceiver (GtkWidget *aWidget,
                                   GtkSelectionData *data)
{
  mBlocking = PR_FALSE;
  mSelectionData.data = nsnull;

  if (data->length < 0)
  {
#ifdef DEBUG_akkana
    g_print("Error retrieving selection: length was %d\n", data->length);
#endif
    return;
  }

  switch (data->type)
  {
    case GDK_SELECTION_TYPE_STRING:
      mSelectionData = *data;
      mSelectionData.data = g_new(guchar, data->length + 1);
      memcpy(mSelectionData.data, data->data, data->length);
      mSelectionData.data[data->length] = '\0';
      return;

    default:
#ifdef DEBUG_akkana
      printf("Can't convert type %s (%ld) to string\n",
             gdk_atom_name (data->type), data->type);
#endif
      return;
  }
}

