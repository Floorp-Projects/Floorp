/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Logging.h"

#include "gfxContext.h"
#include "nsArrayUtils.h"
#include "nsDragService.h"
#include "nsArrayUtils.h"
#include "nsObjCExceptions.h"
#include "nsITransferable.h"
#include "nsString.h"
#include "nsClipboard.h"
#include "nsXPCOM.h"
#include "nsCOMPtr.h"
#include "nsPrimitiveHelpers.h"
#include "nsLinebreakConverter.h"
#include "nsINode.h"
#include "nsRect.h"
#include "nsPoint.h"
#include "mozilla/PresShell.h"
#include "mozilla/dom/Document.h"
#include "mozilla/dom/DocumentInlines.h"
#include "nsIContent.h"
#include "nsView.h"
#include "nsCocoaUtils.h"
#include "mozilla/gfx/2D.h"
#include "gfxPlatform.h"
#include "nsDeviceContext.h"

using namespace mozilla;
using namespace mozilla::gfx;

extern mozilla::LazyLogModule sCocoaLog;

extern NSPasteboard* globalDragPboard;
extern ChildView* gLastDragView;
extern NSEvent* gLastDragMouseDownEvent;
extern bool gUserCancelledDrag;

// This global makes the transferable array available to Cocoa's promised
// file destination callback.
mozilla::StaticRefPtr<nsIArray> gDraggedTransferables;

nsDragService::nsDragService()
    : mNativeDragView(nil), mNativeDragEvent(nil), mDragImageChanged(false) {}

nsDragService::~nsDragService() {}

NSImage* nsDragService::ConstructDragImage(nsINode* aDOMNode,
                                           const Maybe<CSSIntRegion>& aRegion,
                                           NSPoint* aDragPoint) {
  NS_OBJC_BEGIN_TRY_BLOCK_RETURN;

  CGFloat scaleFactor = nsCocoaUtils::GetBackingScaleFactor(mNativeDragView);

  LayoutDeviceIntRect dragRect(0, 0, 20, 20);
  NSImage* image =
      ConstructDragImage(mSourceNode, aRegion, mScreenPosition, &dragRect);
  if (!image) {
    // if no image was returned, just draw a rectangle
    NSSize size;
    size.width =
        nsCocoaUtils::DevPixelsToCocoaPoints(dragRect.width, scaleFactor);
    size.height =
        nsCocoaUtils::DevPixelsToCocoaPoints(dragRect.height, scaleFactor);
    image = [NSImage imageWithSize:size
                           flipped:YES
                    drawingHandler:^BOOL(NSRect dstRect) {
                      [[NSColor grayColor] set];
                      NSBezierPath* path =
                          [NSBezierPath bezierPathWithRect:dstRect];
                      [path setLineWidth:2.0];
                      [path stroke];
                      return YES;
                    }];
  }

  LayoutDeviceIntPoint pt(dragRect.x, dragRect.YMost());
  NSPoint point = nsCocoaUtils::DevPixelsToCocoaPoints(pt, scaleFactor);
  point.y = nsCocoaUtils::FlippedScreenY(point.y);

  point = nsCocoaUtils::ConvertPointFromScreen([mNativeDragView window], point);
  *aDragPoint = [mNativeDragView convertPoint:point fromView:nil];

  return image;

  NS_OBJC_END_TRY_BLOCK_RETURN(nil);
}

NSImage* nsDragService::ConstructDragImage(nsINode* aDOMNode,
                                           const Maybe<CSSIntRegion>& aRegion,
                                           CSSIntPoint aPoint,
                                           LayoutDeviceIntRect* aDragRect) {
  NS_OBJC_BEGIN_TRY_BLOCK_RETURN;

  CGFloat scaleFactor = nsCocoaUtils::GetBackingScaleFactor(mNativeDragView);

  RefPtr<SourceSurface> surface;
  nsPresContext* pc;
  nsresult rv = DrawDrag(aDOMNode, aRegion, aPoint, aDragRect, &surface, &pc);
  if (pc && (!aDragRect->width || !aDragRect->height)) {
    // just use some suitable defaults
    int32_t size = nsCocoaUtils::CocoaPointsToDevPixels(20, scaleFactor);
    aDragRect->SetRect(pc->CSSPixelsToDevPixels(aPoint.x),
                       pc->CSSPixelsToDevPixels(aPoint.y), size, size);
  }

  if (NS_FAILED(rv) || !surface) return nil;

  uint32_t width = aDragRect->width;
  uint32_t height = aDragRect->height;

  RefPtr<DataSourceSurface> dataSurface = Factory::CreateDataSourceSurface(
      IntSize(width, height), SurfaceFormat::B8G8R8A8);
  DataSourceSurface::MappedSurface map;
  if (!dataSurface->Map(DataSourceSurface::MapType::READ_WRITE, &map)) {
    return nil;
  }

  RefPtr<DrawTarget> dt = Factory::CreateDrawTargetForData(
      BackendType::CAIRO, map.mData, dataSurface->GetSize(), map.mStride,
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
      // is premultipled here. Also, Quartz likes RGBA, so do that translation
      // as well.
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

  NSImage* image = [[NSImage alloc]
      initWithSize:NSMakeSize(width / scaleFactor, height / scaleFactor)];
  [image addRepresentation:imageRep];
  [imageRep release];

  return [image autorelease];

  NS_OBJC_END_TRY_BLOCK_RETURN(nil);
}

nsresult nsDragService::InvokeDragSessionImpl(
    nsIArray* aTransferableArray, const Maybe<CSSIntRegion>& aRegion,
    uint32_t aActionType) {
  NS_OBJC_BEGIN_TRY_BLOCK_RETURN;

#ifdef NIGHTLY_BUILD
  MOZ_RELEASE_ASSERT(NS_IsMainThread());
#endif

  if (!gLastDragView) {
    // gLastDragView is non-null between -[ChildView mouseDown:] and -[ChildView
    // mouseUp:]. If we get here with gLastDragView being null, that means that
    // the mouse button has already been released. In that case we need to abort
    // the drag because the OS won't know where to drop whatever's being
    // dragged, and we might end up with a stuck drag & drop session.
    return NS_ERROR_FAILURE;
  }

  mDataItems = aTransferableArray;

  // Save the transferables away in case a promised file callback is invoked.
  gDraggedTransferables = aTransferableArray;

  // We need to retain the view and the event during the drag in case either
  // gets destroyed.
  mNativeDragView = [gLastDragView retain];
  mNativeDragEvent = [gLastDragMouseDownEvent retain];

  gUserCancelledDrag = false;

  NSPasteboardItem* pbItem = [NSPasteboardItem new];
  NSMutableArray* types = [NSMutableArray arrayWithCapacity:5];

  if (gDraggedTransferables) {
    uint32_t count = 0;
    gDraggedTransferables->GetLength(&count);

    for (uint32_t j = 0; j < count; j++) {
      nsCOMPtr<nsITransferable> currentTransferable =
          do_QueryElementAt(aTransferableArray, j);
      if (!currentTransferable) {
        return NS_ERROR_FAILURE;
      }

      // Transform the transferable to an NSDictionary
      NSDictionary* pasteboardOutputDict =
          nsClipboard::PasteboardDictFromTransferable(currentTransferable);
      if (!pasteboardOutputDict) {
        return NS_ERROR_FAILURE;
      }

      // write everything out to the general pasteboard
      [types addObjectsFromArray:[pasteboardOutputDict allKeys]];
      // Gecko is initiating this drag so we always want its own views to
      // consider it. Add our wildcard type to the pasteboard to accomplish
      // this.
      [types addObject:[UTIHelper stringFromPboardType:kMozWildcardPboardType]];
    }
  }
  [pbItem setDataProvider:mNativeDragView forTypes:types];

  NSPoint draggingPoint;
  NSImage* image = ConstructDragImage(mSourceNode, aRegion, &draggingPoint);

  NSRect localDragRect = image.alignmentRect;
  localDragRect.origin.x = draggingPoint.x;
  localDragRect.origin.y = draggingPoint.y - localDragRect.size.height;

  NSDraggingItem* dragItem =
      [[NSDraggingItem alloc] initWithPasteboardWriter:pbItem];
  [pbItem release];
  [dragItem setDraggingFrame:localDragRect contents:image];

  nsBaseDragService::StartDragSession();
  nsBaseDragService::OpenDragPopup();

  NSDraggingSession* draggingSession = [mNativeDragView
      beginDraggingSessionWithItems:[NSArray
                                        arrayWithObject:[dragItem autorelease]]
                              event:mNativeDragEvent
                             source:mNativeDragView];
  draggingSession.animatesToStartingPositionsOnCancelOrFail =
      !mDataTransfer || mDataTransfer->MozShowFailAnimation();

  return NS_OK;

  NS_OBJC_END_TRY_BLOCK_RETURN(NS_ERROR_FAILURE);
}

NS_IMETHODIMP
nsDragService::GetData(nsITransferable* aTransferable, uint32_t aItemIndex) {
  NS_OBJC_BEGIN_TRY_BLOCK_RETURN;

  if (!aTransferable) {
    return NS_ERROR_FAILURE;
  }

  // get flavor list that includes all acceptable flavors (including ones
  // obtained through conversion)
  nsTArray<nsCString> flavors;
  nsresult rv = aTransferable->FlavorsTransferableCanImport(flavors);
  if (NS_FAILED(rv)) {
    return NS_ERROR_FAILURE;
  }

  // if this drag originated within Mozilla we should just use the cached data
  // from when the drag started if possible
  if (mDataItems) {
    nsCOMPtr<nsITransferable> currentTransferable =
        do_QueryElementAt(mDataItems, aItemIndex);
    if (currentTransferable) {
      for (uint32_t i = 0; i < flavors.Length(); i++) {
        nsCString& flavorStr = flavors[i];

        nsCOMPtr<nsISupports> dataSupports;
        rv = currentTransferable->GetTransferData(flavorStr.get(),
                                                  getter_AddRefs(dataSupports));
        if (NS_SUCCEEDED(rv)) {
          aTransferable->SetTransferData(flavorStr.get(), dataSupports);
          return NS_OK;  // maybe try to fill in more types? Is there a point?
        }
      }
    }
  }

  NSArray* droppedItems = [globalDragPboard pasteboardItems];
  if (!droppedItems) {
    return NS_ERROR_FAILURE;
  }

  uint32_t itemCount = [droppedItems count];
  if (aItemIndex >= itemCount) {
    return NS_ERROR_FAILURE;
  }

  NSPasteboardItem* item = [droppedItems objectAtIndex:aItemIndex];
  if (!item) {
    return NS_ERROR_FAILURE;
  }

  // now check the actual clipboard for data
  for (uint32_t i = 0; i < flavors.Length(); i++) {
    nsCocoaUtils::SetTransferDataForTypeFromPasteboardItem(aTransferable,
                                                           flavors[i], item);
  }

  return NS_OK;

  NS_OBJC_END_TRY_BLOCK_RETURN(NS_ERROR_FAILURE);
}

NS_IMETHODIMP
nsDragService::IsDataFlavorSupported(const char* aDataFlavor, bool* _retval) {
  NS_OBJC_BEGIN_TRY_BLOCK_RETURN;

  *_retval = false;

  if (!globalDragPboard) return NS_ERROR_FAILURE;

  nsDependentCString dataFlavor(aDataFlavor);

  // first see if we have data for this in our cached transferable
  if (mDataItems) {
    uint32_t dataItemsCount;
    mDataItems->GetLength(&dataItemsCount);
    for (unsigned int i = 0; i < dataItemsCount; i++) {
      nsCOMPtr<nsITransferable> currentTransferable =
          do_QueryElementAt(mDataItems, i);
      if (!currentTransferable) continue;

      nsTArray<nsCString> flavors;
      nsresult rv = currentTransferable->FlavorsTransferableCanImport(flavors);
      if (NS_FAILED(rv)) continue;

      for (uint32_t j = 0; j < flavors.Length(); j++) {
        if (dataFlavor.Equals(flavors[j])) {
          *_retval = true;
          return NS_OK;
        }
      }
    }
  }

  const NSString* type = nil;
  bool allowFileURL = false;
  if (dataFlavor.EqualsLiteral(kFileMime)) {
    type = [UTIHelper stringFromPboardType:(NSString*)kUTTypeFileURL];
    allowFileURL = true;
  } else if (dataFlavor.EqualsLiteral(kTextMime)) {
    type = [UTIHelper stringFromPboardType:NSPasteboardTypeString];
  } else if (dataFlavor.EqualsLiteral(kHTMLMime)) {
    type = [UTIHelper stringFromPboardType:NSPasteboardTypeHTML];
  } else if (dataFlavor.EqualsLiteral(kURLMime) ||
             dataFlavor.EqualsLiteral(kURLDataMime)) {
    type = [UTIHelper stringFromPboardType:kPublicUrlPboardType];
  } else if (dataFlavor.EqualsLiteral(kURLDescriptionMime)) {
    type = [UTIHelper stringFromPboardType:kPublicUrlNamePboardType];
  } else if (dataFlavor.EqualsLiteral(kRTFMime)) {
    type = [UTIHelper stringFromPboardType:NSPasteboardTypeRTF];
  } else if (dataFlavor.EqualsLiteral(kCustomTypesMime)) {
    type = [UTIHelper stringFromPboardType:kMozCustomTypesPboardType];
  }

  NSString* availableType = [globalDragPboard
      availableTypeFromArray:[NSArray arrayWithObjects:(id)type, nil]];
  if (availableType &&
      nsCocoaUtils::IsValidPasteboardType(availableType, allowFileURL)) {
    *_retval = true;
  }

  return NS_OK;

  NS_OBJC_END_TRY_BLOCK_RETURN(NS_ERROR_FAILURE);
}

NS_IMETHODIMP
nsDragService::GetNumDropItems(uint32_t* aNumItems) {
  NS_OBJC_BEGIN_TRY_BLOCK_RETURN;

  *aNumItems = 0;

  // first check to see if we have a number of items cached
  if (mDataItems) {
    mDataItems->GetLength(aNumItems);
    return NS_OK;
  }

  NSArray* droppedItems = [globalDragPboard pasteboardItems];
  if (droppedItems) {
    *aNumItems = [droppedItems count];
  }

  return NS_OK;

  NS_OBJC_END_TRY_BLOCK_RETURN(NS_ERROR_FAILURE);
}

NS_IMETHODIMP
nsDragService::UpdateDragImage(nsINode* aImage, int32_t aImageX,
                               int32_t aImageY) {
  nsBaseDragService::UpdateDragImage(aImage, aImageX, aImageY);
  mDragImageChanged = true;
  return NS_OK;
}

void nsDragService::DragMovedWithView(NSDraggingSession* aSession,
                                      NSPoint aPoint) {
  aPoint.y = nsCocoaUtils::FlippedScreenY(aPoint.y);

  // XXX It feels like we should be using the backing scale factor at aPoint
  // rather than the initial drag view, but I've seen no ill effects of this.
  CGFloat scaleFactor = nsCocoaUtils::GetBackingScaleFactor(mNativeDragView);
  LayoutDeviceIntPoint devPoint =
      nsCocoaUtils::CocoaPointsToDevPixels(aPoint, scaleFactor);

  // If the image has changed, call enumerateDraggingItemsWithOptions to get
  // the item being dragged and update its image.
  if (mDragImageChanged && mNativeDragView) {
    mDragImageChanged = false;

    nsPresContext* pc = nullptr;
    nsCOMPtr<nsIContent> content = do_QueryInterface(mImage);
    if (content) {
      pc = content->OwnerDoc()->GetPresContext();
    }

    if (pc) {
      void (^changeImageBlock)(NSDraggingItem*, NSInteger, BOOL*) = ^(
          NSDraggingItem* draggingItem, NSInteger idx, BOOL* stop) {
        // We never add more than one item right now, but check just in case.
        if (idx > 0) {
          return;
        }

        nsPoint pt = LayoutDevicePixel::ToAppUnits(
            devPoint, pc->DeviceContext()->AppUnitsPerDevPixel());
        CSSIntPoint screenPoint =
            CSSIntPoint(nsPresContext::AppUnitsToIntCSSPixels(pt.x),
                        nsPresContext::AppUnitsToIntCSSPixels(pt.y));

        // Create a new image; if one isn't returned don't change the current
        // one.
        LayoutDeviceIntRect newRect;
        NSImage* image =
            ConstructDragImage(mSourceNode, Nothing(), screenPoint, &newRect);
        if (image) {
          NSRect draggingRect =
              nsCocoaUtils::GeckoRectToCocoaRectDevPix(newRect, scaleFactor);
          [draggingItem setDraggingFrame:draggingRect contents:image];
        }
      };

      [aSession
          enumerateDraggingItemsWithOptions:NSDraggingItemEnumerationConcurrent
                                    forView:nil
                                    classes:[NSArray
                                                arrayWithObject:
                                                    [NSPasteboardItem class]]
                              searchOptions:@{}
                                 usingBlock:changeImageBlock];
    }
  }

  DragMoved(devPoint.x, devPoint.y);
}

NS_IMETHODIMP
nsDragService::EndDragSession(bool aDoneDrag, uint32_t aKeyModifiers) {
  NS_OBJC_BEGIN_TRY_BLOCK_RETURN;

  if (mNativeDragView) {
    [mNativeDragView release];
    mNativeDragView = nil;
  }
  if (mNativeDragEvent) {
    [mNativeDragEvent release];
    mNativeDragEvent = nil;
  }

  mUserCancelled = gUserCancelledDrag;

  nsresult rv = nsBaseDragService::EndDragSession(aDoneDrag, aKeyModifiers);
  mDataItems = nullptr;
  return rv;

  NS_OBJC_END_TRY_BLOCK_RETURN(NS_ERROR_FAILURE);
}
