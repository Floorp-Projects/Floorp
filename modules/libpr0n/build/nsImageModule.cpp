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
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation. Portions created by Netscape are
 * Copyright (C) 2001 Netscape Communications Corporation. All
 * Rights Reserved.
 * 
 * Contributor(s): 
 *   Stuart Parmenter <pavlov@netscape.com>
 */

#include "nsIGenericFactory.h"
#include "nsIModule.h"

#include "imgCache.h"
#include "imgContainer.h"
#include "imgLoader.h"
#include "imgRequest.h"
#include "imgRequestProxy.h"

// gif
#include "imgContainerGIF.h"
#include "nsGIFDecoder2.h"


// bmp/ico
#include "nsBMPDecoder.h"
#include "nsICODecoder.h"

// png
#include "nsPNGDecoder.h"

// jpeg
#include "nsJPEGDecoder.h"

// xbm
#include "nsXBMDecoder.h"

// ppm
#include "nsPPMDecoder.h"

// objects that just require generic constructors

NS_GENERIC_FACTORY_CONSTRUCTOR(imgCache)
NS_GENERIC_FACTORY_CONSTRUCTOR(imgContainer)
NS_GENERIC_FACTORY_CONSTRUCTOR(imgLoader)
NS_GENERIC_FACTORY_CONSTRUCTOR(imgRequestProxy)

// gif
NS_GENERIC_FACTORY_CONSTRUCTOR(imgContainerGIF)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsGIFDecoder2)

// jpeg
NS_GENERIC_FACTORY_CONSTRUCTOR(nsJPEGDecoder)

// bmp
NS_GENERIC_FACTORY_CONSTRUCTOR(nsICODecoder)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsBMPDecoder)

// png
NS_GENERIC_FACTORY_CONSTRUCTOR(nsPNGDecoder)

// xbm
NS_GENERIC_FACTORY_CONSTRUCTOR(nsXBMDecoder)

// ppm
NS_GENERIC_FACTORY_CONSTRUCTOR(nsPPMDecoder)

static const nsModuleComponentInfo components[] =
{
  { "image cache",
    NS_IMGCACHE_CID,
    "@mozilla.org/image/cache;1",
    imgCacheConstructor, },
  { "image container",
    NS_IMGCONTAINER_CID,
    "@mozilla.org/image/container;1",
    imgContainerConstructor, },
  { "image loader",
    NS_IMGLOADER_CID,
    "@mozilla.org/image/loader;1",
    imgLoaderConstructor, },
  { "image request proxy",
    NS_IMGREQUESTPROXY_CID,
    "@mozilla.org/image/request;1",
    imgRequestProxyConstructor, },

  // gif
  { "GIF image container",
    NS_GIFCONTAINER_CID,
    "@mozilla.org/image/container;1?type=image/gif",
    imgContainerGIFConstructor, },
  { "GIF Decoder",
     NS_GIFDECODER2_CID,
     "@mozilla.org/image/decoder;2?type=image/gif",
     nsGIFDecoder2Constructor, },
  
  // jpeg
  { "JPEG decoder",
    NS_JPEGDECODER_CID,
    "@mozilla.org/image/decoder;2?type=image/jpeg",
    nsJPEGDecoderConstructor, },
  { "JPEG decoder",
    NS_JPEGDECODER_CID,
    "@mozilla.org/image/decoder;2?type=image/pjpeg",
    nsJPEGDecoderConstructor, },
  { "JPEG decoder",
    NS_JPEGDECODER_CID,
    "@mozilla.org/image/decoder;2?type=image/jpg",
    nsJPEGDecoderConstructor, },
  
  // bmp
  { "ICO Decoder",
     NS_ICODECODER_CID,
     "@mozilla.org/image/decoder;2?type=image/x-icon",
     nsICODecoderConstructor, },
  { "BMP Decoder",
     NS_BMPDECODER_CID,
     "@mozilla.org/image/decoder;2?type=image/bmp",
     nsBMPDecoderConstructor, },
  
  // png
  { "PNG Decoder",
    NS_PNGDECODER_CID,
    "@mozilla.org/image/decoder;2?type=image/png",
    nsPNGDecoderConstructor, },
  { "PNG Decoder",
    NS_PNGDECODER_CID,
    "@mozilla.org/image/decoder;2?type=image/x-png",
    nsPNGDecoderConstructor, },

  // xbm
  { "XBM Decoder",
     NS_XBMDECODER_CID,
     "@mozilla.org/image/decoder;2?type=image/x-xbitmap",
     nsXBMDecoderConstructor, },
  { "XBM Decoder",
     NS_XBMDECODER_CID,
     "@mozilla.org/image/decoder;2?type=image/x-xbm",
     nsXBMDecoderConstructor, },
  { "XBM Decoder",
     NS_XBMDECODER_CID,
     "@mozilla.org/image/decoder;2?type=image/xbm",
     nsXBMDecoderConstructor, },
  
  // ppm
  { "pbm decoder",
    NS_PPMDECODER_CID,
    "@mozilla.org/image/decoder;2?type=image/x-portable-bitmap",
    nsPPMDecoderConstructor, },
  { "pgm decoder",
    NS_PPMDECODER_CID,
    "@mozilla.org/image/decoder;2?type=image/x-portable-graymap",
    nsPPMDecoderConstructor, },
  { "ppm decoder",
    NS_PPMDECODER_CID,
    "@mozilla.org/image/decoder;2?type=image/x-portable-pixmap",
    nsPPMDecoderConstructor, },
};

PR_STATIC_CALLBACK(nsresult)
imglib_Initialize(nsIModule* aSelf)
{
  imgCache::Init();
  return NS_OK;
}

PR_STATIC_CALLBACK(void)
imglib_Shutdown(nsIModule* aSelf)
{
  imgCache::Shutdown();
  nsGifShutdown();
}

NS_IMPL_NSGETMODULE_WITH_CTOR_DTOR(nsImageLib2Module, components,
                                   imglib_Initialize, imglib_Shutdown)

