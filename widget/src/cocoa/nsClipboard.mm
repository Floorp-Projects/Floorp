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
 * Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *    Josh Aas <josh@mozilla.com>
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

#include "nsCOMPtr.h"
#include "nsClipboard.h"
#include "nsIClipboardOwner.h"
#include "nsString.h"
#include "nsXPCOM.h"
#include "nsISupportsPrimitives.h"
#include "nsXPIDLString.h"
#include "nsPrimitiveHelpers.h"
#include "nsMemory.h"
#include "nsIImage.h"
#include "nsILocalFile.h"

nsClipboard::nsClipboard() : nsBaseClipboard()
{
}


nsClipboard::~nsClipboard()
{
}


NS_IMETHODIMP
nsClipboard::SetNativeClipboardData(PRInt32 aWhichClipboard)
{
  if ((aWhichClipboard != kGlobalClipboard) || !mTransferable)
    return NS_ERROR_FAILURE;

  mIgnoreEmptyNotification = PR_TRUE;

  NSDictionary* pasteboardOutputDict = PasteboardDictFromTransferable(mTransferable);
  if (!pasteboardOutputDict)
    return NS_ERROR_FAILURE;

  // write everything out to the general pasteboard
  unsigned int outputCount = [pasteboardOutputDict count];
  NSArray* outputKeys = [pasteboardOutputDict allKeys];
  NSPasteboard* generalPBoard = [NSPasteboard generalPasteboard];
  [generalPBoard declareTypes:outputKeys owner:nil];
  for (unsigned int i = 0; i < outputCount; i++) {
    NSString* currentKey = [outputKeys objectAtIndex:i];
    id currentValue = [pasteboardOutputDict valueForKey:currentKey];
    if (currentKey == NSStringPboardType)
      [generalPBoard setString:currentValue forType:currentKey];
    else
      [generalPBoard setData:currentValue forType:currentKey];
  }

  mIgnoreEmptyNotification = PR_FALSE;

  return NS_OK;
}


NS_IMETHODIMP
nsClipboard::GetNativeClipboardData(nsITransferable* aTransferable, PRInt32 aWhichClipboard)
{  
  if ((aWhichClipboard != kGlobalClipboard) || !aTransferable)
    return NS_ERROR_FAILURE;

  return CopyPasteboardDataToTransferable([NSPasteboard generalPasteboard], aTransferable, 0);
}


NS_IMETHODIMP
nsClipboard::HasDataMatchingFlavors(nsISupportsArray* aFlavorList, PRInt32 aWhichClipboard, PRBool* outResult) 
{
  *outResult = PR_FALSE;

  if ((aWhichClipboard != kGlobalClipboard) || !aFlavorList)
    return NS_OK;

  NSPasteboard* generalPBoard = [NSPasteboard generalPasteboard];

  PRUint32 flavorCount;
  aFlavorList->Count(&flavorCount);
  for (PRUint32 i = 0; i < flavorCount; i++) {
    nsCOMPtr<nsISupports> genericFlavor;
    aFlavorList->GetElementAt(i, getter_AddRefs(genericFlavor));
    nsCOMPtr<nsISupportsCString> flavorWrapper(do_QueryInterface(genericFlavor));
    if (flavorWrapper) {
      nsXPIDLCString flavorStr;
      flavorWrapper->ToString(getter_Copies(flavorStr));
      if (strcmp(flavorStr, kUnicodeMime) == 0) {
        NSString* availableType = [generalPBoard availableTypeFromArray:[NSArray arrayWithObject:NSStringPboardType]];
        if (availableType && [availableType isEqualToString:NSStringPboardType]) {
          *outResult = PR_TRUE;
          break;
        }
      }
      else {
        NSString* lookingForType = [NSString stringWithUTF8String:flavorStr];
        NSString* availableType = [generalPBoard availableTypeFromArray:[NSArray arrayWithObject:lookingForType]];
        if (availableType && [availableType isEqualToString:lookingForType]) {
          *outResult = PR_TRUE;
          break;
        }
      }
    }      
  }

  return NS_OK;
}

// static
NSDictionary* 
nsClipboard::PasteboardDictFromTransferable(nsITransferable* aTransferable)
{
  if (!aTransferable)
    return nil;

  NSMutableDictionary* pasteboardOutputDict = [NSMutableDictionary dictionary];

  nsCOMPtr<nsISupportsArray> flavorList;
  nsresult rv = aTransferable->FlavorsTransferableCanExport(getter_AddRefs(flavorList));
  if (NS_FAILED(rv))
    return nil;

  PRUint32 flavorCount;
  flavorList->Count(&flavorCount);
  for (PRUint32 i = 0; i < flavorCount; i++) {
    nsCOMPtr<nsISupports> genericFlavor;
    flavorList->GetElementAt(i, getter_AddRefs(genericFlavor));
    nsCOMPtr<nsISupportsCString> currentFlavor(do_QueryInterface(genericFlavor));
    if (!currentFlavor)
      continue;

    nsXPIDLCString flavorStr;
    currentFlavor->ToString(getter_Copies(flavorStr));

    // printf("writing out clipboard data of type %s\n", flavorStr.get());

    if (strcmp(flavorStr, kPNGImageMime) == 0 || strcmp(flavorStr, kJPEGImageMime) == 0 ||
        strcmp(flavorStr, kGIFImageMime) == 0 || strcmp(flavorStr, kNativeImageMime) == 0) {
      PRUint32 dataSize = 0;
      nsCOMPtr<nsISupports> transferSupports;
      aTransferable->GetTransferData(flavorStr, getter_AddRefs(transferSupports), &dataSize);
      nsCOMPtr<nsISupportsInterfacePointer> ptrPrimitive(do_QueryInterface(transferSupports));
      if (!ptrPrimitive)
        continue;

      nsCOMPtr<nsISupports> primitiveData;
      ptrPrimitive->GetData(getter_AddRefs(primitiveData));

      nsCOMPtr<nsIImage> image(do_QueryInterface(primitiveData));
      if (!image) {
        NS_WARNING("Image isn't an nsIImage in transferable");
        continue;
      }

      if (NS_FAILED(image->LockImagePixels(PR_FALSE)))
        continue;

      PRInt32 height = image->GetHeight();
      PRInt32 stride = image->GetLineStride();
      PRInt32 width = image->GetWidth();
      if ((stride % 4 != 0) || (height < 1) || (width < 1))
        continue;

      PRUint32* imageData = (PRUint32*)image->GetBits();

      PRUint32* reorderedData = (PRUint32*)malloc(height * stride);
      if (!reorderedData)
        continue;

      // We have to reorder data to have alpha last because only Tiger can handle
      // alpha being first.
      PRUint32 imageLength = ((stride * height) / 4);
      for (PRUint32 i = 0; i < imageLength; i++) {
        PRUint32 pixel = imageData[i];
        reorderedData[i] = CFSwapInt32HostToBig((pixel << 8) | (pixel >> 24));
      }

      PRUint8* planes[2];
      planes[0] = (PRUint8*)reorderedData;
      planes[1] = nsnull;
      NSBitmapImageRep* imageRep = [[NSBitmapImageRep alloc] initWithBitmapDataPlanes:planes
                                                                           pixelsWide:width
                                                                           pixelsHigh:height
                                                                        bitsPerSample:8
                                                                      samplesPerPixel:4
                                                                             hasAlpha:YES
                                                                             isPlanar:NO
                                                                       colorSpaceName:NSDeviceRGBColorSpace
                                                                          bytesPerRow:stride
                                                                         bitsPerPixel:32];
      NSData* tiffData = [imageRep TIFFRepresentationUsingCompression:NSTIFFCompressionNone factor:1.0];
      [imageRep release];
      free(reorderedData);

      if (NS_FAILED(image->UnlockImagePixels(PR_FALSE)))
        continue;

      [pasteboardOutputDict setObject:tiffData forKey:NSTIFFPboardType];
    }
    else {
      /* If it isn't an image, we just throw the data on the clipboard with the mime string
       * as its key. If we recognize the data as something we want to export in standard
       * terms, then we do that too.
       */
      void* data = nsnull;
      PRUint32 dataSize = 0;
      nsCOMPtr<nsISupports> genericDataWrapper;
      rv = aTransferable->GetTransferData(flavorStr, getter_AddRefs(genericDataWrapper), &dataSize);
      nsPrimitiveHelpers::CreateDataFromPrimitive(flavorStr, genericDataWrapper, &data, dataSize);

      // if it is kUnicodeMime, it is text we want to export as standard NSStringPboardType
      if (strcmp(flavorStr, kUnicodeMime) == 0) {
        NSString* nativeString = [NSString stringWithCharacters:(const unichar*)data length:(dataSize / sizeof(PRUnichar))];
        // be nice to Carbon apps, normalize the receiver’s contents using Form C.
        nativeString = [nativeString precomposedStringWithCanonicalMapping];
        [pasteboardOutputDict setObject:nativeString forKey:NSStringPboardType];
      }
      else {
        NSString* key = [NSString stringWithUTF8String:flavorStr];
        NSData* value = [NSData dataWithBytes:data length:dataSize];
        [pasteboardOutputDict setObject:value forKey:key];
      }

      nsMemory::Free(data);
    }
  }
  
  return pasteboardOutputDict;
}

// static
nsresult
nsClipboard::CopyPasteboardDataToTransferable(NSPasteboard* aPasteboard, nsITransferable* aTransferable, PRUint32 aItemIndex)
{
  if (!aTransferable || !aPasteboard)
    return NS_ERROR_FAILURE;
  
  // get flavor list that includes all acceptable flavors (including ones obtained through conversion)
  nsCOMPtr<nsISupportsArray> flavorList;
  nsresult rv = aTransferable->FlavorsTransferableCanImport(getter_AddRefs(flavorList));
  if (NS_FAILED(rv))
    return NS_ERROR_FAILURE;

  PRUint32 flavorCount;
  flavorList->Count(&flavorCount);
  for (PRUint32 i = 0; i < flavorCount; i++) {
    nsCOMPtr<nsISupports> genericFlavor;
    flavorList->GetElementAt(i, getter_AddRefs(genericFlavor));
    nsCOMPtr<nsISupportsCString> currentFlavor(do_QueryInterface(genericFlavor));

    if (!currentFlavor)
      continue;

    nsXPIDLCString flavorStr;
    currentFlavor->ToString(getter_Copies(flavorStr));
    
    // printf("looking for clipboard data of type %s\n", flavorStr.get());

    if (strcmp(flavorStr, kFileMime) == 0) {
      NSArray* pFiles = [aPasteboard propertyListForType:NSFilenamesPboardType];
      if (!pFiles || [pFiles count] < (aItemIndex + 1))
        continue;

      NSString* filePath = [pFiles objectAtIndex:aItemIndex];
      if (!filePath)
        continue;

      unsigned int stringLength = [filePath length];
      unsigned int dataLength = (stringLength + 1) * sizeof(PRUnichar); // in bytes
      PRUnichar* clipboardDataPtr = (PRUnichar*)malloc(dataLength);
      [filePath getCharacters:clipboardDataPtr];
      clipboardDataPtr[stringLength] = 0; // null terminate

      nsCOMPtr<nsILocalFile> file;
      nsresult rv = NS_NewLocalFile(nsDependentString(clipboardDataPtr), PR_TRUE, getter_AddRefs(file));
      free(clipboardDataPtr);
      if (NS_FAILED(rv))
        continue;

      nsCOMPtr<nsISupports> genericDataWrapper;
      genericDataWrapper = do_QueryInterface(file);
      aTransferable->SetTransferData(flavorStr, genericDataWrapper, dataLength);

      break;
    }
    else if (strcmp(flavorStr, kUnicodeMime) == 0) {
      NSString* pString = [aPasteboard stringForType:NSStringPboardType];
      if (!pString)
        continue;

      NSData* stringData = [pString dataUsingEncoding:NSUnicodeStringEncoding];
      unsigned int dataLength = [stringData length];
      unsigned char* clipboardDataPtr = (unsigned char*)malloc(dataLength);
      [stringData getBytes:(void*)clipboardDataPtr];

      // The DOM only wants LF, so convert from MacOS line endings to DOM line endings.
      nsLinebreakHelpers::ConvertPlatformToDOMLinebreaks(flavorStr, (void**)&clipboardDataPtr, (PRInt32*)&dataLength);

      // skip BOM (Byte Order Mark to distinguish little or big endian)      
      unsigned char* clipboardDataPtrNoBOM = clipboardDataPtr;
      if ((dataLength > 2) &&
          ((clipboardDataPtr[0] == 0xFE && clipboardDataPtr[1] == 0xFF) ||
           (clipboardDataPtr[0] == 0xFF && clipboardDataPtr[1] == 0xFE))) {
        dataLength -= sizeof(PRUnichar);
        clipboardDataPtrNoBOM += sizeof(PRUnichar);
      }

      nsCOMPtr<nsISupports> genericDataWrapper;
      nsPrimitiveHelpers::CreatePrimitiveForData(flavorStr, clipboardDataPtrNoBOM, dataLength,
                                                 getter_AddRefs(genericDataWrapper));
      aTransferable->SetTransferData(flavorStr, genericDataWrapper, dataLength);
      free(clipboardDataPtr);
      break;
    }
    else if (strcmp(flavorStr, kPNGImageMime) == 0 || strcmp(flavorStr, kJPEGImageMime) == 0 ||
             strcmp(flavorStr, kGIFImageMime) == 0) {
      // We have never supported this on Mac, we could someday but nobody does this. We want this
      // test here so that we don't try to get this as a custom type.
    }
    else {
      // this is some sort of data that mozilla put on the clipboard itself if it exists
      NSData* pData = [aPasteboard dataForType:[NSString stringWithUTF8String:flavorStr]];
      if (!pData)
        continue;

      unsigned int dataLength = [pData length];
      unsigned char* clipboardDataPtr = (unsigned char*)malloc(dataLength);
      [pData getBytes:(void*)clipboardDataPtr];

      // The DOM only wants LF, so convert from MacOS line endings to DOM line endings.
      nsLinebreakHelpers::ConvertPlatformToDOMLinebreaks(flavorStr, (void**)&clipboardDataPtr, (PRInt32*)&dataLength);

      nsCOMPtr<nsISupports> genericDataWrapper;
      nsPrimitiveHelpers::CreatePrimitiveForData(flavorStr, clipboardDataPtr, dataLength,
                                                 getter_AddRefs(genericDataWrapper));
      aTransferable->SetTransferData(flavorStr, genericDataWrapper, dataLength);
      free(clipboardDataPtr);
      break;
    }
  }

  return NS_OK;
}
