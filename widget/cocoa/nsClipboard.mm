/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <algorithm>

#include "mozilla/gfx/2D.h"
#include "mozilla/Logging.h"
#include "mozilla/Unused.h"

#include "gfxPlatform.h"
#include "nsArrayUtils.h"
#include "nsCOMPtr.h"
#include "nsClipboard.h"
#include "nsString.h"
#include "nsISupportsPrimitives.h"
#include "nsPrimitiveHelpers.h"
#include "nsIFile.h"
#include "nsStringStream.h"
#include "nsEscape.h"
#include "nsPrintfCString.h"
#include "nsObjCExceptions.h"
#include "imgIContainer.h"
#include "nsCocoaUtils.h"

using mozilla::gfx::DataSourceSurface;
using mozilla::gfx::SourceSurface;

mozilla::StaticRefPtr<nsITransferable> nsClipboard::sSelectionCache;
int32_t nsClipboard::sSelectionCacheChangeCount = 0;

nsClipboard::nsClipboard()
    : nsBaseClipboard(mozilla::dom::ClipboardCapabilities(
          false /* supportsSelectionClipboard */,
          true /* supportsFindClipboard */,
          true /* supportsSelectionCache */)) {}

nsClipboard::~nsClipboard() { ClearSelectionCache(); }

NS_IMPL_ISUPPORTS_INHERITED0(nsClipboard, nsBaseClipboard)

namespace {

// We separate this into its own function because after an @try, all local
// variables within that function get marked as volatile, and our C++ type
// system doesn't like volatile things.
static NSData* GetDataFromPasteboard(NSPasteboard* aPasteboard,
                                     NSString* aType) {
  NSData* data = nil;
  @try {
    data = [aPasteboard dataForType:aType];
  } @catch (NSException* e) {
    NS_WARNING(nsPrintfCString("Exception raised while getting data from the "
                               "pasteboard: \"%s - %s\"",
                               [[e name] UTF8String], [[e reason] UTF8String])
                   .get());
    mozilla::Unused << e;
  }
  return data;
}

static NSPasteboard* GetPasteboard(int32_t aWhichClipboard) {
  switch (aWhichClipboard) {
    case nsIClipboard::kGlobalClipboard:
      return [NSPasteboard generalPasteboard];
    case nsIClipboard::kFindClipboard:
      return [NSPasteboard pasteboardWithName:NSPasteboardNameFind];
    default:
      return nil;
  }
}

}  // namespace

void nsClipboard::SetSelectionCache(nsITransferable* aTransferable) {
  sSelectionCacheChangeCount++;
  sSelectionCache = aTransferable;
}

void nsClipboard::ClearSelectionCache() { SetSelectionCache(nullptr); }

NS_IMETHODIMP
nsClipboard::SetNativeClipboardData(nsITransferable* aTransferable,
                                    int32_t aWhichClipboard) {
  NS_OBJC_BEGIN_TRY_BLOCK_RETURN;

  MOZ_ASSERT(aTransferable);
  MOZ_ASSERT(nsIClipboard::IsClipboardTypeSupported(aWhichClipboard));

  if (aWhichClipboard == kSelectionCache) {
    SetSelectionCache(aTransferable);
    return NS_OK;
  }

  NSDictionary* pasteboardOutputDict =
      PasteboardDictFromTransferable(aTransferable);
  if (!pasteboardOutputDict) return NS_ERROR_FAILURE;

  unsigned int outputCount = [pasteboardOutputDict count];
  NSArray* outputKeys = [pasteboardOutputDict allKeys];
  NSPasteboard* cocoaPasteboard = GetPasteboard(aWhichClipboard);
  MOZ_ASSERT(cocoaPasteboard);
  if (aWhichClipboard == kFindClipboard) {
    NSString* stringType =
        [UTIHelper stringFromPboardType:NSPasteboardTypeString];
    [cocoaPasteboard declareTypes:[NSArray arrayWithObject:stringType]
                            owner:nil];
  } else {
    // Write everything else out to the general pasteboard.
    MOZ_ASSERT(aWhichClipboard == kGlobalClipboard);
    [cocoaPasteboard declareTypes:outputKeys owner:nil];
  }

  for (unsigned int i = 0; i < outputCount; i++) {
    NSString* currentKey = [outputKeys objectAtIndex:i];
    id currentValue = [pasteboardOutputDict valueForKey:currentKey];
    if (aWhichClipboard == kFindClipboard) {
      if ([currentKey isEqualToString:[UTIHelper stringFromPboardType:
                                                     NSPasteboardTypeString]]) {
        [cocoaPasteboard setString:currentValue forType:currentKey];
      }
    } else {
      if ([currentKey isEqualToString:[UTIHelper stringFromPboardType:
                                                     NSPasteboardTypeString]] ||
          [currentKey
              isEqualToString:[UTIHelper
                                  stringFromPboardType:kPublicUrlPboardType]] ||
          [currentKey
              isEqualToString:
                  [UTIHelper stringFromPboardType:kPublicUrlNamePboardType]]) {
        [cocoaPasteboard setString:currentValue forType:currentKey];
      } else if ([currentKey
                     isEqualToString:
                         [UTIHelper
                             stringFromPboardType:kUrlsWithTitlesPboardType]]) {
        [cocoaPasteboard
            setPropertyList:[pasteboardOutputDict valueForKey:currentKey]
                    forType:currentKey];
      } else if ([currentKey
                     isEqualToString:[UTIHelper stringFromPboardType:
                                                    NSPasteboardTypeHTML]]) {
        [cocoaPasteboard
            setString:(nsClipboard::WrapHtmlForSystemPasteboard(currentValue))
              forType:currentKey];
      } else if ([currentKey
                     isEqualToString:[UTIHelper stringFromPboardType:
                                                    kMozFileUrlsPboardType]]) {
        [cocoaPasteboard writeObjects:currentValue];
      } else if ([currentKey
                     isEqualToString:
                         [UTIHelper
                             stringFromPboardType:(NSString*)kUTTypeFileURL]]) {
        [cocoaPasteboard setString:currentValue forType:currentKey];
      } else if ([currentKey
                     isEqualToString:
                         [UTIHelper
                             stringFromPboardType:kPasteboardConcealedType]]) {
        // It's fine to set the data to null for this field - this field is an
        // addition to a value's other type and works like a flag.
        [cocoaPasteboard setData:NULL forType:currentKey];
      } else {
        [cocoaPasteboard setData:currentValue forType:currentKey];
      }
    }
  }

  return NS_OK;

  NS_OBJC_END_TRY_BLOCK_RETURN(NS_ERROR_FAILURE);
}

nsresult nsClipboard::TransferableFromPasteboard(
    nsITransferable* aTransferable, NSPasteboard* cocoaPasteboard) {
  NS_OBJC_BEGIN_TRY_BLOCK_RETURN;

  // get flavor list that includes all acceptable flavors (including ones
  // obtained through conversion)
  nsTArray<nsCString> flavors;
  nsresult rv = aTransferable->FlavorsTransferableCanImport(flavors);
  if (NS_FAILED(rv)) return NS_ERROR_FAILURE;

  for (uint32_t i = 0; i < flavors.Length(); i++) {
    nsCString& flavorStr = flavors[i];

    // printf("looking for clipboard data of type %s\n", flavorStr.get());

    NSString* pboardType = nil;
    if (nsClipboard::IsStringType(flavorStr, &pboardType)) {
      NSString* pString = [cocoaPasteboard stringForType:pboardType];
      if (!pString) {
        continue;
      }

      NSData* stringData;
      bool isRTF = [pboardType
          isEqualToString:[UTIHelper stringFromPboardType:NSPasteboardTypeRTF]];
      if (isRTF) {
        stringData = [pString dataUsingEncoding:NSASCIIStringEncoding];
      } else {
        stringData = [pString dataUsingEncoding:NSUnicodeStringEncoding];
      }
      unsigned int dataLength = [stringData length];
      void* clipboardDataPtr = malloc(dataLength);
      if (!clipboardDataPtr) {
        return NS_ERROR_OUT_OF_MEMORY;
      }
      [stringData getBytes:clipboardDataPtr length:dataLength];

      // The DOM only wants LF, so convert from MacOS line endings to DOM line
      // endings.
      int32_t signedDataLength = dataLength;
      nsLinebreakHelpers::ConvertPlatformToDOMLinebreaks(
          isRTF, &clipboardDataPtr, &signedDataLength);
      dataLength = signedDataLength;

      // skip BOM (Byte Order Mark to distinguish little or big endian)
      char16_t* clipboardDataPtrNoBOM = (char16_t*)clipboardDataPtr;
      if ((dataLength > 2) && ((clipboardDataPtrNoBOM[0] == 0xFEFF) ||
                               (clipboardDataPtrNoBOM[0] == 0xFFFE))) {
        dataLength -= sizeof(char16_t);
        clipboardDataPtrNoBOM += 1;
      }

      nsCOMPtr<nsISupports> genericDataWrapper;
      nsPrimitiveHelpers::CreatePrimitiveForData(
          flavorStr, clipboardDataPtrNoBOM, dataLength,
          getter_AddRefs(genericDataWrapper));
      aTransferable->SetTransferData(flavorStr.get(), genericDataWrapper);
      free(clipboardDataPtr);
      break;
    } else if (flavorStr.EqualsLiteral(kFileMime)) {
      NSArray* items = [cocoaPasteboard pasteboardItems];
      if (!items || [items count] <= 0) {
        continue;
      }

      // XXX we don't support multiple clipboard item on DOM and XPCOM interface
      // for now, so we only get the data from the first pasteboard item.
      NSPasteboardItem* item = [items objectAtIndex:0];
      if (!item) {
        continue;
      }

      nsCocoaUtils::SetTransferDataForTypeFromPasteboardItem(aTransferable,
                                                             flavorStr, item);
    } else if (flavorStr.EqualsLiteral(kCustomTypesMime)) {
      NSString* type = [cocoaPasteboard
          availableTypeFromArray:
              [NSArray
                  arrayWithObject:[UTIHelper stringFromPboardType:
                                                 kMozCustomTypesPboardType]]];
      if (!type) {
        continue;
      }

      NSData* pasteboardData = GetDataFromPasteboard(cocoaPasteboard, type);
      if (!pasteboardData) {
        continue;
      }

      unsigned int dataLength = [pasteboardData length];
      void* clipboardDataPtr = malloc(dataLength);
      if (!clipboardDataPtr) {
        return NS_ERROR_OUT_OF_MEMORY;
      }
      [pasteboardData getBytes:clipboardDataPtr length:dataLength];

      nsCOMPtr<nsISupports> genericDataWrapper;
      nsPrimitiveHelpers::CreatePrimitiveForData(
          flavorStr, clipboardDataPtr, dataLength,
          getter_AddRefs(genericDataWrapper));

      aTransferable->SetTransferData(flavorStr.get(), genericDataWrapper);
      free(clipboardDataPtr);
    } else if (flavorStr.EqualsLiteral(kJPEGImageMime) ||
               flavorStr.EqualsLiteral(kJPGImageMime) ||
               flavorStr.EqualsLiteral(kPNGImageMime) ||
               flavorStr.EqualsLiteral(kGIFImageMime)) {
      // Figure out if there's data on the pasteboard we can grab (sanity check)
      NSString* type = [cocoaPasteboard
          availableTypeFromArray:
              [NSArray
                  arrayWithObjects:
                      [UTIHelper
                          stringFromPboardType:(NSString*)kUTTypeFileURL],
                      [UTIHelper stringFromPboardType:NSPasteboardTypeTIFF],
                      [UTIHelper stringFromPboardType:NSPasteboardTypePNG],
                      nil]];
      if (!type) continue;

      // Read data off the clipboard
      NSData* pasteboardData = GetDataFromPasteboard(cocoaPasteboard, type);
      if (!pasteboardData) continue;

      // Figure out what type we're converting to
      CFStringRef outputType = NULL;
      if (flavorStr.EqualsLiteral(kJPEGImageMime) ||
          flavorStr.EqualsLiteral(kJPGImageMime))
        outputType = CFSTR("public.jpeg");
      else if (flavorStr.EqualsLiteral(kPNGImageMime))
        outputType = CFSTR("public.png");
      else if (flavorStr.EqualsLiteral(kGIFImageMime))
        outputType = CFSTR("com.compuserve.gif");
      else
        continue;

      // Use ImageIO to interpret the data on the clipboard and transcode.
      // Note that ImageIO, like all CF APIs, allows NULLs to propagate freely
      // and safely in most cases (like ObjC). A notable exception is CFRelease.
      NSDictionary* options = [NSDictionary
          dictionaryWithObjectsAndKeys:(NSNumber*)kCFBooleanTrue,
                                       kCGImageSourceShouldAllowFloat, type,
                                       kCGImageSourceTypeIdentifierHint, nil];
      CGImageSourceRef source = nullptr;
      if (type == [UTIHelper stringFromPboardType:(NSString*)kUTTypeFileURL]) {
        NSString* urlStr = [cocoaPasteboard stringForType:type];
        NSURL* url = [NSURL URLWithString:urlStr];
        source =
            CGImageSourceCreateWithURL((CFURLRef)url, (CFDictionaryRef)options);
      } else {
        source = CGImageSourceCreateWithData((CFDataRef)pasteboardData,
                                             (CFDictionaryRef)options);
      }

      NSMutableData* encodedData = [NSMutableData data];
      CGImageDestinationRef dest = CGImageDestinationCreateWithData(
          (CFMutableDataRef)encodedData, outputType, 1, NULL);
      CGImageDestinationAddImageFromSource(dest, source, 0, NULL);
      bool successfullyConverted = CGImageDestinationFinalize(dest);

      if (successfullyConverted) {
        // Put the converted data in a form Gecko can understand
        nsCOMPtr<nsIInputStream> byteStream;
        NS_NewByteInputStream(getter_AddRefs(byteStream),
                              mozilla::Span((const char*)[encodedData bytes],
                                            [encodedData length]),
                              NS_ASSIGNMENT_COPY);

        aTransferable->SetTransferData(flavorStr.get(), byteStream);
      }

      if (dest) CFRelease(dest);
      if (source) CFRelease(source);

      if (successfullyConverted) {
        // XXX Maybe try to fill in more types? Is there a point?
        break;
      } else {
        continue;
      }
    }
  }

  return NS_OK;

  NS_OBJC_END_TRY_BLOCK_RETURN(NS_ERROR_FAILURE);
}

NS_IMETHODIMP
nsClipboard::GetNativeClipboardData(nsITransferable* aTransferable,
                                    int32_t aWhichClipboard) {
  NS_OBJC_BEGIN_TRY_BLOCK_RETURN;

  MOZ_DIAGNOSTIC_ASSERT(aTransferable);
  MOZ_DIAGNOSTIC_ASSERT(
      nsIClipboard::IsClipboardTypeSupported(aWhichClipboard));

  if (kSelectionCache == aWhichClipboard) {
    if (!sSelectionCache) {
      return NS_OK;
    }

    // get flavor list that includes all acceptable flavors (including ones
    // obtained through conversion)
    nsTArray<nsCString> flavors;
    nsresult rv = aTransferable->FlavorsTransferableCanImport(flavors);
    if (NS_FAILED(rv)) {
      return NS_ERROR_FAILURE;
    }

    for (const auto& flavor : flavors) {
      nsCOMPtr<nsISupports> dataSupports;
      rv = sSelectionCache->GetTransferData(flavor.get(),
                                            getter_AddRefs(dataSupports));
      if (NS_SUCCEEDED(rv)) {
        MOZ_CLIPBOARD_LOG("%s: getting %s from cache.", __FUNCTION__,
                          flavor.get());
        aTransferable->SetTransferData(flavor.get(), dataSupports);
        // XXX Maybe try to fill in more types? Is there a point?
        break;
      }
    }
    return NS_OK;
  }

  NSPasteboard* cocoaPasteboard = GetPasteboard(aWhichClipboard);
  if (!cocoaPasteboard) {
    return NS_ERROR_FAILURE;
  }

  return TransferableFromPasteboard(aTransferable, cocoaPasteboard);

  NS_OBJC_END_TRY_BLOCK_RETURN(NS_ERROR_FAILURE);
}

// returns true if we have *any* of the passed in flavors available for pasting
mozilla::Result<bool, nsresult>
nsClipboard::HasNativeClipboardDataMatchingFlavors(
    const nsTArray<nsCString>& aFlavorList, int32_t aWhichClipboard) {
  NS_OBJC_BEGIN_TRY_BLOCK_RETURN;

  MOZ_DIAGNOSTIC_ASSERT(
      nsIClipboard::IsClipboardTypeSupported(aWhichClipboard));

  if (kSelectionCache == aWhichClipboard) {
    nsTArray<nsCString> transferableFlavors;
    if (sSelectionCache &&
        NS_SUCCEEDED(sSelectionCache->FlavorsTransferableCanExport(
            transferableFlavors))) {
      if (MOZ_CLIPBOARD_LOG_ENABLED()) {
        MOZ_CLIPBOARD_LOG("    SelectionCache types (nums %zu)\n",
                          transferableFlavors.Length());
        for (const auto& transferableFlavor : transferableFlavors) {
          MOZ_CLIPBOARD_LOG("        MIME %s", transferableFlavor.get());
        }
      }

      for (const auto& transferableFlavor : transferableFlavors) {
        for (const auto& flavor : aFlavorList) {
          if (transferableFlavor.Equals(flavor)) {
            MOZ_CLIPBOARD_LOG("    has %s", flavor.get());
            return true;
          }
        }
      }
    }

    if (MOZ_CLIPBOARD_LOG_ENABLED()) {
      MOZ_CLIPBOARD_LOG("    no targets at clipboard (bad match)\n");
    }

    return false;
  }

  NSPasteboard* cocoaPasteboard = GetPasteboard(aWhichClipboard);
  MOZ_ASSERT(cocoaPasteboard);
  if (MOZ_CLIPBOARD_LOG_ENABLED()) {
    NSArray* types = [cocoaPasteboard types];
    uint32_t count = [types count];
    MOZ_CLIPBOARD_LOG("    Pasteboard types (nums %d)\n", count);
    for (uint32_t i = 0; i < count; i++) {
      NSPasteboardType type = [types objectAtIndex:i];
      if (!type) {
        MOZ_CLIPBOARD_LOG("        failed to get MIME\n");
        continue;
      }
      MOZ_CLIPBOARD_LOG("        MIME %s\n", [type UTF8String]);
    }
  }

  for (auto& mimeType : aFlavorList) {
    NSString* pboardType = nil;
    if (nsClipboard::IsStringType(mimeType, &pboardType)) {
      NSString* availableType = [cocoaPasteboard
          availableTypeFromArray:[NSArray arrayWithObject:pboardType]];
      if (availableType && [availableType isEqualToString:pboardType]) {
        MOZ_CLIPBOARD_LOG("    has %s\n", mimeType.get());
        return true;
      }
    } else if (mimeType.EqualsLiteral(kCustomTypesMime)) {
      NSString* availableType = [cocoaPasteboard
          availableTypeFromArray:
              [NSArray
                  arrayWithObject:[UTIHelper stringFromPboardType:
                                                 kMozCustomTypesPboardType]]];
      if (availableType) {
        MOZ_CLIPBOARD_LOG("    has %s\n", mimeType.get());
        return true;
      }
    } else if (mimeType.EqualsLiteral(kJPEGImageMime) ||
               mimeType.EqualsLiteral(kJPGImageMime) ||
               mimeType.EqualsLiteral(kPNGImageMime) ||
               mimeType.EqualsLiteral(kGIFImageMime)) {
      NSString* availableType = [cocoaPasteboard
          availableTypeFromArray:
              [NSArray
                  arrayWithObjects:
                      [UTIHelper stringFromPboardType:NSPasteboardTypeTIFF],
                      [UTIHelper stringFromPboardType:NSPasteboardTypePNG],
                      nil]];
      if (availableType) {
        MOZ_CLIPBOARD_LOG("    has %s\n", mimeType.get());
        return true;
      }
    } else if (mimeType.EqualsLiteral(kFileMime)) {
      NSArray* items = [cocoaPasteboard pasteboardItems];
      if (items && [items count] > 0) {
        // XXX we only check the first pasteboard item as we only get data from
        // first item in TransferableFromPasteboard for now.
        if (NSPasteboardItem* item = [items objectAtIndex:0]) {
          if (NSString *availableType = [item
                  availableTypeFromArray:
                      [NSArray
                          arrayWithObjects:[UTIHelper
                                               stringFromPboardType:
                                                   (NSString*)kUTTypeFileURL],
                                           nil]]) {
            MOZ_CLIPBOARD_LOG("    has %s\n", mimeType.get());
            return true;
          }
        }
      }
    }
  }

  if (MOZ_CLIPBOARD_LOG_ENABLED()) {
    MOZ_CLIPBOARD_LOG("    no targets at clipboard (bad match)\n");
  }

  return false;

  NS_OBJC_END_TRY_BLOCK_RETURN(mozilla::Err(NS_ERROR_FAILURE));
}

// static
mozilla::Maybe<uint32_t> nsClipboard::FindIndexOfImageFlavor(
    const nsTArray<nsCString>& aMIMETypes) {
  for (uint32_t i = 0; i < aMIMETypes.Length(); ++i) {
    if (nsClipboard::IsImageType(aMIMETypes[i])) {
      return mozilla::Some(i);
    }
  }

  return mozilla::Nothing();
}

// This function converts anything that other applications might understand into
// the system format and puts it into a dictionary which it returns. static
NSDictionary* nsClipboard::PasteboardDictFromTransferable(
    nsITransferable* aTransferable) {
  NS_OBJC_BEGIN_TRY_BLOCK_RETURN;

  if (!aTransferable) {
    return nil;
  }

  NSMutableDictionary* pasteboardOutputDict = [NSMutableDictionary dictionary];

  nsTArray<nsCString> flavors;
  nsresult rv = aTransferable->FlavorsTransferableCanExport(flavors);
  if (NS_FAILED(rv)) {
    return nil;
  }

  const mozilla::Maybe<uint32_t> imageFlavorIndex =
      nsClipboard::FindIndexOfImageFlavor(flavors);

  if (imageFlavorIndex) {
    // When right-clicking and "Copy Image" is clicked on macOS, some apps
    // expect the first flavor to be the image flavor. See bug 1689992. For
    // other apps, the order shouldn't matter.
    std::swap(*flavors.begin(), flavors[*imageFlavorIndex]);
  }
  for (uint32_t i = 0; i < flavors.Length(); i++) {
    nsCString& flavorStr = flavors[i];

    MOZ_CLIPBOARD_LOG("writing out clipboard data of type %s (%d)\n",
                      flavorStr.get(), i);

    NSString* pboardType = nil;
    if (nsClipboard::IsStringType(flavorStr, &pboardType)) {
      nsCOMPtr<nsISupports> genericDataWrapper;
      rv = aTransferable->GetTransferData(flavorStr.get(),
                                          getter_AddRefs(genericDataWrapper));
      if (NS_FAILED(rv)) {
        continue;
      }

      nsAutoString data;
      if (nsCOMPtr<nsISupportsString> text =
              do_QueryInterface(genericDataWrapper)) {
        text->GetData(data);
      }

      NSString* nativeString;
      if (!data.IsEmpty()) {
        nativeString = [NSString stringWithCharacters:(const unichar*)data.get()
                                               length:data.Length()];
      } else {
        nativeString = [NSString string];
      }

      // be nice to Carbon apps, normalize the receiver's contents using Form C.
      nativeString = [nativeString precomposedStringWithCanonicalMapping];
      if (nativeString) {
        [pasteboardOutputDict setObject:nativeString forKey:pboardType];
      }

      if (aTransferable->GetIsPrivateData()) {
        // In the case of password strings, we want to include the key for
        // concealed type. These will be flagged as private data.
        [pasteboardOutputDict
            setObject:nativeString
               forKey:[UTIHelper
                          stringFromPboardType:kPasteboardConcealedType]];
      }
    } else if (flavorStr.EqualsLiteral(kCustomTypesMime)) {
      nsCOMPtr<nsISupports> genericDataWrapper;
      rv = aTransferable->GetTransferData(flavorStr.get(),
                                          getter_AddRefs(genericDataWrapper));
      if (NS_FAILED(rv)) {
        continue;
      }

      nsAutoCString data;
      if (nsCOMPtr<nsISupportsCString> text =
              do_QueryInterface(genericDataWrapper)) {
        text->GetData(data);
      }

      if (!data.IsEmpty()) {
        NSData* nativeData = [NSData dataWithBytes:data.get()
                                            length:data.Length()];
        NSString* customType =
            [UTIHelper stringFromPboardType:kMozCustomTypesPboardType];
        if (!nativeData) {
          continue;
        }
        [pasteboardOutputDict setObject:nativeData forKey:customType];
      }
    } else if (nsClipboard::IsImageType(flavorStr)) {
      nsCOMPtr<nsISupports> transferSupports;
      rv = aTransferable->GetTransferData(flavorStr.get(),
                                          getter_AddRefs(transferSupports));
      if (NS_FAILED(rv)) {
        continue;
      }

      nsCOMPtr<imgIContainer> image(do_QueryInterface(transferSupports));
      if (!image) {
        NS_WARNING("Image isn't an imgIContainer in transferable");
        continue;
      }

      RefPtr<SourceSurface> surface = image->GetFrame(
          imgIContainer::FRAME_CURRENT,
          imgIContainer::FLAG_SYNC_DECODE | imgIContainer::FLAG_ASYNC_NOTIFY);
      if (!surface) {
        continue;
      }
      CGImageRef imageRef = NULL;
      rv = nsCocoaUtils::CreateCGImageFromSurface(surface, &imageRef);
      if (NS_FAILED(rv) || !imageRef) {
        continue;
      }

      // Convert the CGImageRef to TIFF and PNG data.
      CFMutableDataRef tiffData = CFDataCreateMutable(kCFAllocatorDefault, 0);
      CFMutableDataRef pngData = CFDataCreateMutable(kCFAllocatorDefault, 0);
      CGImageDestinationRef destRefTIFF = CGImageDestinationCreateWithData(
          tiffData, CFSTR("public.tiff"), 1, NULL);
      CGImageDestinationRef destRefPNG = CGImageDestinationCreateWithData(
          pngData, CFSTR("public.png"), 1, NULL);
      CGImageDestinationAddImage(destRefTIFF, imageRef, NULL);
      CGImageDestinationAddImage(destRefPNG, imageRef, NULL);
      const bool successfullyConvertedTIFF =
          CGImageDestinationFinalize(destRefTIFF);
      const bool successfullyConvertedPNG =
          CGImageDestinationFinalize(destRefPNG);

      CGImageRelease(imageRef);
      if (destRefTIFF) {
        CFRelease(destRefTIFF);
      }
      if (destRefPNG) {
        CFRelease(destRefPNG);
      }

      if (successfullyConvertedTIFF && tiffData) {
        NSString* tiffType =
            [UTIHelper stringFromPboardType:NSPasteboardTypeTIFF];
        [pasteboardOutputDict setObject:(NSMutableData*)tiffData
                                 forKey:tiffType];
      }
      if (successfullyConvertedPNG && pngData) {
        NSString* pngType =
            [UTIHelper stringFromPboardType:NSPasteboardTypePNG];

        [pasteboardOutputDict setObject:(NSMutableData*)pngData forKey:pngType];
      }
      if (tiffData) {
        CFRelease(tiffData);
      }
      if (pngData) {
        CFRelease(pngData);
      }
    } else if (flavorStr.EqualsLiteral(kFileMime)) {
      nsCOMPtr<nsISupports> genericFile;
      rv = aTransferable->GetTransferData(flavorStr.get(),
                                          getter_AddRefs(genericFile));
      if (NS_FAILED(rv)) {
        continue;
      }

      nsCOMPtr<nsIFile> file(do_QueryInterface(genericFile));
      if (!file) {
        continue;
      }

      nsAutoString fileURI;
      rv = file->GetPath(fileURI);
      if (NS_FAILED(rv)) {
        continue;
      }

      NSString* str = nsCocoaUtils::ToNSString(fileURI);
      NSURL* url = [NSURL fileURLWithPath:str isDirectory:NO];
      if (!url || ![url absoluteString]) {
        continue;
      }
      NSString* fileUTType =
          [UTIHelper stringFromPboardType:(NSString*)kUTTypeFileURL];
      [pasteboardOutputDict setObject:[url absoluteString] forKey:fileUTType];
    } else if (flavorStr.EqualsLiteral(kFilePromiseMime)) {
      NSString* urlPromise = [UTIHelper
          stringFromPboardType:(NSString*)kPasteboardTypeFileURLPromise];
      NSString* urlPromiseContent = [UTIHelper
          stringFromPboardType:(NSString*)kPasteboardTypeFilePromiseContent];
      [pasteboardOutputDict setObject:[NSArray arrayWithObject:@""]
                               forKey:urlPromise];
      [pasteboardOutputDict setObject:[NSArray arrayWithObject:@""]
                               forKey:urlPromiseContent];
    } else if (flavorStr.EqualsLiteral(kURLMime)) {
      nsCOMPtr<nsISupports> genericURL;
      rv = aTransferable->GetTransferData(flavorStr.get(),
                                          getter_AddRefs(genericURL));
      nsCOMPtr<nsISupportsString> urlObject(do_QueryInterface(genericURL));

      nsAutoString url;
      urlObject->GetData(url);

      NSString* nativeTitle = nil;

      // A newline embedded in the URL means that the form is actually URL +
      // title. This embedding occurs in nsDragService::GetData.
      int32_t newlinePos = url.FindChar(char16_t('\n'));
      if (newlinePos >= 0) {
        url.Truncate(newlinePos);

        nsAutoString urlTitle;
        urlObject->GetData(urlTitle);
        urlTitle.Mid(urlTitle, newlinePos + 1,
                     urlTitle.Length() - (newlinePos + 1));

        nativeTitle =
            [NSString stringWithCharacters:reinterpret_cast<const unichar*>(
                                               urlTitle.get())
                                    length:urlTitle.Length()];
      }
      // The Finder doesn't like getting random binary data aka
      // Unicode, so change it into an escaped URL containing only
      // ASCII.
      nsAutoCString utf8Data = NS_ConvertUTF16toUTF8(url.get(), url.Length());
      nsAutoCString escData;
      NS_EscapeURL(utf8Data.get(), utf8Data.Length(),
                   esc_OnlyNonASCII | esc_AlwaysCopy, escData);

      NSString* nativeURL = [NSString stringWithUTF8String:escData.get()];
      NSString* publicUrl =
          [UTIHelper stringFromPboardType:kPublicUrlPboardType];
      if (!nativeURL) {
        continue;
      }
      [pasteboardOutputDict setObject:nativeURL forKey:publicUrl];
      if (nativeTitle) {
        NSArray* urlsAndTitles = @[ @[ nativeURL ], @[ nativeTitle ] ];
        NSString* urlName =
            [UTIHelper stringFromPboardType:kPublicUrlNamePboardType];
        NSString* urlsWithTitles =
            [UTIHelper stringFromPboardType:kUrlsWithTitlesPboardType];
        [pasteboardOutputDict setObject:nativeTitle forKey:urlName];
        [pasteboardOutputDict setObject:urlsAndTitles forKey:urlsWithTitles];
      }
    }
    // If it wasn't a type that we recognize as exportable we don't put it on
    // the system clipboard. We'll just access it from our cached transferable
    // when we need it.
  }

  return pasteboardOutputDict;

  NS_OBJC_END_TRY_BLOCK_RETURN(nil);
}

bool nsClipboard::IsStringType(const nsCString& aMIMEType,
                               NSString** aPboardType) {
  if (aMIMEType.EqualsLiteral(kTextMime)) {
    *aPboardType = [UTIHelper stringFromPboardType:NSPasteboardTypeString];
    return true;
  } else if (aMIMEType.EqualsLiteral(kRTFMime)) {
    *aPboardType = [UTIHelper stringFromPboardType:NSPasteboardTypeRTF];
    return true;
  } else if (aMIMEType.EqualsLiteral(kHTMLMime)) {
    *aPboardType = [UTIHelper stringFromPboardType:NSPasteboardTypeHTML];
    return true;
  } else {
    return false;
  }
}

// static
bool nsClipboard::IsImageType(const nsACString& aMIMEType) {
  return aMIMEType.EqualsLiteral(kPNGImageMime) ||
         aMIMEType.EqualsLiteral(kJPEGImageMime) ||
         aMIMEType.EqualsLiteral(kJPGImageMime) ||
         aMIMEType.EqualsLiteral(kGIFImageMime) ||
         aMIMEType.EqualsLiteral(kNativeImageMime);
}

NSString* nsClipboard::WrapHtmlForSystemPasteboard(NSString* aString) {
  NSString* wrapped =
      [NSString stringWithFormat:@"<html>"
                                  "<head>"
                                  "<meta http-equiv=\"content-type\" "
                                  "content=\"text/html; charset=utf-8\">"
                                  "</head>"
                                  "<body>"
                                  "%@"
                                  "</body>"
                                  "</html>",
                                 aString];
  return wrapped;
}

nsresult nsClipboard::EmptyNativeClipboardData(int32_t aWhichClipboard) {
  NS_OBJC_BEGIN_TRY_BLOCK_RETURN;

  MOZ_DIAGNOSTIC_ASSERT(
      nsIClipboard::IsClipboardTypeSupported(aWhichClipboard));

  if (kSelectionCache == aWhichClipboard) {
    ClearSelectionCache();
    return NS_OK;
  }

  if (NSPasteboard* cocoaPasteboard = GetPasteboard(aWhichClipboard)) {
    [cocoaPasteboard clearContents];
  }

  return NS_OK;

  NS_OBJC_END_TRY_BLOCK_RETURN(NS_ERROR_FAILURE);
}

mozilla::Result<int32_t, nsresult>
nsClipboard::GetNativeClipboardSequenceNumber(int32_t aWhichClipboard) {
  NS_OBJC_BEGIN_TRY_BLOCK_RETURN;

  MOZ_DIAGNOSTIC_ASSERT(
      nsIClipboard::IsClipboardTypeSupported(aWhichClipboard));

  if (kSelectionCache == aWhichClipboard) {
    return sSelectionCacheChangeCount;
  }

  NSPasteboard* cocoaPasteboard = GetPasteboard(aWhichClipboard);
  if (!cocoaPasteboard) {
    return mozilla::Err(NS_ERROR_FAILURE);
  }

  return [cocoaPasteboard changeCount];

  NS_OBJC_END_TRY_BLOCK_RETURN(mozilla::Err(NS_ERROR_FAILURE));
}
