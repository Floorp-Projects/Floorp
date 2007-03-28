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
 *   Mats Palmgren <mats.palmgren@bredband.net>
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

#include "nsBaseDragService.h"
#include "nsITransferable.h"

#include "nsIServiceManager.h"
#include "nsITransferable.h"
#include "nsISupportsArray.h"
#include "nsSize.h"
#include "nsIRegion.h"
#include "nsXPCOM.h"
#include "nsISupportsPrimitives.h"
#include "nsCOMPtr.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIFrame.h"
#include "nsIDocument.h"
#include "nsIContent.h"
#include "nsIPresShell.h"
#include "nsIViewManager.h"
#include "nsIScrollableView.h"
#include "nsIDOMNode.h"
#include "nsIDOMMouseEvent.h"
#include "nsISelection.h"
#include "nsISelectionPrivate.h"
#include "nsPresContext.h"
#include "nsIEventStateManager.h"
#include "nsICanvasElement.h"
#include "nsIImage.h"
#include "nsIImageLoadingContent.h"
#include "gfxIImageFrame.h"
#include "imgIContainer.h"
#include "imgIRequest.h"
#include "nsIViewObserver.h"
#include "nsRegion.h"

#ifdef MOZ_CAIRO_GFX
#include "gfxContext.h"
#include "gfxImageSurface.h"

#endif

NS_IMPL_ADDREF(nsBaseDragService)
NS_IMPL_RELEASE(nsBaseDragService)
NS_IMPL_QUERY_INTERFACE2(nsBaseDragService, nsIDragService, nsIDragSession)


//-------------------------------------------------------------------------
//
// DragService constructor
//
//-------------------------------------------------------------------------
nsBaseDragService::nsBaseDragService()
  : mCanDrop(PR_FALSE), mDoingDrag(PR_FALSE), mHasImage(PR_FALSE),
    mDragAction(DRAGDROP_ACTION_NONE), mTargetSize(0,0),
    mImageX(0), mImageY(0), mScreenX(-1), mScreenY(-1)
{
}

//-------------------------------------------------------------------------
//
// DragService destructor
//
//-------------------------------------------------------------------------
nsBaseDragService::~nsBaseDragService()
{
}


//---------------------------------------------------------
NS_IMETHODIMP
nsBaseDragService::SetCanDrop(PRBool aCanDrop)
{
  mCanDrop = aCanDrop;
  return NS_OK;
}

//---------------------------------------------------------
NS_IMETHODIMP
nsBaseDragService::GetCanDrop(PRBool * aCanDrop)
{
  *aCanDrop = mCanDrop;
  return NS_OK;
}

//---------------------------------------------------------
NS_IMETHODIMP
nsBaseDragService::SetDragAction(PRUint32 anAction)
{
  mDragAction = anAction;
  return NS_OK;
}

//---------------------------------------------------------
NS_IMETHODIMP
nsBaseDragService::GetDragAction(PRUint32 * anAction)
{
  *anAction = mDragAction;
  return NS_OK;
}

//---------------------------------------------------------
NS_IMETHODIMP
nsBaseDragService::SetTargetSize(nsSize aDragTargetSize)
{
  mTargetSize = aDragTargetSize;
  return NS_OK;
}

//---------------------------------------------------------
NS_IMETHODIMP
nsBaseDragService::GetTargetSize(nsSize * aDragTargetSize)
{
  *aDragTargetSize = mTargetSize;
  return NS_OK;
}

//-------------------------------------------------------------------------

NS_IMETHODIMP
nsBaseDragService::GetNumDropItems(PRUint32 * aNumItems)
{
  *aNumItems = 0;
  return NS_ERROR_FAILURE;
}


//
// GetSourceDocument
//
// Returns the DOM document where the drag was initiated. This will be
// nsnull if the drag began outside of our application.
//
NS_IMETHODIMP
nsBaseDragService::GetSourceDocument(nsIDOMDocument** aSourceDocument)
{
  *aSourceDocument = mSourceDocument.get();
  NS_IF_ADDREF(*aSourceDocument);

  return NS_OK;
}

//
// GetSourceNode
//
// Returns the DOM node where the drag was initiated. This will be
// nsnull if the drag began outside of our application.
//
NS_IMETHODIMP
nsBaseDragService::GetSourceNode(nsIDOMNode** aSourceNode)
{
  *aSourceNode = mSourceNode.get();
  NS_IF_ADDREF(*aSourceNode);

  return NS_OK;
}


//-------------------------------------------------------------------------

NS_IMETHODIMP
nsBaseDragService::GetData(nsITransferable * aTransferable,
                           PRUint32 aItemIndex)
{
  return NS_ERROR_FAILURE;
}

//-------------------------------------------------------------------------
NS_IMETHODIMP
nsBaseDragService::IsDataFlavorSupported(const char *aDataFlavor,
                                         PRBool *_retval)
{
  return NS_ERROR_FAILURE;
}

//-------------------------------------------------------------------------
NS_IMETHODIMP
nsBaseDragService::InvokeDragSession(nsIDOMNode *aDOMNode,
                                     nsISupportsArray* aTransferableArray,
                                     nsIScriptableRegion* aDragRgn,
                                     PRUint32 aActionType)
{
  NS_ENSURE_TRUE(aDOMNode, NS_ERROR_INVALID_ARG);

  // stash the document of the dom node
  aDOMNode->GetOwnerDocument(getter_AddRefs(mSourceDocument));
  mSourceNode = aDOMNode;

  // When the mouse goes down, the selection code starts a mouse
  // capture. However, this gets in the way of determining drag
  // feedback for things like trees because the event coordinates
  // are in the wrong coord system. Turn off mouse capture in
  // the associated view manager.
  nsCOMPtr<nsIContent> contentNode = do_QueryInterface(aDOMNode);
  if (contentNode) {
    nsIDocument* doc = contentNode->GetCurrentDoc();
    if (doc) {
      nsIPresShell* presShell = doc->GetShellAt(0);
      if (presShell) {
        nsIViewManager* vm = presShell->GetViewManager();
        if (vm) {
          PRBool notUsed;
          vm->GrabMouseEvents(nsnull, notUsed);
        }
      }
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsBaseDragService::InvokeDragSessionWithImage(nsIDOMNode* aDOMNode,
                                              nsISupportsArray* aTransferableArray,
                                              nsIScriptableRegion* aRegion,
                                              PRUint32 aActionType,
                                              nsIDOMNode* aImage,
                                              PRInt32 aImageX, PRInt32 aImageY,
                                              nsIDOMMouseEvent* aDragEvent)
{
  NS_ENSURE_TRUE(aDragEvent, NS_ERROR_NULL_POINTER);

  mSelection = nsnull;
  mHasImage = PR_TRUE;
  mImage = aImage;
  mImageX = aImageX;
  mImageY = aImageY;

  aDragEvent->GetScreenX(&mScreenX);
  aDragEvent->GetScreenY(&mScreenY);

  return InvokeDragSession(aDOMNode, aTransferableArray, aRegion, aActionType);
}

NS_IMETHODIMP
nsBaseDragService::InvokeDragSessionWithSelection(nsISelection* aSelection,
                                                  nsISupportsArray* aTransferableArray,
                                                  PRUint32 aActionType,
                                                  nsIDOMMouseEvent* aDragEvent)
{
  NS_ENSURE_TRUE(aSelection, NS_ERROR_NULL_POINTER);
  NS_ENSURE_TRUE(aDragEvent, NS_ERROR_NULL_POINTER);

  mSelection = aSelection;
  mHasImage = PR_TRUE;
  mImage = nsnull;
  mImageX = 0;
  mImageY = 0;

  aDragEvent->GetScreenX(&mScreenX);
  aDragEvent->GetScreenY(&mScreenY);

  // just get the focused node from the selection
  nsCOMPtr<nsIDOMNode> node;
  aSelection->GetFocusNode(getter_AddRefs(node));

  return InvokeDragSession(node, aTransferableArray, nsnull, aActionType);
}

//-------------------------------------------------------------------------
NS_IMETHODIMP
nsBaseDragService::GetCurrentSession(nsIDragSession ** aSession)
{
  if (!aSession)
    return NS_ERROR_INVALID_ARG;

  // "this" also implements a drag session, so say we are one but only
  // if there is currently a drag going on.
  if (mDoingDrag) {
    *aSession = this;
    NS_ADDREF(*aSession);      // addRef because we're a "getter"
  }
  else
    *aSession = nsnull;

  return NS_OK;
}

//-------------------------------------------------------------------------
NS_IMETHODIMP
nsBaseDragService::StartDragSession()
{
  if (mDoingDrag) {
    return NS_ERROR_FAILURE;
  }
  mDoingDrag = PR_TRUE;
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_IMETHODIMP
nsBaseDragService::EndDragSession()
{
  if (!mDoingDrag) {
    return NS_ERROR_FAILURE;
  }

  mDoingDrag = PR_FALSE;

  // release the source we've been holding on to.
  mSourceDocument = nsnull;
  mSourceNode = nsnull;
  mSelection = nsnull;
  mHasImage = PR_FALSE;
  mImage = nsnull;
  mImageX = 0;
  mImageY = 0;
  mScreenX = -1;
  mScreenY = -1;

  return NS_OK;
}

#ifdef MOZ_CAIRO_GFX

static nsIPresShell*
GetPresShellForContent(nsIDOMNode* aDOMNode)
{
  nsCOMPtr<nsIContent> content = do_QueryInterface(aDOMNode);
  nsCOMPtr<nsIDocument> document = content->GetCurrentDoc();
  if (document) {
    document->FlushPendingNotifications(Flush_Display);

    return document->GetShellAt(0);
  }

  return nsnull;
}

nsresult
nsBaseDragService::DrawDrag(nsIDOMNode* aDOMNode,
                            nsIScriptableRegion* aRegion,
                            PRInt32 aScreenX, PRInt32 aScreenY,
                            nsRect* aScreenDragRect,
                            gfxASurface** aSurface)
{
  *aSurface = nsnull;

  // use a default size, in case of an error.
  aScreenDragRect->x = aScreenX - mImageX;
  aScreenDragRect->y = aScreenY - mImageY;
  aScreenDragRect->width = 20;
  aScreenDragRect->height = 20;

  // if a drag image was specified, use that, otherwise, use the source node
  nsCOMPtr<nsIDOMNode> dragNode = mImage ? mImage.get() : aDOMNode;

  // get the presshell for the node being dragged. If the drag image is not in
  // a document or has no frame, get the presshell from the source drag node
  nsIPresShell* presShell = GetPresShellForContent(dragNode);
  if (!presShell && mImage)
    presShell = GetPresShellForContent(aDOMNode);
  if (!presShell)
    return NS_ERROR_FAILURE;

  // didn't want an image, so just set the screen rectangle to the frame size
  if (!mHasImage) {
    // if a region was specified, set the screen rectangle to the area that
    // the region occupies
    if (aRegion) {
      // the region's coordinates are relative to the root frame
      nsPresContext* pc = presShell->GetPresContext();
      nsIFrame* rootFrame = presShell->GetRootFrame();
      if (rootFrame && pc) {
        nsRect dragRect;
        aRegion->GetBoundingBox(&dragRect.x, &dragRect.y, &dragRect.width, &dragRect.height);
        dragRect.ScaleRoundOut(nsPresContext::AppUnitsPerCSSPixel());
        dragRect.ScaleRoundOut(1.0 / pc->AppUnitsPerDevPixel());

        nsIntRect screenRect = rootFrame->GetScreenRectExternal();
        aScreenDragRect->SetRect(screenRect.x + dragRect.x, screenRect.y + dragRect.y,
                                 dragRect.width, dragRect.height);
      }
    }
    else {
      // otherwise, there was no region so just set the rectangle to
      // the size of the primary frame of the content.
      nsCOMPtr<nsIContent> content = do_QueryInterface(dragNode);
      nsIFrame* frame = presShell->GetPrimaryFrameFor(content);
      if (frame) {
        nsIntRect screenRect = frame->GetScreenRectExternal();
        aScreenDragRect->SetRect(screenRect.x, screenRect.y,
                                 screenRect.width, screenRect.height);
      }
    }

    return NS_OK;
  }

  // draw the image for selections
  if (mSelection) {
    nsPoint pnt(aScreenDragRect->x, aScreenDragRect->y);
    nsRefPtr<gfxASurface> surface = presShell->RenderSelection(mSelection, pnt, aScreenDragRect);
    *aSurface = surface;
    NS_IF_ADDREF(*aSurface);
    return NS_OK;
  }

  // if an custom image was specified, check if it is an image node and draw
  // using the source rather than the displayed image. But if mImage isn't
  // an image, fall through to RenderNode below.
  if (mImage) {
    nsCOMPtr<nsIImageLoadingContent> imageLoader = do_QueryInterface(dragNode);
    // for image nodes, create the drag image from the actual image data
    if (imageLoader) {
      nsPresContext* pc = presShell->GetPresContext();
      if (!pc)
        return NS_ERROR_FAILURE;

      return DrawDragForImage(pc, imageLoader, aScreenX, aScreenY,
                              aScreenDragRect, aSurface);
    }
  }

  // otherwise, just draw the node
  nsCOMPtr<nsIRegion> clipRegion;
  if (aRegion)
    aRegion->GetRegion(getter_AddRefs(clipRegion));

  nsPoint pnt(aScreenDragRect->x, aScreenDragRect->y);
  nsRefPtr<gfxASurface> surface = presShell->RenderNode(dragNode, clipRegion,
                                                        pnt, aScreenDragRect);

  // if an image was specified, reposition the drag rectangle to
  // the supplied offset in mImageX and mImageY.
  if (mImage) {
    aScreenDragRect->x = aScreenX - mImageX;
    aScreenDragRect->y = aScreenY - mImageY;
  }

  *aSurface = surface;
  NS_IF_ADDREF(*aSurface);

  return NS_OK;
}

nsresult
nsBaseDragService::DrawDragForImage(nsPresContext* aPresContext,
                                    nsIImageLoadingContent* aImageLoader,
                                    PRInt32 aScreenX, PRInt32 aScreenY,
                                    nsRect* aScreenDragRect,
                                    gfxASurface** aSurface)
{
  nsCOMPtr<imgIRequest> imgRequest;
  nsresult rv = aImageLoader->GetRequest(nsIImageLoadingContent::CURRENT_REQUEST,
                                        getter_AddRefs(imgRequest));
  NS_ENSURE_SUCCESS(rv, rv);
  if (!imgRequest)
    return NS_ERROR_NOT_AVAILABLE;

  nsCOMPtr<imgIContainer> imgContainer;
  rv = imgRequest->GetImage(getter_AddRefs(imgContainer));
  NS_ENSURE_SUCCESS(rv, rv);
  if (!imgContainer)
    return NS_ERROR_NOT_AVAILABLE;

  // use the size of the image as the size of the drag image
  imgContainer->GetWidth(&aScreenDragRect->width);
  imgContainer->GetHeight(&aScreenDragRect->height);

  nsRect srcRect = *aScreenDragRect;
  srcRect.MoveTo(0, 0);
  nsRect destRect = srcRect;

  if (destRect.width == 0 || destRect.height == 0)
    return NS_ERROR_FAILURE;

  // if the image is larger than half the screen size, scale it down. This
  // scaling algorithm is the same as is used in nsPresShell::PaintRangePaintInfo
  nsIDeviceContext* deviceContext = aPresContext->DeviceContext();
  nsRect maxSize;
  deviceContext->GetClientRect(maxSize);
  nscoord maxWidth = aPresContext->AppUnitsToDevPixels(maxSize.width >> 1);
  nscoord maxHeight = aPresContext->AppUnitsToDevPixels(maxSize.height >> 1);
  if (destRect.width > maxWidth || destRect.height > maxHeight) {
    float scale = 1.0;
    if (destRect.width > maxWidth)
      scale = PR_MIN(scale, float(maxWidth) / destRect.width);
    if (destRect.height > maxHeight)
      scale = PR_MIN(scale, float(maxHeight) / destRect.height);

    destRect.width = NSToIntFloor(float(destRect.width) * scale);
    destRect.height = NSToIntFloor(float(destRect.height) * scale);

    aScreenDragRect->x = NSToIntFloor(aScreenX - float(mImageX) * scale);
    aScreenDragRect->y = NSToIntFloor(aScreenY - float(mImageY) * scale);
    aScreenDragRect->width = destRect.width;
    aScreenDragRect->height = destRect.height;
  }

  nsRefPtr<gfxImageSurface> surface =
    new gfxImageSurface(gfxIntSize(destRect.width, destRect.height),
                        gfxImageSurface::ImageFormatARGB32);
  if (!surface)
    return NS_ERROR_FAILURE;

  *aSurface = surface;
  NS_ADDREF(*aSurface);

  nsCOMPtr<nsIRenderingContext> rc;
  deviceContext->CreateRenderingContextInstance(*getter_AddRefs(rc));
  rc->Init(deviceContext, surface);

  // clear the image before drawing
  gfxContext context(surface);
  context.SetOperator(gfxContext::OPERATOR_CLEAR);
  context.Rectangle(gfxRect(0, 0, destRect.width, destRect.height));
  context.Fill();

  PRInt32 upp = aPresContext->AppUnitsPerDevPixel();
  srcRect.ScaleRoundOut(upp);
  destRect.ScaleRoundOut(upp);
  return rc->DrawImage(imgContainer, srcRect, destRect);
}

#endif
