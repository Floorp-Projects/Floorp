/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim:ts=2:et:sw=2
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Christopher Blizzard
 * <blizzard@mozilla.org>.  Portions created by Christopher Blizzard
 * are Copyright (C) 1998 Christopher Blizzard. All Rights Reserved.
 *
 * Contributor(s):
 *   Christopher Blizzard <blizzard@mozilla.org>
 *   Peter Hartshorn <peter@igelaus.com.au>
*/

#include "nsAppShell.h"
#include "nsDragService.h"
#include "nsWidgetsCID.h"
#include "nsIWidget.h"
#include "nsIServiceManager.h"
#include "nsVoidArray.h"
#include "nsXPIDLString.h"
#include "nsXPCOM.h"
#include "nsISupportsPrimitives.h"
#include "nsPrimitiveHelpers.h"
#include "nsString.h"

#include <X11/extensions/shape.h>

#include "xlibrgb.h"

NS_IMPL_ISUPPORTS_INHERITED1(nsDragService,
                             nsBaseDragService,
                             nsIDragSessionXlib)

/* drag bitmaps */
static const unsigned char drag_bitmap[] = {
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x00,
   0x00, 0xc0, 0x02, 0x00, 0x00, 0x60, 0x06, 0x00, 0x00, 0x38, 0x04, 0x00,
   0x00, 0x0e, 0x0c, 0x00, 0x80, 0x03, 0x08, 0x00, 0xe0, 0x00, 0x18, 0x00,
   0xb0, 0x00, 0x30, 0x00, 0x20, 0x01, 0x60, 0x00, 0x20, 0x02, 0xc0, 0x00,
   0x20, 0x04, 0x80, 0x00, 0x20, 0x02, 0x80, 0x01, 0x20, 0x01, 0x00, 0x03,
   0xa0, 0x00, 0x00, 0x06, 0x60, 0x00, 0x00, 0x0c, 0x60, 0x00, 0x00, 0x08,
   0x20, 0x00, 0x00, 0x0c, 0x60, 0x00, 0x00, 0x03, 0x40, 0x00, 0x80, 0x01,
   0xc0, 0x00, 0xe0, 0x00, 0x80, 0x00, 0x30, 0x00, 0x80, 0x01, 0x0c, 0x00,
   0x00, 0x01, 0x06, 0x00, 0x00, 0x03, 0x03, 0x00, 0x00, 0x82, 0x01, 0x00,
   0x00, 0x64, 0x00, 0x00, 0x00, 0x38, 0x00, 0x00, 0x00, 0x18, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };

static const unsigned char drag_mask[] = {
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x00,
   0x00, 0xc0, 0x03, 0x00, 0x00, 0xe0, 0x07, 0x00, 0x00, 0xf8, 0x07, 0x00,
   0x00, 0xfe, 0x0f, 0x00, 0x80, 0xff, 0x0f, 0x00, 0xe0, 0xff, 0x1f, 0x00,
   0xf0, 0xff, 0x3f, 0x00, 0xe0, 0xff, 0x7f, 0x00, 0xe0, 0xff, 0xff, 0x00,
   0xe0, 0xff, 0xff, 0x00, 0xe0, 0xff, 0xff, 0x01, 0xe0, 0xff, 0xff, 0x03,
   0xe0, 0xff, 0xff, 0x07, 0xe0, 0xff, 0xff, 0x0f, 0xe0, 0xff, 0xff, 0x0f,
   0xe0, 0xff, 0xff, 0x0f, 0xe0, 0xff, 0xff, 0x03, 0xc0, 0xff, 0xff, 0x01,
   0xc0, 0xff, 0xff, 0x00, 0x80, 0xff, 0x3f, 0x00, 0x80, 0xff, 0x0f, 0x00,
   0x00, 0xff, 0x07, 0x00, 0x00, 0xff, 0x03, 0x00, 0x00, 0xfe, 0x01, 0x00,
   0x00, 0x7c, 0x00, 0x00, 0x00, 0x38, 0x00, 0x00, 0x00, 0x18, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };

nsWidget *nsDragService::sWidget = nsnull;
Window    nsDragService::sWindow;
XlibRgbHandle *nsDragService::sXlibRgbHandle;
Display  *nsDragService::sDisplay;
PRBool    nsDragService::mDragging = PR_FALSE;

nsDragService::nsDragService()
{
  sXlibRgbHandle = nsAppShell::GetXlibRgbHandle();
  sDisplay = xxlib_rgb_get_display(sXlibRgbHandle);
  mCanDrop = PR_FALSE;
  sWindow = None;
}

nsDragService::~nsDragService()
{
}

// nsIDragService
NS_IMETHODIMP nsDragService::InvokeDragSession (nsIDOMNode *aDOMNode,
                                                nsISupportsArray *aArrayTransferables,
                                                nsIScriptableRegion *aRegion,
                                                PRUint32 aActionType)
{
  nsBaseDragService::InvokeDragSession(aDOMNode, aArrayTransferables,
                                       aRegion, aActionType);

  /* no data - no dnd */
  if (!aArrayTransferables)
    return NS_ERROR_INVALID_ARG;

  nsresult rv;
  PRUint32 numItemsToDrag = 0;

  mSourceDataItems = aArrayTransferables;

  rv = mSourceDataItems->Count(&numItemsToDrag);
  if (!numItemsToDrag)
    return NS_ERROR_FAILURE;

  mDragging = PR_TRUE;

  CreateDragCursor(aActionType);

  return NS_OK;
}

NS_IMETHODIMP nsDragService::StartDragSession()
{
  mDragging = PR_TRUE;

  return nsBaseDragService::StartDragSession();
}

NS_IMETHODIMP nsDragService::EndDragSession()
{
  if (sWindow) {
    XDestroyWindow(sDisplay, sWindow);
    sWindow = 0;
  }
  mDragging = PR_FALSE;

  return nsBaseDragService::EndDragSession();
}

// nsIDragSession

// For some reason we need this, but GTK does not. Hmmm...
NS_IMETHODIMP nsDragService::GetCurrentSession(nsIDragSession **aSession)
{
  if (!aSession)
    return NS_ERROR_FAILURE;

  if (!mDragging) {
    *aSession = nsnull;
    return NS_OK;
  }

  *aSession = (nsIDragSession *)this;
  NS_ADDREF(*aSession);
  return NS_OK;
}

NS_IMETHODIMP nsDragService::SetCanDrop(PRBool aCanDrop)
{
  mCanDrop = aCanDrop;
  return NS_OK;
}

NS_IMETHODIMP nsDragService::GetCanDrop(PRBool *aCanDrop)
{
  *aCanDrop = mCanDrop;
  return NS_OK;
}

NS_IMETHODIMP nsDragService::GetNumDropItems(PRUint32 *aNumItems)
{
  mSourceDataItems->Count(aNumItems);
  return NS_OK;
}

NS_IMETHODIMP nsDragService::GetData(nsITransferable *aTransferable, PRUint32 anItemIndex)
{
  if (!aTransferable)
    return NS_ERROR_INVALID_ARG;

  nsresult rv = NS_ERROR_FAILURE;
  nsCOMPtr <nsISupportsArray> flavorList;
  rv = aTransferable->FlavorsTransferableCanImport(getter_AddRefs(flavorList));
  if (NS_FAILED(rv))
    return rv;

  PRUint32 cnt, i;
  flavorList->Count (&cnt);

  for (i = 0; i < cnt; ++i) {
    nsCAutoString foundFlavor;
    nsCOMPtr <nsISupports> genericWrapper;

    flavorList->GetElementAt(i, getter_AddRefs(genericWrapper));
    nsCOMPtr <nsISupportsCString> currentFlavor;
    currentFlavor = do_QueryInterface(genericWrapper);

    if (currentFlavor) {
      nsXPIDLCString flavorStr;
      currentFlavor->ToString(getter_Copies(flavorStr));
      foundFlavor = nsCAutoString(flavorStr);

#if 0
      g_print("Looking for data in type %s\n",
          NS_STATIC_CAST(const char*, flavorStr));
#endif

      /* set the data */
      nsCOMPtr <nsISupports> genItem;
      mSourceDataItems->GetElementAt(anItemIndex, getter_AddRefs(genItem));
      
      nsCOMPtr <nsITransferable> item(do_QueryInterface(genItem));
      nsCOMPtr <nsISupports> data;
      PRUint32 dataLen = 0;

      item->GetTransferData(foundFlavor.get(), getter_AddRefs(data), &dataLen);
      aTransferable->SetTransferData(foundFlavor.get(), data, dataLen);
    }
  }

  EndDragSession();

  return NS_OK;
}

NS_IMETHODIMP nsDragService::IsDataFlavorSupported(const char *aDataFlavor, PRBool *_retval)
{
  /* XXX Please implement this - for now - support all flavors */
  *_retval = PR_TRUE;
  return NS_OK;
}

// nsIDragSessionXlib

NS_IMETHODIMP nsDragService::IsDragging(PRBool *result) {
  *result = mDragging;
  return NS_OK;
}

NS_IMETHODIMP nsDragService::UpdatePosition(PRInt32 x, PRInt32 y)
{
  if (sWindow) {
    Window aRoot, aChild;
    int cx, cy;
    unsigned int mask;
    XQueryPointer(sDisplay, sWindow, &aRoot, &aChild, &x, &y, &cx, &cy, &mask); 
    XMoveWindow(sDisplay, sWindow, x, y);
  }
  return NS_OK;
}

void nsDragService::CreateDragCursor(PRUint32 aActionType)
{
  if (sWindow == None) {
    Pixmap aPixmap;
    Pixmap aShapeMask;
    XSetWindowAttributes wattr;
    unsigned long wattr_mask;
    XWMHints wmHints;
    int depth;
    Screen *screen = xxlib_rgb_get_screen(sXlibRgbHandle);
    int screennum = XScreenNumberOfScreen(screen);

    wattr.override_redirect = True;
    wattr.background_pixel  = XWhitePixel(sDisplay, screennum);
    wattr.border_pixel      = XBlackPixel(sDisplay, screennum);
    wattr.colormap          = xxlib_rgb_get_cmap(sXlibRgbHandle);
    wattr_mask = CWOverrideRedirect | CWBorderPixel | CWBackPixel;
    if (wattr.colormap)
      wattr_mask |= CWColormap;
    
    depth = xxlib_rgb_get_depth(sXlibRgbHandle);
    
    /* make a window off-screen at -64, -64 */
    sWindow = XCreateWindow(sDisplay, XRootWindowOfScreen(screen),
                            -64, -64, 32, 32, 0, depth,
                            InputOutput, xxlib_rgb_get_visual(sXlibRgbHandle),
                            wattr_mask, &wattr);
    
    aPixmap = XCreatePixmapFromBitmapData(sDisplay, sWindow,
                                          (char *)drag_bitmap,
                                          32, 32, 0x0, 0xffffffff, depth);

    aShapeMask = XCreatePixmapFromBitmapData(sDisplay, sWindow,
                                             (char *)drag_mask,
                                             32, 32, 0xffffffff, 0x0, 1);

    wmHints.flags = StateHint;
    wmHints.initial_state = NormalState;
    XSetWMProperties(sDisplay, sWindow, nsnull, nsnull, nsnull, 0, nsnull,
                     &wmHints, nsnull);
    XSetTransientForHint(sDisplay, sWindow, sWindow);
    XShapeCombineMask(sDisplay, sWindow, ShapeClip, 0, 0, 
                      aShapeMask, ShapeSet);
    XShapeCombineMask(sDisplay, sWindow, ShapeBounding, 0, 0,
                      aShapeMask, ShapeSet);
    XSetWindowBackgroundPixmap(sDisplay, sWindow, aPixmap);
    XMapWindow(sDisplay, sWindow);
  }
}
