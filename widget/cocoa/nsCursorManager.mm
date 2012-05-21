/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "imgIContainer.h"
#include "nsCocoaUtils.h"
#include "nsCursorManager.h"
#include "nsObjCExceptions.h"
#include <math.h>

static nsCursorManager *gInstance;
static imgIContainer *sCursorImgContainer = nsnull;
static const nsCursor sCustomCursor = eCursorCount;

/*! @category nsCursorManager(PrivateMethods)
    Private methods for the cursor manager class.
*/
@interface nsCursorManager(PrivateMethods)
/*! @method     getCursor:
    @abstract   Get a reference to the native Mac representation of a cursor.
    @discussion Gets a reference to the Mac native implementation of a cursor.
                If the cursor has been requested before, it is retreived from the cursor cache,
                otherwise it is created and cached.
    @param      aCursor the cursor to get
    @result     the Mac native implementation of the cursor
*/
- (nsMacCursor *) getCursor: (nsCursor) aCursor;

/*! @method     setMacCursor:
 @abstract   Set the current Mac native cursor
 @discussion Sets the current cursor - this routine is what actually causes the cursor to change.
 The argument is retained and the old cursor is released.
 @param      aMacCursor the cursor to set
 @result     NS_OK
 */
- (nsresult) setMacCursor: (nsMacCursor*) aMacCursor;

/*! @method     createCursor:
    @abstract   Create a Mac native representation of a cursor.
    @discussion Creates a version of the Mac native representation of this cursor
    @param      aCursor the cursor to create
    @result     the Mac native implementation of the cursor
*/
+ (nsMacCursor *) createCursor: (enum nsCursor) aCursor;

@end

@implementation nsCursorManager

+ (nsCursorManager *) sharedInstance
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NIL;

  if (!gInstance) {
    gInstance = [[nsCursorManager alloc] init];
  }
  return gInstance;

  NS_OBJC_END_TRY_ABORT_BLOCK_NIL;
}

+ (void) dispose
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  [gInstance release];
  gInstance = nil;

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

+ (nsMacCursor *) createCursor: (enum nsCursor) aCursor
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NIL;

  switch(aCursor)
  {
    SEL cursorSelector;
    case eCursor_standard:
      return [nsMacCursor cursorWithCursor:[NSCursor arrowCursor] type:aCursor];
    case eCursor_wait:
    case eCursor_spinning:
    {
      NSCursor* cursor1 = [nsMacCursor cocoaCursorWithImageNamed:@"spin1" hotSpot:NSMakePoint(1.0, 1.0)];
      NSCursor* cursor2 = [nsMacCursor cocoaCursorWithImageNamed:@"spin2" hotSpot:NSMakePoint(1.0, 1.0)];
      NSCursor* cursor3 = [nsMacCursor cocoaCursorWithImageNamed:@"spin3" hotSpot:NSMakePoint(1.0, 1.0)];
      NSCursor* cursor4 = [nsMacCursor cocoaCursorWithImageNamed:@"spin4" hotSpot:NSMakePoint(1.0, 1.0)];
      NSArray* spinCursorFrames = [[[NSArray alloc] initWithObjects:cursor1, cursor2, cursor3, cursor4, nil] autorelease];
      return [nsMacCursor cursorWithFrames:spinCursorFrames type:aCursor];
    }
    case eCursor_select:
      return [nsMacCursor cursorWithCursor:[NSCursor IBeamCursor] type:aCursor];
    case eCursor_hyperlink:
      return [nsMacCursor cursorWithCursor:[NSCursor pointingHandCursor] type:aCursor];
    case eCursor_crosshair:
      return [nsMacCursor cursorWithCursor:[NSCursor crosshairCursor] type:aCursor];
    case eCursor_move:
      return [nsMacCursor cursorWithCursor:[NSCursor openHandCursor] type:aCursor];
    case eCursor_help:
      return [nsMacCursor cursorWithImageNamed:@"help" hotSpot:NSMakePoint(1,1) type:aCursor];
    case eCursor_copy:
      cursorSelector = @selector(dragCopyCursor);
      return [nsMacCursor cursorWithCursor:[NSCursor respondsToSelector:cursorSelector] ?
              [NSCursor performSelector:cursorSelector] :
              [NSCursor arrowCursor] type:aCursor];
    case eCursor_alias:
      cursorSelector = @selector(dragLinkCursor);
      return [nsMacCursor cursorWithCursor:[NSCursor respondsToSelector:cursorSelector] ?
              [NSCursor performSelector:cursorSelector] :
              [NSCursor arrowCursor] type:aCursor];
    case eCursor_context_menu:
      cursorSelector = @selector(contextualMenuCursor);
      return [nsMacCursor cursorWithCursor:[NSCursor respondsToSelector:cursorSelector] ?
              [NSCursor performSelector:cursorSelector] :
              [NSCursor arrowCursor] type:aCursor];
    case eCursor_cell:
      return [nsMacCursor cursorWithCursor:[NSCursor crosshairCursor] type:aCursor];
    case eCursor_grab:
      return [nsMacCursor cursorWithCursor:[NSCursor openHandCursor] type:aCursor];
    case eCursor_grabbing:
      return [nsMacCursor cursorWithCursor:[NSCursor closedHandCursor] type:aCursor];
    case eCursor_zoom_in:
      return [nsMacCursor cursorWithImageNamed:@"zoomIn" hotSpot:NSMakePoint(6,6) type:aCursor];
    case eCursor_zoom_out:
      return [nsMacCursor cursorWithImageNamed:@"zoomOut" hotSpot:NSMakePoint(6,6) type:aCursor];
    case eCursor_vertical_text:
      return [nsMacCursor cursorWithImageNamed:@"vtIBeam" hotSpot:NSMakePoint(7,8) type:aCursor];
    case eCursor_all_scroll:
      return [nsMacCursor cursorWithCursor:[NSCursor openHandCursor] type:aCursor];
    case eCursor_not_allowed:
    case eCursor_no_drop:
      cursorSelector = @selector(operationNotAllowedCursor);
      return [nsMacCursor cursorWithCursor:[NSCursor respondsToSelector:cursorSelector] ?
              [NSCursor performSelector:cursorSelector] :
              [NSCursor arrowCursor] type:aCursor];
    // Resize Cursors:
    // North
    case eCursor_n_resize:
        return [nsMacCursor cursorWithCursor:[NSCursor resizeUpCursor] type:aCursor];
    // North East
    case eCursor_ne_resize:
        return [nsMacCursor cursorWithImageNamed:@"sizeNE" hotSpot:NSMakePoint(8,7) type:aCursor];
    // East
    case eCursor_e_resize:
        return [nsMacCursor cursorWithCursor:[NSCursor resizeRightCursor] type:aCursor];
    // South East
    case eCursor_se_resize:
        return [nsMacCursor cursorWithImageNamed:@"sizeSE" hotSpot:NSMakePoint(8,8) type:aCursor];
    // South
    case eCursor_s_resize:
        return [nsMacCursor cursorWithCursor:[NSCursor resizeDownCursor] type:aCursor];
    // South West
    case eCursor_sw_resize:
        return [nsMacCursor cursorWithImageNamed:@"sizeSW" hotSpot:NSMakePoint(6,8) type:aCursor];
    // West
    case eCursor_w_resize:
        return [nsMacCursor cursorWithCursor:[NSCursor resizeLeftCursor] type:aCursor];
    // North West
    case eCursor_nw_resize:
        return [nsMacCursor cursorWithImageNamed:@"sizeNW" hotSpot:NSMakePoint(7,7) type:aCursor];
    // North & South
    case eCursor_ns_resize:
        return [nsMacCursor cursorWithCursor:[NSCursor resizeUpDownCursor] type:aCursor];
    // East & West
    case eCursor_ew_resize:
        return [nsMacCursor cursorWithCursor:[NSCursor resizeLeftRightCursor] type:aCursor];
    // North East & South West
    case eCursor_nesw_resize:
        return [nsMacCursor cursorWithImageNamed:@"sizeNESW" hotSpot:NSMakePoint(8,8) type:aCursor];
    // North West & South East
    case eCursor_nwse_resize:
        return [nsMacCursor cursorWithImageNamed:@"sizeNWSE" hotSpot:NSMakePoint(8,8) type:aCursor];
    // Column Resize
    case eCursor_col_resize:
        return [nsMacCursor cursorWithImageNamed:@"colResize" hotSpot:NSMakePoint(8,8) type:aCursor];
    // Row Resize
    case eCursor_row_resize:
        return [nsMacCursor cursorWithImageNamed:@"rowResize" hotSpot:NSMakePoint(8,8) type:aCursor];
    default:
        return [nsMacCursor cursorWithCursor:[NSCursor arrowCursor] type:aCursor];
  }

  NS_OBJC_END_TRY_ABORT_BLOCK_NIL;
}

- (id) init
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NIL;

  if ((self = [super init])) {
    mCursors = [[NSMutableDictionary alloc] initWithCapacity:25];
  }
  return self;

  NS_OBJC_END_TRY_ABORT_BLOCK_NIL;
}

- (nsresult) setCursor: (enum nsCursor) aCursor
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  // Some plugins mess with our cursors and set a cursor that even
  // [NSCursor currentCursor] doesn't know about. In case that happens, just
  // reset the state.
  [[NSCursor currentCursor] set];

  nsCursor oldType = [mCurrentMacCursor type];
  if (oldType != aCursor) {
    if (aCursor == eCursor_none) {
      [NSCursor hide];
    } else if (oldType == eCursor_none) {
      [NSCursor unhide];
    }
  }
  [self setMacCursor:[self getCursor:aCursor]];

  // if a custom cursor was previously set, release sCursorImgContainer
  if (oldType == sCustomCursor) {
    NS_IF_RELEASE(sCursorImgContainer);
  }
  return NS_OK;
  
  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

- (nsresult) setMacCursor: (nsMacCursor*) aMacCursor
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  if (mCurrentMacCursor != aMacCursor || ![mCurrentMacCursor isSet]) {
    [aMacCursor retain];
    [mCurrentMacCursor unset];
    [aMacCursor set];
    [mCurrentMacCursor release];
    mCurrentMacCursor = aMacCursor;
  }

  return NS_OK;
  
  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

- (nsresult) setCursorWithImage: (imgIContainer*) aCursorImage hotSpotX: (PRUint32) aHotspotX hotSpotY: (PRUint32) aHotspotY
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;
  // As the user moves the mouse, this gets called repeatedly with the same aCursorImage
  if (sCursorImgContainer == aCursorImage && mCurrentMacCursor) {
    [self setMacCursor:mCurrentMacCursor];
    return NS_OK;
  }
  
  [[NSCursor currentCursor] set];
  PRInt32 width = 0, height = 0;
  aCursorImage->GetWidth(&width);
  aCursorImage->GetHeight(&height);
  // prevent DoS attacks
  if (width > 128 || height > 128) {
    return NS_OK;
  }

  NSImage *cursorImage;
  nsresult rv = nsCocoaUtils::CreateNSImageFromImageContainer(aCursorImage, imgIContainer::FRAME_FIRST, &cursorImage);
  if (NS_FAILED(rv) || !cursorImage) {
    return NS_ERROR_FAILURE;
  }

  // if the hotspot is nonsensical, make it 0,0
  aHotspotX = (aHotspotX > (PRUint32)width - 1) ? 0 : aHotspotX;
  aHotspotY = (aHotspotY > (PRUint32)height - 1) ? 0 : aHotspotY;

  NSPoint hotSpot = ::NSMakePoint(aHotspotX, aHotspotY);
  [self setMacCursor:[nsMacCursor cursorWithCursor:[[NSCursor alloc] initWithImage:cursorImage hotSpot:hotSpot] type:sCustomCursor]];
  [cursorImage release];
  
  NS_IF_RELEASE(sCursorImgContainer);
  sCursorImgContainer = aCursorImage;
  NS_ADDREF(sCursorImgContainer);
  
  return NS_OK;
  
  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

- (nsMacCursor *) getCursor: (enum nsCursor) aCursor
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NIL;

  nsMacCursor * result = [mCursors objectForKey:[NSNumber numberWithInt:aCursor]];
  if (!result) {
    result = [nsCursorManager createCursor:aCursor];
    [mCursors setObject:result forKey:[NSNumber numberWithInt:aCursor]];
  }
  return result;

  NS_OBJC_END_TRY_ABORT_BLOCK_NIL;
}

- (void) dealloc
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  [mCurrentMacCursor unset];
  [mCurrentMacCursor release];
  [mCursors release];
  NS_IF_RELEASE(sCursorImgContainer);
  [super dealloc];

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

@end
