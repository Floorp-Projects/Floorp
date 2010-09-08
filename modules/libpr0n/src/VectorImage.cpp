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

namespace mozilla {
namespace imagelib {

NS_IMPL_ISUPPORTS3(VectorImage,
                   imgIContainer,
                   nsIStreamListener,
                   nsIRequestObserver)

//------------------------------------------------------------------------------
// Constructor / Destructor

VectorImage::VectorImage(imgStatusTracker* aStatusTracker) :
  Image(aStatusTracker) // invoke superclass's constructor
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
                  PRUint32 aFlags)
{
  NS_NOTYETIMPLEMENTED("VectorImage::Init");
  return NS_ERROR_NOT_IMPLEMENTED;
}

void
VectorImage::GetCurrentFrameRect(nsIntRect& aRect)
{
  NS_NOTYETIMPLEMENTED("VectorImage::GetCurrentFrameRect");
}

PRUint32
VectorImage::GetDataSize()
{
  NS_NOTYETIMPLEMENTED("VectorImage::GetDataSize");
  return 0;
}

nsresult
VectorImage::StartAnimation()
{
  NS_NOTYETIMPLEMENTED("VectorImage::StartAnimation");
  return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult
VectorImage::StopAnimation()
{
  NS_NOTYETIMPLEMENTED("VectorImage::StopAnimation");
  return NS_ERROR_NOT_IMPLEMENTED;
}

//------------------------------------------------------------------------------
// imgIContainer methods

//******************************************************************************
/* readonly attribute PRInt32 width; */
NS_IMETHODIMP
VectorImage::GetWidth(PRInt32* aWidth)
{
  NS_NOTYETIMPLEMENTED("VectorImage::GetWidth");
  return NS_ERROR_NOT_IMPLEMENTED;
}

//******************************************************************************
/* readonly attribute PRInt32 height; */
NS_IMETHODIMP
VectorImage::GetHeight(PRInt32* aHeight)
{
  NS_NOTYETIMPLEMENTED("VectorImage::GetHeight");
  return NS_ERROR_NOT_IMPLEMENTED;
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
  NS_NOTYETIMPLEMENTED("VectorImage::GetAnimated");
  return NS_ERROR_NOT_IMPLEMENTED;
}

//******************************************************************************
/* readonly attribute boolean currentFrameIsOpaque; */
NS_IMETHODIMP
VectorImage::GetCurrentFrameIsOpaque(PRBool* aIsOpaque)
{
  NS_NOTYETIMPLEMENTED("VectorImage::GetCurrentFrameIsOpaque");
  return NS_ERROR_NOT_IMPLEMENTED;
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
  NS_NOTYETIMPLEMENTED("VectorImage::ExtractFrame");
  return NS_ERROR_NOT_IMPLEMENTED;
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
  NS_NOTYETIMPLEMENTED("VectorImage::Draw");
  return NS_ERROR_NOT_IMPLEMENTED;
}

//******************************************************************************
/* [notxpcom] nsIFrame GetRootLayoutFrame() */
nsIFrame*
VectorImage::GetRootLayoutFrame()
{
  NS_NOTYETIMPLEMENTED("VectorImage::GetRootLayoutFrame");
  return nsnull;
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
  NS_NOTYETIMPLEMENTED("VectorImage::LockImage");
  return NS_ERROR_NOT_IMPLEMENTED;
}

//******************************************************************************
/* void unlockImage() */
NS_IMETHODIMP
VectorImage::UnlockImage()
{
  NS_NOTYETIMPLEMENTED("VectorImage::UnlockImage");
  return NS_ERROR_NOT_IMPLEMENTED;
}

//******************************************************************************
/* attribute unsigned short animationMode; */
NS_IMETHODIMP
VectorImage::GetAnimationMode(PRUint16* aAnimationMode)
{
  NS_NOTYETIMPLEMENTED("VectorImage::GetAnimationMode");
  return NS_ERROR_NOT_IMPLEMENTED;
}

//******************************************************************************
/* attribute unsigned short animationMode; */
NS_IMETHODIMP
VectorImage::SetAnimationMode(PRUint16 aAnimationMode)
{
  NS_NOTYETIMPLEMENTED("VectorImage::SetAnimationMode");
  return NS_ERROR_NOT_IMPLEMENTED;
}

//******************************************************************************
/* void resetAnimation (); */
NS_IMETHODIMP
VectorImage::ResetAnimation()
{
  NS_NOTYETIMPLEMENTED("VectorImage::ResetAnimation");
  return NS_ERROR_NOT_IMPLEMENTED;
}

//------------------------------------------------------------------------------
// nsIRequestObserver methods

//******************************************************************************
/* void onStartRequest(in nsIRequest request, in nsISupports ctxt); */
NS_IMETHODIMP
VectorImage::OnStartRequest(nsIRequest* aRequest, nsISupports* aCtxt)
{
  NS_NOTYETIMPLEMENTED("VectorImage::OnStartRequest");
  return NS_ERROR_NOT_IMPLEMENTED;
}

//******************************************************************************
/* void onStopRequest(in nsIRequest request, in nsISupports ctxt,
                      in nsresult status); */
NS_IMETHODIMP
VectorImage::OnStopRequest(nsIRequest* aRequest, nsISupports* aCtxt,
                           nsresult aStatus)
{
  NS_NOTYETIMPLEMENTED("VectorImage::OnStopRequest");
  return NS_ERROR_NOT_IMPLEMENTED;
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
  NS_NOTYETIMPLEMENTED("VectorImage::OnDataAvailable");
  return NS_ERROR_NOT_IMPLEMENTED;
}

} // namespace imagelib
} // namespace mozilla
