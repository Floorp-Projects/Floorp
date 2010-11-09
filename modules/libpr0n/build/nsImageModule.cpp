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

#include "nsIDeviceContext.h"
#include "mozilla/ModuleUtils.h"
#include "nsXPCOMCID.h"
#include "nsServiceManagerUtils.h"

#include "RasterImage.h"

/* We end up pulling in windows.h because we eventually hit gfxWindowsSurface;
 * windows.h defines LoadImage, so we have to #undef it or imgLoader::LoadImage
 * gets changed.
 * This #undef needs to be in multiple places because we don't always pull
 * headers in in the same order.
 */
#undef LoadImage

#include "imgLoader.h"
#include "imgRequest.h"
#include "imgRequestProxy.h"
#include "imgTools.h"
#include "DiscardTracker.h"

#include "nsPNGEncoder.h"
#include "nsJPEGEncoder.h"

// objects that just require generic constructors
namespace mozilla {
namespace imagelib {
NS_GENERIC_FACTORY_CONSTRUCTOR(RasterImage)
}
}
using namespace mozilla::imagelib;

NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(imgLoader, Init)
NS_GENERIC_FACTORY_CONSTRUCTOR(imgRequestProxy)
NS_GENERIC_FACTORY_CONSTRUCTOR(imgTools)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsJPEGEncoder)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsPNGEncoder)
NS_DEFINE_NAMED_CID(NS_IMGLOADER_CID);
NS_DEFINE_NAMED_CID(NS_IMGREQUESTPROXY_CID);
NS_DEFINE_NAMED_CID(NS_IMGTOOLS_CID);
NS_DEFINE_NAMED_CID(NS_RASTERIMAGE_CID);
NS_DEFINE_NAMED_CID(NS_JPEGENCODER_CID);
NS_DEFINE_NAMED_CID(NS_PNGENCODER_CID);

static const mozilla::Module::CIDEntry kImageCIDs[] = {
  { &kNS_IMGLOADER_CID, false, NULL, imgLoaderConstructor, },
  { &kNS_IMGREQUESTPROXY_CID, false, NULL, imgRequestProxyConstructor, },
  { &kNS_IMGTOOLS_CID, false, NULL, imgToolsConstructor, },
  { &kNS_RASTERIMAGE_CID, false, NULL, RasterImageConstructor, },
  { &kNS_JPEGENCODER_CID, false, NULL, nsJPEGEncoderConstructor, },
  { &kNS_PNGENCODER_CID, false, NULL, nsPNGEncoderConstructor, },
  { NULL }
};

static const mozilla::Module::ContractIDEntry kImageContracts[] = {
  { "@mozilla.org/image/cache;1", &kNS_IMGLOADER_CID },
  { "@mozilla.org/image/loader;1", &kNS_IMGLOADER_CID },
  { "@mozilla.org/image/request;1", &kNS_IMGREQUESTPROXY_CID },
  { "@mozilla.org/image/tools;1", &kNS_IMGTOOLS_CID },
  { "@mozilla.org/image/rasterimage;1", &kNS_RASTERIMAGE_CID },
  { "@mozilla.org/image/encoder;2?type=image/jpeg", &kNS_JPEGENCODER_CID },
  { "@mozilla.org/image/encoder;2?type=image/png", &kNS_PNGENCODER_CID },
  { NULL }
};

static const mozilla::Module::CategoryEntry kImageCategories[] = {
  { "Gecko-Content-Viewers", "image/gif", "@mozilla.org/content/document-loader-factory;1" },
  { "Gecko-Content-Viewers", "image/jpeg", "@mozilla.org/content/document-loader-factory;1" },
  { "Gecko-Content-Viewers", "image/pjpeg", "@mozilla.org/content/document-loader-factory;1" },
  { "Gecko-Content-Viewers", "image/jpg", "@mozilla.org/content/document-loader-factory;1" },
  { "Gecko-Content-Viewers", "image/x-icon", "@mozilla.org/content/document-loader-factory;1" },
  { "Gecko-Content-Viewers", "image/vnd.microsoft.icon", "@mozilla.org/content/document-loader-factory;1" },
  { "Gecko-Content-Viewers", "image/bmp", "@mozilla.org/content/document-loader-factory;1" },
  { "Gecko-Content-Viewers", "image/x-ms-bmp", "@mozilla.org/content/document-loader-factory;1" },
  { "Gecko-Content-Viewers", "image/icon", "@mozilla.org/content/document-loader-factory;1" },
  { "Gecko-Content-Viewers", "image/png", "@mozilla.org/content/document-loader-factory;1" },
  { "Gecko-Content-Viewers", "image/x-png", "@mozilla.org/content/document-loader-factory;1" },
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
  mozilla::imagelib::DiscardTracker::Shutdown();
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
