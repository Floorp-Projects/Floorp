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

#include "nsSelectionMgr.h"

#include <strstream.h>

#include <gtk/gtksignal.h>

#include <stdio.h>

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kISelectionMgrIID, NS_ISELECTIONMGR_IID);

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
  if (aIID.Equals(kISupportsIID)) 
  {
    *aInstancePtrResult = (void*)(nsISupports*)this;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(nsISelectionMgr::IID())) 
  //if (aIID.Equals(kISelectionMgrIID)) 
  {
    *aInstancePtrResult = (void*)(nsISelectionMgr*)this;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  return !NS_OK;
}

nsSelectionMgr::nsSelectionMgr()
{
  NS_INIT_REFCNT();

  mCopyStream = 0;
}

nsSelectionMgr::~nsSelectionMgr()
{
}

nsresult nsSelectionMgr::GetCopyOStream(ostream** aStream)
{
  if (mCopyStream)
    delete mCopyStream;
  mCopyStream = new ostrstream;
  *aStream = mCopyStream;
  return NS_OK;
}

#if 0
// May or may not need this routine:
static void ClearSelection()
{
  // Before clearing the selection by setting the owner to NULL,
  // we check if we are the actual owner
  if (gdk_selection_owner_get (GDK_SELECTION_PRIMARY) == sWidget->window)
    gtk_selection_owner_set (NULL, GDK_SELECTION_PRIMARY,
                             GDK_CURRENT_TIME);
}
#endif

//
// Here folows a bunch of code which came from GTK's gtktestselection.c:
//

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

GdkAtom seltypes[LAST_SEL_TYPE];

typedef struct _Target {
  gchar *target_name;
  SelType type;
  GdkAtom target;
  gint format;
} Target;

/* The following is a list of all the selection targets defined
   in the ICCCM */

static Target targets[] = {
  { "ADOBE_PORTABLE_DOCUMENT_FORMAT",	    STRING, 	   0, 8 },
  { "APPLE_PICT", 			    APPLE_PICT,    0, 8 },
  { "BACKGROUND",			    PIXEL,         0, 32 },
  { "BITMAP", 				    BITMAP,        0, 32 },
  { "CHARACTER_POSITION",                   SPAN, 	   0, 32 },
  { "CLASS", 				    TEXT, 	   0, 8 },
  { "CLIENT_WINDOW", 			    WINDOW, 	   0, 32 },
  { "COLORMAP", 			    COLORMAP,      0, 32 },
  { "COLUMN_NUMBER", 			    SPAN, 	   0, 32 },
  { "COMPOUND_TEXT", 			    COMPOUND_TEXT, 0, 8 },
  /*  { "DELETE", "NULL", 0, ? }, */
  { "DRAWABLE", 			    DRAWABLE,      0, 32 },
  { "ENCAPSULATED_POSTSCRIPT", 		    STRING, 	   0, 8 },
  { "ENCAPSULATED_POSTSCRIPT_INTERCHANGE",  STRING, 	   0, 8 },
  { "FILE_NAME", 			    TEXT, 	   0, 8 },
  { "FOREGROUND", 			    PIXEL, 	   0, 32 },
  { "HOST_NAME", 			    TEXT, 	   0, 8 },
  /*  { "INSERT_PROPERTY", "NULL", 0, ? NULL }, */
  /*  { "INSERT_SELECTION", "NULL", 0, ? NULL }, */
  { "LENGTH", 				    INTEGER, 	   0, 32 },
  { "LINE_NUMBER", 			    SPAN, 	   0, 32 },
  { "LIST_LENGTH", 			    INTEGER,       0, 32 },
  { "MODULE", 				    TEXT, 	   0, 8 },
  /*  { "MULTIPLE", "ATOM_PAIR", 0, 32 }, */
  { "NAME", 				    TEXT, 	   0, 8 },
  { "ODIF", 				    TEXT,          0, 8 },
  { "OWNER_OS", 			    TEXT, 	   0, 8 },
  { "PIXMAP", 				    PIXMAP,        0, 32 },
  { "POSTSCRIPT", 			    STRING,        0, 8 },
  { "PROCEDURE", 			    TEXT,          0, 8 },
  { "PROCESS",				    INTEGER,       0, 32 },
  { "STRING", 				    STRING,        0, 8 },
  { "TARGETS", 				    ATOM, 	   0, 32 },
  { "TASK", 				    INTEGER,       0, 32 },
  { "TEXT", 				    TEXT,          0, 8  },
  { "TIMESTAMP", 			    INTEGER,       0, 32 },
  { "USER", 				    TEXT, 	   0, 8 },
};

static int num_targets = sizeof(targets)/sizeof(Target);

static GtkTargetEntry targetlist[] = {
  { "STRING",        0, STRING },
  { "TEXT",          0, TEXT },
  { "COMPOUND_TEXT", 0, COMPOUND_TEXT }
};
static gint ntargets = sizeof(targetlist) / sizeof(targetlist[0]);

GtkWidget *selection_text;
GtkWidget *selection_button;
GString *selection_string = NULL;

static void
init_atoms (void)
{
  int i;

  seltypes[SEL_TYPE_NONE] = GDK_NONE;
  seltypes[APPLE_PICT] = gdk_atom_intern ("APPLE_PICT",FALSE);
  seltypes[ATOM]       = gdk_atom_intern ("ATOM",FALSE);
  seltypes[ATOM_PAIR]  = gdk_atom_intern ("ATOM_PAIR",FALSE);
  seltypes[BITMAP]     = gdk_atom_intern ("BITMAP",FALSE);
  seltypes[C_STRING]   = gdk_atom_intern ("C_STRING",FALSE);
  seltypes[COLORMAP]   = gdk_atom_intern ("COLORMAP",FALSE);
  seltypes[COMPOUND_TEXT] = gdk_atom_intern ("COMPOUND_TEXT",FALSE);
  seltypes[DRAWABLE]   = gdk_atom_intern ("DRAWABLE",FALSE);
  seltypes[INTEGER]    = gdk_atom_intern ("INTEGER",FALSE);
  seltypes[PIXEL]      = gdk_atom_intern ("PIXEL",FALSE);
  seltypes[PIXMAP]     = gdk_atom_intern ("PIXMAP",FALSE);
  seltypes[SPAN]       = gdk_atom_intern ("SPAN",FALSE);
  seltypes[STRING]     = gdk_atom_intern ("STRING",FALSE);
  seltypes[TEXT]       = gdk_atom_intern ("TEXT",FALSE);
  seltypes[WINDOW]     = gdk_atom_intern ("WINDOW",FALSE);

  for (i=0; i<num_targets; i++)
    targets[i].target = gdk_atom_intern (targets[i].target_name, FALSE);
}

void nsSelectionMgr::SetTopLevelWidget(GtkWidget* w)
{
  sWidget = w;
}

// The event handler to handle paste requests:
void nsSelectionMgr::SelectionRequestCB( GtkWidget        *widget, 
                                         GtkSelectionData *selection_data,
                                         guint      /*info*/,
                                         guint      /*time*/,
                                         gpointer   data)
{
  if (data)
    ((nsSelectionMgr*)data)->SelectionRequestor(widget, selection_data);
}

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

nsresult nsSelectionMgr::CopyToClipboard()
{
  // we'd better already have a stream and a widget ...
  if (!mCopyStream || !sWidget)
      return NS_ERROR_NOT_INITIALIZED;

  char* str = (char*)mCopyStream->str();
  //printf("owning selection:\n--'%s'--\n", str);

  // XXX How do we get a widget to pass to gtk_selection_owner_set?
  // That's the sticky part of the whole mess
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

  // Now set up the event handler to handle paste requests:
  // This doesn't seem to be working right, or at least,
  // when we paste in an xterm we never get notified
  // of selection_request_events or the others listed below.
  static beenhere = 0;
  if (!beenhere)
  {
    init_atoms();
    gtk_selection_add_targets (sWidget, GDK_SELECTION_PRIMARY,
                               targetlist, ntargets);
    gtk_signal_connect(GTK_OBJECT(sWidget),
                       //"selection_request_event",
                       //"selection_notify_event",
                       "selection_get",
                       //"selection_received",
                       GTK_SIGNAL_FUNC(nsSelectionMgr::SelectionRequestCB),
                       this);
  }

  return NS_OK;
}

nsresult NS_NewSelectionMgr(nsISelectionMgr** aInstancePtrResult)
{
  nsSelectionMgr* sm = new nsSelectionMgr;
  static nsIID iid = NS_ISELECTIONMGR_IID;
  return sm->QueryInterface(iid, (void**) aInstancePtrResult);
}


