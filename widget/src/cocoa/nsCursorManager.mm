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
 * The Original Code is nsCursors.
 *
 * The Initial Developer of the Original Code is 
 * Andrew Thompson.
 * Portions created by the Andrew Thompson are Copyright (C) 2004
 * Andrew Thompson. All Rights Reserved.
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

#include "nsCursorManager.h"
#include "nsObjCExceptions.h"
#include <math.h>

static nsCursorManager *gInstance;
static NSArray* sSpinCursorFrames = nil;

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

  if (!sSpinCursorFrames) {
    NSCursor* cursor1 = [nsMacCursor cocoaCursorWithImageNamed:@"spin1" hotSpot:NSMakePoint(1.0, 1.0)];
    NSCursor* cursor2 = [nsMacCursor cocoaCursorWithImageNamed:@"spin2" hotSpot:NSMakePoint(1.0, 1.0)];
    NSCursor* cursor3 = [nsMacCursor cocoaCursorWithImageNamed:@"spin3" hotSpot:NSMakePoint(1.0, 1.0)];
    NSCursor* cursor4 = [nsMacCursor cocoaCursorWithImageNamed:@"spin4" hotSpot:NSMakePoint(1.0, 1.0)];
    sSpinCursorFrames = [[NSArray alloc] initWithObjects:cursor1, cursor2, cursor3, cursor4, nil];
  }
  return gInstance;

  NS_OBJC_END_TRY_ABORT_BLOCK_NIL;
}

+ (void) dispose
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  [gInstance release];
  gInstance = nil;

  [sSpinCursorFrames release];
  sSpinCursorFrames = nil;

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

+ (nsMacCursor *) createCursor: (enum nsCursor) aCursor 
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NIL;

  switch(aCursor)
  {
    case eCursor_standard:
      return [nsMacCursor cursorWithCursor: [NSCursor arrowCursor]];
    case eCursor_wait:
    case eCursor_spinning:
      return [nsMacCursor cursorWithFrames: sSpinCursorFrames];
    case eCursor_select:
      return [nsMacCursor cursorWithCursor: [NSCursor IBeamCursor]];
    case eCursor_hyperlink:
      return [nsMacCursor cursorWithCursor: [NSCursor pointingHandCursor]];
    case eCursor_crosshair:
      return [nsMacCursor cursorWithCursor: [NSCursor crosshairCursor]];
    case eCursor_move:
      return [nsMacCursor cursorWithCursor: [NSCursor openHandCursor]];
    case eCursor_help:
      return [nsMacCursor cursorWithImageNamed: @"help" hotSpot: NSMakePoint(1,1)];
    case eCursor_copy:
      return [nsMacCursor cursorWithCursor: [NSCursor arrowCursor]]; //XXX needs real implementation
    case eCursor_alias:
      return [nsMacCursor cursorWithCursor: [NSCursor arrowCursor]]; //XXX needs real implementation
    case eCursor_context_menu:
      return [nsMacCursor cursorWithCursor: [NSCursor arrowCursor]]; //XXX needs real implementation

    case eCursor_cell:
      return [nsMacCursor cursorWithCursor: [NSCursor crosshairCursor]];
    case eCursor_grab:
      return [nsMacCursor cursorWithCursor: [NSCursor openHandCursor]];
    case eCursor_grabbing:
      return [nsMacCursor cursorWithCursor: [NSCursor closedHandCursor]];
    case eCursor_zoom_in:
      return [nsMacCursor cursorWithImageNamed: @"zoomIn" hotSpot: NSMakePoint(6,6)];
    case eCursor_zoom_out:
      return [nsMacCursor cursorWithImageNamed: @"zoomOut" hotSpot: NSMakePoint(6,6)];
    case eCursor_vertical_text:
      return [nsMacCursor cursorWithImageNamed: @"vtIBeam" hotSpot: NSMakePoint(7,8)];
    case eCursor_all_scroll:
      return [nsMacCursor cursorWithCursor: [NSCursor openHandCursor]];;
    case eCursor_not_allowed:
    case eCursor_no_drop:
      return [nsMacCursor cursorWithCursor: [NSCursor arrowCursor]]; //XXX needs real implementation

    // Resize Cursors:
    //North
    case eCursor_n_resize:
        return [nsMacCursor cursorWithCursor: [NSCursor resizeUpCursor]];
    //North East
    case eCursor_ne_resize:
        return [nsMacCursor cursorWithImageNamed: @"sizeNE" hotSpot: NSMakePoint(8,7)];
    //East
    case eCursor_e_resize:        
        return [nsMacCursor cursorWithCursor: [NSCursor resizeRightCursor]];
    //South East
    case eCursor_se_resize:
        return [nsMacCursor cursorWithImageNamed: @"sizeSE" hotSpot: NSMakePoint(8,8)];
    //South
    case eCursor_s_resize:
        return [nsMacCursor cursorWithCursor: [NSCursor resizeDownCursor]];
    //South West
    case eCursor_sw_resize:
        return [nsMacCursor cursorWithImageNamed: @"sizeSW" hotSpot: NSMakePoint(6,8)];
    //West
    case eCursor_w_resize:
        return [nsMacCursor cursorWithCursor: [NSCursor resizeLeftCursor]];
    //North West
    case eCursor_nw_resize:
        return [nsMacCursor cursorWithImageNamed: @"sizeNW" hotSpot: NSMakePoint(7,7)];
    //North & South
    case eCursor_ns_resize:
        return [nsMacCursor cursorWithCursor: [NSCursor resizeUpDownCursor]];                         
    //East & West
    case eCursor_ew_resize:
        return [nsMacCursor cursorWithCursor: [NSCursor resizeLeftRightCursor]];                  
    //North East & South West
    case eCursor_nesw_resize:
        return [nsMacCursor cursorWithImageNamed: @"sizeNESW" hotSpot: NSMakePoint(8,8)];
    //North West & South East        
    case eCursor_nwse_resize:
        return [nsMacCursor cursorWithImageNamed: @"sizeNWSE" hotSpot: NSMakePoint(8,8)];
    //Column Resize
    case eCursor_col_resize:
        return [nsMacCursor cursorWithImageNamed: @"colResize" hotSpot: NSMakePoint(8,8)];
    //Row Resize        
    case eCursor_row_resize:
        return [nsMacCursor cursorWithImageNamed: @"rowResize" hotSpot: NSMakePoint(8,8)];
    default:
      return [nsMacCursor cursorWithCursor: [NSCursor arrowCursor]];
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

- (void) setCursor: (enum nsCursor) aCursor
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  // Some plugins mess with our cursors and set a cursor that even
  // [NSCursor currentCursor] doesn't know about. In case that happens, just
  // reset the state.
  [[NSCursor currentCursor] set];

  nsMacCursor* currentCursor = [self getCursor: mCurrentCursor];

  if (aCursor != mCurrentCursor || ![currentCursor isSet]) {
    [currentCursor unset];
    [[self getCursor: aCursor] set];
  }

  if (mCurrentCursor != aCursor) {
    if (aCursor == eCursor_none) {
      [NSCursor hide];
    } else if (mCurrentCursor == eCursor_none) {
      [NSCursor unhide];
    }
  }

  mCurrentCursor = aCursor;

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

- (nsMacCursor *) getCursor: (enum nsCursor) aCursor
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NIL;

  nsMacCursor * result = [mCursors objectForKey: [NSNumber numberWithInt: aCursor]];
  if (!result) {
    result = [nsCursorManager createCursor: aCursor];
    [mCursors setObject: result forKey: [NSNumber numberWithInt: aCursor]];
  }
  return result;

  NS_OBJC_END_TRY_ABORT_BLOCK_NIL;
}

- (void) dealloc
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  [[self getCursor: mCurrentCursor] unset];
  [mCursors release];    
  [super dealloc];

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

@end
