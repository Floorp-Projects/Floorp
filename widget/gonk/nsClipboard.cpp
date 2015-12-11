/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsClipboard.h"

#include "gfxDrawable.h"
#include "gfxUtils.h"
#include "ImageOps.h"
#include "imgIContainer.h"
#include "imgTools.h"
#include "mozilla/dom/ContentChild.h"
#include "mozilla/Preferences.h"
#include "nsClipboardProxy.h"
#include "nsISupportsPrimitives.h"
#include "nsComponentManagerUtils.h"
#include "nsCOMPtr.h"
#include "nsServiceManagerUtils.h"
#include "nsStringStream.h"
#include "nsXULAppAPI.h"

using namespace mozilla;
using mozilla::dom::ContentChild;

#define LOG_TAG "Clipboard"
#define LOGI(args...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, ## args)
#define LOGE(args...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, ## args)


NS_IMPL_ISUPPORTS(nsClipboard, nsIClipboard)

nsClipboard::nsClipboard()
  : mClipboard(mozilla::MakeUnique<GonkClipboardData>())
{
}

NS_IMETHODIMP
nsClipboard::SetData(nsITransferable *aTransferable,
                     nsIClipboardOwner *anOwner,
                     int32_t aWhichClipboard)
{
  if (aWhichClipboard != kGlobalClipboard) {
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  if (!XRE_IsParentProcess()) {
    // Re-direct to the clipboard proxy.
    RefPtr<nsClipboardProxy> clipboardProxy = new nsClipboardProxy();
    return clipboardProxy->SetData(aTransferable, anOwner, aWhichClipboard);
  }

  // Clear out the clipboard in order to set the new data.
  EmptyClipboard(aWhichClipboard);

  // Use a pref to toggle rich text/non-text support.
  if (Preferences::GetBool("clipboard.plainTextOnly")) {
    nsCOMPtr<nsISupports> clip;
    uint32_t len;
    nsresult rv = aTransferable->GetTransferData(kUnicodeMime,
                                                 getter_AddRefs(clip),
                                                 &len);
    if (NS_FAILED(rv)) {
      return rv;
    }
    nsCOMPtr<nsISupportsString> wideString = do_QueryInterface(clip);
    if (!wideString) {
      return NS_ERROR_NOT_IMPLEMENTED;
    }
    nsAutoString utf16string;
    wideString->GetData(utf16string);
    mClipboard->SetText(utf16string);
    return NS_OK;
  }

  // Get the types of supported flavors.
  nsCOMPtr<nsISupportsArray> flavorList;
  nsresult rv = aTransferable->FlavorsTransferableCanExport(getter_AddRefs(flavorList));
  if (!flavorList || NS_FAILED(rv)) {
    return NS_ERROR_FAILURE;
  }

  uint32_t flavorCount = 0;
  flavorList->Count(&flavorCount);
  bool imageAdded = false;
  for (uint32_t i = 0; i < flavorCount; ++i) {
    nsCOMPtr<nsISupportsCString> currentFlavor = do_QueryElementAt(flavorList, i);

    if (currentFlavor) {
      // MIME type
      nsXPIDLCString flavorStr;
      currentFlavor->ToString(getter_Copies(flavorStr));

      // Clip is the data which will be sent to the clipboard.
      nsCOMPtr<nsISupports> clip;
      uint32_t len;

      if (flavorStr.EqualsLiteral(kUnicodeMime)) {
        // text/plain
        rv = aTransferable->GetTransferData(flavorStr, getter_AddRefs(clip), &len);
        nsCOMPtr<nsISupportsString> wideString = do_QueryInterface(clip);
        if (!wideString || NS_FAILED(rv)) {
          continue;
        }

        nsAutoString utf16string;
        wideString->GetData(utf16string);
        mClipboard->SetText(utf16string);
      } else if (flavorStr.EqualsLiteral(kHTMLMime)) {
        // text/html
        rv = aTransferable->GetTransferData(flavorStr, getter_AddRefs(clip), &len);
        nsCOMPtr<nsISupportsString> wideString = do_QueryInterface(clip);
        if (!wideString || NS_FAILED(rv)) {
          continue;
        }

        nsAutoString utf16string;
        wideString->GetData(utf16string);
        mClipboard->SetHTML(utf16string);
      } else if (!imageAdded && // image is added only once to the clipboard.
                 (flavorStr.EqualsLiteral(kNativeImageMime) ||
                  flavorStr.EqualsLiteral(kPNGImageMime) ||
                  flavorStr.EqualsLiteral(kJPEGImageMime) ||
                  flavorStr.EqualsLiteral(kJPGImageMime))) {
        // image/[png|jpeg|jpg] or application/x-moz-nativeimage

        // Look through our transfer data for the image.
        static const char* const imageMimeTypes[] = {
          kNativeImageMime, kPNGImageMime, kJPEGImageMime, kJPGImageMime };

        nsCOMPtr<nsISupportsInterfacePointer> imgPtr;
        for (uint32_t i = 0; !imgPtr && i < ArrayLength(imageMimeTypes); ++i) {
          aTransferable->GetTransferData(imageMimeTypes[i], getter_AddRefs(clip), &len);
          imgPtr = do_QueryInterface(clip);
        }
        if (!imgPtr) {
          continue;
        }

        nsCOMPtr<nsISupports> imageData;
        imgPtr->GetData(getter_AddRefs(imageData));
        nsCOMPtr<imgIContainer> image(do_QueryInterface(imageData));
        if (!image) {
          continue;
        }

        RefPtr<gfx::SourceSurface> surface =
          image->GetFrame(imgIContainer::FRAME_CURRENT,
                          imgIContainer::FLAG_SYNC_DECODE);
        if (!surface) {
          continue;
        }

        RefPtr<gfx::DataSourceSurface> dataSurface;
        if (surface->GetFormat() == gfx::SurfaceFormat::B8G8R8A8) {
          dataSurface = surface->GetDataSurface();
        } else {
          // Convert format to SurfaceFormat::B8G8R8A8.
          dataSurface = gfxUtils::CopySurfaceToDataSourceSurfaceWithFormat(surface, gfx::SurfaceFormat::B8G8R8A8);
        }

        mClipboard->SetImage(dataSurface);
        imageAdded = true;
      }
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsClipboard::GetData(nsITransferable *aTransferable,
                     int32_t aWhichClipboard)
{
  if (aWhichClipboard != kGlobalClipboard) {
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  if (!XRE_IsParentProcess()) {
    // Re-direct to the clipboard proxy.
    RefPtr<nsClipboardProxy> clipboardProxy = new nsClipboardProxy();
    return clipboardProxy->GetData(aTransferable, aWhichClipboard);
  }

  // Use a pref to toggle rich text/non-text support.
  if (Preferences::GetBool("clipboard.plainTextOnly")) {
    nsresult rv;
    nsCOMPtr<nsISupportsString> dataWrapper =
      do_CreateInstance(NS_SUPPORTS_STRING_CONTRACTID, &rv);
    rv = dataWrapper->SetData(mClipboard->GetText());
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
    nsCOMPtr<nsISupports> genericDataWrapper = do_QueryInterface(dataWrapper);
    uint32_t len = mClipboard->GetText().Length() * sizeof(char16_t);
    rv = aTransferable->SetTransferData(kUnicodeMime, genericDataWrapper, len);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
    return NS_OK;
  }

  // Get flavor list that includes all acceptable flavors (including
  // ones obtained through conversion).
  // Note: We don't need to call nsITransferable::AddDataFlavor here
  //       because ContentParent already did.
  nsCOMPtr<nsISupportsArray> flavorList;
  nsresult rv = aTransferable->FlavorsTransferableCanImport(getter_AddRefs(flavorList));

  if (!flavorList || NS_FAILED(rv)) {
    return NS_ERROR_FAILURE;
  }

  // Walk through flavors and see which flavor matches the one being pasted.
  uint32_t flavorCount;
  flavorList->Count(&flavorCount);

  for (uint32_t i = 0; i < flavorCount; ++i) {
    nsCOMPtr<nsISupportsCString> currentFlavor = do_QueryElementAt(flavorList, i);

    if (currentFlavor) {
      // flavorStr is the mime type.
      nsXPIDLCString flavorStr;
      currentFlavor->ToString(getter_Copies(flavorStr));

      // text/plain, text/Unicode
      if (flavorStr.EqualsLiteral(kUnicodeMime) && mClipboard->HasText()) {
        nsresult rv;
        nsCOMPtr<nsISupportsString> dataWrapper = do_CreateInstance(NS_SUPPORTS_STRING_CONTRACTID, &rv);
        rv = dataWrapper->SetData(mClipboard->GetText());
        if (NS_WARN_IF(NS_FAILED(rv))) {
          continue;
        }

        nsCOMPtr<nsISupports> genericDataWrapper = do_QueryInterface(dataWrapper);
        uint32_t len = mClipboard->GetText().Length() * sizeof(char16_t);
        rv = aTransferable->SetTransferData(flavorStr, genericDataWrapper, len);
        if (NS_WARN_IF(NS_FAILED(rv))) {
          continue;
        }
        break;
      }

      // text/html
      if (flavorStr.EqualsLiteral(kHTMLMime) && mClipboard->HasHTML()) {
        nsresult rv;
        nsCOMPtr<nsISupportsString> dataWrapper = do_CreateInstance(NS_SUPPORTS_STRING_CONTRACTID, &rv);
        rv = dataWrapper->SetData(mClipboard->GetHTML());
        if (NS_WARN_IF(NS_FAILED(rv))) {
          continue;
        }

        nsCOMPtr<nsISupports> genericDataWrapper = do_QueryInterface(dataWrapper);
        uint32_t len = mClipboard->GetHTML().Length() * sizeof(char16_t);
        rv = aTransferable->SetTransferData(flavorStr, genericDataWrapper, len);
        if (NS_WARN_IF(NS_FAILED(rv))) {
          continue;
        }
        break;
      }

      // image/[png|jpeg|jpg]
      if ((flavorStr.EqualsLiteral(kPNGImageMime) ||
           flavorStr.EqualsLiteral(kJPEGImageMime) ||
           flavorStr.EqualsLiteral(kJPGImageMime)) &&
          mClipboard->HasImage() ) {
        // Get image buffer from clipboard.
        RefPtr<gfx::DataSourceSurface> image = mClipboard->GetImage();

        // Encode according to MIME type.
        RefPtr<gfxDrawable> drawable = new gfxSurfaceDrawable(image, image->GetSize());
        nsCOMPtr<imgIContainer> imageContainer(image::ImageOps::CreateFromDrawable(drawable));
        nsCOMPtr<imgITools> imgTool = do_GetService(NS_IMGTOOLS_CID);

        nsCOMPtr<nsIInputStream> byteStream;
        imgTool->EncodeImage(imageContainer, flavorStr, EmptyString(), getter_AddRefs(byteStream));

        // Set transferable.
        nsresult rv = aTransferable->SetTransferData(flavorStr,
                                                     byteStream,
                                                     sizeof(nsIInputStream*));
        if (NS_WARN_IF(NS_FAILED(rv))) {
          continue;
        }
        break;
      }
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsClipboard::EmptyClipboard(int32_t aWhichClipboard)
{
  if (aWhichClipboard != kGlobalClipboard) {
    return NS_ERROR_NOT_IMPLEMENTED;
  }
  if (XRE_IsParentProcess()) {
    mClipboard->Clear();
  } else {
    ContentChild::GetSingleton()->SendEmptyClipboard(aWhichClipboard);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsClipboard::HasDataMatchingFlavors(const char **aFlavorList,
                                    uint32_t aLength, int32_t aWhichClipboard,
                                    bool *aHasType)
{
  *aHasType = false;
  if (aWhichClipboard != kGlobalClipboard) {
    return NS_ERROR_NOT_IMPLEMENTED;
  }
  if (XRE_IsParentProcess()) {
    // Retrieve the union of all aHasType in aFlavorList
    for (uint32_t i = 0; i < aLength; ++i) {
      const char *flavor = aFlavorList[i];
      if (!flavor) {
        continue;
      }
      if (!strcmp(flavor, kUnicodeMime) && mClipboard->HasText()) {
        *aHasType = true;
      } else if (!strcmp(flavor, kHTMLMime) && mClipboard->HasHTML()) {
        *aHasType = true;
      } else if (!strcmp(flavor, kJPEGImageMime) ||
                 !strcmp(flavor, kJPGImageMime) ||
                 !strcmp(flavor, kPNGImageMime)) {
        // We will encode the image into any format you want, so we don't
        // need to check each specific format
        if (mClipboard->HasImage()) {
          *aHasType = true;
        }
      }
    }
  } else {
    RefPtr<nsClipboardProxy> clipboardProxy = new nsClipboardProxy();
    return clipboardProxy->HasDataMatchingFlavors(aFlavorList, aLength, aWhichClipboard, aHasType);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsClipboard::SupportsSelectionClipboard(bool *aIsSupported)
{
  *aIsSupported = false;
  return NS_OK;
}

NS_IMETHODIMP
nsClipboard::SupportsFindClipboard(bool* _retval)
{
  NS_ENSURE_ARG_POINTER(_retval);

  *_retval = false;
  return NS_OK;
}

