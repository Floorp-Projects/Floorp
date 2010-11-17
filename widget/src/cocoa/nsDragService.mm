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

#ifdef MOZ_LOGGING
#define FORCE_PR_LOG
#endif
#include "prlog.h"

#include "nsDragService.h"
#include "nsObjCExceptions.h"
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
#include "nsICharsetConverterManager.h"
#include "nsIIOService.h"
#include "nsNetUtil.h"
#include "nsIDocument.h"
#include "nsIContent.h"
#include "nsIFrame.h"
#include "nsIView.h"
#include "nsIRegion.h"
#include "gfxASurface.h"
#include "gfxContext.h"

#import <Cocoa/Cocoa.h>

#ifdef PR_LOGGING
extern PRLogModuleInfo* sCocoaLog;
#endif

extern NSPasteboard* globalDragPboard;
extern NSView* gLastDragView;
extern NSEvent* gLastDragMouseDownEvent;
extern PRBool gUserCancelledDrag;

// This global makes the transferable array available to Cocoa's promised
// file destination callback.
nsISupportsArray *gDraggedTransferables = nsnull;

NSString* const kWildcardPboardType = @"MozillaWildcard";
NSString* const kCorePboardType_url  = @"CorePasteboardFlavorType 0x75726C20"; // 'url '  url
NSString* const kCorePboardType_urld = @"CorePasteboardFlavorType 0x75726C64"; // 'urld'  desc
NSString* const kCorePboardType_urln = @"CorePasteboardFlavorType 0x75726C6E"; // 'urln'  title

nsDragService::nsDragService()
{
  mNativeDragView = nil;
  mNativeDragEvent = nil;
}

nsDragService::~nsDragService()
{
}

static nsresult SetUpDragClipboard(nsISupportsArray* aTransferableArray)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

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
    unsigned int typeCount = [pasteboardOutputDict count];
    NSMutableArray* types = [NSMutableArray arrayWithCapacity:typeCount + 1];
    [types addObjectsFromArray:[pasteboardOutputDict allKeys]];
    // Gecko is initiating this drag so we always want its own views to consider
    // it. Add our wildcard type to the pasteboard to accomplish this.
    [types addObject:kWildcardPboardType]; // we don't increase the count for the loop below on purpose
    [dragPBoard declareTypes:types owner:nil];
    for (unsigned int i = 0; i < typeCount; i++) {
      NSString* currentKey = [types objectAtIndex:i];
      id currentValue = [pasteboardOutputDict valueForKey:currentKey];
      if (currentKey == NSStringPboardType ||
          currentKey == kCorePboardType_url ||
          currentKey == kCorePboardType_urld ||
          currentKey == kCorePboardType_urln) {
        [dragPBoard setString:currentValue forType:currentKey];
      }
      else if (currentKey == NSHTMLPboardType) {
        [dragPBoard setString:(nsClipboard::WrapHtmlForSystemPasteboard(currentValue))
                      forType:currentKey];
      }
      else if (currentKey == NSTIFFPboardType) {
        [dragPBoard setData:currentValue forType:currentKey];
      }
      else if (currentKey == NSFilesPromisePboardType) {
        [dragPBoard setPropertyList:currentValue forType:currentKey];        
      }
    }
  }

  return NS_OK;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

NSImage*
nsDragService::ConstructDragImage(nsIDOMNode* aDOMNode,
                                  nsIntRect* aDragRect,
                                  nsIScriptableRegion* aRegion)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NIL;

  NSPoint screenPoint = [[gLastDragView window] convertBaseToScreen:[gLastDragMouseDownEvent locationInWindow]];
  // Y coordinates are bottom to top, so reverse this
  if ([[NSScreen screens] count] > 0)
    screenPoint.y = NSMaxY([[[NSScreen screens] objectAtIndex:0] frame]) - screenPoint.y;

  nsRefPtr<gfxASurface> surface;
  nsPresContext* pc;
  nsresult rv = DrawDrag(aDOMNode, aRegion,
                         NSToIntRound(screenPoint.x), NSToIntRound(screenPoint.y),
                         aDragRect, getter_AddRefs(surface), &pc);
  if (!aDragRect->width || !aDragRect->height) {
    // just use some suitable defaults
    aDragRect->SetRect(NSToIntRound(screenPoint.x), NSToIntRound(screenPoint.y), 20, 20);
  }

  if (NS_FAILED(rv) || !surface)
    return nil;

  PRUint32 width = aDragRect->width;
  PRUint32 height = aDragRect->height;

  nsRefPtr<gfxImageSurface> imgSurface = new gfxImageSurface(
    gfxIntSize(width, height), gfxImageSurface::ImageFormatARGB32);
  if (!imgSurface)
    return nil;

  nsRefPtr<gfxContext> context = new gfxContext(imgSurface);
  if (!context)
    return nil;

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
      // Reduce transparency overall by multipying by a factor. Remember, Alpha
      // is premultipled here. Also, Quartz likes RGBA, so do that translation as well.
#ifdef IS_BIG_ENDIAN
      dest[0] = PRUint8(src[1] * DRAG_TRANSLUCENCY);
      dest[1] = PRUint8(src[2] * DRAG_TRANSLUCENCY);
      dest[2] = PRUint8(src[3] * DRAG_TRANSLUCENCY);
      dest[3] = PRUint8(src[0] * DRAG_TRANSLUCENCY);
#else
      dest[0] = PRUint8(src[2] * DRAG_TRANSLUCENCY);
      dest[1] = PRUint8(src[1] * DRAG_TRANSLUCENCY);
      dest[2] = PRUint8(src[0] * DRAG_TRANSLUCENCY);
      dest[3] = PRUint8(src[3] * DRAG_TRANSLUCENCY);
#endif
      src += 4;
      dest += 4;
    }
  }

  NSImage* image = [[NSImage alloc] initWithSize:NSMakeSize((float)width, (float)height)];
  [image addRepresentation:imageRep];
  [imageRep release];

  return [image autorelease];

  NS_OBJC_END_TRY_ABORT_BLOCK_NIL;
}

// We can only invoke NSView's 'dragImage:at:offset:event:pasteboard:source:slideBack:' from
// within NSView's 'mouseDown:' or 'mouseDragged:'. Luckily 'mouseDragged' is always on the
// stack when InvokeDragSession gets called.
NS_IMETHODIMP
nsDragService::InvokeDragSession(nsIDOMNode* aDOMNode, nsISupportsArray* aTransferableArray,
                                 nsIScriptableRegion* aDragRgn, PRUint32 aActionType)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  nsresult rv = nsBaseDragService::InvokeDragSession(aDOMNode,
                                                     aTransferableArray,
                                                     aDragRgn, aActionType);
  NS_ENSURE_SUCCESS(rv, rv);

  mDataItems = aTransferableArray;

  // put data on the clipboard
  if (NS_FAILED(SetUpDragClipboard(aTransferableArray)))
    return NS_ERROR_FAILURE;

  nsIntRect dragRect(0, 0, 20, 20);
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

  point = [[gLastDragView window] convertScreenToBase: point];
  NSPoint localPoint = [gLastDragView convertPoint:point fromView:nil];
 
  // Save the transferables away in case a promised file callback is invoked.
  gDraggedTransferables = aTransferableArray;

  nsBaseDragService::StartDragSession();

  // We need to retain the view and the event during the drag in case either gets destroyed.
  mNativeDragView = [gLastDragView retain];
  mNativeDragEvent = [gLastDragMouseDownEvent retain];

  gUserCancelledDrag = PR_FALSE;
  [mNativeDragView dragImage:image
                          at:localPoint
                      offset:NSZeroSize
                       event:mNativeDragEvent
                  pasteboard:[NSPasteboard pasteboardWithName:NSDragPboard]
                      source:mNativeDragView
                   slideBack:YES];
  gUserCancelledDrag = PR_FALSE;

  if (mDoingDrag)
    nsBaseDragService::EndDragSession(PR_FALSE);
  
  return NS_OK;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

NS_IMETHODIMP
nsDragService::GetData(nsITransferable* aTransferable, PRUint32 aItemIndex)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

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

    PR_LOG(sCocoaLog, PR_LOG_ALWAYS, ("nsDragService::GetData: looking for clipboard data of type %s\n", flavorStr.get()));

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

    NSString *pboardType = NSStringPboardType;

    if (nsClipboard::IsStringType(flavorStr, &pboardType) ||
        flavorStr.EqualsLiteral(kURLMime) ||
        flavorStr.EqualsLiteral(kURLDataMime) ||
        flavorStr.EqualsLiteral(kURLDescriptionMime)) {
      NSString* pString = [globalDragPboard stringForType:pboardType];
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

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

NS_IMETHODIMP
nsDragService::IsDataFlavorSupported(const char *aDataFlavor, PRBool *_retval)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  *_retval = PR_FALSE;

  if (!globalDragPboard)
    return NS_ERROR_FAILURE;

  nsDependentCString dataFlavor(aDataFlavor);

  // first see if we have data for this in our cached transferable
  if (mDataItems) {
    PRUint32 dataItemsCount;
    mDataItems->Count(&dataItemsCount);
    for (unsigned int i = 0; i < dataItemsCount; i++) {
      nsCOMPtr<nsISupports> currentTransferableSupports;
      mDataItems->GetElementAt(i, getter_AddRefs(currentTransferableSupports));
      if (!currentTransferableSupports)
        continue;

      nsCOMPtr<nsITransferable> currentTransferable(do_QueryInterface(currentTransferableSupports));
      if (!currentTransferable)
        continue;

      nsCOMPtr<nsISupportsArray> flavorList;
      nsresult rv = currentTransferable->FlavorsTransferableCanImport(getter_AddRefs(flavorList));
      if (NS_FAILED(rv))
        continue;

      PRUint32 flavorCount;
      flavorList->Count(&flavorCount);
      for (PRUint32 j = 0; j < flavorCount; j++) {
        nsCOMPtr<nsISupports> genericFlavor;
        flavorList->GetElementAt(j, getter_AddRefs(genericFlavor));
        nsCOMPtr<nsISupportsCString> currentFlavor(do_QueryInterface(genericFlavor));
        if (!currentFlavor)
          continue;
        nsXPIDLCString flavorStr;
        currentFlavor->ToString(getter_Copies(flavorStr));
        if (dataFlavor.Equals(flavorStr)) {
          *_retval = PR_TRUE;
          return NS_OK;
        }
      }
    }
  }

  NSString *pboardType = nil;

  if (dataFlavor.EqualsLiteral(kFileMime)) {
    NSString* availableType = [globalDragPboard availableTypeFromArray:[NSArray arrayWithObject:NSFilenamesPboardType]];
    if (availableType && [availableType isEqualToString:NSFilenamesPboardType])
      *_retval = PR_TRUE;
  }
  else if (dataFlavor.EqualsLiteral(kURLMime)) {
    NSString* availableType = [globalDragPboard availableTypeFromArray:[NSArray arrayWithObject:kCorePboardType_url]];
    if (availableType && [availableType isEqualToString:kCorePboardType_url])
      *_retval = PR_TRUE;
  }
  else if (nsClipboard::IsStringType(dataFlavor, &pboardType)) {
    NSString* availableType = [globalDragPboard availableTypeFromArray:[NSArray arrayWithObject:pboardType]];
    if (availableType && [availableType isEqualToString:pboardType])
      *_retval = PR_TRUE;
  }

  return NS_OK;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

NS_IMETHODIMP
nsDragService::GetNumDropItems(PRUint32* aNumItems)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

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

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

NS_IMETHODIMP
nsDragService::EndDragSession(PRBool aDoneDrag)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  if (mNativeDragView) {
    [mNativeDragView release];
    mNativeDragView = nil;
  }
  if (mNativeDragEvent) {
    [mNativeDragEvent release];
    mNativeDragEvent = nil;
  }

  mUserCancelled = gUserCancelledDrag;

  nsresult rv = nsBaseDragService::EndDragSession(aDoneDrag);
  mDataItems = nsnull;
  return rv;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}
