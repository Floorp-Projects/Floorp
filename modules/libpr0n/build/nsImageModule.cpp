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
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Stuart Parmenter <pavlov@netscape.com>
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

#ifdef XP_MAC
#define IMG_BUILD_gif 1
#define IMG_BUILD_bmp 1
#define IMG_BUILD_png 1
#define IMG_BUILD_jpeg 1
#define IMG_BUILD_xbm 1
#endif

#include "nsIGenericFactory.h"
#include "nsIModule.h"
#include "nsICategoryManager.h"
#include "nsXPCOMCID.h"
#include "nsIServiceManagerUtils.h"

#include "imgCache.h"
#include "imgContainer.h"
#include "imgLoader.h"
#include "imgRequest.h"
#include "imgRequestProxy.h"

#ifdef IMG_BUILD_gif
// gif
#include "imgContainerGIF.h"
#include "nsGIFDecoder2.h"
#endif

#ifdef IMG_BUILD_bmp
// bmp/ico
#include "nsBMPDecoder.h"
#include "nsICODecoder.h"
#endif

#ifdef IMG_BUILD_png
// png
#include "nsPNGDecoder.h"
#endif

#ifdef IMG_BUILD_jpeg
// jpeg
#include "nsJPEGDecoder.h"
#endif

#ifdef IMG_BUILD_xbm
// xbm
#include "nsXBMDecoder.h"
#endif

// objects that just require generic constructors

NS_GENERIC_FACTORY_CONSTRUCTOR(imgCache)
NS_GENERIC_FACTORY_CONSTRUCTOR(imgContainer)
NS_GENERIC_FACTORY_CONSTRUCTOR(imgLoader)
NS_GENERIC_FACTORY_CONSTRUCTOR(imgRequestProxy)

#ifdef IMG_BUILD_gif
// gif
NS_GENERIC_FACTORY_CONSTRUCTOR(imgContainerGIF)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsGIFDecoder2)
#endif

#ifdef IMG_BUILD_jpeg
// jpeg
NS_GENERIC_FACTORY_CONSTRUCTOR(nsJPEGDecoder)
#endif

#ifdef IMG_BUILD_bmp
// bmp
NS_GENERIC_FACTORY_CONSTRUCTOR(nsICODecoder)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsBMPDecoder)
#endif

#ifdef IMG_BUILD_png
// png
NS_GENERIC_FACTORY_CONSTRUCTOR(nsPNGDecoder)
#endif

#ifdef IMG_BUILD_xbm
// xbm
NS_GENERIC_FACTORY_CONSTRUCTOR(nsXBMDecoder)
#endif

static const char* gImageMimeTypes[] = {
#ifdef IMG_BUILD_gif
  "image/gif",
#endif
#ifdef IMG_BUILD_jpeg
  "image/jpeg",
  "image/pjpeg",
  "image/jpg",
#endif
#ifdef IMG_BUILD_bmp
  "image/x-icon",
  "image/vnd.microsoft.icon",
  "image/bmp",
#endif
#ifdef IMG_BUILD_png
  "image/png",
  "image/x-png",
#endif
#ifdef IMG_BUILD_xbm
  "image/x-xbitmap",
  "image/x-xbm",
  "image/xbm"
#endif
};

static NS_METHOD ImageRegisterProc(nsIComponentManager *aCompMgr,
                                   nsIFile *aPath,
                                   const char *registryLocation,
                                   const char *componentType,
                                   const nsModuleComponentInfo *info) {
  nsresult rv;
  nsCOMPtr<nsICategoryManager> catMan(do_GetService(NS_CATEGORYMANAGER_CONTRACTID, &rv));
  if (NS_FAILED(rv))
    return rv;
  for (unsigned i = 0; i < sizeof(gImageMimeTypes)/sizeof(*gImageMimeTypes); i++) {
    catMan->AddCategoryEntry("Gecko-Content-Viewers", gImageMimeTypes[i],
                             "@mozilla.org/content/document-loader-factory;1",
                             PR_TRUE, PR_TRUE, nsnull);
  }

  catMan->AddCategoryEntry("content-sniffing-services", "@mozilla.org/image/loader;1",
                           "@mozilla.org/image/loader;1", PR_TRUE, PR_TRUE,
                           nsnull);
  return NS_OK;
}

static NS_METHOD ImageUnregisterProc(nsIComponentManager *aCompMgr,
                                     nsIFile *aPath,
                                     const char *registryLocation,
                                     const nsModuleComponentInfo *info) {
  nsresult rv;
  nsCOMPtr<nsICategoryManager> catMan(do_GetService(NS_CATEGORYMANAGER_CONTRACTID, &rv));
  if (NS_FAILED(rv))
    return rv;
  for (unsigned i = 0; i < sizeof(gImageMimeTypes)/sizeof(*gImageMimeTypes); i++)
    catMan->DeleteCategoryEntry("Gecko-Content-Viewers", gImageMimeTypes[i], PR_TRUE);

  return NS_OK;
}

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
    imgLoaderConstructor,
    ImageRegisterProc, /* register the decoder mime types here */
    ImageUnregisterProc, },
  { "image request proxy",
    NS_IMGREQUESTPROXY_CID,
    "@mozilla.org/image/request;1",
    imgRequestProxyConstructor, },

#ifdef IMG_BUILD_gif
  // gif
  { "GIF image container",
    NS_GIFCONTAINER_CID,
    "@mozilla.org/image/container;1?type=image/gif",
    imgContainerGIFConstructor, },
  { "GIF Decoder",
     NS_GIFDECODER2_CID,
     "@mozilla.org/image/decoder;2?type=image/gif",
     nsGIFDecoder2Constructor, },
#endif

#ifdef IMG_BUILD_jpeg
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
#endif

#ifdef IMG_BUILD_bmp
  // bmp
  { "ICO Decoder",
     NS_ICODECODER_CID,
     "@mozilla.org/image/decoder;2?type=image/x-icon",
     nsICODecoderConstructor, },
  { "ICO Decoder",
     NS_ICODECODER_CID,
     "@mozilla.org/image/decoder;2?type=image/vnd.microsoft.icon",
     nsICODecoderConstructor, },
  { "BMP Decoder",
     NS_BMPDECODER_CID,
     "@mozilla.org/image/decoder;2?type=image/bmp",
     nsBMPDecoderConstructor, },
#endif

#ifdef IMG_BUILD_png
  // png
  { "PNG Decoder",
    NS_PNGDECODER_CID,
    "@mozilla.org/image/decoder;2?type=image/png",
    nsPNGDecoderConstructor, },
  { "PNG Decoder",
    NS_PNGDECODER_CID,
    "@mozilla.org/image/decoder;2?type=image/x-png",
    nsPNGDecoderConstructor, },
#endif

#ifdef IMG_BUILD_xbm
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
#endif
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
#ifdef IMG_BUILD_gif
  nsGifShutdown();
#endif
}

NS_IMPL_NSGETMODULE_WITH_CTOR_DTOR(nsImageLib2Module, components,
                                   imglib_Initialize, imglib_Shutdown)

