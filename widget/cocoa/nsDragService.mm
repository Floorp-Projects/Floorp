/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Logging.h"

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
#include "nsIIOService.h"
#include "nsIDocument.h"
#include "nsIContent.h"
#include "nsView.h"
#include "gfxContext.h"
#include "nsCocoaUtils.h"
#include "mozilla/gfx/2D.h"
#include "gfxPlatform.h"

using namespace mozilla;
using namespace mozilla::gfx;

extern PRLogModuleInfo* sCocoaLog;

extern void EnsureLogInitialized();

extern NSPasteboard* globalDragPboard;
extern NSView* gLastDragView;
extern NSEvent* gLastDragMouseDownEvent;
extern bool gUserCancelledDrag;

// This global makes the transferable array available to Cocoa's promised
// file destination callback.
nsISupportsArray *gDraggedTransferables = nullptr;

NSString* const kWildcardPboardType = @"MozillaWildcard";
NSString* const kCorePboardType_url  = @"CorePasteboardFlavorType 0x75726C20"; // 'url '  url
NSString* const kCorePboardType_urld = @"CorePasteboardFlavorType 0x75726C64"; // 'urld'  desc
NSString* const kCorePboardType_urln = @"CorePasteboardFlavorType 0x75726C6E"; // 'urln'  title
NSString* const kUTTypeURLName = @"public.url-name";

nsDragService::nsDragService()
{
  mNativeDragView = nil;
  mNativeDragEvent = nil;

  EnsureLogInitialized();
}

nsDragService::~nsDragService()
{
}

static nsresult SetUpDragClipboard(nsISupportsArray* aTransferableArray)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  if (!aTransferableArray)
    return NS_ERROR_FAILURE;

  uint32_t count = 0;
  aTransferableArray->Count(&count);

  NSPasteboard* dragPBoard = [NSPasteboard pasteboardWithName:NSDragPboard];

  for (uint32_t j = 0; j < count; j++) {
    nsCOMPtr<nsISupports> currentTransferableSupports;
    aTransferableArray->GetElementAt(j, getter_AddRefs(currentTransferableSupports));
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
    for (unsigned int k = 0; k < typeCount; k++) {
      NSString* currentKey = [types objectAtIndex:k];
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
      else if (currentKey == NSFilesPromisePboardType ||
               currentKey == NSFilenamesPboardType) {
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

  NSPoint screenPoint =
    [[gLastDragView window] convertBaseToScreen:
      [gLastDragMouseDownEvent locationInWindow]];
  // Y coordinates are bottom to top, so reverse this
  screenPoint.y = nsCocoaUtils::FlippedScreenY(screenPoint.y);

  CGFloat scaleFactor = nsCocoaUtils::GetBackingScaleFactor(gLastDragView);

  RefPtr<SourceSurface> surface;
  nsPresContext* pc;
  nsresult rv = DrawDrag(aDOMNode, aRegion,
                         NSToIntRound(screenPoint.x),
                         NSToIntRound(screenPoint.y),
                         aDragRect, &surface, &pc);
  if (!aDragRect->width || !aDragRect->height) {
    // just use some suitable defaults
    int32_t size = nsCocoaUtils::CocoaPointsToDevPixels(20, scaleFactor);
    aDragRect->SetRect(nsCocoaUtils::CocoaPointsToDevPixels(screenPoint.x, scaleFactor),
                       nsCocoaUtils::CocoaPointsToDevPixels(screenPoint.y, scaleFactor),
                       size, size);
  }

  if (NS_FAILED(rv) || !surface)
    return nil;

  uint32_t width = aDragRect->width;
  uint32_t height = aDragRect->height;



  RefPtr<DataSourceSurface> dataSurface =
    Factory::CreateDataSourceSurface(IntSize(width, height),
                                     SurfaceFormat::B8G8R8A8);
  DataSourceSurface::MappedSurface map;
  if (!dataSurface->Map(DataSourceSurface::MapType::READ_WRITE, &map)) {
    return nil;
  }

  RefPtr<DrawTarget> dt =
    Factory::CreateDrawTargetForData(BackendType::CAIRO,
                                     map.mData,
                                     dataSurface->GetSize(),
                                     map.mStride,
                                     dataSurface->GetFormat());
  if (!dt) {
    dataSurface->Unmap();
    return nil;
  }

  dt->FillRect(gfx::Rect(0, 0, width, height),
               SurfacePattern(surface, ExtendMode::CLAMP),
               DrawOptions(1.0f, CompositionOp::OP_SOURCE));

  NSBitmapImageRep* imageRep =
    [[NSBitmapImageRep alloc] initWithBitmapDataPlanes:NULL
                                            pixelsWide:width
                                            pixelsHigh:height
                                         bitsPerSample:8
                                       samplesPerPixel:4
                                              hasAlpha:YES
                                              isPlanar:NO
                                        colorSpaceName:NSDeviceRGBColorSpace
                                           bytesPerRow:width * 4
                                          bitsPerPixel:32];

  uint8_t* dest = [imageRep bitmapData];
  for (uint32_t i = 0; i < height; ++i) {
    uint8_t* src = map.mData + i * map.mStride;
    for (uint32_t j = 0; j < width; ++j) {
      // Reduce transparency overall by multipying by a factor. Remember, Alpha
      // is premultipled here. Also, Quartz likes RGBA, so do that translation as well.
#ifdef IS_BIG_ENDIAN
      dest[0] = uint8_t(src[1] * DRAG_TRANSLUCENCY);
      dest[1] = uint8_t(src[2] * DRAG_TRANSLUCENCY);
      dest[2] = uint8_t(src[3] * DRAG_TRANSLUCENCY);
      dest[3] = uint8_t(src[0] * DRAG_TRANSLUCENCY);
#else
      dest[0] = uint8_t(src[2] * DRAG_TRANSLUCENCY);
      dest[1] = uint8_t(src[1] * DRAG_TRANSLUCENCY);
      dest[2] = uint8_t(src[0] * DRAG_TRANSLUCENCY);
      dest[3] = uint8_t(src[3] * DRAG_TRANSLUCENCY);
#endif
      src += 4;
      dest += 4;
    }
  }
  dataSurface->Unmap();

  NSImage* image =
    [[NSImage alloc] initWithSize:NSMakeSize(width / scaleFactor,
                                             height / scaleFactor)];
  [image addRepresentation:imageRep];
  [imageRep release];

  return [image autorelease];

  NS_OBJC_END_TRY_ABORT_BLOCK_NIL;
}

bool
nsDragService::IsValidType(NSString* availableType, bool allowFileURL)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  // Prevent exposing fileURL for non-fileURL type.
  // We need URL provided by dropped webloc file, but don't need file's URL.
  // kUTTypeFileURL is returned by [NSPasteboard availableTypeFromArray:] for
  // kUTTypeURL, since it conforms to kUTTypeURL.
  if (!allowFileURL && [availableType isEqualToString:(id)kUTTypeFileURL]) {
    return false;
  }

  return true;

  NS_OBJC_END_TRY_ABORT_BLOCK(false);
}

NSString*
nsDragService::GetStringForType(NSPasteboardItem* item, const NSString* type,
                                bool allowFileURL)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NIL;

  NSString* availableType = [item availableTypeFromArray:[NSArray arrayWithObjects:(id)type, nil]];
  if (availableType && IsValidType(availableType, allowFileURL)) {
    return [item stringForType:(id)availableType];
  }

  return nil;

  NS_OBJC_END_TRY_ABORT_BLOCK_NIL;
}

NSString*
nsDragService::GetTitleForURL(NSPasteboardItem* item)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NIL;

  NSString* name = GetStringForType(item, (const NSString*)kUTTypeURLName);
  if (name) {
    return name;
  }

  NSString* filePath = GetFilePath(item);
  if (filePath) {
    return [filePath lastPathComponent];
  }

  return nil;

  NS_OBJC_END_TRY_ABORT_BLOCK_NIL;
}

NSString*
nsDragService::GetFilePath(NSPasteboardItem* item)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NIL;

  NSString* urlString = GetStringForType(item, (const NSString*)kUTTypeFileURL, true);
  if (urlString) {
    NSURL* url = [NSURL URLWithString:urlString];
    if (url) {
      return [url path];
    }
  }

  return nil;

  NS_OBJC_END_TRY_ABORT_BLOCK_NIL;
}

// We can only invoke NSView's 'dragImage:at:offset:event:pasteboard:source:slideBack:' from
// within NSView's 'mouseDown:' or 'mouseDragged:'. Luckily 'mouseDragged' is always on the
// stack when InvokeDragSession gets called.
nsresult
nsDragService::InvokeDragSessionImpl(nsISupportsArray* aTransferableArray,
                                     nsIScriptableRegion* aDragRgn,
                                     uint32_t aActionType)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  mDataItems = aTransferableArray;

  // put data on the clipboard
  if (NS_FAILED(SetUpDragClipboard(aTransferableArray)))
    return NS_ERROR_FAILURE;

  nsIntRect dragRect(0, 0, 20, 20);
  NSImage* image = ConstructDragImage(mSourceNode, &dragRect, aDragRgn);
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

  LayoutDeviceIntPoint pt(dragRect.x, dragRect.YMost());
  CGFloat scaleFactor = nsCocoaUtils::GetBackingScaleFactor(gLastDragView);
  NSPoint point = nsCocoaUtils::DevPixelsToCocoaPoints(pt, scaleFactor);
  point.y = nsCocoaUtils::FlippedScreenY(point.y);

  point = [[gLastDragView window] convertScreenToBase: point];
  NSPoint localPoint = [gLastDragView convertPoint:point fromView:nil];
 
  // Save the transferables away in case a promised file callback is invoked.
  gDraggedTransferables = aTransferableArray;

  nsBaseDragService::StartDragSession();
  nsBaseDragService::OpenDragPopup();

  // We need to retain the view and the event during the drag in case either gets destroyed.
  mNativeDragView = [gLastDragView retain];
  mNativeDragEvent = [gLastDragMouseDownEvent retain];

  gUserCancelledDrag = false;
  [mNativeDragView dragImage:image
                          at:localPoint
                      offset:NSZeroSize
                       event:mNativeDragEvent
                  pasteboard:[NSPasteboard pasteboardWithName:NSDragPboard]
                      source:mNativeDragView
                   slideBack:YES];
  gUserCancelledDrag = false;

  if (mDoingDrag)
    nsBaseDragService::EndDragSession(false);
  
  return NS_OK;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

NS_IMETHODIMP
nsDragService::GetData(nsITransferable* aTransferable, uint32_t aItemIndex)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  if (!aTransferable)
    return NS_ERROR_FAILURE;

  // get flavor list that includes all acceptable flavors (including ones obtained through conversion)
  nsCOMPtr<nsISupportsArray> flavorList;
  nsresult rv = aTransferable->FlavorsTransferableCanImport(getter_AddRefs(flavorList));
  if (NS_FAILED(rv))
    return NS_ERROR_FAILURE;

  uint32_t acceptableFlavorCount;
  flavorList->Count(&acceptableFlavorCount);

  // if this drag originated within Mozilla we should just use the cached data from
  // when the drag started if possible
  if (mDataItems) {
    nsCOMPtr<nsISupports> currentTransferableSupports;
    mDataItems->GetElementAt(aItemIndex, getter_AddRefs(currentTransferableSupports));
    if (currentTransferableSupports) {
      nsCOMPtr<nsITransferable> currentTransferable(do_QueryInterface(currentTransferableSupports));
      if (currentTransferable) {
        for (uint32_t i = 0; i < acceptableFlavorCount; i++) {
          nsCOMPtr<nsISupports> genericFlavor;
          flavorList->GetElementAt(i, getter_AddRefs(genericFlavor));
          nsCOMPtr<nsISupportsCString> currentFlavor(do_QueryInterface(genericFlavor));
          if (!currentFlavor)
            continue;
          nsXPIDLCString flavorStr;
          currentFlavor->ToString(getter_Copies(flavorStr));

          nsCOMPtr<nsISupports> dataSupports;
          uint32_t dataSize = 0;
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
  for (uint32_t i = 0; i < acceptableFlavorCount; i++) {
    nsCOMPtr<nsISupports> genericFlavor;
    flavorList->GetElementAt(i, getter_AddRefs(genericFlavor));
    nsCOMPtr<nsISupportsCString> currentFlavor(do_QueryInterface(genericFlavor));

    if (!currentFlavor)
      continue;

    nsXPIDLCString flavorStr;
    currentFlavor->ToString(getter_Copies(flavorStr));

    MOZ_LOG(sCocoaLog, LogLevel::Info, ("nsDragService::GetData: looking for clipboard data of type %s\n", flavorStr.get()));

    NSArray* droppedItems = [globalDragPboard pasteboardItems];
    if (!droppedItems) {
      continue;
    }

    uint32_t itemCount = [droppedItems count];
    if (aItemIndex >= itemCount) {
      continue;
    }

    NSPasteboardItem* item = [droppedItems objectAtIndex:aItemIndex];
    if (!item) {
      continue;
    }

    if (flavorStr.EqualsLiteral(kFileMime)) {
      NSString* filePath = GetFilePath(item);
      if (!filePath)
        continue;

      unsigned int stringLength = [filePath length];
      unsigned int dataLength = (stringLength + 1) * sizeof(char16_t); // in bytes
      char16_t* clipboardDataPtr = (char16_t*)malloc(dataLength);
      if (!clipboardDataPtr)
        return NS_ERROR_OUT_OF_MEMORY;
      [filePath getCharacters:reinterpret_cast<unichar*>(clipboardDataPtr)];
      clipboardDataPtr[stringLength] = 0; // null terminate

      nsCOMPtr<nsIFile> file;
      rv = NS_NewLocalFile(nsDependentString(clipboardDataPtr), true, getter_AddRefs(file));
      free(clipboardDataPtr);
      if (NS_FAILED(rv))
        continue;

      aTransferable->SetTransferData(flavorStr, file, dataLength);
      
      break;
    }

    NSString* pString = nil;
    if (flavorStr.EqualsLiteral(kUnicodeMime)) {
      pString = GetStringForType(item, (const NSString*)kUTTypeUTF8PlainText);
    } else if (flavorStr.EqualsLiteral(kHTMLMime)) {
      pString = GetStringForType(item, (const NSString*)kUTTypeHTML);
    } else if (flavorStr.EqualsLiteral(kURLMime)) {
      pString = GetStringForType(item, (const NSString*)kUTTypeURL);
      if (pString) {
        NSString* title = GetTitleForURL(item);
        if (!title) {
          title = pString;
        }
        pString = [NSString stringWithFormat:@"%@\n%@", pString, title];
      }
    } else if (flavorStr.EqualsLiteral(kURLDataMime)) {
      pString = GetStringForType(item, (const NSString*)kUTTypeURL);
    } else if (flavorStr.EqualsLiteral(kURLDescriptionMime)) {
      pString = GetTitleForURL(item);
    } else if (flavorStr.EqualsLiteral(kRTFMime)) {
      pString = GetStringForType(item, (const NSString*)kUTTypeRTF);
    }
    if (pString) {
      NSData* stringData;
      if (flavorStr.EqualsLiteral(kRTFMime)) {
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

    // We have never supported this on Mac OS X, we should someday. Normally dragging images
    // in is accomplished with a file path drag instead of the image data itself.
    /*
    if (flavorStr.EqualsLiteral(kPNGImageMime) || flavorStr.EqualsLiteral(kJPEGImageMime) ||
        flavorStr.EqualsLiteral(kJPGImageMime) || flavorStr.EqualsLiteral(kGIFImageMime)) {

    }
    */
  }
  return NS_OK;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

NS_IMETHODIMP
nsDragService::IsDataFlavorSupported(const char *aDataFlavor, bool *_retval)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  *_retval = false;

  if (!globalDragPboard)
    return NS_ERROR_FAILURE;

  nsDependentCString dataFlavor(aDataFlavor);

  // first see if we have data for this in our cached transferable
  if (mDataItems) {
    uint32_t dataItemsCount;
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

      uint32_t flavorCount;
      flavorList->Count(&flavorCount);
      for (uint32_t j = 0; j < flavorCount; j++) {
        nsCOMPtr<nsISupports> genericFlavor;
        flavorList->GetElementAt(j, getter_AddRefs(genericFlavor));
        nsCOMPtr<nsISupportsCString> currentFlavor(do_QueryInterface(genericFlavor));
        if (!currentFlavor)
          continue;
        nsXPIDLCString flavorStr;
        currentFlavor->ToString(getter_Copies(flavorStr));
        if (dataFlavor.Equals(flavorStr)) {
          *_retval = true;
          return NS_OK;
        }
      }
    }
  }

  const NSString* type = nil;
  bool allowFileURL = false;
  if (dataFlavor.EqualsLiteral(kFileMime)) {
    type = (const NSString*)kUTTypeFileURL;
    allowFileURL = true;
  } else if (dataFlavor.EqualsLiteral(kUnicodeMime)) {
    type = (const NSString*)kUTTypeUTF8PlainText;
  } else if (dataFlavor.EqualsLiteral(kHTMLMime)) {
    type = (const NSString*)kUTTypeHTML;
  } else if (dataFlavor.EqualsLiteral(kURLMime) ||
             dataFlavor.EqualsLiteral(kURLDataMime)) {
    type = (const NSString*)kUTTypeURL;
  } else if (dataFlavor.EqualsLiteral(kURLDescriptionMime)) {
    type = (const NSString*)kUTTypeURLName;
  } else if (dataFlavor.EqualsLiteral(kRTFMime)) {
    type = (const NSString*)kUTTypeRTF;
  }
  NSString* availableType = [globalDragPboard availableTypeFromArray:[NSArray arrayWithObjects:(id)type, nil]];
  if (availableType && IsValidType(availableType, allowFileURL)) {
    *_retval = true;
  }

  return NS_OK;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

NS_IMETHODIMP
nsDragService::GetNumDropItems(uint32_t* aNumItems)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  *aNumItems = 0;

  // first check to see if we have a number of items cached
  if (mDataItems) {
    mDataItems->Count(aNumItems);
    return NS_OK;
  }

  NSArray* droppedItems = [globalDragPboard pasteboardItems];
  if (droppedItems) {
    *aNumItems = [droppedItems count];
  }

  return NS_OK;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

NS_IMETHODIMP
nsDragService::EndDragSession(bool aDoneDrag)
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
  mDataItems = nullptr;
  return rv;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}
