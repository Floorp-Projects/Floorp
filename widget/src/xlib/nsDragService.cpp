/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 * Christopher Blizzard <blizzard@mozilla.org>
 * Peter Hartshorn <peter@igelaus.com.au>
*/

#include "nsDragService.h"
#include "nsWidgetsCID.h"
#include "nsIWidget.h"
#include "nsIServiceManager.h"
#include "nsVoidArray.h"
#include "nsXPIDLString.h"
#include "nsISupportsPrimitives.h"
#include "nsPrimitiveHelpers.h"
#include "nsString.h"

#include <X11/xpm.h>
#include <X11/extensions/shape.h>

#include "xlibrgb.h"

NS_IMPL_ADDREF_INHERITED(nsDragService, nsBaseDragService)
NS_IMPL_RELEASE_INHERITED(nsDragService, nsBaseDragService)
NS_IMPL_QUERY_INTERFACE3(nsDragService, nsIDragService, nsIDragSession, nsIDragSessionXlib)

nsWidget *nsDragService::sWidget = nsnull;
Window    nsDragService::sWindow;
Display  *nsDragService::sDisplay;
PRBool    nsDragService::mDragging = PR_FALSE;

#undef DEBUG_peter
nsDragService::nsDragService()
{
  mCanDrop = PR_FALSE;
  sWindow = 0;
}


nsDragService::~nsDragService()
{
}

// nsIDragService
NS_IMETHODIMP nsDragService::InvokeDragSession (nsIDOMNode *aDOMNode,
                                                nsISupportsArray * anArrayTransferables,
                                                nsIScriptableRegion * aRegion,
                                                PRUint32 aActionType)
{
  nsresult rv;
  PRUint32 numItemsToDrag = 0;

  if (!anArrayTransferables)
    return NS_ERROR_INVALID_ARG;

  mDataItems = anArrayTransferables;
  NS_ADDREF(mDataItems);

  rv = mDataItems->Count(&numItemsToDrag);
  if (!numItemsToDrag)
    return NS_ERROR_FAILURE;

  if (numItemsToDrag > 1) {
    fprintf(stderr, "nsDragService: Cannot drag more than one item!\n");
    return NS_ERROR_FAILURE;
  }
  mDragging = PR_TRUE;

  if (sWindow == 0) {
    Pixmap aPixmap;
    Pixmap aShapeMask;
    XpmAttributes attr;
    XSetWindowAttributes wattr;
    XWMHints wmHints;

    wattr.override_redirect = true;
    
    sWindow = XCreateWindow(xlib_rgb_get_display(),
                            DefaultRootWindow(xlib_rgb_get_display()),
                            0,0,32,32,0,xlib_rgb_get_depth(),
                            InputOutput,CopyFromParent,
                            CWOverrideRedirect, &wattr);
    
    attr.valuemask = 0;
    fprintf(stderr, "%s\n", XpmGetErrorString(
    XpmCreatePixmapFromData(xlib_rgb_get_display(),
                            sWindow,
                            drag_xpm,
                            &aPixmap,
                            &aShapeMask,
                            &attr))
    );
    wmHints.flags = StateHint;
    wmHints.initial_state = NormalState;
    XSetWMProperties(xlib_rgb_get_display(), sWindow, NULL, NULL, NULL, 0, NULL,
                     &wmHints, NULL);
    XSetTransientForHint(xlib_rgb_get_display(), sWindow, sWindow);
    XShapeCombineMask(xlib_rgb_get_display(), sWindow, ShapeClip,
                      0,0,aShapeMask, ShapeSet);
    XShapeCombineMask(xlib_rgb_get_display(), sWindow, ShapeBounding,
                      0,0,aShapeMask, ShapeSet);
    XMapWindow(xlib_rgb_get_display(), sWindow);
    GC agc = XCreateGC(xlib_rgb_get_display(), sWindow, 0, NULL);
    XCopyArea(xlib_rgb_get_display(), aPixmap, sWindow, agc,
              0,0,32,32,0,0);
    XFreeGC(xlib_rgb_get_display(), agc);
  }
  return NS_OK;
}




NS_IMETHODIMP nsDragService::StartDragSession()
{
  mDragging = PR_TRUE;
  return NS_OK;
}

NS_IMETHODIMP nsDragService::EndDragSession()
{
  if (sWindow) {
    XDestroyWindow(xlib_rgb_get_display(), sWindow);
    sWindow = 0;
  }
  mDragging = PR_FALSE;
  return NS_OK;
}


// nsIDragSession

// For some reason we need this, but GTK does not. Hmmm...
NS_IMETHODIMP nsDragService::GetCurrentSession (nsIDragSession **aSession)
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

NS_IMETHODIMP nsDragService::SetCanDrop(PRBool           aCanDrop)
{
  mCanDrop = aCanDrop;
  return NS_OK;
}

NS_IMETHODIMP nsDragService::GetCanDrop(PRBool          *aCanDrop)
{
  *aCanDrop = mCanDrop;
  return NS_OK;
}

NS_IMETHODIMP nsDragService::GetNumDropItems(PRUint32 * aNumItems)
{
  // FIXME: Currently we can only drop one item, cos we are lame, just like gtk
  *aNumItems = 1;
  return NS_OK;
}

NS_IMETHODIMP nsDragService::GetData(nsITransferable * aTransferable, PRUint32 anItemIndex)
{
  nsCOMPtr<nsISupports> genItem;

  mDataItems->GetElementAt(0, getter_AddRefs(genItem));

  nsCOMPtr<nsITransferable> item (do_QueryInterface(genItem));
  nsCOMPtr<nsISupports> data;
  PRUint32 dataLen = 0;

  // FIXME: we can only drag and drop data of type "text/unicode"

  item->GetTransferData("text/unicode", getter_AddRefs(data), &dataLen);

  aTransferable->SetTransferData("text/unicode", data, dataLen);

  EndDragSession();

  return NS_OK;
}

NS_IMETHODIMP nsDragService::IsDataFlavorSupported (const char *aDataFlavor, PRBool *_retval)
{
  //FIXME: we are lame

  const char *name = "text/unicode";

  if (strcmp(name, aDataFlavor) == 0) {
    *_retval = PR_TRUE;
  } else {
    *_retval = PR_FALSE;
  }
  return NS_OK;
}

//nsIDragSessionXlib
NS_IMETHODIMP nsDragService::IsDragging(PRBool *result) {
  *result = mDragging;
  return NS_OK;
}

NS_IMETHODIMP nsDragService::UpdatePosition(PRInt32 x, PRInt32 y){
  if (sWindow) {
    Window aRoot, aChild;
    int cx, cy;
    unsigned int mask;
    XQueryPointer(xlib_rgb_get_display(),
                  sWindow, &aRoot, &aChild,
                  &x, &y, &cx, &cy, &mask); 
    XMoveWindow(xlib_rgb_get_display(), sWindow, x, y);
  }
  return NS_OK;
}
