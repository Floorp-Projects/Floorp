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
 *   Pierre Phaneuf <pp@ludusdesign.com>
 */

#include "nsClipboard.h"

#include "nsCOMPtr.h"

#include "nsIClipboardOwner.h"
#include "nsITransferable.h"   // kTextMime

#include "nsIWidget.h"
#include "nsIServiceManager.h"
#include "nsWidgetsCID.h"
#include "nsXPIDLString.h"
#include "nsPrimitiveHelpers.h"
#include "nsISupportsPrimitives.h"

#include <View.h>
#include <Clipboard.h>
#include <Message.h>

// The class statics:
BView *nsClipboard::sView = 0;

NS_IMPL_ADDREF_INHERITED(nsClipboard, nsBaseClipboard)
NS_IMPL_RELEASE_INHERITED(nsClipboard, nsBaseClipboard)

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
//  mSelectionData.data = nsnull;
//  mSelectionData.length = 0;
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

//  // Remove all our event handlers:
//  if (sView &&
//      gdk_selection_owner_get (GDK_SELECTION_PRIMARY) == sWidget->window)
//    gtk_selection_remove_all(sWidget);
//
//  // free the selection data, if any
//  if (mSelectionData.data != nsnull)
//    g_free(mSelectionData.data);
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

  if (aIID.Equals(NS_GET_IID(nsIClipboard))) {
    *aInstancePtr = (void*) ((nsIClipboard*)this);
    NS_ADDREF_THIS();
    return NS_OK;
  }

  return rv;
}


void nsClipboard::SetTopLevelView(BView *v)
{
  // Don't set up any more event handlers if we're being called twice
  // for the same toplevel widget
  if (sView == v)
    return;

  if (sView != 0 && sView->Window() != 0)
    return;

  if(v == 0 || v->Window() == 0)
  {
#ifdef DEBUG_CLIPBOARD
    printf("  nsClipboard::SetTopLevelView: widget passed in is null or has no window!\n");
#endif /* DEBUG_CLIPBOARD */
    return;
  }

#ifdef DEBUG_CLIPBOARD
  printf("  nsClipboard::SetTopLevelView\n");
#endif /* DEBUG_CLIPBOARD */
  
//  // If we're changing from one widget to another
//  // (shouldn't generally happen), clear the old event handlers:
//  if (sView &&
//      gdk_selection_owner_get (GDK_SELECTION_PRIMARY) == sWidget->window)
//    gtk_selection_remove_all(sWidget);
//
//  sWidget = w;
//
//  // Get the clipboard from the service manager.
//  nsresult rv;
//  NS_WITH_SERVICE(nsIClipboard, clipboard, kCClipboardCID, &rv);
//
//  if (!NS_SUCCEEDED(rv)) {
//    printf("Couldn't get clipboard service!\n");
//    return;
//  }
//
//  // Handle selection requests if we called gtk_selection_add_target:
//  gtk_signal_connect(GTK_OBJECT(sWidget), "selection_get",
//                     GTK_SIGNAL_FUNC(nsClipboard::SelectionGetCB),
//                     clipboard);
//
//  // When someone else takes the selection away:
//  gtk_signal_connect(GTK_OBJECT(sWidget), "selection_clear_event",
//                     GTK_SIGNAL_FUNC(nsClipboard::SelectionClearCB),
//                     clipboard);
//
//  // Set up the paste handler:
//  gtk_signal_connect(GTK_OBJECT(sWidget), "selection_received",
//                     GTK_SIGNAL_FUNC(nsClipboard::SelectionReceivedCB),
//                     clipboard);
//
//#if 0
//  // Handle selection requests if we called gtk_selection_add_targets:
//  gtk_signal_connect(GTK_OBJECT(sWidget), "selection_request_event",
//                     GTK_SIGNAL_FUNC(nsClipboard::SelectionRequestCB),
//                     clipboard);
//  
//  // Watch this, experimenting with Gtk :-)
//  gtk_signal_connect(GTK_OBJECT(sWidget), "selection_notify_event",
//                     GTK_SIGNAL_FUNC(nsClipboard::SelectionNotifyCB),
//                     clipboard);
//#endif
//
//  // Hmm, sometimes we need this, sometimes not.  I'm not clear why.
//  // See also long comment above on why we don't register a whole target list.
//
//  // Register all the target types we handle:
//  gtk_selection_add_target(sWidget, 
//                           GDK_SELECTION_PRIMARY,
//                           GDK_SELECTION_TYPE_STRING,
//                           GDK_SELECTION_TYPE_STRING);
}


/**
  * 
  *
  */
NS_IMETHODIMP nsClipboard::SetNativeClipboardData(PRInt32 aWhichClipboard)
{
  mIgnoreEmptyNotification = PR_TRUE;

#ifdef DEBUG_CLIPBOARD
  printf("  nsClipboard::SetNativeClipboardData()\n");
#endif /* DEBUG_CLIPBOARD */

  // make sure we have a good transferable
  if (nsnull == mTransferable) {
#ifdef DEBUG_CLIPBOARD
    printf("  SetNativeClipboardData: no transferable!\n");
#endif /* DEBUG_CLIPBOARD */
    return NS_ERROR_FAILURE;
  }

  // lock the native clipboard
  if (!be_clipboard->Lock())
    return NS_ERROR_FAILURE;

  // clear the native clipboard
  nsresult rv = NS_ERROR_FAILURE;
  if (B_OK == be_clipboard->Clear()) {
    // set data to the native clipboard
    BMessage *msg = be_clipboard->Data();
    if (NULL != msg) {
      // Get the transferable list of data flavors
      nsCOMPtr<nsISupportsArray> dfList;
      mTransferable->FlavorsTransferableCanExport(getter_AddRefs(dfList));

      // Walk through flavors that contain data and register them
      // into the BMessage as supported flavors
      PRUint32 i;
      PRUint32 cnt;
      dfList->Count(&cnt);
      for (i = 0; i < cnt; i++) {
	nsCOMPtr<nsISupports> genericFlavor;
	dfList->GetElementAt(i, getter_AddRefs(genericFlavor));
	nsCOMPtr<nsISupportsString> currentFlavor ( do_QueryInterface(genericFlavor));
	if (NULL != currentFlavor) {
	  nsXPIDLCString flavorStr;
	  currentFlavor->ToString(getter_Copies(flavorStr));

#ifdef DEBUG_CLIPBOARD
	  printf("nsClipboard: %d = %s\n", i, (const char *)flavorStr);
#endif /* DEBUG_CLIPBOARD */
	  if (0 == strcmp(flavorStr, kUnicodeMime)) {
	    void *data = nsnull;
	    PRUint32 dataSize = 0;
	    nsCOMPtr<nsISupports> genericDataWrapper;
	    rv = mTransferable->GetTransferData(flavorStr, getter_AddRefs(genericDataWrapper), &dataSize);
            nsPrimitiveHelpers::CreateDataFromPrimitive(flavorStr, genericDataWrapper, &data, dataSize);
#ifdef DEBUG_CLIPBOARD
	    if (NS_FAILED(rv))
	      printf("nsClipboard: Error getting data from transferable\n");
#endif /* DEBUG_CLIPBOARD */
	    if ((0 != dataSize) && (NULL != data)) {
	      const char *utf8Str = NS_ConvertUCS2toUTF8((const PRUnichar*)data, (PRUint32)dataSize);
	      uint32 utf8Len = strlen(utf8Str);
	      if (B_OK == msg->AddData(kTextMime, B_MIME_TYPE, (void *)utf8Str, utf8Len))
		rv = NS_OK;
	    }
#ifdef DEBUG_CLIPBOARD
	    else {
	      printf("nsClipboard: Error null data from transferable\n");
	    }
#endif /* DEBUG_CLIPBOARD */
	  }
	}
      }
    }
  }
  if (B_OK != be_clipboard->Commit())
    rv = NS_ERROR_FAILURE;
  be_clipboard->Unlock();

  mIgnoreEmptyNotification = PR_FALSE;

  return rv;
}


//
// The blocking Paste routine
//
NS_IMETHODIMP
nsClipboard::GetNativeClipboardData(nsITransferable * aTransferable, PRInt32 aWhichClipboard )
{
#ifdef DEBUG_CLIPBOARD
  printf("  nsClipboard::GetNativeClipboardData()\n");
#endif /* DEBUG_CLIPBOARD */

  // make sure we have a good transferable
  if (nsnull == aTransferable) {
    printf("  GetNativeClipboardData: Transferable is null!\n");
    return NS_ERROR_FAILURE;
  }

  // get native clipboard data
  if (!be_clipboard->Lock())
    return NS_ERROR_FAILURE;

  BMessage *msg = be_clipboard->Data();
  nsresult rv = NS_ERROR_FAILURE;

  if (NULL != msg) {
    const void *data;
    ssize_t size;
    status_t rc = msg->FindData(kTextMime, B_MIME_TYPE, &data, &size);
    if ((B_OK == rc) && (NULL != data) && (0 != size)) {
      nsString ucs2Str = NS_ConvertUTF8toUCS2((const char*)data, (PRUint32)size);
      nsCOMPtr<nsISupports> genericDataWrapper;
      nsXPIDLCString mime;
      mime = kUnicodeMime;
      nsPrimitiveHelpers::CreatePrimitiveForData(mime, (void*)ucs2Str.GetUnicode(), ucs2Str.Length() * 2, getter_AddRefs(genericDataWrapper));
      aTransferable->SetTransferData(mime, genericDataWrapper, ucs2Str.Length() * 2);
      rv = NS_OK;
    }
  }
  be_clipboard->Unlock();

  return rv;
}

//
// Called when the data from a paste comes in:
// 
//void
//nsClipboard::SelectionReceivedCB (GtkWidget *aWidget,
//                                  GtkSelectionData *aSelectionData,
//                                  gpointer aData)
//{
//#ifdef DEBUG_CLIPBOARD
//  printf("  nsClipboard::SelectionReceivedCB\n");
//#endif /* DEBUG_CLIPBOARD */
//
//  // ARGHH!  GTK doesn't pass the arg to the callback, so we can't
//  // get "this" back!  Until we solve this, get it from the service mgr:
//  nsresult rv;
//  NS_WITH_SERVICE(nsIClipboard, iclipboard, kCClipboardCID, &rv);
//
//  if (NS_FAILED(rv)) {
//    printf("Couldn't get clipboard service!\n");
//    return;
//  }
//  nsClipboard* clipboard = (nsClipboard*)iclipboard;
//  if (!clipboard) {
//    printf("couldn't convert nsIClipboard to nsClipboard\n");
//    return;
//  }
//
//  clipboard->SelectionReceiver(aWidget, aSelectionData);
//}
//
//void
//nsClipboard::SelectionReceiver (GtkWidget *aWidget,
//                                GtkSelectionData *aSelectionData)
//{
//  mBlocking = PR_FALSE;
//
//  if (aSelectionData->length < 0)
//  {
//    printf("Error retrieving selection: length was %d\n",
//           aSelectionData->length);
//    return;
//  }
//
//  switch (aSelectionData->type)
//  {
//    case GDK_SELECTION_TYPE_STRING:
//      mSelectionData = *aSelectionData;
//      mSelectionData.data = g_new(guchar, aSelectionData->length + 1);
//      memcpy(mSelectionData.data,
//             aSelectionData->data, aSelectionData->length);
//      // Null terminate in case anyone cares,
//      // and so we can print the string for debugging:
//      mSelectionData.data[aSelectionData->length] = '\0';
//      mSelectionData.length = aSelectionData->length;
//      return;
//
//    default:
//      printf("Can't convert type %s (%ld) to string\n",
//             gdk_atom_name (aSelectionData->type), aSelectionData->type);
//      return;
//  }
//}
//
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
//// This is the callback which is called when another app
//// requests the selection.
////
//void nsClipboard::SelectionGetCB(GtkWidget        *widget,
//                                 GtkSelectionData *aSelectionData,
//                                 guint      /*info*/,
//                                 guint      /*time*/,
//                                 gpointer   aData)
//{ 
//#ifdef DEBUG_CLIPBOARD
//  printf("  nsClipboard::SelectionGetCB\n"); 
//#endif /* DEBUG_CLIPBOARD */
//
//  nsClipboard *clipboard = (nsClipboard *)aData;
//
//  void     *clipboardData;
//  PRUint32 dataLength;
//  nsresult rv;
//
//  // Make sure we have a transferable:
//  if (!clipboard->mTransferable) {
//    printf("Clipboard has no transferable!\n");
//    return;
//  }
//
//  // XXX hack, string-only for now.
//  // Create string data-flavor.
//  nsString dataFlavor (kTextMime);
//
//  // Get data out of transferable.
//  rv = clipboard->mTransferable->GetTransferData(&dataFlavor, 
//                                                 &clipboardData,
//                                                 &dataLength);
//
//  // Currently we only offer the data in GDK_SELECTION_TYPE_STRING format.
//  if (NS_SUCCEEDED(rv) && clipboardData && dataLength > 0) {
//    gtk_selection_data_set(aSelectionData,
//                           GDK_SELECTION_TYPE_STRING, 8,
//                           (unsigned char *)clipboardData,
//                           dataLength);
//  }
//  else
//    printf("Transferable didn't support the data flavor\n");
//}
//
//
//
//// Called when another app requests selection ownership:
//void nsClipboard::SelectionClearCB(GtkWidget *widget,
//                                   GdkEventSelection *event,
//                                   gpointer data)
//{
//#ifdef DEBUG_CLIPBOARD
//  printf("  nsClipboard::SelectionClearCB\n");
//#endif /* DEBUG_CLIPBOARD */
//}
//
//
//// The routine called when another app asks for the content of the selection
//void
//nsClipboard::SelectionRequestCB (GtkWidget *aWidget,
//                                 GtkSelectionData *aSelectionData,
//                                 gpointer aData)
//{
//#ifdef DEBUG_CLIPBOARD
//  printf("  nsClipboard::SelectionRequestCB\n");
//#endif /* DEBUG_CLIPBOARD */
//}
//
//void
//nsClipboard::SelectionNotifyCB (GtkWidget *aWidget,
//                                  GtkSelectionData *aSelectionData,
//                                  gpointer aData)
//{
//#ifdef DEBUG_CLIPBOARD
//   printf("  nsClipboard::SelectionNotifyCB\n");
//#endif /* DEBUG_CLIPBOARD */
//}
