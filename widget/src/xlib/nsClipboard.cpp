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
 *   Peter Hartshorn <peter@igelaus.com.au>
 */

/* TODO:
 * Currently this only supports the transfer of TEXT! FIXME
 */

#include "nsClipboard.h"

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include "string.h"

#include "nsCOMPtr.h"
#include "nsFileSpec.h"
#include "nsCRT.h"
#include "nsISupportsArray.h"
#include "nsISupportsPrimitives.h"

#include "nsIComponentManager.h"
#include "nsIServiceManager.h"
#include "nsWidgetsCID.h"
#include "nsXPIDLString.h"
#include "nsPrimitiveHelpers.h"

#include "nsTextFormatter.h"
#include "nsVoidArray.h"

#include "xlibrgb.h"


#include "nsIServiceManager.h"
#include "nsICharsetConverterManager.h"
// unicode conversion
#define NS_IMPL_IDS
#  include "nsIPlatformCharset.h"
#undef NS_IMPL_IDS


// The class statics:
nsCOMPtr<nsITransferable>   nsClipboard::mTransferable = nsnull;
Window                      nsClipboard::sWindow;
Display                    *nsClipboard::sDisplay;

#if defined(DEBUG_mcafee) || defined(DEBUG_pavlov)
#define DEBUG_CLIPBOARD
#endif

NS_IMPL_ISUPPORTS1(nsClipboard, nsIClipboard);

nsClipboard::nsClipboard() {
  NS_INIT_REFCNT();

  sDisplay = xlib_rgb_get_display();

  Init();
}

nsClipboard::~nsClipboard() {
}

// Initialize the clipboard and create a nsWidget for communications

void nsClipboard::Init() {
  sWidget = new nsWidget();
  const nsRect rect(0,0,100,100);

  sWidget->Create((nsIWidget *)nsnull, rect, Callback,
                  (nsIDeviceContext *)nsnull, (nsIAppShell *)nsnull,
                  (nsIToolkit *)nsnull, (nsWidgetInitData *)nsnull);
  sWindow = (Window)sWidget->GetNativeData(NS_NATIVE_WINDOW);

  XSelectInput(sDisplay, sWindow, 0x0fffff);
}

// This is the callback function for our nsWidget. It is given the
// XEvent from nsAppShell.

nsEventStatus PR_CALLBACK nsClipboard::Callback(nsGUIEvent *event) {
  XEvent *ev = (XEvent *)event->nativeMsg;

  // Check the event type
  if (ev->type == SelectionRequest) {
    if (mTransferable == nsnull) {
      fprintf(stderr, "nsClipboard::Callback: null transferable\n");
      return nsEventStatus_eIgnore;
    }

    // Get the data from the Transferable

    char *dataFlavor = kUnicodeMime;
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

NS_IMETHODIMP nsClipboard::SetData(nsITransferable *aTransferable,
                                   nsIClipboardOwner *anOwner,
                                   PRInt32 aWhichClipboard) 
{
  if (XSetSelectionOwner(sDisplay, XA_PRIMARY, sWindow, CurrentTime))
    if (XGetSelectionOwner(sDisplay, XA_PRIMARY) != sWindow) {
      fprintf(stderr, "nsClipboard::SetData: Cannot get ownership\n");
      return NS_ERROR_FAILURE;
    }
  mTransferable = aTransferable;
  return NS_OK;
}

NS_IMETHODIMP nsClipboard::GetData(nsITransferable *aTransferable,
                                   PRInt32 aWhcihClipboard)
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

  // If we currently own the selection, we will handle the paste 
  // internally, otherwise get the data from the X server

  if (XGetSelectionOwner(sDisplay, XA_PRIMARY) == sWindow) {
    char *dataFlavor = kUnicodeMime;
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
    data = (unsigned char *)malloc(16384);
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
        bytes = strlen((char *)data);
      }
    }
    mBlocking = PR_FALSE;

    // Place the data in the transferable
    PRInt32 length = bytes;
    PRUnichar *testing = (PRUnichar *)malloc(strlen((char *)data)*2+1);
    nsPrimitiveHelpers::ConvertPlatformPlainTextToUnicode((const char *)data,
                                                          length,
                                                          &testing,
                                                          &length);

    nsCOMPtr<nsISupports> genDataWrapper;
    nsPrimitiveHelpers::CreatePrimitiveForData("text/unicode",
                                               testing, length,
                                               getter_AddRefs(genDataWrapper));

    aTransferable->SetTransferData("text/unicode",
                                   genDataWrapper,
                                   bytes);
    free(data);
    free(testing);
  }
  return NS_OK;
}

NS_IMETHODIMP nsClipboard::EmptyClipboard(PRInt32 aWhichClipboard) {
  return NS_OK;
}

NS_IMETHODIMP nsClipboard::ForceDataToClipboard(PRInt32 aWhichClipboard) {
  return NS_OK;
}

NS_IMETHODIMP nsClipboard::HasDataMatchingFlavors(nsISupportsArray *aFlavorList,
                                                  PRInt32 aWhichClipboard,
                                                  PRBool *_retval) {
  *_retval = PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP nsClipboard::SupportsSelectionClipboard(PRBool *_retval) {
  *_retval = PR_TRUE;
  return NS_OK;
}
