/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
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
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Peter Hartshorn <peter@igelaus.com.au>
 *   Ken Faulkner <faulkner@igelaus.com.au>
 *   Dan Rosen <dr@netscape.com>
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

/* TODO:
 * Currently this only supports the transfer of TEXT! FIXME
 */

#include "nsAppShell.h"
#include "nsClipboard.h"

#include "nsCOMPtr.h"
#include "nsCRT.h"
#include "nsISupportsArray.h"
#include "nsXPCOM.h"
#include "nsISupportsPrimitives.h"

#include "nsIComponentManager.h"
#include "nsIServiceManager.h"
#include "nsWidgetsCID.h"
#include "nsXPIDLString.h"
#include "nsPrimitiveHelpers.h"

#include "nsTextFormatter.h"
#include "nsVoidArray.h"

#include <X11/Xlib.h>
#include <X11/Xatom.h>

#include "xlibrgb.h"


#include "nsIServiceManager.h"
#include "nsICharsetConverterManager.h"
// unicode conversion
#  include "nsIPlatformCharset.h"


// The class statics:
nsITransferable            *nsClipboard::mTransferable = nsnull;
Window                      nsClipboard::sWindow;
Display                    *nsClipboard::sDisplay;

#if defined(DEBUG_mcafee) || defined(DEBUG_pavlov)
#define DEBUG_CLIPBOARD
#endif

NS_IMPL_ISUPPORTS1(nsClipboard, nsIClipboard)

nsClipboard::nsClipboard() {
  sDisplay = xxlib_rgb_get_display(nsAppShell::GetXlibRgbHandle());

  Init();
}

nsClipboard::~nsClipboard() {
  NS_IF_RELEASE(sWidget);
}

/*static*/ void nsClipboard::Shutdown() {
  NS_IF_RELEASE(mTransferable);
}

// Initialize the clipboard and create a nsWidget for communications

void nsClipboard::Init() {
  NS_ASSERTION(!sWidget, "already initialized");

  sWidget = new nsWidget();
  if (!sWidget) return;
  NS_ADDREF(sWidget);

  const nsRect rect(0,0,100,100);

  sWidget->Create((nsIWidget *)nsnull, rect, Callback,
                  (nsIDeviceContext *)nsnull, (nsIAppShell *)nsnull,
                  (nsIToolkit *)nsnull, (nsWidgetInitData *)nsnull);
  sWindow = (Window)sWidget->GetNativeData(NS_NATIVE_WINDOW);

  XSelectInput(sDisplay, sWindow, 0x0fffff);
}

// This is the callback function for our nsWidget. It is given the
// XEvent from nsAppShell.
// FIXME: We _should_ assign mTransferable here depending on if its a 
// selectionrequest
nsEventStatus PR_CALLBACK nsClipboard::Callback(nsGUIEvent *event) {
  XEvent *ev = (XEvent *)event->nativeMsg;
  
  /* may be nsnull in the |event->message == NS_DESTROY| case... */
  if(ev == nsnull)
    return nsEventStatus_eIgnore;

  // Check the event type
  if (ev->type == SelectionRequest) {
    if (mTransferable == nsnull) {
      fprintf(stderr, "nsClipboard::Callback: null transferable\n");
      return nsEventStatus_eIgnore;
    }

    // Get the data from the Transferable

    const char *dataFlavor = kUnicodeMime;
    nsCOMPtr<nsISupports> genDataWrapper;
    nsresult rv;
    PRUint32 dataLength;
    void *data;
    data = malloc(16384);
    rv = mTransferable->GetTransferData(dataFlavor,
                                        getter_AddRefs(genDataWrapper),
                                        &dataLength);
    nsPrimitiveHelpers::CreateDataFromPrimitive(dataFlavor, genDataWrapper,
                                                &data, dataLength);
    if (NS_SUCCEEDED(rv) && data && dataLength) {
      char *plainText = nsnull;
      PRUnichar* unicodeData = NS_REINTERPRET_CAST(PRUnichar*, data);
      PRInt32 plainLen = 0;
      nsPrimitiveHelpers::ConvertUnicodeToPlatformPlainText(unicodeData,
                                                            dataLength/2,
                                                            &plainText,
                                                            &plainLen);
      if (data) {
        free(data);
        data = plainText;
        dataLength = plainLen;
      }

      // Set the window property to contain the data
      XChangeProperty(sDisplay,
                      ev->xselectionrequest.requestor,
                      ev->xselectionrequest.property, 
                      ev->xselectionrequest.target,
                      8, PropModeReplace,
                      (unsigned char *)data, dataLength);

      // Send the requestor a SelectionNotify event
      XEvent aEvent;
      aEvent.type = SelectionNotify;
      aEvent.xselection.serial = ev->xselectionrequest.serial;
      aEvent.xselection.display = ev->xselectionrequest.display;
      aEvent.xselection.requestor = ev->xselectionrequest.requestor;
      aEvent.xselection.selection = ev->xselectionrequest.selection;
      aEvent.xselection.target = ev->xselectionrequest.target;
      aEvent.xselection.property = ev->xselectionrequest.property;
      aEvent.xselection.time = CurrentTime;
      XSendEvent(sDisplay, ev->xselectionrequest.requestor, 1, 0, &aEvent);
   }
  }
  return nsEventStatus_eIgnore;
}

nsITransferable *nsClipboard::GetTransferable(PRInt32 aWhichClipboard)
{
  nsITransferable *transferable = nsnull;
  switch (aWhichClipboard)
  {
  case kGlobalClipboard:
    transferable = mGlobalTransferable;
    break;
  case kSelectionClipboard:
    transferable = mSelectionTransferable;
    break;
  }
  return transferable;
}

// Ripped from GTK. Does the writing to the appropriate transferable.
// Does *some* flavour stuff, but not all!!!
// FIXME: Needs to be completed.
NS_IMETHODIMP nsClipboard::SetNativeClipboardData(PRInt32 aWhichClipboard)
{
  // bomb out if we cannot get ownership.
  if (XSetSelectionOwner(sDisplay, XA_PRIMARY, sWindow, CurrentTime))
    if (XGetSelectionOwner(sDisplay, XA_PRIMARY) != sWindow) {
      fprintf(stderr, "nsClipboard::SetData: Cannot get ownership\n");
      return NS_ERROR_FAILURE;
    }
  
  // get flavor list that includes all flavors that can be written (including ones 
  // obtained through conversion)
  nsCOMPtr<nsISupportsArray> flavorList;
  nsCOMPtr<nsITransferable> transferable(GetTransferable(aWhichClipboard));

  // FIXME Need to make sure mTransferable has reference to selectionclipboard.
  // This solves the problem with copying to an external app.
  // but cannot be sure if its fully correct until menu copy/paste is working.
  if (aWhichClipboard == kSelectionClipboard) {
    NS_IF_RELEASE(mTransferable);
    mTransferable = transferable; 
    NS_IF_ADDREF(mTransferable);
  }
  
  // make sure we have a good transferable
  if (nsnull == transferable) {
#ifdef DEBUG_faulkner
    fprintf(stderr, "nsClipboard::SetNativeClipboardData(): no transferable!\n");
#endif /* DEBUG_faulkner */
    return NS_ERROR_FAILURE;
  }

  nsresult errCode = transferable->FlavorsTransferableCanExport ( getter_AddRefs(flavorList) );
  if ( NS_FAILED(errCode) )
    return NS_ERROR_FAILURE;

  PRUint32 cnt;
  flavorList->Count(&cnt);
  for ( PRUint32 i=0; i<cnt; ++i )
  {
    nsCOMPtr<nsISupports> genericFlavor;
    flavorList->GetElementAt ( i, getter_AddRefs(genericFlavor) );
    nsCOMPtr<nsISupportsCString> currentFlavor ( do_QueryInterface(genericFlavor) );
    if ( currentFlavor ) {
      nsXPIDLCString flavorStr;
      currentFlavor->ToString(getter_Copies(flavorStr));

      // FIXME Do we need to register this in XLIB? KenF
      // add these types as selection targets
      //      RegisterFormat(flavorStr, selectionAtom);
    }
  }

  mIgnoreEmptyNotification = PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP nsClipboard::SetData(nsITransferable *aTransferable,
                                   nsIClipboardOwner *anOwner,
                                   PRInt32 aWhichClipboard) 
{
  if (XSetSelectionOwner(sDisplay, XA_PRIMARY, sWindow, CurrentTime))
    if (XGetSelectionOwner(sDisplay, XA_PRIMARY) != sWindow) {
      fprintf(stderr, "nsClipboard::SetData: Cannot get ownership\n");
      return NS_ERROR_FAILURE;
    }

  // Check from GTK. (must double check this!!)
  if ((aTransferable == mGlobalTransferable.get() &&
       anOwner == mGlobalOwner.get() &&
       aWhichClipboard == kGlobalClipboard ) ||
      (aTransferable == mSelectionTransferable.get() &&
       anOwner == mSelectionOwner.get() &&
       aWhichClipboard == kSelectionClipboard)
      )
    {
      return NS_OK;
    }
  
  EmptyClipboard(aWhichClipboard);

  switch(aWhichClipboard) {
  case kSelectionClipboard:
    mSelectionOwner = anOwner;
    mSelectionTransferable = aTransferable;
    break;
  case kGlobalClipboard:
    mGlobalOwner = anOwner;
    mGlobalTransferable = aTransferable;
    break;
  }
  
  return SetNativeClipboardData(aWhichClipboard);
}

NS_IMETHODIMP nsClipboard::GetData(nsITransferable *aTransferable,
                                   PRInt32 aWhichClipboard)
{
  unsigned char *data = 0;
  unsigned long bytes = 0;
  Bool only_if_exists;
  Atom data_atom;
  int i;

  if (aTransferable == nsnull) {
    fprintf(stderr, "nsClipboard::GetData: NULL transferable\n");
    return NS_ERROR_FAILURE;
  }


  // Get which transferable we should use.
  NS_IF_RELEASE(mTransferable);
  mTransferable = GetTransferable(aWhichClipboard);
  NS_ASSERTION(!mTransferable,"mTransferable is null!! see bug 80181");
  if (!mTransferable) return NS_ERROR_FAILURE;
  NS_ADDREF(mTransferable);
  
  // If we currently own the selection, we will handle the paste 
  // internally, otherwise get the data from the X server

  if (XGetSelectionOwner(sDisplay, XA_PRIMARY) == sWindow) {
    const char *dataFlavor = kUnicodeMime;
    nsCOMPtr<nsISupports> genDataWrapper;
    nsresult rv;
    PRUint32 dataLength;
    nsCOMPtr<nsITransferable> trans = do_QueryInterface(aTransferable);
    if (!trans)
      return NS_ERROR_FAILURE;

    rv = mTransferable->GetTransferData(dataFlavor,
                                        getter_AddRefs(genDataWrapper),
                                        &dataLength);
    if (NS_SUCCEEDED(rv)) {
      rv = trans->SetTransferData(dataFlavor,
                                  genDataWrapper,
                                  dataLength);
    }
  } else {
    data_atom = XInternAtom(sDisplay, "DATA_ATOM", only_if_exists = False);
    XConvertSelection(sDisplay, XA_PRIMARY, XA_STRING, data_atom,
                      sWindow, CurrentTime);

    // Wait for the SelectNotify event
    mBlocking = PR_TRUE;
    XEvent event;
    for (i=0; (mBlocking == PR_TRUE) && i<10000; i++) {
      if (XPending(sDisplay)) {
        XNextEvent(sDisplay, &event);
        if (event.type == SelectionNotify) {
          mBlocking = PR_FALSE;
        }
      }
    }
    // If we got the event, mBlocking will still be true.
    // So, get the data.
    if (mBlocking == PR_FALSE) {
      Atom type;
      int format;
      unsigned long items;
      if (event.xselection.property != None) {
        XGetWindowProperty(sDisplay, event.xselection.requestor, 
                           event.xselection.property, 0, 16384/4,
                           0, AnyPropertyType,
                           &type, &format, &items, &bytes, &data);
        // XXX could return BadValue or BadWindow or ...
        bytes = strlen((char *)data);
      }
    }
    mBlocking = PR_FALSE;

    // Place the data in the transferable
    PRInt32 length = 0;
    PRUnichar *testing = nsnull;
    const char *constData = "";
    if (data)
       constData = (char*) data;
    nsPrimitiveHelpers::ConvertPlatformPlainTextToUnicode(constData,
                                                          (PRInt32)bytes,
                                                          &testing,
                                                          &length);

    nsCOMPtr<nsISupports> genDataWrapper;
    nsPrimitiveHelpers::CreatePrimitiveForData("text/unicode",
                                               testing, length,
                                               getter_AddRefs(genDataWrapper));

    aTransferable->SetTransferData("text/unicode",
                                   genDataWrapper,
                                   length);
    if (data)
      XFree(data);
    free(testing);
  }
  return NS_OK;
}

NS_IMETHODIMP nsClipboard::EmptyClipboard(PRInt32 aWhichClipboard) 
{
  if (mIgnoreEmptyNotification) {
    return NS_OK;
  }

  switch(aWhichClipboard) {
  case kSelectionClipboard:
    if (mSelectionOwner) {
      mSelectionOwner->LosingOwnership(mSelectionTransferable);
      mSelectionOwner = nsnull;
    }
    mSelectionTransferable = nsnull;
    break;
  case kGlobalClipboard:
    if (mGlobalOwner) {
      mGlobalOwner->LosingOwnership(mGlobalTransferable);
      mGlobalOwner = nsnull;
    }
    mGlobalTransferable = nsnull;
    break;
  }
  return NS_OK;
}

NS_IMETHODIMP nsClipboard::HasDataMatchingFlavors(nsISupportsArray *aFlavorList,
                                                  PRInt32 aWhichClipboard,
                                                  PRBool *_retval) {
  *_retval = PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP nsClipboard::SupportsSelectionClipboard(PRBool *_retval) {
  NS_ENSURE_ARG_POINTER(_retval);

  *_retval = PR_TRUE; // we support the selection clipboard on unix.
  return NS_OK;
}
