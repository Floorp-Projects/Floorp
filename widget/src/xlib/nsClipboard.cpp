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

#include "nsIClipboardOwner.h"
#include "nsITransferable.h"   // kTextMime

#include "nsIWidget.h"
#include "nsIServiceManager.h"
#include "nsWidgetsCID.h"


// The class statics:
//GtkWidget* nsClipboard::sWidget = nsnull;

NS_IMPL_ADDREF_INHERITED(nsClipboard, nsBaseClipboard)
NS_IMPL_RELEASE_INHERITED(nsClipboard, nsBaseClipboard)

static NS_DEFINE_CID(kCClipboardCID,       NS_CLIPBOARD_CID);

#if defined(DEBUG_akkana) || defined(DEBUG_mcafee) || defined(DEBUG_pavlov) || defined(DEBUG_blizzard)
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
  CreateInvisibleWindow();
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
  if (sWindow &&
      XGetSelectionOwner(sDisplay, PRIMARY) == sWindow)
    ;
    //gtk_selection_remove_all(sWidget);

  // free the selection data, if any
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

  if (aIID.Equals(nsIClipboard::GetIID())) {
    *aInstancePtr = (void*) ((nsIClipboard*)this);
    NS_ADDREF_THIS();
    return NS_OK;
  }

  return rv;
}


//
// Create an invisible window to own the selection
//
void nsClipboard::CreateInvisibleWindow(void)
{
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
  if (sWindow && XGetSelectionOwner(sDisplay, PRIMARY) == sWindow)
    return NS_OK;

  // register as the selection owner:
  if (XSetSelectionOwner(sDisplay, PRIMARY, sWindow, CurrentTime) != 0)
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
  nsCOMPtr<nsITransferable> trans = do_QueryInterface(aTransferable);
  if (!trans)
    return rv;

  //
  // We can't call the copy callback when we're blocking on the paste callback;
  // so if this app is already the selection owner, we need to copy our own
  // data without going through the X server.
  //
  if (XGetSelectionOwner(sDisplay, PRIMARY) == sWindow)
  {
    // XXX only support text/plain for now
    nsAutoString  dataFlavor(kTextMime);

    // Get data out of our existing transferable.
    void     *clipboardData;
    PRUint32 dataLength;
    rv = mTransferable->GetTransferData(&dataFlavor, 
                                        &clipboardData,
                                        &dataLength);
    if (NS_SUCCEEDED(rv))
      rv = trans->SetTransferData(&dataFlavor,
                                  clipboardData, dataLength);
    return rv;
  }

  // Only support plaintext for now:

  // What is "property" (set to None here)?
  // Answer (from ORA Xlib book, p. 548):
  // If the property field is not None, the owner should place the
  // data resulting from converting the selection into the specified
  // property on the requestor window, setting the type as before.
  //
  // This should queue a SelectionNotify event (see that callback)
  XConvertSelection(sDisplay,
                    XA_PRIMARY, XA_STRING, None,
                    sWindow, CurrentTime);
  // XXX we're not supposed to use CurrentTime, we're supposed to use
  // the timestamp of the event causing us to request the selection.
  // Alas, we don't have that information any more by the time we get here.

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
    gtk_main_iteration_do(PR_TRUE);
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
  nsAutoString  dataFlavor(kTextMime);
  trans->SetTransferData(&dataFlavor,
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
  nsresult rv;
  NS_WITH_SERVICE(nsIClipboard, iclipboard, kCClipboardCID, &rv);

  if (NS_FAILED(rv)) {
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
nsClipboard::SelectionReceiver (Window aWindow,
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
    case XA_STRING:
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
void nsClipboard::SelectionRequestCB(Window aWindow,
                                     Atom selection,
                                     Atom target,
                                     Atom property,
                                     Window requestor,
                                     Time time)
{ 
#ifdef DEBUG_CLIPBOARD
  printf("  nsClipboard::SelectionRequestCB\n");
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
  nsString dataFlavor (kTextMime);

  // Get data out of transferable.
  rv = clipboard->mTransferable->GetTransferData(&dataFlavor, 
                                                 &clipboardData,
                                                 &dataLength);

  // Currently we only offer the data in GDK_SELECTION_TYPE_STRING format.
  if (NS_SUCCEEDED(rv) && clipboardData && dataLength > 0) {
    gtk_selection_data_set(aSelectionData,
                           GDK_SELECTION_TYPE_STRING, 8,
                           (unsigned char *)clipboardData,
                           dataLength);
  }
  else
    printf("Transferable didn't support the data flavor\n");
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

//
// The callback called after we've sent a convert selection request
// when the server calls us back with the data.
//
void
nsClipboard::SelectionNotifyCB(Window window,
                               Atom selection,
                               Atom target,
                               Atom property,
                               Time time)
{
#ifdef DEBUG_CLIPBOARD
   printf("  nsClipboard::SelectionNotifyCB\n");
#endif /* DEBUG_CLIPBOARD */

   // Un-block us, whether this works or not:
   mBlocking = PR_FALSE;

   if (property == None)
   {
#ifdef DEBUG_CLIPBOARD
     printf("Selection was refused!\n");
#endif /* DEBUG_CLIPBOARD */
     return;
   }

   // I don't know what all these parameters are!
   // So we're just throwing away the data.
   do
   {
     long offset;
     long length;
     Atom actualType;
     unsigned long nitems;
     unsigned long bytesAfter;
     unsigned char* propStr;
     XGetWindowProperty(sDisplay, sWindow, property,
                        offset, length, FALSE, AnyPropertyType,
                        &actualType, &nitems, &bytesAfter, propStr);
   } while (bytesAfter > 0);

   // Now delete that window property to show we've gotten all the data:
   XGetWindowProperty(sDisplay, sWindow, property,
                      offset, length, TRUE, AnyPropertyType,
                      &actualType, &nitems, &bytesAfter, propStr);
}
