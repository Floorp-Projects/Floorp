/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Logging.h"

#include "mozilla/Unused.h"

#include "gfxPlatform.h"
#include "nsArrayUtils.h"
#include "nsCOMPtr.h"
#include "nsClipboard.h"
#include "nsString.h"
#include "nsISupportsPrimitives.h"
#include "nsXPIDLString.h"
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

#define IMAGE_PASTEBOARD_TYPES NSPasteboardTypeTIFF, \
                               NSPasteboardTypePNG, \
                               nil

extern PRLogModuleInfo* sCocoaLog;

extern void EnsureLogInitialized();

mozilla::StaticRefPtr<nsITransferable> nsClipboard::sSelectionCache;

nsClipboard::nsClipboard()
  : mCachedClipboard(-1)
  , mChangeCount(0)
  , mIgnoreEmptyNotification(false)
{
  EnsureLogInitialized();
}

nsClipboard::~nsClipboard()
{
  EmptyClipboard(kGlobalClipboard);
  EmptyClipboard(kFindClipboard);
  ClearSelectionCache();
}

NS_IMPL_ISUPPORTS(nsClipboard, nsIClipboard)

// We separate this into its own function because after an @try, all local
// variables within that function get marked as volatile, and our C++ type
// system doesn't like volatile things.
static NSData*
GetDataFromPasteboard(NSPasteboard* aPasteboard, NSString* aType)
{
  NSData *data = nil;
  @try {
    data = [aPasteboard dataForType:aType];
  } @catch (NSException* e) {
    NS_WARNING(nsPrintfCString("Exception raised while getting data from the pasteboard: \"%s - %s\"",
                               [[e name] UTF8String], [[e reason] UTF8String]).get());
    mozilla::Unused << e;
  }
  return data;
}

void
nsClipboard::SetSelectionCache(nsITransferable *aTransferable)
{
  sSelectionCache = aTransferable;
}

void
nsClipboard::ClearSelectionCache()
{
  sSelectionCache = nullptr;
}

NS_IMETHODIMP
nsClipboard::SetNativeClipboardData(int32_t aWhichClipboard)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  if ((aWhichClipboard != kGlobalClipboard && aWhichClipboard != kFindClipboard) || !mTransferable)
    return NS_ERROR_FAILURE;

  mIgnoreEmptyNotification = true;

  NSDictionary* pasteboardOutputDict = PasteboardDictFromTransferable(mTransferable);
  if (!pasteboardOutputDict)
    return NS_ERROR_FAILURE;

  unsigned int outputCount = [pasteboardOutputDict count];
  NSArray* outputKeys = [pasteboardOutputDict allKeys];
  NSPasteboard* cocoaPasteboard;
  if (aWhichClipboard == kFindClipboard) {
    cocoaPasteboard = [NSPasteboard pasteboardWithName:NSFindPboard];
    [cocoaPasteboard declareTypes:
      [NSArray arrayWithObject:NSPasteboardTypeString] owner:nil];
  } else {
    // Write everything else out to the general pasteboard.
    cocoaPasteboard = [NSPasteboard generalPasteboard];
    [cocoaPasteboard declareTypes:outputKeys owner:nil];
  }

  for (unsigned int i = 0; i < outputCount; i++) {
    NSString* currentKey = [outputKeys objectAtIndex:i];
    id currentValue = [pasteboardOutputDict valueForKey:currentKey];
    if (aWhichClipboard == kFindClipboard) {
      if ([currentKey isEqualToString:NSPasteboardTypeString])
        [cocoaPasteboard setString:currentValue forType:currentKey];
    } else {
      if ([currentKey isEqualToString:NSPasteboardTypeString] ||
          [currentKey isEqualToString:kCorePboardType_url] ||
          [currentKey isEqualToString:kCorePboardType_urld] ||
          [currentKey isEqualToString:kCorePboardType_urln]) {
        [cocoaPasteboard setString:currentValue forType:currentKey];
      } else if ([currentKey isEqualToString:NSPasteboardTypeHTML]) {
        [cocoaPasteboard setString:(nsClipboard::WrapHtmlForSystemPasteboard(currentValue))
                         forType:currentKey];
      } else {
        [cocoaPasteboard setData:currentValue forType:currentKey];
      }
    }
  }

  mCachedClipboard = aWhichClipboard;
  mChangeCount = [cocoaPasteboard changeCount];

  mIgnoreEmptyNotification = false;

  return NS_OK;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

nsresult
nsClipboard::TransferableFromPasteboard(nsITransferable *aTransferable, NSPasteboard *cocoaPasteboard)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  // get flavor list that includes all acceptable flavors (including ones obtained through conversion)
  nsCOMPtr<nsIArray> flavorList;
  nsresult rv = aTransferable->FlavorsTransferableCanImport(getter_AddRefs(flavorList));
  if (NS_FAILED(rv))
    return NS_ERROR_FAILURE;

  uint32_t flavorCount;
  flavorList->GetLength(&flavorCount);

  for (uint32_t i = 0; i < flavorCount; i++) {
    nsCOMPtr<nsISupportsCString> currentFlavor = do_QueryElementAt(flavorList, i);
    if (!currentFlavor)
      continue;

    nsXPIDLCString flavorStr;
    currentFlavor->ToString(getter_Copies(flavorStr)); // i has a flavr

    // printf("looking for clipboard data of type %s\n", flavorStr.get());

    NSString *pboardType = nil;
    if (nsClipboard::IsStringType(flavorStr, &pboardType)) {
      NSString* pString = [cocoaPasteboard stringForType:pboardType];
      if (!pString)
        continue;

      NSData* stringData;
      if ([pboardType isEqualToString:NSPasteboardTypeRTF]) {
        stringData = [pString dataUsingEncoding:NSASCIIStringEncoding];
      } else {
        stringData = [pString dataUsingEncoding:NSUnicodeStringEncoding];
      }
      unsigned int dataLength = [stringData length];
      void* clipboardDataPtr = malloc(dataLength);
      if (!clipboardDataPtr)
        return NS_ERROR_OUT_OF_MEMORY;
      [stringData getBytes:clipboardDataPtr];

      // The DOM only wants LF, so convert from MacOS line endings to DOM line endings.
      int32_t signedDataLength = dataLength;
      nsLinebreakHelpers::ConvertPlatformToDOMLinebreaks(flavorStr, &clipboardDataPtr, &signedDataLength);
      dataLength = signedDataLength;

      // skip BOM (Byte Order Mark to distinguish little or big endian)      
      char16_t* clipboardDataPtrNoBOM = (char16_t*)clipboardDataPtr;
      if ((dataLength > 2) &&
          ((clipboardDataPtrNoBOM[0] == 0xFEFF) ||
           (clipboardDataPtrNoBOM[0] == 0xFFFE))) {
        dataLength -= sizeof(char16_t);
        clipboardDataPtrNoBOM += 1;
      }

      nsCOMPtr<nsISupports> genericDataWrapper;
      nsPrimitiveHelpers::CreatePrimitiveForData(flavorStr, clipboardDataPtrNoBOM, dataLength,
                                                 getter_AddRefs(genericDataWrapper));
      aTransferable->SetTransferData(flavorStr, genericDataWrapper, dataLength);
      free(clipboardDataPtr);
      break;
    }
    else if (flavorStr.EqualsLiteral(kCustomTypesMime)) {
      NSString* type = [cocoaPasteboard availableTypeFromArray:[NSArray arrayWithObject:kCustomTypesPboardType]];
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
      [pasteboardData getBytes:clipboardDataPtr];

      nsCOMPtr<nsISupports> genericDataWrapper;
      nsPrimitiveHelpers::CreatePrimitiveForData(flavorStr, clipboardDataPtr, dataLength,
                                                 getter_AddRefs(genericDataWrapper));

      aTransferable->SetTransferData(flavorStr, genericDataWrapper, dataLength);
      free(clipboardDataPtr);
    }
    else if (flavorStr.EqualsLiteral(kJPEGImageMime) ||
             flavorStr.EqualsLiteral(kJPGImageMime) ||
             flavorStr.EqualsLiteral(kPNGImageMime) ||
             flavorStr.EqualsLiteral(kGIFImageMime)) {
      // Figure out if there's data on the pasteboard we can grab (sanity check)
      NSString *type = [cocoaPasteboard availableTypeFromArray:[NSArray arrayWithObjects:IMAGE_PASTEBOARD_TYPES]];
      if (!type)
        continue;

      // Read data off the clipboard
      NSData *pasteboardData = GetDataFromPasteboard(cocoaPasteboard, type);
      if (!pasteboardData)
        continue;

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
      NSDictionary *options = [NSDictionary dictionaryWithObjectsAndKeys:
                                (NSNumber*)kCFBooleanTrue,
                                kCGImageSourceShouldAllowFloat,
                                (type == NSPasteboardTypeTIFF ? @"public.tiff" :
                                                                @"public.png"),
                                kCGImageSourceTypeIdentifierHint, nil];

      CGImageSourceRef source = CGImageSourceCreateWithData((CFDataRef)pasteboardData, 
                                                            (CFDictionaryRef)options);
      NSMutableData *encodedData = [NSMutableData data];
      CGImageDestinationRef dest = CGImageDestinationCreateWithData((CFMutableDataRef)encodedData,
                                                                    outputType,
                                                                    1, NULL);
      CGImageDestinationAddImageFromSource(dest, source, 0, NULL);
      bool successfullyConverted = CGImageDestinationFinalize(dest);

      if (successfullyConverted) {
        // Put the converted data in a form Gecko can understand
        nsCOMPtr<nsIInputStream> byteStream;
        NS_NewByteInputStream(getter_AddRefs(byteStream), (const char*)[encodedData bytes],
                                   [encodedData length], NS_ASSIGNMENT_COPY);
  
        aTransferable->SetTransferData(flavorStr, byteStream, sizeof(nsIInputStream*));
      }

      if (dest)
        CFRelease(dest);
      if (source)
        CFRelease(source);
      
      if (successfullyConverted)
        break;
      else
        continue;
    }
  }

  return NS_OK;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

NS_IMETHODIMP
nsClipboard::GetNativeClipboardData(nsITransferable* aTransferable, int32_t aWhichClipboard)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  if ((aWhichClipboard != kGlobalClipboard && aWhichClipboard != kFindClipboard) || !aTransferable)
    return NS_ERROR_FAILURE;

  NSPasteboard* cocoaPasteboard;
  if (aWhichClipboard == kFindClipboard) {
    cocoaPasteboard = [NSPasteboard pasteboardWithName:NSFindPboard];
  } else {
    cocoaPasteboard = [NSPasteboard generalPasteboard];
  }
  if (!cocoaPasteboard)
    return NS_ERROR_FAILURE;

  // get flavor list that includes all acceptable flavors (including ones obtained through conversion)
  nsCOMPtr<nsIArray> flavorList;
  nsresult rv = aTransferable->FlavorsTransferableCanImport(getter_AddRefs(flavorList));
  if (NS_FAILED(rv))
    return NS_ERROR_FAILURE;

  uint32_t flavorCount;
  flavorList->GetLength(&flavorCount);

  // If we were the last ones to put something on the pasteboard, then just use the cached
  // transferable. Otherwise clear it because it isn't relevant any more.
  if (mCachedClipboard == aWhichClipboard &&
      mChangeCount == [cocoaPasteboard changeCount]) {
    if (mTransferable) {
      for (uint32_t i = 0; i < flavorCount; i++) {
        nsCOMPtr<nsISupportsCString> currentFlavor = do_QueryElementAt(flavorList, i);
        if (!currentFlavor)
          continue;

        nsXPIDLCString flavorStr;
        currentFlavor->ToString(getter_Copies(flavorStr));

        nsCOMPtr<nsISupports> dataSupports;
        uint32_t dataSize = 0;
        rv = mTransferable->GetTransferData(flavorStr, getter_AddRefs(dataSupports), &dataSize);
        if (NS_SUCCEEDED(rv)) {
          aTransferable->SetTransferData(flavorStr, dataSupports, dataSize);
          return NS_OK; // maybe try to fill in more types? Is there a point?
        }
      }
    }
  } else {
    EmptyClipboard(aWhichClipboard);
  }

  // at this point we can't satisfy the request from cache data so let's look
  // for things other people put on the system clipboard

  return nsClipboard::TransferableFromPasteboard(aTransferable, cocoaPasteboard);

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

// returns true if we have *any* of the passed in flavors available for pasting
NS_IMETHODIMP
nsClipboard::HasDataMatchingFlavors(const char** aFlavorList, uint32_t aLength,
                                    int32_t aWhichClipboard, bool* outResult)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  *outResult = false;

  if ((aWhichClipboard != kGlobalClipboard) || !aFlavorList)
    return NS_OK;

  // first see if we have data for this in our cached transferable
  if (mTransferable) {
    nsCOMPtr<nsIArray> transferableFlavorList;
    nsresult rv = mTransferable->FlavorsTransferableCanImport(getter_AddRefs(transferableFlavorList));
    if (NS_SUCCEEDED(rv)) {
      uint32_t transferableFlavorCount;
      transferableFlavorList->GetLength(&transferableFlavorCount);
      for (uint32_t j = 0; j < transferableFlavorCount; j++) {
        nsCOMPtr<nsISupportsCString> currentTransferableFlavor =
            do_QueryElementAt(transferableFlavorList, j);
        if (!currentTransferableFlavor)
          continue;
        nsXPIDLCString transferableFlavorStr;
        currentTransferableFlavor->ToString(getter_Copies(transferableFlavorStr));

        for (uint32_t k = 0; k < aLength; k++) {
          if (transferableFlavorStr.Equals(aFlavorList[k])) {
            *outResult = true;
            return NS_OK;
          }
        }
      }      
    }    
  }

  NSPasteboard* generalPBoard = [NSPasteboard generalPasteboard];

  for (uint32_t i = 0; i < aLength; i++) {
    nsDependentCString mimeType(aFlavorList[i]);
    NSString *pboardType = nil;

    if (nsClipboard::IsStringType(mimeType, &pboardType)) {
      NSString* availableType = [generalPBoard availableTypeFromArray:[NSArray arrayWithObject:pboardType]];
      if (availableType && [availableType isEqualToString:pboardType]) {
        *outResult = true;
        break;
      }
    } else if (!strcmp(aFlavorList[i], kCustomTypesMime)) {
      NSString* availableType = [generalPBoard availableTypeFromArray:[NSArray arrayWithObject:kCustomTypesPboardType]];
      if (availableType) {
        *outResult = true;
        break;
      }
    } else if (!strcmp(aFlavorList[i], kJPEGImageMime) ||
               !strcmp(aFlavorList[i], kJPGImageMime) ||
               !strcmp(aFlavorList[i], kPNGImageMime) ||
               !strcmp(aFlavorList[i], kGIFImageMime)) {
      NSString* availableType = [generalPBoard availableTypeFromArray:
                                  [NSArray arrayWithObjects:IMAGE_PASTEBOARD_TYPES]];
      if (availableType) {
        *outResult = true;
        break;
      }
    }
  }

  return NS_OK;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

NS_IMETHODIMP
nsClipboard::SupportsFindClipboard(bool *_retval)
{
  NS_ENSURE_ARG_POINTER(_retval);
  *_retval = true;
  return NS_OK;
}

// This function converts anything that other applications might understand into the system format
// and puts it into a dictionary which it returns.
// static
NSDictionary* 
nsClipboard::PasteboardDictFromTransferable(nsITransferable* aTransferable)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NIL;

  if (!aTransferable)
    return nil;

  NSMutableDictionary* pasteboardOutputDict = [NSMutableDictionary dictionary];

  nsCOMPtr<nsIArray> flavorList;
  nsresult rv = aTransferable->FlavorsTransferableCanExport(getter_AddRefs(flavorList));
  if (NS_FAILED(rv))
    return nil;

  uint32_t flavorCount;
  flavorList->GetLength(&flavorCount);
  for (uint32_t i = 0; i < flavorCount; i++) {
    nsCOMPtr<nsISupportsCString> currentFlavor = do_QueryElementAt(flavorList, i);
    if (!currentFlavor)
      continue;

    nsXPIDLCString flavorStr;
    currentFlavor->ToString(getter_Copies(flavorStr));

    MOZ_LOG(sCocoaLog, LogLevel::Info, ("writing out clipboard data of type %s (%d)\n", flavorStr.get(), i));

    NSString *pboardType = nil;

    if (nsClipboard::IsStringType(flavorStr, &pboardType)) {
      void* data = nullptr;
      uint32_t dataSize = 0;
      nsCOMPtr<nsISupports> genericDataWrapper;
      rv = aTransferable->GetTransferData(flavorStr, getter_AddRefs(genericDataWrapper), &dataSize);
      nsPrimitiveHelpers::CreateDataFromPrimitive(flavorStr, genericDataWrapper, &data, dataSize);

      NSString* nativeString;
      if (data)
        nativeString = [NSString stringWithCharacters:(const unichar*)data length:(dataSize / sizeof(char16_t))];
      else
        nativeString = [NSString string];
      
      // be nice to Carbon apps, normalize the receiver's contents using Form C.
      nativeString = [nativeString precomposedStringWithCanonicalMapping];

      [pasteboardOutputDict setObject:nativeString forKey:pboardType];
      
      free(data);
    }
    else if (flavorStr.EqualsLiteral(kCustomTypesMime)) {
      void* data = nullptr;
      uint32_t dataSize = 0;
      nsCOMPtr<nsISupports> genericDataWrapper;
      rv = aTransferable->GetTransferData(flavorStr, getter_AddRefs(genericDataWrapper), &dataSize);
      nsPrimitiveHelpers::CreateDataFromPrimitive(flavorStr, genericDataWrapper, &data, dataSize);

      if (data) {
        NSData* nativeData = [NSData dataWithBytes:data length:dataSize];

        [pasteboardOutputDict setObject:nativeData forKey:kCustomTypesPboardType];
        free(data);
      }
    }
    else if (flavorStr.EqualsLiteral(kPNGImageMime) || flavorStr.EqualsLiteral(kJPEGImageMime) ||
             flavorStr.EqualsLiteral(kJPGImageMime) || flavorStr.EqualsLiteral(kGIFImageMime) ||
             flavorStr.EqualsLiteral(kNativeImageMime)) {
      uint32_t dataSize = 0;
      nsCOMPtr<nsISupports> transferSupports;
      aTransferable->GetTransferData(flavorStr, getter_AddRefs(transferSupports), &dataSize);
      nsCOMPtr<nsISupportsInterfacePointer> ptrPrimitive(do_QueryInterface(transferSupports));
      if (!ptrPrimitive)
        continue;

      nsCOMPtr<nsISupports> primitiveData;
      ptrPrimitive->GetData(getter_AddRefs(primitiveData));

      nsCOMPtr<imgIContainer> image(do_QueryInterface(primitiveData));
      if (!image) {
        NS_WARNING("Image isn't an imgIContainer in transferable");
        continue;
      }

      RefPtr<SourceSurface> surface =
        image->GetFrame(imgIContainer::FRAME_CURRENT,
                        imgIContainer::FLAG_SYNC_DECODE);
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
      CGImageDestinationRef destRef = CGImageDestinationCreateWithData(tiffData,
                                                                       CFSTR("public.tiff"),
                                                                       1,
                                                                       NULL);
      CGImageDestinationAddImage(destRef, imageRef, NULL);
      bool successfullyConverted = CGImageDestinationFinalize(destRef);

      CGImageRelease(imageRef);
      if (destRef)
        CFRelease(destRef);

      if (!successfullyConverted) {
        if (tiffData)
          CFRelease(tiffData);
        continue;
      }

      [pasteboardOutputDict setObject:(NSMutableData*)tiffData
                               forKey:NSPasteboardTypeTIFF];
      if (tiffData)
        CFRelease(tiffData);
    }
    else if (flavorStr.EqualsLiteral(kFileMime)) {
      uint32_t len = 0;
      nsCOMPtr<nsISupports> genericFile;
      rv = aTransferable->GetTransferData(flavorStr, getter_AddRefs(genericFile), &len);
      if (NS_FAILED(rv)) {
        continue;
      }

      nsCOMPtr<nsIFile> file(do_QueryInterface(genericFile));
      if (!file) {
        nsCOMPtr<nsISupportsInterfacePointer> ptr(do_QueryInterface(genericFile));

        if (ptr) {
          ptr->GetData(getter_AddRefs(genericFile));
          file = do_QueryInterface(genericFile);
        }
      }

      if (!file) {
        continue;
      }

      nsAutoString fileURI;
      rv = file->GetPath(fileURI);
      if (NS_FAILED(rv)) {
        continue;
      }

      NSString* str = nsCocoaUtils::ToNSString(fileURI);
      NSArray* fileList = [NSArray arrayWithObjects:str, nil];
      [pasteboardOutputDict setObject:fileList forKey:NSFilenamesPboardType];
    }
    else if (flavorStr.EqualsLiteral(kFilePromiseMime)) {
      [pasteboardOutputDict setObject:[NSArray arrayWithObject:@""]
                               forKey:(NSString*)kPasteboardTypeFileURLPromise];
    }
    else if (flavorStr.EqualsLiteral(kURLMime)) {
      uint32_t len = 0;
      nsCOMPtr<nsISupports> genericURL;
      rv = aTransferable->GetTransferData(flavorStr, getter_AddRefs(genericURL), &len);
      nsCOMPtr<nsISupportsString> urlObject(do_QueryInterface(genericURL));

      nsAutoString url;
      urlObject->GetData(url);

      // A newline embedded in the URL means that the form is actually URL + title.
      int32_t newlinePos = url.FindChar(char16_t('\n'));
      if (newlinePos >= 0) {
        url.Truncate(newlinePos);

        nsAutoString urlTitle;
        urlObject->GetData(urlTitle);
        urlTitle.Mid(urlTitle, newlinePos + 1, len - (newlinePos + 1));

        NSString *nativeTitle = [[NSString alloc] initWithCharacters:reinterpret_cast<const unichar*>(urlTitle.get())
                                                              length:urlTitle.Length()];
        // be nice to Carbon apps, normalize the receiver's contents using Form C.
        [pasteboardOutputDict setObject:[nativeTitle precomposedStringWithCanonicalMapping] forKey:kCorePboardType_urln];
        // Also put the title out as 'urld', since some recipients will look for that.
        [pasteboardOutputDict setObject:[nativeTitle precomposedStringWithCanonicalMapping] forKey:kCorePboardType_urld];
        [nativeTitle release];
      }

      // The Finder doesn't like getting random binary data aka
      // Unicode, so change it into an escaped URL containing only
      // ASCII.
      nsAutoCString utf8Data = NS_ConvertUTF16toUTF8(url.get(), url.Length());
      nsAutoCString escData;
      NS_EscapeURL(utf8Data.get(), utf8Data.Length(), esc_OnlyNonASCII|esc_AlwaysCopy, escData);

      // printf("Escaped url is %s, length %d\n", escData.get(), escData.Length());

      NSString *nativeURL = [NSString stringWithUTF8String:escData.get()];
      [pasteboardOutputDict setObject:nativeURL forKey:kCorePboardType_url];
    }
    // If it wasn't a type that we recognize as exportable we don't put it on the system
    // clipboard. We'll just access it from our cached transferable when we need it.
  }

  return pasteboardOutputDict;

  NS_OBJC_END_TRY_ABORT_BLOCK_NIL;
}

bool nsClipboard::IsStringType(const nsCString& aMIMEType, NSString** aPasteboardType)
{
  if (aMIMEType.EqualsLiteral(kUnicodeMime)) {
    *aPasteboardType = NSPasteboardTypeString;
    return true;
  } else if (aMIMEType.EqualsLiteral(kRTFMime)) {
    *aPasteboardType = NSPasteboardTypeRTF;
    return true;
  } else if (aMIMEType.EqualsLiteral(kHTMLMime)) {
    *aPasteboardType = NSPasteboardTypeHTML;
    return true;
  } else {
    return false;
  }
}

NSString* nsClipboard::WrapHtmlForSystemPasteboard(NSString* aString)
{
  NSString* wrapped =
    [NSString stringWithFormat:
      @"<html>"
         "<head>"
           "<meta http-equiv=\"content-type\" content=\"text/html; charset=utf-8\">"
         "</head>"
         "<body>"
           "%@"
         "</body>"
       "</html>", aString];
  return wrapped;
}

/**
  * Sets the transferable object
  *
  */
NS_IMETHODIMP
nsClipboard::SetData(nsITransferable* aTransferable, nsIClipboardOwner* anOwner,
                     int32_t aWhichClipboard)
{
  NS_ASSERTION (aTransferable, "clipboard given a null transferable");

  if (aWhichClipboard == kSelectionCache) {
    if (aTransferable) {
      SetSelectionCache(aTransferable);
      return NS_OK;
    }
    return NS_ERROR_FAILURE;
  }

  if (aTransferable == mTransferable && anOwner == mClipboardOwner) {
    return NS_OK;
  }
  bool selectClipPresent;
  SupportsSelectionClipboard(&selectClipPresent);
  bool findClipPresent;
  SupportsFindClipboard(&findClipPresent);
  if (!selectClipPresent && !findClipPresent && aWhichClipboard != kGlobalClipboard) {
    return NS_ERROR_FAILURE;
  }

  EmptyClipboard(aWhichClipboard);

  mClipboardOwner = anOwner;
  mTransferable = aTransferable;

  nsresult rv = NS_ERROR_FAILURE;
  if (mTransferable) {
    rv = SetNativeClipboardData(aWhichClipboard);
  }

  return rv;
}

/**
  * Gets the transferable object
  *
  */
NS_IMETHODIMP
nsClipboard::GetData(nsITransferable* aTransferable, int32_t aWhichClipboard)
{
  NS_ASSERTION (aTransferable, "clipboard given a null transferable");

  bool selectClipPresent;
  SupportsSelectionClipboard(&selectClipPresent);
  bool findClipPresent;
  SupportsFindClipboard(&findClipPresent);
  if (!selectClipPresent && !findClipPresent && aWhichClipboard != kGlobalClipboard)
    return NS_ERROR_FAILURE;

  if (aTransferable) {
    return GetNativeClipboardData(aTransferable, aWhichClipboard);
  }

  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsClipboard::EmptyClipboard(int32_t aWhichClipboard)
{
  if (aWhichClipboard == kSelectionCache) {
    ClearSelectionCache();
    return NS_OK;
  }

  bool selectClipPresent;
  SupportsSelectionClipboard(&selectClipPresent);
  bool findClipPresent;
  SupportsFindClipboard(&findClipPresent);
  if (!selectClipPresent && !findClipPresent && aWhichClipboard != kGlobalClipboard) {
    return NS_ERROR_FAILURE;
  }

  if (mIgnoreEmptyNotification) {
    return NS_OK;
  }

  if (mClipboardOwner) {
    mClipboardOwner->LosingOwnership(mTransferable);
    mClipboardOwner = nullptr;
  }

  mTransferable = nullptr;
  return NS_OK;
}

NS_IMETHODIMP
nsClipboard::SupportsSelectionClipboard(bool* _retval)
{
  *_retval = false;   // we don't support the selection clipboard by default.
  return NS_OK;
}
