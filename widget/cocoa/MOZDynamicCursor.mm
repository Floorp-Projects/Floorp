/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "imgIContainer.h"
#include "nsCocoaUtils.h"
#include "MOZDynamicCursor.h"
#include "nsObjCExceptions.h"
#include "nsDirectoryServiceDefs.h"
#include "nsIFile.h"
#include <math.h>

static MOZDynamicCursor* gInstance;
static CGFloat sCurrentCursorScaleFactor = 0.0f;
static nsIWidget::Cursor sCurrentCursor;
static constexpr nsCursor kCustomCursor = eCursorCount;

@interface MOZDynamicCursor (PrivateMethods)
+ (NSCursor*)freshCursorWithType:(nsCursor)aCursor;
- (NSCursor*)cursorWithType:(nsCursor)aCursor;

// Set the cursor.
- (void)setCursor:(NSCursor*)aMacCursor;

@end

@interface NSCursor (CreateWithImageNamed)
+ (NSCursor*)cursorWithImageNamed:(NSString*)imageName hotSpot:(NSPoint)aPoint;
@end

@interface NSCursor (Undocumented)
// busyButClickableCursor is an undocumented NSCursor API, but has been in use since
// at least OS X 10.4 and through 10.9.
+ (NSCursor*)busyButClickableCursor;
@end

@implementation MOZDynamicCursor

+ (MOZDynamicCursor*)sharedInstance {
  if (!gInstance) {
    gInstance = [[MOZDynamicCursor alloc] init];
  }
  return gInstance;
}

+ (NSCursor*)freshCursorWithType:(enum nsCursor)aCursor {
  switch (aCursor) {
    case eCursor_standard:
      return [NSCursor arrowCursor];
    case eCursor_wait:
    case eCursor_spinning:
      return [NSCursor busyButClickableCursor];
    case eCursor_select:
      return [NSCursor IBeamCursor];
    case eCursor_hyperlink:
      return [NSCursor pointingHandCursor];
    case eCursor_crosshair:
      return [NSCursor crosshairCursor];
    case eCursor_move:
      return [NSCursor cursorWithImageNamed:@"move" hotSpot:NSMakePoint(12, 12)];
    case eCursor_help:
      return [NSCursor cursorWithImageNamed:@"help" hotSpot:NSMakePoint(12, 12)];
    case eCursor_copy:
      return [NSCursor dragCopyCursor];
    case eCursor_alias:
      return [NSCursor dragLinkCursor];
    case eCursor_context_menu:
      return [NSCursor contextualMenuCursor];
    case eCursor_cell:
      return [NSCursor cursorWithImageNamed:@"cell" hotSpot:NSMakePoint(12, 12)];
    case eCursor_grab:
      return [NSCursor openHandCursor];
    case eCursor_grabbing:
      return [NSCursor closedHandCursor];
    case eCursor_zoom_in:
      return [NSCursor cursorWithImageNamed:@"zoomIn" hotSpot:NSMakePoint(10, 10)];
    case eCursor_zoom_out:
      return [NSCursor cursorWithImageNamed:@"zoomOut" hotSpot:NSMakePoint(10, 10)];
    case eCursor_vertical_text:
      return [NSCursor cursorWithImageNamed:@"vtIBeam" hotSpot:NSMakePoint(12, 11)];
    case eCursor_all_scroll:
      return [NSCursor openHandCursor];
    case eCursor_not_allowed:
    case eCursor_no_drop:
      return [NSCursor operationNotAllowedCursor];
    // Resize Cursors:
    // North
    case eCursor_n_resize:
      return [NSCursor resizeUpCursor];
    // North East
    case eCursor_ne_resize:
      return [NSCursor cursorWithImageNamed:@"sizeNE" hotSpot:NSMakePoint(12, 11)];
    // East
    case eCursor_e_resize:
      return [NSCursor resizeRightCursor];
    // South East
    case eCursor_se_resize:
      return [NSCursor cursorWithImageNamed:@"sizeSE" hotSpot:NSMakePoint(12, 12)];
    // South
    case eCursor_s_resize:
      return [NSCursor resizeDownCursor];
    // South West
    case eCursor_sw_resize:
      return [NSCursor cursorWithImageNamed:@"sizeSW" hotSpot:NSMakePoint(10, 12)];
    // West
    case eCursor_w_resize:
      return [NSCursor resizeLeftCursor];
    // North West
    case eCursor_nw_resize:
      return [NSCursor cursorWithImageNamed:@"sizeNW" hotSpot:NSMakePoint(11, 11)];
    // North & South
    case eCursor_ns_resize:
      return [NSCursor resizeUpDownCursor];
    // East & West
    case eCursor_ew_resize:
      return [NSCursor resizeLeftRightCursor];
    // North East & South West
    case eCursor_nesw_resize:
      return [NSCursor cursorWithImageNamed:@"sizeNESW" hotSpot:NSMakePoint(12, 12)];
    // North West & South East
    case eCursor_nwse_resize:
      return [NSCursor cursorWithImageNamed:@"sizeNWSE" hotSpot:NSMakePoint(12, 12)];
    // Column Resize
    case eCursor_col_resize:
      return [NSCursor cursorWithImageNamed:@"colResize" hotSpot:NSMakePoint(12, 12)];
    // Row Resize
    case eCursor_row_resize:
      return [NSCursor cursorWithImageNamed:@"rowResize" hotSpot:NSMakePoint(12, 12)];
    default:
      return [NSCursor arrowCursor];
  }
}

- (id)init {
  if ((self = [super init])) {
    mCursors = [[NSMutableDictionary alloc] initWithCapacity:25];
    mCurrentCursor = [NSCursor arrowCursor];
    mCurrentCursorType = eCursor_standard;
  }
  return self;
}

- (void)setNonCustomCursor:(const nsIWidget::Cursor&)aCursor {
  [self setCursor:[self cursorWithType:aCursor.mDefaultCursor] type:aCursor.mDefaultCursor];

  sCurrentCursor = aCursor;
}

- (void)setCursor:(NSCursor*)aMacCursor type:(nsCursor)aType {
  if (mCurrentCursorType != aType) {
    if (aType == eCursor_none) {
      [NSCursor hide];
    } else if (mCurrentCursorType == eCursor_none) {
      [NSCursor unhide];
    }
    mCurrentCursorType = aType;
  }

  if (mCurrentCursor != aMacCursor) {
    [mCurrentCursor release];
    mCurrentCursor = [aMacCursor retain];
    [mCurrentCursor set];
  }
}

- (nsresult)setCustomCursor:(const nsIWidget::Cursor&)aCursor
          widgetScaleFactor:(CGFloat)scaleFactor {
  // As the user moves the mouse, this gets called repeatedly with the same aCursorImage
  if (sCurrentCursor == aCursor && sCurrentCursorScaleFactor == scaleFactor && mCurrentCursor) {
    return NS_OK;
  }

  sCurrentCursor = aCursor;
  sCurrentCursorScaleFactor = scaleFactor;

  if (!aCursor.IsCustom()) {
    return NS_ERROR_FAILURE;
  }

  nsIntSize size = nsIWidget::CustomCursorSize(aCursor);
  // prevent DoS attacks
  if (size.width > 128 || size.height > 128) {
    return NS_ERROR_FAILURE;
  }

  NSImage* cursorImage;
  nsresult rv = nsCocoaUtils::CreateNSImageFromImageContainer(
      aCursor.mContainer, imgIContainer::FRAME_FIRST, nullptr, nullptr, &cursorImage, scaleFactor);
  if (NS_FAILED(rv) || !cursorImage) {
    return NS_ERROR_FAILURE;
  }

  {
    NSSize cocoaSize = NSMakeSize(size.width, size.height);
    [cursorImage setSize:cocoaSize];
    [[[cursorImage representations] objectAtIndex:0] setSize:cocoaSize];
  }

  // if the hotspot is nonsensical, make it 0,0
  uint32_t hotspotX = aCursor.mHotspotX > (uint32_t(size.width) - 1) ? 0 : aCursor.mHotspotX;
  uint32_t hotspotY = aCursor.mHotspotY > (uint32_t(size.height) - 1) ? 0 : aCursor.mHotspotY;
  NSPoint hotSpot = ::NSMakePoint(hotspotX, hotspotY);
  [self setCursor:[[NSCursor alloc] initWithImage:cursorImage hotSpot:hotSpot] type:kCustomCursor];
  [cursorImage release];
  return NS_OK;
}

- (NSCursor*)cursorWithType:(enum nsCursor)aCursor {
  NSCursor* result = [mCursors objectForKey:[NSNumber numberWithInt:aCursor]];
  if (!result) {
    result = [MOZDynamicCursor freshCursorWithType:aCursor];
    [mCursors setObject:result forKey:[NSNumber numberWithInt:aCursor]];
  }
  return result;
}

// This method gets called by ChildView's cursor rect (or rather its underlying
// NSTrackingArea) whenever the mouse enters it, for example after a dragging
// operation, after a menu closes, or when the mouse enters a window.
- (void)set {
  [mCurrentCursor set];
}

- (void)dealloc {
  [mCurrentCursor release];
  [mCursors release];
  sCurrentCursor = {};
  [super dealloc];
}

@end

@implementation NSCursor (CreateWithImageName)

+ (NSCursor*)cursorWithImageNamed:(NSString*)imageName hotSpot:(NSPoint)aPoint {
  nsCOMPtr<nsIFile> resDir;
  nsAutoCString resPath;
  NSString *pathToImage, *pathToHiDpiImage;
  NSImage *cursorImage, *hiDpiCursorImage;

  nsresult rv = NS_GetSpecialDirectory(NS_GRE_DIR, getter_AddRefs(resDir));
  if (NS_FAILED(rv)) goto INIT_FAILURE;
  resDir->AppendNative("res"_ns);
  resDir->AppendNative("cursors"_ns);

  rv = resDir->GetNativePath(resPath);
  if (NS_FAILED(rv)) goto INIT_FAILURE;

  pathToImage = [NSString stringWithUTF8String:(const char*)resPath.get()];
  if (!pathToImage) goto INIT_FAILURE;
  pathToImage = [pathToImage stringByAppendingPathComponent:imageName];
  pathToHiDpiImage = [pathToImage stringByAppendingString:@"@2x"];
  // Add same extension to both image paths.
  pathToImage = [pathToImage stringByAppendingPathExtension:@"png"];
  pathToHiDpiImage = [pathToHiDpiImage stringByAppendingPathExtension:@"png"];

  cursorImage = [[[NSImage alloc] initWithContentsOfFile:pathToImage] autorelease];
  if (!cursorImage) goto INIT_FAILURE;

  // Note 1: There are a few different ways to get a hidpi image via
  // initWithContentsOfFile. We let the OS handle this here: when the
  // file basename ends in "@2x", it will be displayed at native resolution
  // instead of being pixel-doubled. See bug 784909 comment 7 for alternates ways.
  //
  // Note 2: The OS is picky, and will ignore the hidpi representation
  // unless it is exactly twice the size of the lowdpi image.
  hiDpiCursorImage = [[[NSImage alloc] initWithContentsOfFile:pathToHiDpiImage] autorelease];
  if (hiDpiCursorImage) {
    NSImageRep* imageRep = [[hiDpiCursorImage representations] objectAtIndex:0];
    [cursorImage addRepresentation:imageRep];
  }
  return [[[NSCursor alloc] initWithImage:cursorImage hotSpot:aPoint] autorelease];

INIT_FAILURE:
  NS_WARNING("Problem getting path to cursor image file!");
  [self release];
  return nil;
}

@end
