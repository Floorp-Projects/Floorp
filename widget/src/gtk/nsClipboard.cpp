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

#include "nsClipboard.h"

#include "nsCOMPtr.h"

#include "nsISupportsArray.h"
#include "nsIClipboardOwner.h"
#include "nsDataFlavor.h"

#include "nsIWidget.h"
#include "nsIComponentManager.h"
#include "nsIServiceManager.h"
#include "nsWidgetsCID.h"

// interface definitions
static NS_DEFINE_IID(kIDataFlavorIID,    NS_IDATAFLAVOR_IID);

static NS_DEFINE_IID(kIWidgetIID,        NS_IWIDGET_IID);
static NS_DEFINE_IID(kWindowCID,         NS_WINDOW_CID);

static GtkTargetEntry targets[] = {
  { "strings n stuff", GDK_SELECTION_TYPE_STRING, GDK_SELECTION_TYPE_STRING }
};

// The class statics:
GtkWidget* nsClipboard::sWidget = 0;

NS_IMPL_ADDREF_INHERITED(nsClipboard, nsBaseClipboard)
NS_IMPL_RELEASE_INHERITED(nsClipboard, nsBaseClipboard)

static NS_DEFINE_IID(kIClipboardIID,       NS_ICLIPBOARD_IID);
static NS_DEFINE_CID(kCClipboardCID,       NS_CLIPBOARD_CID);
 

//-------------------------------------------------------------------------
//
// nsClipboard constructor
//
//-------------------------------------------------------------------------
nsClipboard::nsClipboard() : nsBaseClipboard()
{
  printf("  nsClipboard::nsClipboard()\n");

  //NS_INIT_REFCNT();
  mIgnoreEmptyNotification = PR_FALSE;
  mWindow         = nsnull;
  mClipboardOwner = nsnull;
  mTransferable   = nsnull;

  // Create a Native window for the shell container...
  //nsresult rv = nsComponentManager::CreateInstance(kWindowCID, nsnull, kIWidgetIID, (void**)&mWindow);
  //mWindow->Show(PR_FALSE);
  //mWindow->Resize(1,1,PR_FALSE);
}

//-------------------------------------------------------------------------
//
// nsClipboard destructor
//
//-------------------------------------------------------------------------
nsClipboard::~nsClipboard()
{
  printf("  nsClipboard::~nsClipboard()\n");  

  NS_IF_RELEASE(mWindow);
}

/**
 * @param aIID The name of the class implementing the method
 * @param _classiiddef The name of the #define symbol that defines the IID
 * for the class (e.g. NS_ISUPPORTS_IID)
 * 
*/ 
nsresult nsClipboard::QueryInterface(const nsIID& aIID, void** aInstancePtr)
{
  printf("  nsClipboard::QueryInterface()\n");

  if (NULL == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }

  nsresult rv = NS_NOINTERFACE;

  static NS_DEFINE_IID(kIClipboard, NS_ICLIPBOARD_IID);
  if (aIID.Equals(kIClipboard)) {
    *aInstancePtr = (void*) ((nsIClipboard*)this);
    NS_ADDREF_THIS();
    return NS_OK;
  }

  return rv;
}


/**
  * 
  *
  */
NS_IMETHODIMP nsClipboard::SetNativeClipboardData()
{
  mIgnoreEmptyNotification = PR_TRUE;

  printf("  nsClipboard::SetNativeClipboardData()\n");

  // make sure we have a good transferable
  if (nsnull == mTransferable) {
    return NS_ERROR_FAILURE;
  }

  // If we're already the selection owner, don't need to do anything,
  // we'll already get the events:
  if (gdk_selection_owner_get (GDK_SELECTION_PRIMARY) == sWidget->window)
    return NS_OK;

  // Clear the native clipboard
  if (sWidget &&
      gdk_selection_owner_get (GDK_SELECTION_PRIMARY) == sWidget->window)
    gtk_selection_remove_all(sWidget);


  // register as the selection owner:
  gint have_selection =
    gtk_selection_owner_set(sWidget,
                            GDK_SELECTION_PRIMARY,
                            GDK_CURRENT_TIME);
  if (have_selection == 0)
    return NS_ERROR_FAILURE;

  mIgnoreEmptyNotification = PR_FALSE;

  return NS_OK;
}


/**
  * 
  *
  */
NS_IMETHODIMP nsClipboard::GetNativeClipboardData(nsITransferable * aTransferable)
{
  nsresult rv = NS_OK;

  printf("  nsClipboard::GetNativeClipboardData()\n");

  // make sure we have a good transferable
  if (nsnull == aTransferable) {
    return NS_ERROR_FAILURE;
  }
  
  return rv;
}


/**
  * No-op.
  *
  */
NS_IMETHODIMP nsClipboard::ForceDataToClipboard()
{
  printf("  nsClipboard::ForceDataToClipboard()\n");

  // make sure we have a good transferable
  if (nsnull == mTransferable) {
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}



void nsClipboard::SetTopLevelWidget(GtkWidget* w)
{
  printf("  nsClipboard::SetTopLevelWidget\n");
  
  // Don't set up any more event handlers if we're being called twice
  // for the same toplevel widget
  if (sWidget == w)
    return;

  sWidget = w;

  // Get the clipboard from the service manager.
  nsIClipboard* clipboard;
  nsresult rv = nsServiceManager::GetService(kCClipboardCID,
                                             kIClipboardIID,
                                             (nsISupports **)&clipboard);
  if (!NS_SUCCEEDED(rv)) {
    return;
  }

  // Respond to requests for the selection:
  gtk_signal_connect(GTK_OBJECT(sWidget), "selection_get",
                     GTK_SIGNAL_FUNC(nsClipboard::SelectionGetCB),
                     clipboard);

  // When someone else takes the selection away:
  gtk_signal_connect(GTK_OBJECT(sWidget), "selection_clear_event",
                     GTK_SIGNAL_FUNC(nsClipboard::SelectionClearCB),
                     clipboard);

  // Set up the paste handler:
  gtk_signal_connect(GTK_OBJECT(sWidget), "selection_received",
                     GTK_SIGNAL_FUNC(nsClipboard::SelectionReceivedCB),
                     clipboard);

  // Watch this, experimenting with Gtk :-)
  gtk_signal_connect(GTK_OBJECT(sWidget), "selection_request_event",
                     GTK_SIGNAL_FUNC(nsClipboard::SelectionRequestCB),
                     clipboard);
  
  // Watch this, experimenting with Gtk :-)
  gtk_signal_connect(GTK_OBJECT(sWidget), "selection_notify_event",
                     GTK_SIGNAL_FUNC(nsClipboard::SelectionNotifyCB),
                     clipboard);



  // Hmm, sometimes we need this, sometimes not.  I'm not clear why.
  // Register all the target types we handle:
  gtk_selection_add_targets(w, GDK_SELECTION_PRIMARY,
                           targets,
                           1);

  // We're done with our reference to the clipboard.
  NS_IF_RELEASE(clipboard);
}


// 
// The event handler to handle selection requests: 
// 
void nsClipboard::SelectionGetCB(GtkWidget        *widget,
                                 GtkSelectionData *selection_data,
                                 guint      /*info*/,
                                 guint      /*time*/,
                                 gpointer   data) 
{ 
  printf("  nsClipboard::SelectionGetCB\n"); 

  nsClipboard *clipboard = (nsClipboard *) data;
  void     *clipboardData;
  PRUint32 dataLength;

  nsresult rv;

  // XXX hack, string-only for now.
  // Create string data-flavor.
  nsDataFlavor *dataFlavor = new nsDataFlavor();
  dataFlavor->Init("text/plain","text/plain");

  // Get data out of transferable.
  rv = clipboard->mTransferable->GetTransferData(dataFlavor, 
                                      &clipboardData,
                                      &dataLength);
  

  printf("  nsClipboard::SelectionRequestCB, clipboardData = %s%s\n", clipboardData);

  // Currently we only offer the data in GDK_SELECTION_TYPE_STRING format.
  if(NS_SUCCEEDED(rv)) {
    gtk_selection_data_set(selection_data,
                           GDK_SELECTION_TYPE_STRING,
                           8, 
                           (unsigned char *)clipboardData,
                           dataLength);
    // the format arg, "8", indicates string data with no endianness
    
  }

  delete dataFlavor;
} 



// Called when another app requests the selection: 
void nsClipboard::SelectionClearCB(GtkWidget *widget, 
                                   GdkEventSelection *event, 
                                   gpointer data) 
{ 
  printf("  nsClipboard::SelectionClearCB\n");
}
 

void 
nsClipboard::SelectionReceivedCB (GtkWidget *aWidget, 
                                  GtkSelectionData *aSelectionData, 
                                  gpointer aData) 
{ 
  // ARGHH!  GTK doesn't pass the arg to the callback, so we can't 
  // get "this" back!  Until we solve this, use the global: 
 
   printf("  nsClipboard::SelectionReceivedCB\n");  
} 


void 
nsClipboard::SelectionRequestCB (GtkWidget *aWidget, 
                                  GtkSelectionData *aSelectionData, 
                                  gpointer aData) 
{ 
   printf("  nsClipboard::SelectionRequestCB\n");  
} 

void 
nsClipboard::SelectionNotifyCB (GtkWidget *aWidget, 
                                  GtkSelectionData *aSelectionData, 
                                  gpointer aData) 
{ 
   printf("  nsClipboard::SelectionNotifyCB\n");  
} 
