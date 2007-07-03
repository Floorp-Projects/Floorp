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

NSString* const kWildcardPboardType = @"MozillaWildcard";

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

  NSPasteboard* dragPBoard = [NSPasteboard pasteboardWithName:NSDragPboard];

  for (PRUint32 i = 0; i < count; i++) {
    nsCOMPtr<nsISupports> currentTransferableSupports;
    aTransferableArray->GetElementAt(i, getter_AddRefs(currentTransferableSupports));
    if (!currentTransferableSupports)
      return NS_ERROR_FAILURE;

    nsCOMPtr<nsITransferable> currentTransferable(do_QueryInterface(currentTransferableSupports));
    if (!currentTransferable)
      return NS_ERROR_FAILURE;

    // Transform the transferable to an NSDictionary
    NSDictionary* pasteboardOutputDict = nsClipboard::PasteboardDictFromTransferable(currentTransferable);
    if (!pasteboardOutputDict)
      return NS_ERROR_FAILURE;

    // write everything out to the general pasteboard
    unsigned int outputCount = [pasteboardOutputDict count];
    NSArray* outputKeys = [pasteboardOutputDict allKeys];
    [dragPBoard declareTypes:outputKeys owner:nil];
    for (unsigned int i = 0; i < outputCount; i++) {
      NSString* currentKey = [outputKeys objectAtIndex:i];
      id currentValue = [pasteboardOutputDict valueForKey:currentKey];
      if (currentKey == NSStringPboardType) {
        [dragPBoard setString:currentValue forType:currentKey];
      }
      else if (currentKey == NSTIFFPboardType) {
        [dragPBoard setData:currentValue forType:currentKey];
      }
    }
  }

  // Gecko is initiating this drag so we always want its own views to consider
  // it. Add our wildcard type to the pasteboard to accomplish this. Note that the
  // wildcard type is not declared above but it doesn't seem to matter.
  [dragPBoard setData:nil forType:kWildcardPboardType];

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

  mDataItems = aTransferableArray;

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

  PRUint32 acceptableFlavorCount;
  flavorList->Count(&acceptableFlavorCount);

  // if this drag originated within Mozilla we should just use the cached data from
  // when the drag started if possible
  if (mDataItems) {
    nsCOMPtr<nsISupports> currentTransferableSupports;
    mDataItems->GetElementAt(aItemIndex, getter_AddRefs(currentTransferableSupports));
    if (currentTransferableSupports) {
      nsCOMPtr<nsITransferable> currentTransferable(do_QueryInterface(currentTransferableSupports));
      if (currentTransferable) {
        for (PRUint32 i = 0; i < acceptableFlavorCount; i++) {
          nsCOMPtr<nsISupports> genericFlavor;
          flavorList->GetElementAt(i, getter_AddRefs(genericFlavor));
          nsCOMPtr<nsISupportsCString> currentFlavor(do_QueryInterface(genericFlavor));
          if (!currentFlavor)
            continue;
          nsXPIDLCString flavorStr;
          currentFlavor->ToString(getter_Copies(flavorStr));

          nsCOMPtr<nsISupports> dataSupports;
          PRUint32 dataSize = 0;
          rv = currentTransferable->GetTransferData(flavorStr, getter_AddRefs(dataSupports), &dataSize);
          if (NS_SUCCEEDED(rv)) {
            aTransferable->SetTransferData(flavorStr, dataSupports, dataSize);
            return NS_OK; // maybe try to fill in more types? Is there a point?
          }
        }
      }
    }
  }

  // now check the actual clipboard for data
  for (PRUint32 i = 0; i < acceptableFlavorCount; i++) {
    nsCOMPtr<nsISupports> genericFlavor;
    flavorList->GetElementAt(i, getter_AddRefs(genericFlavor));
    nsCOMPtr<nsISupportsCString> currentFlavor(do_QueryInterface(genericFlavor));

    if (!currentFlavor)
      continue;

    nsXPIDLCString flavorStr;
    currentFlavor->ToString(getter_Copies(flavorStr));

    // printf("looking for clipboard data of type %s\n", flavorStr.get());

    if (flavorStr.EqualsLiteral(kFileMime)) {
      NSArray* pFiles = [globalDragPboard propertyListForType:NSFilenamesPboardType];
      if (!pFiles || [pFiles count] < (aItemIndex + 1))
        continue;

      NSString* filePath = [pFiles objectAtIndex:aItemIndex];
      if (!filePath)
        continue;

      unsigned int stringLength = [filePath length];
      unsigned int dataLength = (stringLength + 1) * sizeof(PRUnichar); // in bytes
      PRUnichar* clipboardDataPtr = (PRUnichar*)malloc(dataLength);
      if (!clipboardDataPtr)
        return NS_ERROR_OUT_OF_MEMORY;
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

    if (flavorStr.EqualsLiteral(kUnicodeMime)) {
      NSString* pString = [globalDragPboard stringForType:NSStringPboardType];
      if (!pString)
        continue;

      NSData* stringData = [pString dataUsingEncoding:NSUnicodeStringEncoding];
      unsigned int dataLength = [stringData length];
      void* clipboardDataPtr = malloc(dataLength);
      if (!clipboardDataPtr)
        return NS_ERROR_OUT_OF_MEMORY;
      [stringData getBytes:clipboardDataPtr];

      // The DOM only wants LF, so convert from MacOS line endings to DOM line endings.
      PRInt32 signedDataLength = dataLength;
      nsLinebreakHelpers::ConvertPlatformToDOMLinebreaks(flavorStr, &clipboardDataPtr, &signedDataLength);
      dataLength = signedDataLength;

      // skip BOM (Byte Order Mark to distinguish little or big endian)      
      PRUnichar* clipboardDataPtrNoBOM = (PRUnichar*)clipboardDataPtr;
      if ((dataLength > 2) &&
          ((clipboardDataPtrNoBOM[0] == 0xFEFF) ||
           (clipboardDataPtrNoBOM[0] == 0xFFFE))) {
        dataLength -= sizeof(PRUnichar);
        clipboardDataPtrNoBOM += 1;
      }

      nsCOMPtr<nsISupports> genericDataWrapper;
      nsPrimitiveHelpers::CreatePrimitiveForData(flavorStr, clipboardDataPtrNoBOM, dataLength,
                                                 getter_AddRefs(genericDataWrapper));
      aTransferable->SetTransferData(flavorStr, genericDataWrapper, dataLength);
      free(clipboardDataPtr);
      break;
    }

    // We have never supported this on Mac OS X, we should someday. Normally dragging images
    // in is accomplished with a file path drag instead of the image data itself.
    /*
    if (flavorStr.EqualsLiteral(kPNGImageMime) || flavorStr.EqualsLiteral(kJPEGImageMime) ||
        flavorStr.EqualsLiteral(kGIFImageMime)) {

    }
    */
  }
  return NS_OK;
}


NS_IMETHODIMP
nsDragService::IsDataFlavorSupported(const char *aDataFlavor, PRBool *_retval)
{
  *_retval = PR_FALSE;

  if (!globalDragPboard)
    return NS_ERROR_FAILURE;

  nsDependentCString dataFlavor(aDataFlavor);

  if (dataFlavor.EqualsLiteral(kFileMime)) {
    NSString* availableType = [globalDragPboard availableTypeFromArray:[NSArray arrayWithObject:NSFilenamesPboardType]];
    if (availableType && [availableType isEqualToString:NSFilenamesPboardType])
      *_retval = PR_TRUE;
  }
  else if (dataFlavor.EqualsLiteral(kUnicodeMime)) {
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

  // first check to see if we have a number of items cached
  if (mDataItems) {
    mDataItems->Count(aNumItems);
    return NS_OK;
  }

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


NS_IMETHODIMP
nsDragService::EndDragSession(PRBool aDoneDrag)
{
  mDataItems = nsnull;
  return nsBaseDragService::EndDragSession(aDoneDrag);
}
