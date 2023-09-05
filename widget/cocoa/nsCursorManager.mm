/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "imgIContainer.h"
#include "nsCocoaUtils.h"
#include "nsCursorManager.h"
#include "nsObjCExceptions.h"
#include <math.h>

static nsCursorManager* gInstance;
static CGFloat sCurrentCursorScaleFactor = 0.0f;
static nsIWidget::Cursor sCurrentCursor;
static constexpr nsCursor kCustomCursor = eCursorCount;

/*! @category nsCursorManager(PrivateMethods)
    Private methods for the cursor manager class.
*/
@interface nsCursorManager (PrivateMethods)
/*! @method     getCursor:
    @abstract   Get a reference to the native Mac representation of a cursor.
    @discussion Gets a reference to the Mac native implementation of a cursor.
                If the cursor has been requested before, it is retreived from
                the cursor cache, otherwise it is created and cached.
    @param      aCursor the cursor to get
    @result     the Mac native implementation of the cursor
*/
- (nsMacCursor*)getCursor:(nsCursor)aCursor;

/*! @method     setMacCursor:
 @abstract   Set the current Mac native cursor
 @discussion Sets the current cursor - this routine is what actually causes the
             cursor to change. The argument is retained and the old cursor is
             released.
 @param      aMacCursor the cursor to set
 @result     NS_OK
 */
- (nsresult)setMacCursor:(nsMacCursor*)aMacCursor;

/*! @method     createCursor:
    @abstract   Create a Mac native representation of a cursor.
    @discussion Creates a version of the Mac native representation of this
                cursor.
    @param      aCursor the cursor to create
    @result     the Mac native implementation of the cursor
*/
+ (nsMacCursor*)createCursor:(enum nsCursor)aCursor;

@end

@implementation nsCursorManager

+ (nsCursorManager*)sharedInstance {
  NS_OBJC_BEGIN_TRY_BLOCK_RETURN;

  if (!gInstance) {
    gInstance = [[nsCursorManager alloc] init];
  }
  return gInstance;

  NS_OBJC_END_TRY_BLOCK_RETURN(nil);
}

+ (void)dispose {
  NS_OBJC_BEGIN_TRY_IGNORE_BLOCK;

  [gInstance release];
  gInstance = nil;

  NS_OBJC_END_TRY_IGNORE_BLOCK;
}

+ (nsMacCursor*)createCursor:(enum nsCursor)aCursor {
  NS_OBJC_BEGIN_TRY_BLOCK_RETURN;

  switch (aCursor) {
    case eCursor_standard:
      return [nsMacCursor cursorWithCursor:[NSCursor arrowCursor] type:aCursor];
    case eCursor_wait:
    case eCursor_spinning: {
      return [nsMacCursor cursorWithCursor:[NSCursor busyButClickableCursor]
                                      type:aCursor];
    }
    case eCursor_select:
      return [nsMacCursor cursorWithCursor:[NSCursor IBeamCursor] type:aCursor];
    case eCursor_hyperlink:
      return [nsMacCursor cursorWithCursor:[NSCursor pointingHandCursor]
                                      type:aCursor];
    case eCursor_crosshair:
      return [nsMacCursor cursorWithCursor:[NSCursor crosshairCursor]
                                      type:aCursor];
    case eCursor_move:
      return [nsMacCursor cursorWithImageNamed:@"move"
                                       hotSpot:NSMakePoint(12, 12)
                                          type:aCursor];
    case eCursor_help:
      return [nsMacCursor cursorWithImageNamed:@"help"
                                       hotSpot:NSMakePoint(12, 12)
                                          type:aCursor];
    case eCursor_copy: {
      SEL cursorSelector = @selector(dragCopyCursor);
      return [nsMacCursor
          cursorWithCursor:[NSCursor respondsToSelector:cursorSelector]
                               ? [NSCursor performSelector:cursorSelector]
                               : [NSCursor arrowCursor]
                      type:aCursor];
    }
    case eCursor_alias: {
      SEL cursorSelector = @selector(dragLinkCursor);
      return [nsMacCursor
          cursorWithCursor:[NSCursor respondsToSelector:cursorSelector]
                               ? [NSCursor performSelector:cursorSelector]
                               : [NSCursor arrowCursor]
                      type:aCursor];
    }
    case eCursor_context_menu: {
      SEL cursorSelector = @selector(contextualMenuCursor);
      return [nsMacCursor
          cursorWithCursor:[NSCursor respondsToSelector:cursorSelector]
                               ? [NSCursor performSelector:cursorSelector]
                               : [NSCursor arrowCursor]
                      type:aCursor];
    }
    case eCursor_cell:
      return [nsMacCursor cursorWithImageNamed:@"cell"
                                       hotSpot:NSMakePoint(12, 12)
                                          type:aCursor];
    case eCursor_grab:
      return [nsMacCursor cursorWithCursor:[NSCursor openHandCursor]
                                      type:aCursor];
    case eCursor_grabbing:
      return [nsMacCursor cursorWithCursor:[NSCursor closedHandCursor]
                                      type:aCursor];
    case eCursor_zoom_in:
      return [nsMacCursor cursorWithImageNamed:@"zoomIn"
                                       hotSpot:NSMakePoint(10, 10)
                                          type:aCursor];
    case eCursor_zoom_out:
      return [nsMacCursor cursorWithImageNamed:@"zoomOut"
                                       hotSpot:NSMakePoint(10, 10)
                                          type:aCursor];
    case eCursor_vertical_text:
      return [nsMacCursor cursorWithImageNamed:@"vtIBeam"
                                       hotSpot:NSMakePoint(12, 11)
                                          type:aCursor];
    case eCursor_all_scroll:
      return [nsMacCursor cursorWithCursor:[NSCursor openHandCursor]
                                      type:aCursor];
    case eCursor_not_allowed:
    case eCursor_no_drop: {
      SEL cursorSelector = @selector(operationNotAllowedCursor);
      return [nsMacCursor
          cursorWithCursor:[NSCursor respondsToSelector:cursorSelector]
                               ? [NSCursor performSelector:cursorSelector]
                               : [NSCursor arrowCursor]
                      type:aCursor];
    }
    // Resize Cursors:
    // North
    case eCursor_n_resize:
      return [nsMacCursor cursorWithCursor:[NSCursor resizeUpCursor]
                                      type:aCursor];
    // North East
    case eCursor_ne_resize:
      return [nsMacCursor cursorWithImageNamed:@"sizeNE"
                                       hotSpot:NSMakePoint(12, 11)
                                          type:aCursor];
    // East
    case eCursor_e_resize:
      return [nsMacCursor cursorWithCursor:[NSCursor resizeRightCursor]
                                      type:aCursor];
    // South East
    case eCursor_se_resize:
      return [nsMacCursor cursorWithImageNamed:@"sizeSE"
                                       hotSpot:NSMakePoint(12, 12)
                                          type:aCursor];
    // South
    case eCursor_s_resize:
      return [nsMacCursor cursorWithCursor:[NSCursor resizeDownCursor]
                                      type:aCursor];
    // South West
    case eCursor_sw_resize:
      return [nsMacCursor cursorWithImageNamed:@"sizeSW"
                                       hotSpot:NSMakePoint(10, 12)
                                          type:aCursor];
    // West
    case eCursor_w_resize:
      return [nsMacCursor cursorWithCursor:[NSCursor resizeLeftCursor]
                                      type:aCursor];
    // North West
    case eCursor_nw_resize:
      return [nsMacCursor cursorWithImageNamed:@"sizeNW"
                                       hotSpot:NSMakePoint(11, 11)
                                          type:aCursor];
    // North & South
    case eCursor_ns_resize:
      return [nsMacCursor cursorWithCursor:[NSCursor resizeUpDownCursor]
                                      type:aCursor];
    // East & West
    case eCursor_ew_resize:
      return [nsMacCursor cursorWithCursor:[NSCursor resizeLeftRightCursor]
                                      type:aCursor];
    // North East & South West
    case eCursor_nesw_resize:
      return [nsMacCursor cursorWithImageNamed:@"sizeNESW"
                                       hotSpot:NSMakePoint(12, 12)
                                          type:aCursor];
    // North West & South East
    case eCursor_nwse_resize:
      return [nsMacCursor cursorWithImageNamed:@"sizeNWSE"
                                       hotSpot:NSMakePoint(12, 12)
                                          type:aCursor];
    // Column Resize
    case eCursor_col_resize:
      return [nsMacCursor cursorWithImageNamed:@"colResize"
                                       hotSpot:NSMakePoint(12, 12)
                                          type:aCursor];
    // Row Resize
    case eCursor_row_resize:
      return [nsMacCursor cursorWithImageNamed:@"rowResize"
                                       hotSpot:NSMakePoint(12, 12)
                                          type:aCursor];
    default:
      return [nsMacCursor cursorWithCursor:[NSCursor arrowCursor] type:aCursor];
  }

  NS_OBJC_END_TRY_BLOCK_RETURN(nil);
}

- (id)init {
  NS_OBJC_BEGIN_TRY_BLOCK_RETURN;

  if ((self = [super init])) {
    mCursors = [[NSMutableDictionary alloc] initWithCapacity:25];
  }
  return self;

  NS_OBJC_END_TRY_BLOCK_RETURN(nil);
}

- (nsresult)setNonCustomCursor:(const nsIWidget::Cursor&)aCursor {
  NS_OBJC_BEGIN_TRY_BLOCK_RETURN;

  [self setMacCursor:[self getCursor:aCursor.mDefaultCursor]];

  sCurrentCursor = aCursor;
  return NS_OK;

  NS_OBJC_END_TRY_BLOCK_RETURN(NS_ERROR_FAILURE);
}

- (nsresult)setMacCursor:(nsMacCursor*)aMacCursor {
  NS_OBJC_BEGIN_TRY_BLOCK_RETURN;

  nsCursor oldType = [mCurrentMacCursor type];
  nsCursor newType = [aMacCursor type];
  if (oldType != newType) {
    if (newType == eCursor_none) {
      [NSCursor hide];
    } else if (oldType == eCursor_none) {
      [NSCursor unhide];
    }
  }

  if (mCurrentMacCursor != aMacCursor || ![mCurrentMacCursor isSet]) {
    [aMacCursor retain];
    [mCurrentMacCursor unset];
    [aMacCursor set];
    [mCurrentMacCursor release];
    mCurrentMacCursor = aMacCursor;
  }

  return NS_OK;

  NS_OBJC_END_TRY_BLOCK_RETURN(NS_ERROR_FAILURE);
}

- (nsresult)setCustomCursor:(const nsIWidget::Cursor&)aCursor
          widgetScaleFactor:(CGFloat)scaleFactor {
  NS_OBJC_BEGIN_TRY_BLOCK_RETURN;

  // As the user moves the mouse, this gets called repeatedly with the same
  // aCursorImage
  if (sCurrentCursor == aCursor && sCurrentCursorScaleFactor == scaleFactor &&
      mCurrentMacCursor) {
    // Native dragging can unset our cursor apparently (see bug 1739352).
    if (MOZ_UNLIKELY(![mCurrentMacCursor isSet])) {
      [mCurrentMacCursor set];
    }
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
      aCursor.mContainer, imgIContainer::FRAME_FIRST, nullptr, nullptr,
      &cursorImage, scaleFactor);
  if (NS_FAILED(rv) || !cursorImage) {
    return NS_ERROR_FAILURE;
  }

  {
    NSSize cocoaSize = NSMakeSize(size.width, size.height);
    [cursorImage setSize:cocoaSize];
    [[[cursorImage representations] objectAtIndex:0] setSize:cocoaSize];
  }

  // if the hotspot is nonsensical, make it 0,0
  uint32_t hotspotX =
      aCursor.mHotspotX > (uint32_t(size.width) - 1) ? 0 : aCursor.mHotspotX;
  uint32_t hotspotY =
      aCursor.mHotspotY > (uint32_t(size.height) - 1) ? 0 : aCursor.mHotspotY;
  NSPoint hotSpot = ::NSMakePoint(hotspotX, hotspotY);
  [self setMacCursor:[nsMacCursor cursorWithCursor:[[NSCursor alloc]
                                                       initWithImage:cursorImage
                                                             hotSpot:hotSpot]
                                              type:kCustomCursor]];
  [cursorImage release];
  return NS_OK;

  NS_OBJC_END_TRY_BLOCK_RETURN(NS_ERROR_FAILURE);
}

- (nsMacCursor*)getCursor:(enum nsCursor)aCursor {
  NS_OBJC_BEGIN_TRY_BLOCK_RETURN;

  nsMacCursor* result =
      [mCursors objectForKey:[NSNumber numberWithInt:aCursor]];
  if (!result) {
    result = [nsCursorManager createCursor:aCursor];
    [mCursors setObject:result forKey:[NSNumber numberWithInt:aCursor]];
  }
  return result;

  NS_OBJC_END_TRY_BLOCK_RETURN(nil);
}

- (void)dealloc {
  NS_OBJC_BEGIN_TRY_IGNORE_BLOCK;

  [mCurrentMacCursor unset];
  [mCurrentMacCursor release];
  [mCursors release];
  sCurrentCursor = {};
  [super dealloc];

  NS_OBJC_END_TRY_IGNORE_BLOCK;
}

@end
