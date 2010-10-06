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
 * the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Daniel Holbert <dholbert@mozilla.com>
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

#include "VectorImage.h"
#include "imgIDecoderObserver.h"
#include "SVGDocumentWrapper.h"
#include "gfxContext.h"
#include "gfxPlatform.h"
#include "nsPresContext.h"
#include "nsRect.h"
#include "nsIDocumentViewer.h"
#include "nsIObserverService.h"
#include "nsIPresShell.h"
#include "nsIStreamListener.h"
#include "nsComponentManagerUtils.h"
#include "nsServiceManagerUtils.h"
#include "nsSVGUtils.h"  // for nsSVGUtils::ConvertToSurfaceSize
#include "nsSVGEffects.h" // for nsSVGRenderingObserver
#include "gfxDrawable.h"
#include "gfxUtils.h"
#include "nsSVGSVGElement.h"

using namespace mozilla::dom;

namespace mozilla {
namespace imagelib {

#ifdef MOZ_ENABLE_LIBXUL
// Helper-class: SVGRootRenderingObserver
class SVGRootRenderingObserver : public nsSVGRenderingObserver {
public:
  SVGRootRenderingObserver(SVGDocumentWrapper* aDocWrapper,
                           VectorImage*        aVectorImage)
    : nsSVGRenderingObserver(),
      mDocWrapper(aDocWrapper),
      mVectorImage(aVectorImage)
  {
    StartListening();
    Element* elem = GetTarget();
    if (elem) {
      nsSVGEffects::AddRenderingObserver(elem, this);
      mInObserverList = PR_TRUE;
    }
#ifdef DEBUG
    else {
      NS_ABORT_IF_FALSE(!mInObserverList,
                        "Have no target, so we can't be in "
                        "target's observer list...");
    }
#endif
  }

  virtual ~SVGRootRenderingObserver()
  {
    StopListening();
  }

protected:
  virtual Element* GetTarget()
  {
    return mDocWrapper->GetRootSVGElem();
  }

  virtual void DoUpdate()
  {
    Element* elem = GetTarget();
    if (!elem)
      return;

    if (!mDocWrapper->ShouldIgnoreInvalidation()) {
      nsIFrame* frame = elem->GetPrimaryFrame();
      if (!frame || frame->PresContext()->PresShell()->IsDestroying()) {
        // We're being destroyed. Bail out.
        return;
      }

      mVectorImage->InvalidateObserver();
    }

    // Our caller might've removed us from rendering-observer list.
    // Add ourselves back!
    if (!mInObserverList) {
      nsSVGEffects::AddRenderingObserver(elem, this);
      mInObserverList = PR_TRUE;
    }
  }

  // Private data
  nsRefPtr<SVGDocumentWrapper> mDocWrapper;
  VectorImage* mVectorImage;   // Raw pointer because it owns me.
};
#endif // MOZ_ENABLE_LIBXUL

// Helper-class: SVGDrawingCallback
class SVGDrawingCallback : public gfxDrawingCallback {
public:
  SVGDrawingCallback(SVGDocumentWrapper* aSVGDocumentWrapper,
                     const nsIntRect& aViewport,
                     PRUint32 aImageFlags) :
    mSVGDocumentWrapper(aSVGDocumentWrapper),
    mViewport(aViewport),
    mImageFlags(aImageFlags)
  {}
  virtual PRBool operator()(gfxContext* aContext,
                            const gfxRect& aFillRect,
                            const gfxPattern::GraphicsFilter& aFilter,
                            const gfxMatrix& aTransform);
private:
  nsRefPtr<SVGDocumentWrapper> mSVGDocumentWrapper;
  const nsIntRect mViewport;
  PRUint32        mImageFlags;
};

// Based loosely on nsSVGIntegrationUtils' PaintFrameCallback::operator()
PRBool
SVGDrawingCallback::operator()(gfxContext* aContext,
                               const gfxRect& aFillRect,
                               const gfxPattern::GraphicsFilter& aFilter,
                               const gfxMatrix& aTransform)
{
  NS_ABORT_IF_FALSE(mSVGDocumentWrapper, "need an SVGDocumentWrapper");

  // Get (& sanity-check) the helper-doc's presShell
  nsCOMPtr<nsIPresShell> presShell;
  if (NS_FAILED(mSVGDocumentWrapper->GetPresShell(getter_AddRefs(presShell)))) {
    NS_WARNING("Unable to draw -- presShell lookup failed");
    return NS_ERROR_FAILURE;
  }
  NS_ABORT_IF_FALSE(presShell, "GetPresShell succeeded but returned null");

  aContext->Save();

  // Clip to aFillRect so that we don't paint outside.
  aContext->NewPath();
  aContext->Rectangle(aFillRect);
  aContext->Clip();
  gfxMatrix savedMatrix(aContext->CurrentMatrix());

  aContext->Multiply(gfxMatrix(aTransform).Invert());


  nsPresContext* presContext = presShell->GetPresContext();
  NS_ABORT_IF_FALSE(presContext, "pres shell w/out pres context");

  nsRect svgRect(presContext->DevPixelsToAppUnits(mViewport.x),
                 presContext->DevPixelsToAppUnits(mViewport.y),
                 presContext->DevPixelsToAppUnits(mViewport.width),
                 presContext->DevPixelsToAppUnits(mViewport.height));

  PRUint32 renderDocFlags = nsIPresShell::RENDER_IGNORE_VIEWPORT_SCROLLING;
  if (!(mImageFlags & imgIContainer::FLAG_SYNC_DECODE)) {
    renderDocFlags |= nsIPresShell::RENDER_ASYNC_DECODE_IMAGES;
  }

  presShell->RenderDocument(svgRect, renderDocFlags,
                            NS_RGBA(0, 0, 0, 0), // transparent
                            aContext);

  aContext->SetMatrix(savedMatrix);
  aContext->Restore();

  return PR_TRUE;
}

// Implement VectorImage's nsISupports-inherited methods
NS_IMPL_ISUPPORTS3(VectorImage,
                   imgIContainer,
                   nsIStreamListener,
                   nsIRequestObserver)

//------------------------------------------------------------------------------
// Constructor / Destructor

VectorImage::VectorImage(imgStatusTracker* aStatusTracker) :
  Image(aStatusTracker), // invoke superclass's constructor
  mRestrictedRegion(0, 0, 0, 0),
  mLastRenderedSize(0, 0),
  mAnimationMode(kNormalAnimMode),
  mIsInitialized(PR_FALSE),
  mIsFullyLoaded(PR_FALSE),
  mHaveAnimations(PR_FALSE),
  mHaveRestrictedRegion(PR_FALSE)
{
}

VectorImage::~VectorImage()
{
}

//------------------------------------------------------------------------------
// Methods inherited from Image.h

nsresult
VectorImage::Init(imgIDecoderObserver* aObserver,
                  const char* aMimeType,
                  const char* aURIString,
                  PRUint32 aFlags)
{
  // We don't support re-initialization
  if (mIsInitialized)
    return NS_ERROR_ILLEGAL_VALUE;

  NS_ABORT_IF_FALSE(!mIsFullyLoaded && !mHaveAnimations &&
                    !mHaveRestrictedRegion && !mError,
                    "Flags unexpectedly set before initialization");

  mObserver = do_GetWeakReference(aObserver);
  NS_ABORT_IF_FALSE(!strcmp(aMimeType, SVG_MIMETYPE), "Unexpected mimetype");

  mIsInitialized = PR_TRUE;

  return NS_OK;
}

void
VectorImage::GetCurrentFrameRect(nsIntRect& aRect)
{
  aRect = nsIntRect::GetMaxSizedIntRect();
}

PRUint32
VectorImage::GetDecodedDataSize()
{
  // XXXdholbert TODO: return num bytes used by helper SVG doc. (bug 590790)
  return sizeof(*this);
}

PRUint32
VectorImage::GetSourceDataSize()
{
  // We're not storing the source data -- we just feed that directly to
  // our helper SVG document as we receive it, for it to parse.
  // So 0 is an appropriate return value here.
  return 0;
}

nsresult
VectorImage::StartAnimation()
{
  if (mError)
    return NS_ERROR_FAILURE;

  if (mAnimationMode == kDontAnimMode ||
      !mIsFullyLoaded || !mHaveAnimations) {
    // Animation disabled, or helper-document not finished or lacks animations.
    return NS_OK;
  }

  mSVGDocumentWrapper->StartAnimation();

  return NS_OK;
}

nsresult
VectorImage::StopAnimation()
{
  if (mError)
    return NS_ERROR_FAILURE;

  if (!mIsFullyLoaded || !mHaveAnimations) {
    return NS_OK;
  }

  mSVGDocumentWrapper->StopAnimation();

  return NS_OK;
}

//------------------------------------------------------------------------------
// imgIContainer methods

//******************************************************************************
/* readonly attribute PRInt32 width; */
NS_IMETHODIMP
VectorImage::GetWidth(PRInt32* aWidth)
{
  if (mError || !mIsFullyLoaded) {
    return NS_ERROR_FAILURE;
  }

  if (!mSVGDocumentWrapper->GetWidthOrHeight(SVGDocumentWrapper::eWidth,
                                             *aWidth)) {
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

//******************************************************************************
/* readonly attribute PRInt32 height; */
NS_IMETHODIMP
VectorImage::GetHeight(PRInt32* aHeight)
{
  if (mError || !mIsFullyLoaded) {
    return NS_ERROR_FAILURE;
  }

  if (!mSVGDocumentWrapper->GetWidthOrHeight(SVGDocumentWrapper::eHeight,
                                             *aHeight)) {
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

//******************************************************************************
/* readonly attribute unsigned short type; */
NS_IMETHODIMP
VectorImage::GetType(PRUint16* aType)
{
  *aType = imgIContainer::TYPE_VECTOR;
  return NS_OK;
}

//******************************************************************************
/* readonly attribute boolean animated; */
NS_IMETHODIMP
VectorImage::GetAnimated(PRBool* aAnimated)
{
  if (mError || !mIsFullyLoaded)
    return NS_ERROR_FAILURE;

  *aAnimated = mSVGDocumentWrapper->IsAnimated();
  return NS_OK;
}

//******************************************************************************
/* readonly attribute boolean currentFrameIsOpaque; */
NS_IMETHODIMP
VectorImage::GetCurrentFrameIsOpaque(PRBool* aIsOpaque)
{
  NS_ENSURE_ARG_POINTER(aIsOpaque);
  *aIsOpaque = PR_FALSE;   // In general, SVG content is not opaque.
  return NS_OK;
}

//******************************************************************************
/* [noscript] gfxASurface getFrame(in PRUint32 aWhichFrame,
 *                                 in PRUint32 aFlags; */
NS_IMETHODIMP
VectorImage::GetFrame(PRUint32 aWhichFrame,
                      PRUint32 aFlags,
                      gfxASurface** _retval)
{
  NS_NOTYETIMPLEMENTED("VectorImage::GetFrame");
  return NS_ERROR_NOT_IMPLEMENTED;
}

//******************************************************************************
/* [noscript] gfxImageSurface copyFrame(in PRUint32 aWhichFrame,
 *                                      in PRUint32 aFlags); */
NS_IMETHODIMP
VectorImage::CopyFrame(PRUint32 aWhichFrame,
                       PRUint32 aFlags,
                       gfxImageSurface** _retval)
{
  if (aWhichFrame > FRAME_MAX_VALUE)
    return NS_ERROR_INVALID_ARG;

  if (mError)
    return NS_ERROR_FAILURE;

  NS_NOTYETIMPLEMENTED("VectorImage::CopyFrame");
  return NS_ERROR_NOT_IMPLEMENTED;
}

//******************************************************************************
/* [noscript] imgIContainer extractFrame(PRUint32 aWhichFrame,
 *                                       [const] in nsIntRect aRegion,
 *                                       in PRUint32 aFlags); */
NS_IMETHODIMP
VectorImage::ExtractFrame(PRUint32 aWhichFrame,
                          const nsIntRect& aRegion,
                          PRUint32 aFlags,
                          imgIContainer** _retval)
{
  NS_ENSURE_ARG_POINTER(_retval);
  if (mError || !mIsFullyLoaded)
    return NS_ERROR_FAILURE;

  // XXXdholbert NOTE: This method assumes FRAME_CURRENT (not FRAME_FIRST)
  // right now, because mozilla doesn't actually contain any clients of this
  // method that use FRAME_FIRST.  If it's needed, we *could* handle
  // FRAME_FIRST by saving the helper-doc's current SMIL time, seeking it to
  // time 0, rendering to a RasterImage, and then restoring our saved time.
  if (aWhichFrame != FRAME_CURRENT) {
    NS_WARNING("VectorImage::ExtractFrame with something other than "
               "FRAME_CURRENT isn't supported yet. Assuming FRAME_CURRENT.");
  }

  // XXXdholbert This method also doesn't actually freeze animation in the
  // returned imgIContainer, because it shares our helper-document. To
  // get a true snapshot, we need to clone the document - see bug 590792.

  // Make a new container with same SVG document.
  nsRefPtr<VectorImage> extractedImg = new VectorImage();
  extractedImg->mSVGDocumentWrapper = mSVGDocumentWrapper;
  extractedImg->mAnimationMode = kDontAnimMode;

  extractedImg->mRestrictedRegion.x = aRegion.x;
  extractedImg->mRestrictedRegion.y = aRegion.y;

  // (disallow negative width/height on our restricted region)
  extractedImg->mRestrictedRegion.width  = NS_MAX(aRegion.width,  0);
  extractedImg->mRestrictedRegion.height = NS_MAX(aRegion.height, 0);

  extractedImg->mIsInitialized = PR_TRUE;
  extractedImg->mIsFullyLoaded = PR_TRUE;
  extractedImg->mHaveRestrictedRegion = PR_TRUE;

  *_retval = extractedImg.forget().get();
  return NS_OK;
}


//******************************************************************************
/* [noscript] void draw(in gfxContext aContext,
 *                      in gfxGraphicsFilter aFilter,
 *                      [const] in gfxMatrix aUserSpaceToImageSpace,
 *                      [const] in gfxRect aFill,
 *                      [const] in nsIntRect aSubimage,
 *                      [const] in nsIntSize aViewportSize,
 *                      in PRUint32 aFlags); */
NS_IMETHODIMP
VectorImage::Draw(gfxContext* aContext,
                  gfxPattern::GraphicsFilter aFilter,
                  const gfxMatrix& aUserSpaceToImageSpace,
                  const gfxRect& aFill,
                  const nsIntRect& aSubimage,
                  const nsIntSize& aViewportSize,
                  PRUint32 aFlags)
{
  if (mError || !mIsFullyLoaded)
    return NS_ERROR_FAILURE;

  NS_ENSURE_ARG_POINTER(aContext);

  if (aViewportSize != mLastRenderedSize) {
    mSVGDocumentWrapper->UpdateViewportBounds(aViewportSize);
    mLastRenderedSize = aViewportSize;
  }

  nsIntSize imageSize = mHaveRestrictedRegion ?
    mRestrictedRegion.Size() : aViewportSize;

  // XXXdholbert Do we need to convert image size from
  // CSS pixels to dev pixels here? (is gfxCallbackDrawable's 2nd arg in dev
  // pixels?)
  gfxIntSize imageSizeGfx(imageSize.width, imageSize.height);

  // Based on imgFrame::Draw
  gfxRect sourceRect = aUserSpaceToImageSpace.Transform(aFill);
  gfxRect imageRect(0, 0, imageSize.width, imageSize.height);
  gfxRect subimage(aSubimage.x, aSubimage.y, aSubimage.width, aSubimage.height);


  nsRefPtr<gfxDrawingCallback> cb =
    new SVGDrawingCallback(mSVGDocumentWrapper,
                           mHaveRestrictedRegion ?
                           mRestrictedRegion :
                           nsIntRect(nsIntPoint(0, 0), aViewportSize),
                           aFlags);

  nsRefPtr<gfxDrawable> drawable = new gfxCallbackDrawable(cb, imageSizeGfx);

  gfxUtils::DrawPixelSnapped(aContext, drawable,
                             aUserSpaceToImageSpace,
                             subimage, sourceRect, imageRect, aFill,
                             gfxASurface::ImageFormatARGB32, aFilter);

  return NS_OK;
}

//******************************************************************************
/* [notxpcom] nsIFrame GetRootLayoutFrame() */
nsIFrame*
VectorImage::GetRootLayoutFrame()
{
  return mSVGDocumentWrapper->GetRootLayoutFrame();
}

//******************************************************************************
/* void requestDecode() */
NS_IMETHODIMP
VectorImage::RequestDecode()
{
  // Nothing to do for SVG images
  return NS_OK;
}

//******************************************************************************
/* void lockImage() */
NS_IMETHODIMP
VectorImage::LockImage()
{
  // This method is for image-discarding, which only applies to RasterImages.
  return NS_OK;
}

//******************************************************************************
/* void unlockImage() */
NS_IMETHODIMP
VectorImage::UnlockImage()
{
  // This method is for image-discarding, which only applies to RasterImages.
  return NS_OK;
}

//******************************************************************************
/* attribute unsigned short animationMode; */
NS_IMETHODIMP
VectorImage::GetAnimationMode(PRUint16* aAnimationMode)
{
  if (mError)
    return NS_ERROR_FAILURE;

  NS_ENSURE_ARG_POINTER(aAnimationMode);
  
  *aAnimationMode = mAnimationMode;
  return NS_OK;
}

//******************************************************************************
/* attribute unsigned short animationMode; */
NS_IMETHODIMP
VectorImage::SetAnimationMode(PRUint16 aAnimationMode)
{
  // NOTE: This is just a simpler form of RasterImage::SetAnimationMode.
  // (Simpler because SVG animations don't have a concept of "loop once" mode)
  if (mError)
    return NS_ERROR_FAILURE;

  NS_ASSERTION(aAnimationMode == kNormalAnimMode ||
               aAnimationMode == kDontAnimMode ||
               aAnimationMode == kLoopOnceAnimMode,
               "An unrecognized Animation Mode is being set!");

  mAnimationMode = aAnimationMode;

  if (mAnimationMode == kDontAnimMode) {
    StopAnimation();
  } else { // kNormalAnimMode or kLoopOnceAnimMode (treated the same here)
    StartAnimation();
  }

  return NS_OK;
}

//******************************************************************************
/* void resetAnimation (); */
NS_IMETHODIMP
VectorImage::ResetAnimation()
{
  if (mError)
    return NS_ERROR_FAILURE;

  if (!mIsFullyLoaded || !mHaveAnimations) {
    return NS_OK; // There are no animations to be reset.
  }

  mSVGDocumentWrapper->ResetAnimation();
  
  return NS_OK;
}

//------------------------------------------------------------------------------
// nsIRequestObserver methods

//******************************************************************************
/* void onStartRequest(in nsIRequest request, in nsISupports ctxt); */
NS_IMETHODIMP
VectorImage::OnStartRequest(nsIRequest* aRequest, nsISupports* aCtxt)
{
  NS_ABORT_IF_FALSE(!mSVGDocumentWrapper,
                    "Repeated call to OnStartRequest -- can this happen?");

  mSVGDocumentWrapper = new SVGDocumentWrapper();
  nsresult rv = mSVGDocumentWrapper->OnStartRequest(aRequest, aCtxt);
  if (NS_FAILED(rv)) {
    mSVGDocumentWrapper = nsnull;
    mError = PR_TRUE;
  }

  return rv;
}

//******************************************************************************
/* void onStopRequest(in nsIRequest request, in nsISupports ctxt,
                      in nsresult status); */
NS_IMETHODIMP
VectorImage::OnStopRequest(nsIRequest* aRequest, nsISupports* aCtxt,
                           nsresult aStatus)
{
  if (mError)
    return NS_ERROR_FAILURE;

  NS_ABORT_IF_FALSE(!mIsFullyLoaded && !mHaveAnimations,
                    "these flags shouldn't get set until OnStopRequest. "
                    "Duplicate calls to OnStopRequest?");

  nsresult rv = mSVGDocumentWrapper->OnStopRequest(aRequest, aCtxt, aStatus);
  if (!mSVGDocumentWrapper->ParsedSuccessfully()) {
    // XXXdholbert Need to do something more here -- right now, this just
    // makes us draw the "object" icon, rather than the (jagged) "broken image"
    // icon.  See bug 594505.
    mError = PR_TRUE;
    return rv;
  }

  mIsFullyLoaded = PR_TRUE;
  mHaveAnimations = mSVGDocumentWrapper->IsAnimated();

  if (mHaveAnimations && mAnimationMode == kDontAnimMode) {
    // We're not supposed to be animating -- stop any animation before our
    // SVG document's timeline gets a chance to progress.
    mSVGDocumentWrapper->StopAnimation();
  }

#ifdef MOZ_ENABLE_LIBXUL
  // Start listening to our image for rendering updates
  mRenderingObserver = new SVGRootRenderingObserver(mSVGDocumentWrapper, this);
#endif // MOZ_ENABLE_LIBXUL

  // Tell *our* observers that we're done loading
  nsCOMPtr<imgIDecoderObserver> observer = do_QueryReferent(mObserver);
  if (observer) {
    // NOTE: This signals that width/height are available.
    observer->OnStartContainer(nsnull, this);

    observer->FrameChanged(this, &nsIntRect::GetMaxSizedIntRect());
    observer->OnStopFrame(nsnull, 0);
    observer->OnStopDecode(nsnull, NS_OK, nsnull);
  }

  return rv;
}

//------------------------------------------------------------------------------
// nsIStreamListener method

//******************************************************************************
/* void onDataAvailable(in nsIRequest request, in nsISupports ctxt,
                        in nsIInputStream inStr, in unsigned long sourceOffset,
                        in unsigned long count); */
NS_IMETHODIMP
VectorImage::OnDataAvailable(nsIRequest* aRequest, nsISupports* aCtxt,
                             nsIInputStream* aInStr, PRUint32 aSourceOffset,
                             PRUint32 aCount)
{
  return mSVGDocumentWrapper->OnDataAvailable(aRequest, aCtxt, aInStr,
                                              aSourceOffset, aCount);
}

// --------------------------
// Invalidation helper method

void
VectorImage::InvalidateObserver()
{
  nsCOMPtr<imgIContainerObserver> observer(do_QueryReferent(mObserver));
  if (observer) {
    observer->FrameChanged(this, &nsIntRect::GetMaxSizedIntRect());
  }
}

} // namespace imagelib
} // namespace mozilla
