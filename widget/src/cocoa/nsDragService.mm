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


// Returns the rect for the drag in native view coordinates.
//
// Note that for text drags, this returns an incorrect rect. This bug
// exists in the old carbon implementation too.
static NSRect GetDragRect(nsIDOMNode* node, nsIScriptableRegion* aDragRgn)
{
  // Set up a default rect in case something goes wrong.
  // It'll at least indicate that *something* is getting dragged
  NSRect outRect = NSMakeRect(0, 0, 50, 50);

  // we're going to need an nsIFrame* no matter what, so get it now
  if (!node)
    return outRect;
  nsCOMPtr<nsIContent> content = do_QueryInterface(node);
  if (!content)
    return outRect;
  nsIDocument* doc = content->GetCurrentDoc();
  if (!doc)
    return outRect;
  nsIPresShell* presShell = doc->GetShellAt(0);
  if (!presShell)
    return outRect;
  nsIFrame* frame = presShell->GetPrimaryFrameFor(content);
  if (!frame)
    return outRect;

  if (aDragRgn) {
    nsCOMPtr<nsIRegion> geckoRegion;
    aDragRgn->GetRegion(getter_AddRefs(geckoRegion));
    if (!geckoRegion)
      return outRect;

    // bounding box for the drag is in window coordinates
    PRInt32 x, y, width, height;
    geckoRegion->GetBoundingBox(&x, &y, &width, &height);
    nsRect rect = nsRect(x, y, width, height);
    // printf("drag region is x=%d, y=%d, width=%d, height=%d\n", x, y, width, height);

    nsCOMPtr<nsIWidget> widget = frame->GetWindow();
    if (!widget)
      return outRect;

    nsRect widgetScreenBounds;
    widget->GetScreenBounds(widgetScreenBounds);

    outRect.origin.x = (float)(rect.x - widgetScreenBounds.x);
    outRect.origin.y = (float)(rect.y - widgetScreenBounds.y + rect.height);
    outRect.size.width = (float)rect.width;
    outRect.size.height = (float)rect.height;
  }
  else {
    nsRect rect = frame->GetRect();

    // find offset from our view
    nsIView *containingView = nsnull;
    nsPoint  viewOffset(0,0);
    frame->GetOffsetFromView(viewOffset, &containingView);
    if (!containingView)
      return outRect;

    // get the widget offset
    nsPoint widgetOffset;
    containingView->GetNearestWidget(&widgetOffset);

    nsPresContext* presContext = frame->GetPresContext();

    nsRect screenOffset;                                
    screenOffset.MoveBy(presContext->AppUnitsToDevPixels(widgetOffset.x +
                                                         viewOffset.x),
                        presContext->AppUnitsToDevPixels(widgetOffset.y +
                                                         viewOffset.y));

    outRect.origin.x = (float)screenOffset.x;
    outRect.origin.y = (float)screenOffset.y +
                       (float)presContext->AppUnitsToDevPixels(rect.height);
    outRect.size.width = (float)presContext->AppUnitsToDevPixels(rect.width);
    outRect.size.height = (float)presContext->AppUnitsToDevPixels(rect.height);
  }

  return outRect;
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

  // create the image for the drag, it isn't awesome but it'll do for now
  NSRect dragRect = GetDragRect(aDOMNode, aDragRgn);
  NSImage* image = [[[NSImage alloc] initWithSize:dragRect.size] autorelease];
  [image lockFocus];
  [[NSColor grayColor] set];
  NSBezierPath* path = [NSBezierPath bezierPath];
  [path setLineWidth:2.0];
  [path moveToPoint:NSMakePoint(0, 0)];
  [path lineToPoint:NSMakePoint(0, dragRect.size.height)];
  [path lineToPoint:NSMakePoint(dragRect.size.width, dragRect.size.height)];
  [path lineToPoint:NSMakePoint(dragRect.size.width, 0)];
  [path lineToPoint:NSMakePoint(0, 0)];
  [path stroke];
  [image unlockFocus];

  // Make sure that the drag rect encompasses the current mouse location.
  // This correction code has been very methodically tested, on every edge of
  // a drag rect. Do not change this code unless you've done that.
  //
  // Get the mouse coordinates in local view coords
  NSPoint localPoint = [globalDragView convertPoint:[globalDragEvent locationInWindow] fromView:nil];
  // fix up the X coords
  if (localPoint.x < dragRect.origin.x)
    dragRect.origin.x = localPoint.x;
  else if (localPoint.x > (dragRect.origin.x + dragRect.size.width))
    dragRect.origin.x = localPoint.x - dragRect.size.width;
  // now fix up the Y coords
  if (localPoint.y < (dragRect.origin.y - dragRect.size.height))
    dragRect.origin.y = localPoint.y + dragRect.size.height;
  else if (localPoint.y > dragRect.origin.y)
    dragRect.origin.y = localPoint.y;

  nsBaseDragService::StartDragSession();
  [globalDragView dragImage:image
                         at:dragRect.origin
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
