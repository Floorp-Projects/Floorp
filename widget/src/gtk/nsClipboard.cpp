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
#include "nsIGenericTransferable.h"

#include "nsIWidget.h"
#include "nsIComponentManager.h"
#include "nsIServiceManager.h"
#include "nsWidgetsCID.h"

// interface definitions
static NS_DEFINE_IID(kIDataFlavorIID,    NS_IDATAFLAVOR_IID);

static NS_DEFINE_IID(kIWidgetIID,        NS_IWIDGET_IID);
static NS_DEFINE_IID(kWindowCID,         NS_WINDOW_CID);


// The class statics:
GtkWidget* nsClipboard::sWidget = 0;

NS_IMPL_ADDREF_INHERITED(nsClipboard, nsBaseClipboard)
NS_IMPL_RELEASE_INHERITED(nsClipboard, nsBaseClipboard)

static NS_DEFINE_IID(kIClipboardIID,       NS_ICLIPBOARD_IID);
static NS_DEFINE_CID(kCClipboardCID,       NS_CLIPBOARD_CID);

#if defined(DEBUG_akkana) || defined(DEBUG_mcafee)
#define DEBUG_CLIPBOARD
#endif
 

//-------------------------------------------------------------------------
//
// nsClipboard constructor
//
//-------------------------------------------------------------------------
nsClipboard::nsClipboard() : nsBaseClipboard()
{
#ifdef DEBUG_CLIPBOARD
  printf("  nsClipboard::nsClipboard()\n");
#endif /* DEBUG_CLIPBOARD */

  //NS_INIT_REFCNT();
  mIgnoreEmptyNotification = PR_FALSE;
  mClipboardOwner = nsnull;
  mTransferable   = nsnull;
  mSelectionData.data = nsnull;
  mSelectionData.length = 0;
}

//-------------------------------------------------------------------------
//
// nsClipboard destructor
//
//-------------------------------------------------------------------------
nsClipboard::~nsClipboard()
{
#ifdef DEBUG_CLIPBOARD
  printf("  nsClipboard::~nsClipboard()\n");  
#endif /* DEBUG_CLIPBOARD */

  // Remove all our event handlers:
  if (sWidget &&
      gdk_selection_owner_get (GDK_SELECTION_PRIMARY) == sWidget->window)
    gtk_selection_remove_all(sWidget);

  // free the selection data, if any
  if (mSelectionData.data != nsnull)
    g_free(mSelectionData.data);
}

/**
 * @param aIID The name of the class implementing the method
 * @param _classiiddef The name of the #define symbol that defines the IID
 * for the class (e.g. NS_ISUPPORTS_IID)
 * 
*/ 
nsresult nsClipboard::QueryInterface(const nsIID& aIID, void** aInstancePtr)
{
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


// 
// GTK Weirdness!
// This is here in the hope of being able to call
//  gtk_selection_add_targets(w, GDK_SELECTION_PRIMARY,
//                            targets,
//                            1);
// instead of
//   gtk_selection_add_target(sWidget, 
//                            GDK_SELECTION_PRIMARY,
//                            GDK_SELECTION_TYPE_STRING,
//                            GDK_SELECTION_TYPE_STRING);
// but it turns out that this changes the whole gtk selection model;
// when calling add_targets copy uses selection_clear_event and the
// data structure needs to be filled in in a way that we haven't
// figured out; when using add_target copy uses selection_get and
// the data structure is already filled in as much as it needs to be.
// Some gtk internals wizard will need to solve this mystery before
// we can use add_targets().
//static GtkTargetEntry targets[] = {
//  { "strings n stuff", GDK_SELECTION_TYPE_STRING, GDK_SELECTION_TYPE_STRING }
//};
//

void nsClipboard::SetTopLevelWidget(GtkWidget* w)
{
#ifdef DEBUG_CLIPBOARD
  printf("  nsClipboard::SetTopLevelWidget\n");
#endif /* DEBUG_CLIPBOARD */
  
  // Don't set up any more event handlers if we're being called twice
  // for the same toplevel widget
  if (sWidget == w)
    return;

  // If we're changing from one widget to another
  // (shouldn't generally happen), clear the old event handlers:
  if (sWidget &&
      gdk_selection_owner_get (GDK_SELECTION_PRIMARY) == sWidget->window)
    gtk_selection_remove_all(sWidget);

  sWidget = w;

  // Get the clipboard from the service manager.
  nsIClipboard* clipboard;
  nsresult rv = nsServiceManager::GetService(kCClipboardCID,
                                             kIClipboardIID,
                                             (nsISupports **)&clipboard);
  if (!NS_SUCCEEDED(rv)) {
    printf("Couldn't get clipboard service!\n");
    return;
  }

  // Handle selection requests if we called gtk_selection_add_target:
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

#if 0
  // Handle selection requests if we called gtk_selection_add_targets:
  gtk_signal_connect(GTK_OBJECT(sWidget), "selection_request_event",
                     GTK_SIGNAL_FUNC(nsClipboard::SelectionRequestCB),
                     clipboard);
  
  // Watch this, experimenting with Gtk :-)
  gtk_signal_connect(GTK_OBJECT(sWidget), "selection_notify_event",
                     GTK_SIGNAL_FUNC(nsClipboard::SelectionNotifyCB),
                     clipboard);
#endif

  // Hmm, sometimes we need this, sometimes not.  I'm not clear why.
  // See also long comment above on why we don't register a whole target list.

  // Register all the target types we handle:
  gtk_selection_add_target(sWidget, 
                           GDK_SELECTION_PRIMARY,
                           GDK_SELECTION_TYPE_STRING,
                           GDK_SELECTION_TYPE_STRING);

  // We're done with our reference to the clipboard.
  NS_IF_RELEASE(clipboard);
}


/**
  * 
  *
  */
NS_IMETHODIMP nsClipboard::SetNativeClipboardData()
{
  mIgnoreEmptyNotification = PR_TRUE;

#ifdef DEBUG_CLIPBOARD
  printf("  nsClipboard::SetNativeClipboardData()\n");
#endif /* DEBUG_CLIPBOARD */

  // make sure we have a good transferable
  if (nsnull == mTransferable) {
    printf("  SetNativeClipboardData: no transferable!\n");
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


//
// The blocking Paste routine
//
NS_IMETHODIMP
nsClipboard::GetNativeClipboardData(nsITransferable * aTransferable)
{
  nsresult rv = NS_OK;

#ifdef DEBUG_CLIPBOARD
  printf("  nsClipboard::GetNativeClipboardData()\n");
#endif /* DEBUG_CLIPBOARD */

  // make sure we have a good transferable
  if (nsnull == aTransferable) {
    printf("  GetNativeClipboardData: Transferable is null!\n");
    return NS_ERROR_FAILURE;
  }

  // Dunno why we need to do this, copying the win32 code ...
  nsCOMPtr<nsIGenericTransferable> genericTrans = do_QueryInterface(aTransferable);
  if (!genericTrans)
    return rv;

  //
  // We can't call the copy callback when we're blocking on the paste callback;
  // so if this app is already the selection owner, we need to copy our own
  // data without going through the X server.
  //
  if (gdk_selection_owner_get (GDK_SELECTION_PRIMARY) == sWidget->window)
  {
    nsDataFlavor *dataFlavor = new nsDataFlavor();
    // XXX only support text/plain for now
    dataFlavor->Init(kTextMime, kTextMime);

    // Get data out of our existing transferable.
    void     *clipboardData;
    PRUint32 dataLength;
    rv = mTransferable->GetTransferData(dataFlavor, 
                                        &clipboardData,
                                        &dataLength);
    if (NS_SUCCEEDED(rv))
      rv = genericTrans->SetTransferData(dataFlavor,
                                         clipboardData, dataLength);
    return rv;
  }

#define ONLY_SUPPORT_PLAIN_TEXT 1
#ifdef ONLY_SUPPORT_PLAIN_TEXT
  gtk_selection_convert(sWidget, GDK_SELECTION_PRIMARY,
                        GDK_SELECTION_TYPE_STRING, GDK_CURRENT_TIME);
  // Tried to use straight Xlib call but this would need more work:
  //XConvertSelection(GDK_WINDOW_XDISPLAY(sWidget->window),
  //                  XA_PRIMARY, XA_STRING, gdk_selection_property, 
  //                  GDK_WINDOW_XWINDOW(sWidget->window), GDK_CURRENT_TIME);

#else /* ONLY_SUPPORT_PLAIN_TEXT */
    //
    // XXX This code isn't implemented for Unix yet!
    // Instead of SetTransferData it will have to call gtk_selection_convert.
    //

  // Get the transferable list of data flavors
  nsISupportsArray * dfList;
  aTransferable->GetTransferDataFlavors(&dfList);

  // Walk through flavors and see which flavor matches the one being pasted:
  PRUint32 i;
  for (i=0;i<dfList->Count();i++) {
    nsIDataFlavor * df;
    nsISupports * supports = dfList->ElementAt(i);
    if (NS_OK == supports->QueryInterface(kIDataFlavorIID, (void **)&df)) {
      nsString mime;
      df->GetMimeType(mime);
      UINT format = GetFormat(mime);

      void   * data;
      PRUint32 dataLen;

      if (nsnull != aDataObject) {
        res = GetNativeDataOffClipboard(aDataObject, format, &data, &dataLen);
        if (NS_OK == res) {
          genericTrans->SetTransferData(df, data, dataLen);
        }
      } else if (nsnull != aWindow) {
        res = GetNativeDataOffClipboard(aWindow, format, &data, &dataLen);
        if (NS_OK == res) {
          genericTrans->SetTransferData(df, data, dataLen);
        }
      } 
      NS_IF_RELEASE(df);
    }
    NS_RELEASE(supports);
  }
#endif /* ONLY_SUPPORT_PLAIN_TEXT */

  //
  // We've told X what type to send, and we just have to wait
  // for the callback saying that the data have been transferred.
  //

  // Set a flag saying that we're blocking waiting for the callback:
  mBlocking = PR_TRUE;
#ifdef DEBUG_CLIPBOARD
  printf("Waiting for the callback\n");
#endif /* DEBUG_CLIPBOARD */

  // Now we need to wait until the callback comes in ...
  // i is in case we get a runaway (yuck).
  for (int i=0; mBlocking == PR_TRUE && i < 10000; ++i)
  {
    gtk_main_iteration_do(TRUE);
  }

#ifdef DEBUG_CLIPBOARD
  printf("Got the callback: '%s', %d\n",
         mSelectionData.data, mSelectionData.length);
#endif /* DEBUG_CLIPBOARD */

  // We're back from the callback, no longer blocking:
  mBlocking = PR_FALSE;

  // 
  // Now we have data in mSelectionData.data.
  // We just have to copy it to the transferable.
  // 
  nsDataFlavor *dataFlavor = new nsDataFlavor();
  dataFlavor->Init(kTextMime, kTextMime);
  genericTrans->SetTransferData(dataFlavor,
                                mSelectionData.data, mSelectionData.length);

  // Can't free the selection data -- the transferable just saves a pointer.
  // But the transferable is responsible for freeing it, so we have to
  // consider it freed now:
  //g_free(mSelectionData.data);
  mSelectionData.data = nsnull;
  mSelectionData.length = 0;

  return NS_OK;
}

//
// Called when the data from a paste comes in:
// 
void
nsClipboard::SelectionReceivedCB (GtkWidget *aWidget,
                                  GtkSelectionData *aSelectionData,
                                  gpointer aData)
{
#ifdef DEBUG_CLIPBOARD
  printf("  nsClipboard::SelectionReceivedCB\n");
#endif /* DEBUG_CLIPBOARD */

  // ARGHH!  GTK doesn't pass the arg to the callback, so we can't
  // get "this" back!  Until we solve this, get it from the service mgr:
  nsIClipboard* iclipboard;
  nsresult rv = nsServiceManager::GetService(kCClipboardCID,
                                             kIClipboardIID,
                                             (nsISupports **)&iclipboard);
  if (!NS_SUCCEEDED(rv)) {
    printf("Couldn't get clipboard service!\n");
    return;
  }
  nsClipboard* clipboard = (nsClipboard*)iclipboard;
  if (!clipboard) {
    printf("couldn't convert nsIClipboard to nsClipboard\n");
    return;
  }

  clipboard->SelectionReceiver(aWidget, aSelectionData);
}

void
nsClipboard::SelectionReceiver (GtkWidget *aWidget,
                                GtkSelectionData *aSelectionData)
{
  mBlocking = PR_FALSE;

  if (aSelectionData->length < 0)
  {
    printf("Error retrieving selection: length was %d\n",
           aSelectionData->length);
    return;
  }

  switch (aSelectionData->type)
  {
    case GDK_SELECTION_TYPE_STRING:
      mSelectionData = *aSelectionData;
      mSelectionData.data = g_new(guchar, aSelectionData->length + 1);
      memcpy(mSelectionData.data,
             aSelectionData->data, aSelectionData->length);
      // Null terminate in case anyone cares,
      // and so we can print the string for debugging:
      mSelectionData.data[aSelectionData->length] = '\0';
      mSelectionData.length = aSelectionData->length;
      return;

    default:
      printf("Can't convert type %s (%ld) to string\n",
             gdk_atom_name (aSelectionData->type), aSelectionData->type);
      return;
  }
}

/**
  * No-op.
  *
  */
NS_IMETHODIMP nsClipboard::ForceDataToClipboard()
{
#ifdef DEBUG_CLIPBOARD
  printf("  nsClipboard::ForceDataToClipboard()\n");
#endif /* DEBUG_CLIPBOARD */

  // make sure we have a good transferable
  if (nsnull == mTransferable) {
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

//
// This is the callback which is called when another app
// requests the selection.
//
void nsClipboard::SelectionGetCB(GtkWidget        *widget,
                                 GtkSelectionData *aSelectionData,
                                 guint      /*info*/,
                                 guint      /*time*/,
                                 gpointer   aData)
{ 
#ifdef DEBUG_CLIPBOARD
  printf("  nsClipboard::SelectionGetCB\n"); 
#endif /* DEBUG_CLIPBOARD */

  nsClipboard *clipboard = (nsClipboard *)aData;

  void     *clipboardData;
  PRUint32 dataLength;
  nsresult rv;

  // Make sure we have a transferable:
  if (!clipboard->mTransferable) {
    printf("Clipboard has no transferable!\n");
    return;
  }

  // XXX hack, string-only for now.
  // Create string data-flavor.
  nsDataFlavor *dataFlavor = new nsDataFlavor();
  dataFlavor->Init(kTextMime, kTextMime);

  // Get data out of transferable.
  rv = clipboard->mTransferable->GetTransferData(dataFlavor, 
                                                 &clipboardData,
                                                 &dataLength);

  // Currently we only offer the data in GDK_SELECTION_TYPE_STRING format.
  if (NS_SUCCEEDED(rv) && clipboardData && dataLength > 0) {
    gtk_selection_data_set(aSelectionData,
                           GDK_SELECTION_TYPE_STRING, 8,
                           (unsigned char *)clipboardData,
                           dataLength-1);
    // Need to subtract one from dataLength, passed in from the transferable,
    // to avoid getting a bogus null char.
    // the format arg, "8", indicates string data with no endianness
  }
  else
    printf("Transferable didn't support the data flavor\n");

  delete dataFlavor;
} 



// Called when another app requests selection ownership:
void nsClipboard::SelectionClearCB(GtkWidget *widget, 
                                   GdkEventSelection *event, 
                                   gpointer data) 
{ 
#ifdef DEBUG_CLIPBOARD
  printf("  nsClipboard::SelectionClearCB\n");
#endif /* DEBUG_CLIPBOARD */
}
 

// The routine called when another app asks for the content of the selection
void 
nsClipboard::SelectionRequestCB (GtkWidget *aWidget, 
                                 GtkSelectionData *aSelectionData, 
                                 gpointer aData) 
{ 
#ifdef DEBUG_CLIPBOARD
  printf("  nsClipboard::SelectionRequestCB\n");  
#endif /* DEBUG_CLIPBOARD */
} 

void 
nsClipboard::SelectionNotifyCB (GtkWidget *aWidget, 
                                  GtkSelectionData *aSelectionData, 
                                  gpointer aData) 
{ 
#ifdef DEBUG_CLIPBOARD
   printf("  nsClipboard::SelectionNotifyCB\n");  
#endif /* DEBUG_CLIPBOARD */
} 
