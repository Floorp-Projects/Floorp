/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Initial Developer of the Original Code is Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Justin Dolske <dolske@mozilla.com> (original author)
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

#include "imgTools.h"
#include "nsCOMPtr.h"
#include "nsString.h"
#include "ImageErrors.h"
#include "imgIContainer.h"
#include "imgILoad.h"
#include "imgIDecoder.h"
#include "imgIEncoder.h"
#include "imgIDecoderObserver.h"
#include "imgIContainerObserver.h"
#include "gfxContext.h"
#include "nsStringStream.h"
#include "nsComponentManagerUtils.h"
#include "nsWeakReference.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsStreamUtils.h"
#include "nsNetUtil.h"


/* ========== Utility classes ========== */


class HelperLoader : public imgILoad,
                     public imgIDecoderObserver,
                     public nsSupportsWeakReference
{
  public:
    NS_DECL_ISUPPORTS
    NS_DECL_IMGILOAD
    NS_DECL_IMGIDECODEROBSERVER
    NS_DECL_IMGICONTAINEROBSERVER
    HelperLoader(void);

  private:
    nsCOMPtr<imgIContainer> mContainer;
};

NS_IMPL_ISUPPORTS4 (HelperLoader, imgILoad, imgIDecoderObserver, imgIContainerObserver, nsISupportsWeakReference)

HelperLoader::HelperLoader (void)
{
}

/* Implement imgILoad::image getter */
NS_IMETHODIMP
HelperLoader::GetImage(imgIContainer **aImage)
{
  *aImage = mContainer;
  NS_IF_ADDREF (*aImage);
  return NS_OK;
}

/* Implement imgILoad::image setter */
NS_IMETHODIMP
HelperLoader::SetImage(imgIContainer *aImage)
{
  mContainer = aImage;
  return NS_OK;
}

/* Implement imgILoad::isMultiPartChannel getter */
NS_IMETHODIMP
HelperLoader::GetIsMultiPartChannel(PRBool *aIsMultiPartChannel)
{
  *aIsMultiPartChannel = PR_FALSE;
  return NS_OK;
}

/* Implement imgIDecoderObserver::onStartRequest() */
NS_IMETHODIMP
HelperLoader::OnStartRequest(imgIRequest *aRequest)
{
  return NS_OK;
}

/* Implement imgIDecoderObserver::onStartDecode() */
NS_IMETHODIMP
HelperLoader::OnStartDecode(imgIRequest *aRequest)
{
  return NS_OK;
}

/* Implement imgIDecoderObserver::onStartContainer() */
NS_IMETHODIMP
HelperLoader::OnStartContainer(imgIRequest *aRequest, imgIContainer
*aContainer)
{
  return NS_OK;
}

/* Implement imgIDecoderObserver::onStartFrame() */
NS_IMETHODIMP
HelperLoader::OnStartFrame(imgIRequest *aRequest, PRUint32 aFrame)
{
  return NS_OK;
}

/* Implement imgIDecoderObserver::onDataAvailable() */
NS_IMETHODIMP
HelperLoader::OnDataAvailable(imgIRequest *aRequest, PRBool aCurrentFrame, const nsIntRect * aRect)
{
  return NS_OK;
}

/* Implement imgIDecoderObserver::onStopFrame() */
NS_IMETHODIMP
HelperLoader::OnStopFrame(imgIRequest *aRequest, PRUint32 aFrame)
{
  return NS_OK;
}

/* Implement imgIDecoderObserver::onStopContainer() */
NS_IMETHODIMP
HelperLoader::OnStopContainer(imgIRequest *aRequest, imgIContainer
*aContainer)
{
  return NS_OK;
}

/* Implement imgIDecoderObserver::onStopDecode() */
NS_IMETHODIMP
HelperLoader::OnStopDecode(imgIRequest *aRequest, nsresult status, const
PRUnichar *statusArg)
{
  return NS_OK;
}

/* Implement imgIDecoderObserver::onStopRequest() */
NS_IMETHODIMP
HelperLoader::OnStopRequest(imgIRequest *aRequest, PRBool aIsLastPart)
{
  return NS_OK;
}
  
/* implement imgIContainerObserver::frameChanged() */
NS_IMETHODIMP
HelperLoader::FrameChanged(imgIContainer *aContainer, nsIntRect * aDirtyRect)
{
  return NS_OK;
}



/* ========== imgITools implementation ========== */



NS_IMPL_ISUPPORTS1(imgTools, imgITools)

imgTools::imgTools()
{
  /* member initializers and constructor code */
}

imgTools::~imgTools()
{
  /* destructor code */
}


NS_IMETHODIMP imgTools::DecodeImageData(nsIInputStream* aInStr,
                                        const nsACString& aMimeType,
                                        imgIContainer **aContainer)
{
  nsresult rv;

  // Get an image decoder for our media type
  nsCAutoString decoderCID(
    NS_LITERAL_CSTRING("@mozilla.org/image/decoder;2?type=") + aMimeType);

  nsCOMPtr<imgIDecoder> decoder = do_CreateInstance(decoderCID.get());
  if (!decoder)
    return NS_IMAGELIB_ERROR_NO_DECODER;

  // Init the decoder, we use a small utility class here.
  nsCOMPtr<imgILoad> loader = new HelperLoader();
  if (!loader)
    return NS_ERROR_OUT_OF_MEMORY;

  // If caller provided an existing container, use it.
  if (*aContainer)
    loader->SetImage(*aContainer);

  rv = decoder->Init(loader);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIInputStream> inStream = aInStr;
  if (!NS_InputStreamIsBuffered(aInStr)) {
    nsCOMPtr<nsIInputStream> bufStream;
    rv = NS_NewBufferedInputStream(getter_AddRefs(bufStream), aInStr, 1024);
    if (NS_SUCCEEDED(rv))
      inStream = bufStream;
  }

  PRUint32 length;
  rv = inStream->Available(&length);
  NS_ENSURE_SUCCESS(rv, rv);

  PRUint32 written;
  rv = decoder->WriteFrom(inStream, length, &written);
  NS_ENSURE_SUCCESS(rv, rv);
  if (written != length)
    NS_WARNING("decoder didn't eat all of its vegetables");
  rv = decoder->Flush();
  NS_ENSURE_SUCCESS(rv, rv);
  rv = decoder->Close();
  NS_ENSURE_SUCCESS(rv, rv);

  // If caller didn't provide an existing container, return the new one.
  if (!*aContainer)
    loader->GetImage(aContainer);

  return NS_OK;
}


NS_IMETHODIMP imgTools::EncodeImage(imgIContainer *aContainer,
                                          const nsACString& aMimeType,
                                          nsIInputStream **aStream)
{
    return EncodeScaledImage(aContainer, aMimeType, 0, 0, aStream);
}


NS_IMETHODIMP imgTools::EncodeScaledImage(imgIContainer *aContainer,
                                          const nsACString& aMimeType,
                                          PRInt32 aScaledWidth,
                                          PRInt32 aScaledHeight,
                                          nsIInputStream **aStream)
{
  nsresult rv;
  PRBool doScaling = PR_TRUE;
  PRUint8 *bitmapData;
  PRUint32 bitmapDataLength, strideSize;

  // If no scaled size is specified, we'll just encode the image at its
  // original size (no scaling).
  if (aScaledWidth == 0 && aScaledHeight == 0) {
    doScaling = PR_FALSE;
  } else {
    NS_ENSURE_ARG(aScaledWidth > 0);
    NS_ENSURE_ARG(aScaledHeight > 0);
  }

  // Get an image encoder for the media type
  nsCAutoString encoderCID(
    NS_LITERAL_CSTRING("@mozilla.org/image/encoder;2?type=") + aMimeType);

  nsCOMPtr<imgIEncoder> encoder = do_CreateInstance(encoderCID.get());
  if (!encoder)
    return NS_IMAGELIB_ERROR_NO_ENCODER;

  // Use frame 0 from the image container.
  nsRefPtr<gfxImageSurface> frame;
  rv = aContainer->CopyCurrentFrame(getter_AddRefs(frame));
  NS_ENSURE_SUCCESS(rv, rv);
  if (!frame)
    return NS_ERROR_NOT_AVAILABLE;

  PRInt32 w = frame->Width(), h = frame->Height();
  if (!w || !h)
    return NS_ERROR_FAILURE;

  nsRefPtr<gfxImageSurface> dest;

  if (!doScaling) {
    // If we're not scaling the image, use the actual width/height.
    aScaledWidth  = w;
    aScaledHeight = h;

    bitmapData = frame->Data();
    if (!bitmapData)
      return NS_ERROR_FAILURE;

    strideSize = frame->Stride();
    bitmapDataLength = aScaledHeight * strideSize;

  } else {
    // Prepare to draw a scaled version of the image to a temporary surface...

    // Create a temporary image surface
    dest = new gfxImageSurface(gfxIntSize(aScaledWidth, aScaledHeight),
                               gfxASurface::ImageFormatARGB32);
    if (!dest)
      return NS_ERROR_OUT_OF_MEMORY;

    gfxContext ctx(dest);

    // Set scaling
    gfxFloat sw = (double) aScaledWidth / w;
    gfxFloat sh = (double) aScaledHeight / h;
    ctx.Scale(sw, sh);

    // Paint a scaled image
    ctx.SetOperator(gfxContext::OPERATOR_SOURCE);
    ctx.SetSource(frame);
    ctx.Paint();

    bitmapData = dest->Data();
    strideSize = dest->Stride();
    bitmapDataLength = aScaledHeight * strideSize;
  }

  // Encode the bitmap
  rv = encoder->InitFromData(bitmapData, bitmapDataLength,
                             aScaledWidth, aScaledHeight, strideSize,
                             imgIEncoder::INPUT_FORMAT_HOSTARGB, EmptyString());

  NS_ENSURE_SUCCESS(rv, rv);

  return CallQueryInterface(encoder, aStream);
}
