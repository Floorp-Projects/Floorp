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

#include "nsImgBuildDefines.h"

#ifdef XP_MAC
#define IMG_BUILD_gif 1
#define IMG_BUILD_bmp 1
#define IMG_BUILD_png 1
#define IMG_BUILD_jpeg 1
#endif

#include "nsIDeviceContext.h"
#include "mozilla/ModuleUtils.h"
#include "nsXPCOMCID.h"
#include "nsServiceManagerUtils.h"

#include "imgContainer.h"
#include "imgLoader.h"
#include "imgRequest.h"
#include "imgRequestProxy.h"
#include "imgTools.h"
#include "imgDiscardTracker.h"

#ifdef IMG_BUILD_DECODER_gif
// gif
#include "nsGIFDecoder2.h"
#endif

#ifdef IMG_BUILD_DECODER_bmp
// bmp/ico
#include "nsBMPDecoder.h"
#include "nsICODecoder.h"
#endif

#ifdef IMG_BUILD_DECODER_png
// png
#include "nsPNGDecoder.h"
#endif

#ifdef IMG_BUILD_DECODER_jpeg
// jpeg
#include "nsJPEGDecoder.h"
#endif

#ifdef IMG_BUILD_ENCODER_png
// png
#include "nsPNGEncoder.h"
#endif
#ifdef IMG_BUILD_ENCODER_jpeg
// jpeg
#include "nsJPEGEncoder.h"
#endif


// objects that just require generic constructors

NS_GENERIC_FACTORY_CONSTRUCTOR(imgContainer)
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(imgLoader, Init)
NS_GENERIC_FACTORY_CONSTRUCTOR(imgRequestProxy)
NS_GENERIC_FACTORY_CONSTRUCTOR(imgTools)

#ifdef IMG_BUILD_DECODER_gif
// gif
NS_GENERIC_FACTORY_CONSTRUCTOR(nsGIFDecoder2)
#endif

#ifdef IMG_BUILD_DECODER_jpeg
// jpeg
NS_GENERIC_FACTORY_CONSTRUCTOR(nsJPEGDecoder)
#endif
#ifdef IMG_BUILD_ENCODER_jpeg
// jpeg
NS_GENERIC_FACTORY_CONSTRUCTOR(nsJPEGEncoder)
#endif

#ifdef IMG_BUILD_DECODER_bmp
// bmp
NS_GENERIC_FACTORY_CONSTRUCTOR(nsICODecoder)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsBMPDecoder)
#endif

#ifdef IMG_BUILD_DECODER_png
// png
NS_GENERIC_FACTORY_CONSTRUCTOR(nsPNGDecoder)
#endif
#ifdef IMG_BUILD_ENCODER_png
// png
NS_GENERIC_FACTORY_CONSTRUCTOR(nsPNGEncoder)
#endif

NS_DEFINE_NAMED_CID(NS_IMGLOADER_CID);
NS_DEFINE_NAMED_CID(NS_IMGCONTAINER_CID);
NS_DEFINE_NAMED_CID(NS_IMGREQUESTPROXY_CID);
NS_DEFINE_NAMED_CID(NS_IMGTOOLS_CID);
#ifdef IMG_BUILD_DECODER_gif
NS_DEFINE_NAMED_CID(NS_GIFDECODER2_CID);
#endif
#ifdef IMG_BUILD_DECODER_jpeg
NS_DEFINE_NAMED_CID(NS_JPEGDECODER_CID);
#endif
#ifdef IMG_BUILD_ENCODER_jpeg
NS_DEFINE_NAMED_CID(NS_JPEGENCODER_CID);
#endif
#ifdef IMG_BUILD_DECODER_bmp
NS_DEFINE_NAMED_CID(NS_ICODECODER_CID);
NS_DEFINE_NAMED_CID(NS_BMPDECODER_CID);
#endif
#ifdef IMG_BUILD_DECODER_png
NS_DEFINE_NAMED_CID(NS_PNGDECODER_CID);
#endif
#ifdef IMG_BUILD_ENCODER_png
NS_DEFINE_NAMED_CID(NS_PNGENCODER_CID);
#endif


static const mozilla::Module::CIDEntry kImageCIDs[] = {
  { &kNS_IMGLOADER_CID, false, NULL, imgLoaderConstructor, },
  { &kNS_IMGCONTAINER_CID, false, NULL, imgContainerConstructor, },
  { &kNS_IMGREQUESTPROXY_CID, false, NULL, imgRequestProxyConstructor, },
  { &kNS_IMGTOOLS_CID, false, NULL, imgToolsConstructor, },
#ifdef IMG_BUILD_DECODER_gif
  { &kNS_GIFDECODER2_CID, false, NULL, nsGIFDecoder2Constructor, },
#endif
#ifdef IMG_BUILD_DECODER_jpeg
  { &kNS_JPEGDECODER_CID, false, NULL, nsJPEGDecoderConstructor, },
#endif
#ifdef IMG_BUILD_ENCODER_jpeg
  { &kNS_JPEGENCODER_CID, false, NULL, nsJPEGEncoderConstructor, },
#endif
#ifdef IMG_BUILD_DECODER_bmp
  { &kNS_ICODECODER_CID, false, NULL, nsICODecoderConstructor, },
  { &kNS_BMPDECODER_CID, false, NULL, nsBMPDecoderConstructor, },
#endif
#ifdef IMG_BUILD_DECODER_png
  { &kNS_PNGDECODER_CID, false, NULL, nsPNGDecoderConstructor, },
#endif
#ifdef IMG_BUILD_ENCODER_png
  { &kNS_PNGENCODER_CID, false, NULL, nsPNGEncoderConstructor, },
#endif
  { NULL }
};

static const mozilla::Module::ContractIDEntry kImageContracts[] = {
  { "@mozilla.org/image/cache;1", &kNS_IMGLOADER_CID },
  { "@mozilla.org/image/container;3", &kNS_IMGCONTAINER_CID },
  { "@mozilla.org/image/loader;1", &kNS_IMGLOADER_CID },
  { "@mozilla.org/image/request;1", &kNS_IMGREQUESTPROXY_CID },
  { "@mozilla.org/image/tools;1", &kNS_IMGTOOLS_CID },
#ifdef IMG_BUILD_DECODER_gif
  { "@mozilla.org/image/decoder;3?type=image/gif", &kNS_GIFDECODER2_CID },
#endif
#ifdef IMG_BUILD_DECODER_jpeg
  { "@mozilla.org/image/decoder;3?type=image/jpeg", &kNS_JPEGDECODER_CID },
  { "@mozilla.org/image/decoder;3?type=image/pjpeg", &kNS_JPEGDECODER_CID },
  { "@mozilla.org/image/decoder;3?type=image/jpg", &kNS_JPEGDECODER_CID },
#endif
#ifdef IMG_BUILD_ENCODER_jpeg
  { "@mozilla.org/image/encoder;2?type=image/jpeg", &kNS_JPEGENCODER_CID },
#endif
#ifdef IMG_BUILD_DECODER_bmp
  { "@mozilla.org/image/decoder;3?type=image/x-icon", &kNS_ICODECODER_CID },
  { "@mozilla.org/image/decoder;3?type=image/vnd.microsoft.icon", &kNS_ICODECODER_CID },
  { "@mozilla.org/image/decoder;3?type=image/bmp", &kNS_BMPDECODER_CID },
  { "@mozilla.org/image/decoder;3?type=image/x-ms-bmp", &kNS_BMPDECODER_CID },
#endif
#ifdef IMG_BUILD_DECODER_png
  { "@mozilla.org/image/decoder;3?type=image/png", &kNS_PNGDECODER_CID },
  { "@mozilla.org/image/decoder;3?type=image/x-png", &kNS_PNGDECODER_CID },
#endif
#ifdef IMG_BUILD_ENCODER_png
  { "@mozilla.org/image/encoder;2?type=image/png", &kNS_PNGENCODER_CID },
#endif
  { NULL }
};

static const mozilla::Module::CategoryEntry kImageCategories[] = {
#ifdef IMG_BUILD_DECODER_gif
  { "Gecko-Content-Viewers", "image/gif", "@mozilla.org/content/document-loader-factory;1" },
#endif
#ifdef IMG_BUILD_DECODER_jpeg
  { "Gecko-Content-Viewers", "image/jpeg", "@mozilla.org/content/document-loader-factory;1" },
  { "Gecko-Content-Viewers", "image/pjpeg", "@mozilla.org/content/document-loader-factory;1" },
  { "Gecko-Content-Viewers", "image/jpg", "@mozilla.org/content/document-loader-factory;1" },
#endif
#ifdef IMG_BUILD_DECODER_bmp
  { "Gecko-Content-Viewers", "image/x-icon", "@mozilla.org/content/document-loader-factory;1" },
  { "Gecko-Content-Viewers", "image/vnd.microsoft.icon", "@mozilla.org/content/document-loader-factory;1" },
  { "Gecko-Content-Viewers", "image/bmp", "@mozilla.org/content/document-loader-factory;1" },
  { "Gecko-Content-Viewers", "image/x-ms-bmp", "@mozilla.org/content/document-loader-factory;1" },
#endif
#ifdef IMG_BUILD_DECODER_png
  { "Gecko-Content-Viewers", "image/png", "@mozilla.org/content/document-loader-factory;1" },
  { "Gecko-Content-Viewers", "image/x-png", "@mozilla.org/content/document-loader-factory;1" },
#endif
  { "content-sniffing-services", "@mozilla.org/image/loader;1", "@mozilla.org/image/loader;1" },
  { NULL }
};

static nsresult
imglib_Initialize()
{
  // Hack: We need the gfx module to be initialized because we use gfxPlatform
  // in imgFrame. Request something from the gfx module to ensure that
  // everything's set up for us.
  nsCOMPtr<nsIDeviceContext> devctx = 
    do_CreateInstance("@mozilla.org/gfx/devicecontext;1");

  imgLoader::InitCache();
  return NS_OK;
}

static void
imglib_Shutdown()
{
  imgLoader::Shutdown();
  imgDiscardTracker::Shutdown();
}

static const mozilla::Module kImageModule = {
  mozilla::Module::kVersion,
  kImageCIDs,
  kImageContracts,
  kImageCategories,
  NULL,
  imglib_Initialize,
  imglib_Shutdown
};

NSMODULE_DEFN(nsImageLib2Module) = &kImageModule;
