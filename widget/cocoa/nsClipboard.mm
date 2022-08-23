/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <algorithm>

#include "mozilla/Logging.h"

#include "mozilla/Unused.h"

#include "gfxPlatform.h"
#include "nsArrayUtils.h"
#include "nsCOMPtr.h"
#include "nsClipboard.h"
#include "nsString.h"
#include "nsISupportsPrimitives.h"
#include "nsPrimitiveHelpers.h"
#include "nsMemory.h"
#include "nsIFile.h"
#include "nsStringStream.h"
#include "nsDragService.h"
#include "nsEscape.h"
#include "nsPrintfCString.h"
#include "nsObjCExceptions.h"
#include "imgIContainer.h"
#include "nsCocoaUtils.h"

using mozilla::gfx::DataSourceSurface;
using mozilla::gfx::SourceSurface;
using mozilla::LogLevel;

mozilla::StaticRefPtr<nsITransferable> nsClipboard::sSelectionCache;

@implementation UTIHelper

+ (NSString*)stringFromPboardType:(NSString*)aType {
  if ([aType isEqualToString:kMozWildcardPboardType] ||
      [aType isEqualToString:kMozCustomTypesPboardType] ||
      [aType isEqualToString:kPublicUrlPboardType] ||
      [aType isEqualToString:kPublicUrlNamePboardType] ||
      [aType isEqualToString:kMozFileUrlsPboardType] ||
      [aType isEqualToString:(NSString*)kPasteboardTypeFileURLPromise] ||
      [aType isEqualToString:(NSString*)kPasteboardTypeFilePromiseContent] ||
      [aType isEqualToString:(NSString*)kUTTypeFileURL] ||
      [aType isEqualToString:NSStringPboardType] ||
      [aType isEqualToString:NSPasteboardTypeString] ||
      [aType isEqualToString:NSPasteboardTypeHTML] || [aType isEqualToString:NSPasteboardTypeRTF] ||
      [aType isEqualToString:NSPasteboardTypeTIFF] || [aType isEqualToString:NSPasteboardTypePNG]) {
    return [NSString stringWithString:aType];
  }
  NSString* dynamicType = (NSString*)UTTypeCreatePreferredIdentifierForTag(
      kUTTagClassNSPboardType, (CFStringRef)aType, kUTTypeData);
  NSString* result = [NSString stringWithString:dynamicType];
  [dynamicType release];
  return result;
}

@end  // UTIHelper

nsClipboard::nsClipboard() : nsBaseClipboard(), mCachedClipboard(-1), mChangeCount(0) {}

nsClipboard::~nsClipboard() { ClearSelectionCache(); }

NS_IMPL_ISUPPORTS_INHERITED0(nsClipboard, nsBaseClipboard)

// We separate this into its own function because after an @try, all local
// variables within that function get marked as volatile, and our C++ type
// system doesn't like volatile things.
static NSData* GetDataFromPasteboard(NSPasteboard* aPasteboard, NSString* aType) {
  NSData* data = nil;
  @try {
    data = [aPasteboard dataForType:aType];
  } @catch (NSException* e) {
    NS_WARNING(
        nsPrintfCString("Exception raised while getting data from the pasteboard: \"%s - %s\"",
                        [[e name] UTF8String], [[e reason] UTF8String])
            .get());
    mozilla::Unused << e;
  }
  return data;
}

void nsClipboard::SetSelectionCache(nsITransferable* aTransferable) {
  sSelectionCache = aTransferable;
}

void nsClipboard::ClearSelectionCache() { sSelectionCache = nullptr; }

NS_IMETHODIMP
nsClipboard::SetNativeClipboardData(int32_t aWhichClipboard) {
  NS_OBJC_BEGIN_TRY_BLOCK_RETURN;

  if ((aWhichClipboard != kGlobalClipboard && aWhichClipboard != kFindClipboard) || !mTransferable)
    return NS_ERROR_FAILURE;

  NSDictionary* pasteboardOutputDict = PasteboardDictFromTransferable(mTransferable);
  if (!pasteboardOutputDict) return NS_ERROR_FAILURE;

  unsigned int outputCount = [pasteboardOutputDict count];
  NSArray* outputKeys = [pasteboardOutputDict allKeys];
  NSPasteboard* cocoaPasteboard;
  if (aWhichClipboard == kFindClipboard) {
    cocoaPasteboard = [NSPasteboard pasteboardWithName:NSFindPboard];
    NSString* stringType = [UTIHelper stringFromPboardType:NSPasteboardTypeString];
    [cocoaPasteboard declareTypes:[NSArray arrayWithObject:stringType] owner:nil];
  } else {
    // Write everything else out to the general pasteboard.
    cocoaPasteboard = [NSPasteboard generalPasteboard];
    [cocoaPasteboard declareTypes:outputKeys owner:nil];
  }

  for (unsigned int i = 0; i < outputCount; i++) {
    NSString* currentKey = [outputKeys objectAtIndex:i];
    id currentValue = [pasteboardOutputDict valueForKey:currentKey];
    if (aWhichClipboard == kFindClipboard) {
      if ([currentKey isEqualToString:[UTIHelper stringFromPboardType:NSPasteboardTypeString]]) {
        [cocoaPasteboard setString:currentValue forType:currentKey];
      }
    } else {
      if ([currentKey isEqualToString:[UTIHelper stringFromPboardType:NSPasteboardTypeString]] ||
          [currentKey isEqualToString:[UTIHelper stringFromPboardType:kPublicUrlPboardType]] ||
          [currentKey isEqualToString:[UTIHelper stringFromPboardType:kPublicUrlNamePboardType]]) {
        [cocoaPasteboard setString:currentValue forType:currentKey];
      } else if ([currentKey
                     isEqualToString:[UTIHelper stringFromPboardType:kUrlsWithTitlesPboardType]]) {
        [cocoaPasteboard setPropertyList:[pasteboardOutputDict valueForKey:currentKey]
                                 forType:currentKey];
      } else if ([currentKey
                     isEqualToString:[UTIHelper stringFromPboardType:NSPasteboardTypeHTML]]) {
        [cocoaPasteboard setString:(nsClipboard::WrapHtmlForSystemPasteboard(currentValue))
                           forType:currentKey];
      } else if ([currentKey
                     isEqualToString:[UTIHelper stringFromPboardType:kMozFileUrlsPboardType]]) {
        [cocoaPasteboard writeObjects:currentValue];
      } else {
        [cocoaPasteboard setData:currentValue forType:currentKey];
      }
    }
  }

  mCachedClipboard = aWhichClipboard;
  mChangeCount = [cocoaPasteboard changeCount];

  return NS_OK;

  NS_OBJC_END_TRY_BLOCK_RETURN(NS_ERROR_FAILURE);
}

nsresult nsClipboard::TransferableFromPasteboard(nsITransferable* aTransferable,
                                                 NSPasteboard* cocoaPasteboard) {
  NS_OBJC_BEGIN_TRY_BLOCK_RETURN;

  // get flavor list that includes all acceptable flavors (including ones obtained through
  // conversion)
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
      if ([pboardType isEqualToString:[UTIHelper stringFromPboardType:NSPasteboardTypeRTF]]) {
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

      // The DOM only wants LF, so convert from MacOS line endings to DOM line endings.
      int32_t signedDataLength = dataLength;
      nsLinebreakHelpers::ConvertPlatformToDOMLinebreaks(flavorStr, &clipboardDataPtr,
                                                         &signedDataLength);
      dataLength = signedDataLength;

      // skip BOM (Byte Order Mark to distinguish little or big endian)
      char16_t* clipboardDataPtrNoBOM = (char16_t*)clipboardDataPtr;
      if ((dataLength > 2) &&
          ((clipboardDataPtrNoBOM[0] == 0xFEFF) || (clipboardDataPtrNoBOM[0] == 0xFFFE))) {
        dataLength -= sizeof(char16_t);
        clipboardDataPtrNoBOM += 1;
      }

      nsCOMPtr<nsISupports> genericDataWrapper;
      nsPrimitiveHelpers::CreatePrimitiveForData(flavorStr, clipboardDataPtrNoBOM, dataLength,
                                                 getter_AddRefs(genericDataWrapper));
      aTransferable->SetTransferData(flavorStr.get(), genericDataWrapper);
      free(clipboardDataPtr);
      break;
    } else if (flavorStr.EqualsLiteral(kCustomTypesMime)) {
      NSString* type = [cocoaPasteboard
          availableTypeFromArray:
              [NSArray arrayWithObject:[UTIHelper stringFromPboardType:kMozCustomTypesPboardType]]];
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
      nsPrimitiveHelpers::CreatePrimitiveForData(flavorStr, clipboardDataPtr, dataLength,
                                                 getter_AddRefs(genericDataWrapper));

      aTransferable->SetTransferData(flavorStr.get(), genericDataWrapper);
      free(clipboardDataPtr);
    } else if (flavorStr.EqualsLiteral(kJPEGImageMime) || flavorStr.EqualsLiteral(kJPGImageMime) ||
               flavorStr.EqualsLiteral(kPNGImageMime) || flavorStr.EqualsLiteral(kGIFImageMime)) {
      // Figure out if there's data on the pasteboard we can grab (sanity check)
      NSString* type = [cocoaPasteboard
          availableTypeFromArray:
              [NSArray arrayWithObjects:[UTIHelper stringFromPboardType:(NSString*)kUTTypeFileURL],
                                        [UTIHelper stringFromPboardType:NSPasteboardTypeTIFF],
                                        [UTIHelper stringFromPboardType:NSPasteboardTypePNG], nil]];
      if (!type) continue;

      // Read data off the clipboard
      NSData* pasteboardData = GetDataFromPasteboard(cocoaPasteboard, type);
      if (!pasteboardData) continue;

      // Figure out what type we're converting to
      CFStringRef outputType = NULL;
      if (flavorStr.EqualsLiteral(kJPEGImageMime) || flavorStr.EqualsLiteral(kJPGImageMime))
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
          dictionaryWithObjectsAndKeys:(NSNumber*)kCFBooleanTrue, kCGImageSourceShouldAllowFloat,
                                       type, kCGImageSourceTypeIdentifierHint, nil];
      CGImageSourceRef source = nullptr;
      if (type == [UTIHelper stringFromPboardType:(NSString*)kUTTypeFileURL]) {
        NSString* urlStr = [cocoaPasteboard stringForType:type];
        NSURL* url = [NSURL URLWithString:urlStr];
        source = CGImageSourceCreateWithURL((CFURLRef)url, (CFDictionaryRef)options);
      } else {
        source = CGImageSourceCreateWithData((CFDataRef)pasteboardData, (CFDictionaryRef)options);
      }

      NSMutableData* encodedData = [NSMutableData data];
      CGImageDestinationRef dest =
          CGImageDestinationCreateWithData((CFMutableDataRef)encodedData, outputType, 1, NULL);
      CGImageDestinationAddImageFromSource(dest, source, 0, NULL);
      bool successfullyConverted = CGImageDestinationFinalize(dest);

      if (successfullyConverted) {
        // Put the converted data in a form Gecko can understand
        nsCOMPtr<nsIInputStream> byteStream;
        NS_NewByteInputStream(getter_AddRefs(byteStream),
                              mozilla::Span((const char*)[encodedData bytes], [encodedData length]),
                              NS_ASSIGNMENT_COPY);

        aTransferable->SetTransferData(flavorStr.get(), byteStream);
      }

      if (dest) CFRelease(dest);
      if (source) CFRelease(source);

      if (successfullyConverted)
        break;
      else
        continue;
    }
  }

  return NS_OK;

  NS_OBJC_END_TRY_BLOCK_RETURN(NS_ERROR_FAILURE);
}

NS_IMETHODIMP
nsClipboard::GetNativeClipboardData(nsITransferable* aTransferable, int32_t aWhichClipboard) {
  NS_OBJC_BEGIN_TRY_BLOCK_RETURN;

  if ((aWhichClipboard != kGlobalClipboard && aWhichClipboard != kFindClipboard) || !aTransferable)
    return NS_ERROR_FAILURE;

  NSPasteboard* cocoaPasteboard;
  if (aWhichClipboard == kFindClipboard) {
    cocoaPasteboard = [NSPasteboard pasteboardWithName:NSFindPboard];
  } else {
    cocoaPasteboard = [NSPasteboard generalPasteboard];
  }
  if (!cocoaPasteboard) return NS_ERROR_FAILURE;

  // get flavor list that includes all acceptable flavors (including ones obtained through
  // conversion)
  nsTArray<nsCString> flavors;
  nsresult rv = aTransferable->FlavorsTransferableCanImport(flavors);
  if (NS_FAILED(rv)) return NS_ERROR_FAILURE;

  // If we were the last ones to put something on the pasteboard, then just use the cached
  // transferable. Otherwise clear it because it isn't relevant any more.
  if (mCachedClipboard == aWhichClipboard && mChangeCount == [cocoaPasteboard changeCount]) {
    if (mTransferable) {
      for (uint32_t i = 0; i < flavors.Length(); i++) {
        nsCString& flavorStr = flavors[i];

        nsCOMPtr<nsISupports> dataSupports;
        rv = mTransferable->GetTransferData(flavorStr.get(), getter_AddRefs(dataSupports));
        if (NS_SUCCEEDED(rv)) {
          aTransferable->SetTransferData(flavorStr.get(), dataSupports);
          return NS_OK;  // maybe try to fill in more types? Is there a point?
        }
      }
    }
  } else {
    // Remove transferable cache only. Don't clear system clipboard.
    mEmptyingForSetData = true;
    EmptyClipboard(aWhichClipboard);
    mEmptyingForSetData = false;
  }

  // at this point we can't satisfy the request from cache data so let's look
  // for things other people put on the system clipboard

  return nsClipboard::TransferableFromPasteboard(aTransferable, cocoaPasteboard);

  NS_OBJC_END_TRY_BLOCK_RETURN(NS_ERROR_FAILURE);
}

// returns true if we have *any* of the passed in flavors available for pasting
NS_IMETHODIMP
nsClipboard::HasDataMatchingFlavors(const nsTArray<nsCString>& aFlavorList, int32_t aWhichClipboard,
                                    bool* outResult) {
  NS_OBJC_BEGIN_TRY_BLOCK_RETURN;

  CLIPBOARD_LOG("%s: clipboard=%i", __FUNCTION__, aWhichClipboard);
  CLIPBOARD_LOG("    Asking for content:\n");
  for (auto& flavor : aFlavorList) {
    CLIPBOARD_LOG("        MIME %s\n", flavor.get());
  }

  *outResult = false;

  if (aWhichClipboard != kGlobalClipboard) return NS_OK;

  // first see if we have data for this in our cached transferable
  if (mTransferable) {
    nsTArray<nsCString> flavors;
    nsresult rv = mTransferable->FlavorsTransferableCanImport(flavors);
    if (NS_SUCCEEDED(rv)) {
      if (CLIPBOARD_LOG_ENABLED()) {
        CLIPBOARD_LOG("    Cached transferable types (nums %zu)\n", flavors.Length());
        for (uint32_t j = 0; j < flavors.Length(); j++) {
          CLIPBOARD_LOG("        MIME %s\n", flavors[j].get());
        }
      }

      for (uint32_t j = 0; j < flavors.Length(); j++) {
        const nsCString& transferableFlavorStr = flavors[j];

        for (uint32_t k = 0; k < aFlavorList.Length(); k++) {
          if (transferableFlavorStr.Equals(aFlavorList[k])) {
            CLIPBOARD_LOG("    has %s\n", aFlavorList[k].get());
            *outResult = true;
            return NS_OK;
          }
        }
      }
    }
  }

  NSPasteboard* generalPBoard = [NSPasteboard generalPasteboard];

  if (CLIPBOARD_LOG_ENABLED()) {
    NSArray* types = [generalPBoard types];
    uint32_t count = [types count];
    CLIPBOARD_LOG("    Pasteboard types (nums %d)\n", count);
    for (uint32_t i = 0; i < count; i++) {
      NSPasteboardType type = [types objectAtIndex:i];
      if (!type) {
        CLIPBOARD_LOG("        failed to get MIME\n");
        continue;
      }
      CLIPBOARD_LOG("        MIME %s\n", [type UTF8String]);
    }
  }

  for (auto& mimeType : aFlavorList) {
    NSString* pboardType = nil;
    if (nsClipboard::IsStringType(mimeType, &pboardType)) {
      NSString* availableType =
          [generalPBoard availableTypeFromArray:[NSArray arrayWithObject:pboardType]];
      if (availableType && [availableType isEqualToString:pboardType]) {
        CLIPBOARD_LOG("    has %s\n", mimeType.get());
        *outResult = true;
        break;
      }
    } else if (mimeType.EqualsLiteral(kCustomTypesMime)) {
      NSString* availableType = [generalPBoard
          availableTypeFromArray:
              [NSArray arrayWithObject:[UTIHelper stringFromPboardType:kMozCustomTypesPboardType]]];
      if (availableType) {
        CLIPBOARD_LOG("    has %s\n", mimeType.get());
        *outResult = true;
        break;
      }
    } else if (mimeType.EqualsLiteral(kJPEGImageMime) || mimeType.EqualsLiteral(kJPGImageMime) ||
               mimeType.EqualsLiteral(kPNGImageMime) || mimeType.EqualsLiteral(kGIFImageMime)) {
      NSString* availableType = [generalPBoard
          availableTypeFromArray:
              [NSArray arrayWithObjects:[UTIHelper stringFromPboardType:NSPasteboardTypeTIFF],
                                        [UTIHelper stringFromPboardType:NSPasteboardTypePNG], nil]];
      if (availableType) {
        CLIPBOARD_LOG("    has %s\n", mimeType.get());
        *outResult = true;
        break;
      }
    }
  }

  if (CLIPBOARD_LOG_ENABLED() && !(*outResult)) {
    CLIPBOARD_LOG("    no targets at clipboard (bad match)\n");
  }

  return NS_OK;

  NS_OBJC_END_TRY_BLOCK_RETURN(NS_ERROR_FAILURE);
}

NS_IMETHODIMP
nsClipboard::SupportsFindClipboard(bool* _retval) {
  NS_ENSURE_ARG_POINTER(_retval);
  *_retval = true;
  return NS_OK;
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

// This function converts anything that other applications might understand into the system format
// and puts it into a dictionary which it returns.
// static
NSDictionary* nsClipboard::PasteboardDictFromTransferable(nsITransferable* aTransferable) {
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

  const mozilla::Maybe<uint32_t> imageFlavorIndex = nsClipboard::FindIndexOfImageFlavor(flavors);

  if (imageFlavorIndex) {
    // When right-clicking and "Copy Image" is clicked on macOS, some apps expect the
    // first flavor to be the image flavor. See bug 1689992. For other apps, the
    // order shouldn't matter.
    std::swap(*flavors.begin(), flavors[*imageFlavorIndex]);
  }
  for (uint32_t i = 0; i < flavors.Length(); i++) {
    nsCString& flavorStr = flavors[i];

    CLIPBOARD_LOG("writing out clipboard data of type %s (%d)\n", flavorStr.get(), i);

    NSString* pboardType = nil;
    if (nsClipboard::IsStringType(flavorStr, &pboardType)) {
      nsCOMPtr<nsISupports> genericDataWrapper;
      rv = aTransferable->GetTransferData(flavorStr.get(), getter_AddRefs(genericDataWrapper));
      if (NS_FAILED(rv)) {
        continue;
      }

      nsAutoString data;
      if (nsCOMPtr<nsISupportsString> text = do_QueryInterface(genericDataWrapper)) {
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
    } else if (flavorStr.EqualsLiteral(kCustomTypesMime)) {
      nsCOMPtr<nsISupports> genericDataWrapper;
      rv = aTransferable->GetTransferData(flavorStr.get(), getter_AddRefs(genericDataWrapper));
      if (NS_FAILED(rv)) {
        continue;
      }

      nsAutoCString data;
      if (nsCOMPtr<nsISupportsCString> text = do_QueryInterface(genericDataWrapper)) {
        text->GetData(data);
      }

      if (!data.IsEmpty()) {
        NSData* nativeData = [NSData dataWithBytes:data.get() length:data.Length()];
        NSString* customType = [UTIHelper stringFromPboardType:kMozCustomTypesPboardType];
        if (!nativeData) {
          continue;
        }
        [pasteboardOutputDict setObject:nativeData forKey:customType];
      }
    } else if (nsClipboard::IsImageType(flavorStr)) {
      nsCOMPtr<nsISupports> transferSupports;
      rv = aTransferable->GetTransferData(flavorStr.get(), getter_AddRefs(transferSupports));
      if (NS_FAILED(rv)) {
        continue;
      }

      nsCOMPtr<imgIContainer> image(do_QueryInterface(transferSupports));
      if (!image) {
        NS_WARNING("Image isn't an imgIContainer in transferable");
        continue;
      }

      RefPtr<SourceSurface> surface =
          image->GetFrame(imgIContainer::FRAME_CURRENT,
                          imgIContainer::FLAG_SYNC_DECODE | imgIContainer::FLAG_ASYNC_NOTIFY);
      if (!surface) {
        continue;
      }
      CGImageRef imageRef = NULL;
      rv = nsCocoaUtils::CreateCGImageFromSurface(surface, &imageRef);
      if (NS_FAILED(rv) || !imageRef) {
        continue;
      }

      // Convert the CGImageRef to TIFF data.
      CFMutableDataRef tiffData = CFDataCreateMutable(kCFAllocatorDefault, 0);
      CGImageDestinationRef destRef =
          CGImageDestinationCreateWithData(tiffData, CFSTR("public.tiff"), 1, NULL);
      CGImageDestinationAddImage(destRef, imageRef, NULL);
      bool successfullyConverted = CGImageDestinationFinalize(destRef);

      CGImageRelease(imageRef);
      if (destRef) {
        CFRelease(destRef);
      }

      if (!successfullyConverted || !tiffData) {
        if (tiffData) {
          CFRelease(tiffData);
        }
        continue;
      }

      NSString* tiffType = [UTIHelper stringFromPboardType:NSPasteboardTypeTIFF];
      [pasteboardOutputDict setObject:(NSMutableData*)tiffData forKey:tiffType];
      CFRelease(tiffData);
    } else if (flavorStr.EqualsLiteral(kFileMime)) {
      nsCOMPtr<nsISupports> genericFile;
      rv = aTransferable->GetTransferData(flavorStr.get(), getter_AddRefs(genericFile));
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
      NSString* fileUTType = [UTIHelper stringFromPboardType:(NSString*)kUTTypeFileURL];
      if (!url || ![url absoluteString]) {
        continue;
      }
      [pasteboardOutputDict setObject:[url absoluteString] forKey:fileUTType];
    } else if (flavorStr.EqualsLiteral(kFilePromiseMime)) {
      NSString* urlPromise =
          [UTIHelper stringFromPboardType:(NSString*)kPasteboardTypeFileURLPromise];
      NSString* urlPromiseContent =
          [UTIHelper stringFromPboardType:(NSString*)kPasteboardTypeFilePromiseContent];
      [pasteboardOutputDict setObject:[NSArray arrayWithObject:@""] forKey:urlPromise];
      [pasteboardOutputDict setObject:[NSArray arrayWithObject:@""] forKey:urlPromiseContent];
    } else if (flavorStr.EqualsLiteral(kURLMime)) {
      nsCOMPtr<nsISupports> genericURL;
      rv = aTransferable->GetTransferData(flavorStr.get(), getter_AddRefs(genericURL));
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
        urlTitle.Mid(urlTitle, newlinePos + 1, urlTitle.Length() - (newlinePos + 1));

        nativeTitle =
            [NSString stringWithCharacters:reinterpret_cast<const unichar*>(urlTitle.get())
                                    length:urlTitle.Length()];
      }
      // The Finder doesn't like getting random binary data aka
      // Unicode, so change it into an escaped URL containing only
      // ASCII.
      nsAutoCString utf8Data = NS_ConvertUTF16toUTF8(url.get(), url.Length());
      nsAutoCString escData;
      NS_EscapeURL(utf8Data.get(), utf8Data.Length(), esc_OnlyNonASCII | esc_AlwaysCopy, escData);

      NSString* nativeURL = [NSString stringWithUTF8String:escData.get()];
      NSString* publicUrl = [UTIHelper stringFromPboardType:kPublicUrlPboardType];
      if (!nativeURL) {
        continue;
      }
      [pasteboardOutputDict setObject:nativeURL forKey:publicUrl];
      if (nativeTitle) {
        NSArray* urlsAndTitles = @[ @[ nativeURL ], @[ nativeTitle ] ];
        NSString* urlName = [UTIHelper stringFromPboardType:kPublicUrlNamePboardType];
        NSString* urlsWithTitles = [UTIHelper stringFromPboardType:kUrlsWithTitlesPboardType];
        [pasteboardOutputDict setObject:nativeTitle forKey:urlName];
        [pasteboardOutputDict setObject:urlsAndTitles forKey:urlsWithTitles];
      }
    }
    // If it wasn't a type that we recognize as exportable we don't put it on the system
    // clipboard. We'll just access it from our cached transferable when we need it.
  }

  return pasteboardOutputDict;

  NS_OBJC_END_TRY_BLOCK_RETURN(nil);
}

bool nsClipboard::IsStringType(const nsCString& aMIMEType, NSString** aPboardType) {
  if (aMIMEType.EqualsLiteral(kUnicodeMime)) {
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
  return aMIMEType.EqualsLiteral(kPNGImageMime) || aMIMEType.EqualsLiteral(kJPEGImageMime) ||
         aMIMEType.EqualsLiteral(kJPGImageMime) || aMIMEType.EqualsLiteral(kGIFImageMime) ||
         aMIMEType.EqualsLiteral(kNativeImageMime);
}

NSString* nsClipboard::WrapHtmlForSystemPasteboard(NSString* aString) {
  NSString* wrapped = [NSString
      stringWithFormat:@"<html>"
                        "<head>"
                        "<meta http-equiv=\"content-type\" content=\"text/html; charset=utf-8\">"
                        "</head>"
                        "<body>"
                        "%@"
                        "</body>"
                        "</html>",
                       aString];
  return wrapped;
}

/**
 * Sets the transferable object
 *
 */
NS_IMETHODIMP
nsClipboard::SetData(nsITransferable* aTransferable, nsIClipboardOwner* anOwner,
                     int32_t aWhichClipboard) {
  NS_ASSERTION(aTransferable, "clipboard given a null transferable");

  if (aWhichClipboard == kSelectionCache) {
    if (aTransferable) {
      SetSelectionCache(aTransferable);
      return NS_OK;
    }
    return NS_ERROR_FAILURE;
  }

  return nsBaseClipboard::SetData(aTransferable, anOwner, aWhichClipboard);
}

NS_IMETHODIMP
nsClipboard::EmptyClipboard(int32_t aWhichClipboard) {
  NS_OBJC_BEGIN_TRY_BLOCK_RETURN;

  if (aWhichClipboard == kSelectionCache) {
    ClearSelectionCache();
    return NS_OK;
  }

  if (!mEmptyingForSetData) {
    NSPasteboard* cocoaPasteboard = nullptr;
    if (aWhichClipboard == kFindClipboard) {
      cocoaPasteboard = [NSPasteboard pasteboardWithName:NSFindPboard];
    } else if (aWhichClipboard == kGlobalClipboard) {
      cocoaPasteboard = [NSPasteboard generalPasteboard];
    }
    if (cocoaPasteboard) {
      [cocoaPasteboard clearContents];
      mChangeCount = [cocoaPasteboard changeCount];
    }
  }

  return nsBaseClipboard::EmptyClipboard(aWhichClipboard);

  NS_OBJC_END_TRY_BLOCK_RETURN(NS_ERROR_FAILURE);
}
