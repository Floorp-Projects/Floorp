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
 *   Josh Aas <josh@mozilla.com>
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

#include "nsDragService.h"
#include "nsITransferable.h"
#include "nsString.h"
#include "nsClipboard.h"
#include "nsXPCOM.h"
#include "nsISupportsPrimitives.h"
#include "nsCOMPtr.h"
#include "nsPrimitiveHelpers.h"
#include "nsLinebreakConverter.h"
#include "nsIMacUtils.h"
#include "nsIDOMNode.h"
#include "nsRect.h"
#include "nsPoint.h"
#include "nsIWidget.h"
#include "nsIImage.h"
#include "nsICharsetConverterManager.h"
#include "nsIIOService.h"
#include "nsNetUtil.h"
#include "nsIDocument.h"
#include "nsIContent.h"
#include "nsIPresShell.h"
#include "nsIFrame.h"
#include "nsIView.h"
#include "nsIRegion.h"
#include "gfxASurface.h"
#include "gfxContext.h"

#import <Cocoa/Cocoa.h>

extern NSPasteboard* globalDragPboard;
extern NSView* globalDragView;
extern NSEvent* globalDragEvent;

NS_IMPL_ADDREF_INHERITED(nsDragService, nsBaseDragService)
NS_IMPL_RELEASE_INHERITED(nsDragService, nsBaseDragService)
NS_IMPL_QUERY_INTERFACE2(nsDragService, nsIDragService, nsIDragSession)

nsDragService::nsDragService()
{

}


nsDragService::~nsDragService()
{

}

static nsresult SetUpDragClipboard(nsISupportsArray* aTransferableArray)
{
  if (!aTransferableArray)
    return NS_ERROR_FAILURE;

  PRUint32 count = 0;
  aTransferableArray->Count(&count);

  for (PRUint32 i = 0; i < count; i++) {
    nsCOMPtr<nsISupports> currentTransferableSupports;
    aTransferableArray->GetElementAt(i, getter_AddRefs(currentTransferableSupports));
    if (!currentTransferableSupports)
      return NS_ERROR_FAILURE;

    nsCOMPtr<nsITransferable> currentTransferable(do_QueryInterface(currentTransferableSupports));
    if (!currentTransferable)
      return NS_ERROR_FAILURE;

    NSMutableDictionary* pasteboardOutputDict = [NSMutableDictionary dictionaryWithCapacity:1];
    
    nsCOMPtr<nsISupportsArray> flavorList;
    nsresult rv = currentTransferable->FlavorsTransferableCanExport(getter_AddRefs(flavorList));
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

      // printf("writing out clipboard data of type %s\n", flavorStr.get());

      if (strcmp(flavorStr, kPNGImageMime) == 0 || strcmp(flavorStr, kJPEGImageMime) == 0 ||
          strcmp(flavorStr, kGIFImageMime) == 0 || strcmp(flavorStr, kNativeImageMime) == 0) {
        PRUint32 dataSize = 0;
        nsCOMPtr<nsISupports> transferSupports;
        currentTransferable->GetTransferData(flavorStr, getter_AddRefs(transferSupports), &dataSize);
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
        rv = currentTransferable->GetTransferData(flavorStr, getter_AddRefs(genericDataWrapper), &dataSize);
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
    
    // write everything out to the general pasteboard
    unsigned int outputCount = [pasteboardOutputDict count];
    NSArray* outputKeys = [pasteboardOutputDict allKeys];
    NSPasteboard* generalPBoard = [NSPasteboard pasteboardWithName:NSDragPboard];
    [generalPBoard declareTypes:outputKeys owner:nil];
    for (unsigned int i = 0; i < outputCount; i++) {
      NSString* currentKey = [outputKeys objectAtIndex:i];
      id currentValue = [pasteboardOutputDict valueForKey:currentKey];
      if (currentKey == NSStringPboardType)
        [generalPBoard setString:currentValue forType:currentKey];
      else
        [generalPBoard setData:currentValue forType:currentKey];
    }
  }

  return NS_OK;  
}


NSImage*
nsDragService::ConstructDragImage(nsIDOMNode* aDOMNode,
                                  nsRect* aDragRect,
                                  nsIScriptableRegion* aRegion)
{
  NSPoint screenPoint = [[globalDragView window] convertBaseToScreen:[globalDragEvent locationInWindow]];
  // Y coordinates are bottom to top, so reverse this
  if ([[NSScreen screens] count] > 0)
    screenPoint.y = NSMaxY([[[NSScreen screens] objectAtIndex:0] frame]) - screenPoint.y;

  nsRefPtr<gfxASurface> surface;
  nsresult rv = DrawDrag(aDOMNode, aRegion,
                         NSToIntRound(screenPoint.x), NSToIntRound(screenPoint.y),
                         aDragRect, getter_AddRefs(surface));
  if (!aDragRect->width || !aDragRect->height) {
    // just use some suitable defaults
    aDragRect->SetRect(NSToIntRound(screenPoint.x), NSToIntRound(screenPoint.y), 20, 20);
  }

  if (NS_FAILED(rv) || !surface)
    return nsnull;

  PRUint32 width = aDragRect->width;
  PRUint32 height = aDragRect->height;

  nsRefPtr<gfxImageSurface> imgSurface = new gfxImageSurface(
    gfxIntSize(width, height), gfxImageSurface::ImageFormatARGB32);
  if (!imgSurface)
    return nsnull;

  nsRefPtr<gfxContext> context = new gfxContext(imgSurface);
  if (!context)
    return nsnull;

  context->SetOperator(gfxContext::OPERATOR_SOURCE);
  context->SetSource(surface);
  context->Paint();

  PRUint32* imageData = (PRUint32*)imgSurface->Data();
  PRInt32 stride = imgSurface->Stride();

  NSBitmapImageRep* imageRep = [[NSBitmapImageRep alloc] initWithBitmapDataPlanes:NULL
                                                                       pixelsWide:width
                                                                       pixelsHigh:height
                                                                    bitsPerSample:8
                                                                  samplesPerPixel:4
                                                                         hasAlpha:YES
                                                                         isPlanar:NO
                                                                   colorSpaceName:NSDeviceRGBColorSpace
                                                                      bytesPerRow:width * 4
                                                                     bitsPerPixel:32];

  PRUint8* dest = [imageRep bitmapData];
  for (PRUint32 i = 0; i < height; ++i) {
    PRUint8* src = (PRUint8 *)imageData + i * stride;
    for (PRUint32 j = 0; j < width; ++j) {
      // reduce transparency overall by multipying by a factor
#ifdef IS_BIG_ENDIAN
      dest[0] = src[1];
      dest[1] = src[2];
      dest[2] = src[3];
      dest[3] = PRUint8(src[0] * DRAG_TRANSLUCENCY);
#else
      dest[0] = src[2];
      dest[1] = src[1];
      dest[2] = src[0];
      dest[3] = PRUint8(src[3] * DRAG_TRANSLUCENCY);
#endif
      src += 4;
      dest += 4;
    }
  }

  NSImage* image = [NSImage alloc];
  [image addRepresentation:imageRep];

  return [image autorelease];
}

// We can only invoke NSView's 'dragImage:at:offset:event:pasteboard:source:slideBack:' from
// within NSView's 'mouseDown:' or 'mouseDragged:'. Luckily 'mouseDragged' is always on the
// stack when InvokeDragSession gets called.
NS_IMETHODIMP
nsDragService::InvokeDragSession(nsIDOMNode* aDOMNode, nsISupportsArray* aTransferableArray,
                                 nsIScriptableRegion* aDragRgn, PRUint32 aActionType)
{
  nsBaseDragService::InvokeDragSession(aDOMNode, aTransferableArray, aDragRgn, aActionType);

  // put data on the clipboard
  if (NS_FAILED(SetUpDragClipboard(aTransferableArray)))
    return NS_ERROR_FAILURE;

  nsRect dragRect(0, 0, 20, 20);
  NSImage* image = ConstructDragImage(aDOMNode, &dragRect, aDragRgn);
  if (!image) {
    // if no image was returned, just draw a rectangle
    NSSize size;
    size.width = dragRect.width;
    size.height = dragRect.height;
    image = [[NSImage alloc] initWithSize:size];
    [image lockFocus];
    [[NSColor grayColor] set];
    NSBezierPath* path = [NSBezierPath bezierPath];
    [path setLineWidth:2.0];
    [path moveToPoint:NSMakePoint(0, 0)];
    [path lineToPoint:NSMakePoint(0, size.height)];
    [path lineToPoint:NSMakePoint(size.width, size.height)];
    [path lineToPoint:NSMakePoint(size.width, 0)];
    [path lineToPoint:NSMakePoint(0, 0)];
    [path stroke];
    [image unlockFocus];
  }

  NSPoint point;
  point.x = dragRect.x;
  if ([[NSScreen screens] count] > 0)
    point.y = NSMaxY([[[NSScreen screens] objectAtIndex:0] frame]) - dragRect.YMost();
  else
    point.y = dragRect.y;

  point = [[globalDragView window] convertScreenToBase: point];
  NSPoint localPoint = [globalDragView convertPoint:point fromView:nil];
 
  nsBaseDragService::StartDragSession();
  [globalDragView dragImage:image
                         at:localPoint
                     offset:NSMakeSize(0,0)
                      event:globalDragEvent
                 pasteboard:[NSPasteboard pasteboardWithName:NSDragPboard]
                     source:globalDragView
                  slideBack:YES];
  nsBaseDragService::EndDragSession();

  return NS_OK;
}


NS_IMETHODIMP
nsDragService::GetData(nsITransferable* aTransferable, PRUint32 aItemIndex)
{
  if (!aTransferable)
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
      NSArray* pFiles = [globalDragPboard propertyListForType:NSFilenamesPboardType];
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
      NSString* pString = [globalDragPboard stringForType:NSStringPboardType];
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
      NSData* pData = [globalDragPboard dataForType:[NSString stringWithUTF8String:flavorStr]];
      if (!pData)
        continue;

      unsigned int dataLength = [pData length];
      unsigned char* clipboardDataPtr = (unsigned char*)malloc(dataLength);
      [pData getBytes:(void*)clipboardDataPtr];

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


NS_IMETHODIMP
nsDragService::IsDataFlavorSupported(const char *aDataFlavor, PRBool *_retval)
{
  *_retval = PR_FALSE;

  if (!globalDragPboard)
    return NS_ERROR_FAILURE;
  
  if (strcmp(aDataFlavor, kFileMime) == 0) {
    NSString* availableType = [globalDragPboard availableTypeFromArray:[NSArray arrayWithObject:NSFilenamesPboardType]];
    if (availableType && [availableType isEqualToString:NSFilenamesPboardType])
      *_retval = PR_TRUE;
  }
  else if (strcmp(aDataFlavor, kUnicodeMime) == 0) {
    NSString* availableType = [globalDragPboard availableTypeFromArray:[NSArray arrayWithObject:NSStringPboardType]];
    if (availableType && [availableType isEqualToString:NSStringPboardType])
      *_retval = PR_TRUE;
  }
  else {
    NSString* lookingForType = [NSString stringWithUTF8String:aDataFlavor];
    NSString* availableType = [globalDragPboard availableTypeFromArray:[NSArray arrayWithObject:lookingForType]];
    if (availableType && [availableType isEqualToString:lookingForType])
      *_retval = PR_TRUE;
  }

  return NS_OK;
}


NS_IMETHODIMP
nsDragService::GetNumDropItems(PRUint32* aNumItems)
{
  *aNumItems = 0;
  
  // if there is a clipboard and there is something on it, then there is at least 1 item
  NSArray* clipboardTypes = [globalDragPboard types];
  if (globalDragPboard && [clipboardTypes count] > 0)
    *aNumItems = 1;
  else 
    return NS_OK;
  
  // if there is a list of files, send the number of files in that list
  NSArray* fileNames = [globalDragPboard propertyListForType:NSFilenamesPboardType];
  if (fileNames)
    *aNumItems = [fileNames count];
  
  return NS_OK;
}
