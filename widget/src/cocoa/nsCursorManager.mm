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
* The Initial Developer of the Original Code is Andrew Thompson.
* Portions created by the Andrew Thompson are Copyright (C) 2004
* Andrew Thompson. All Rights Reserved.
*
* Contributor(s):
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

#import "nsCursorManager.h"
#import "math.h"

static nsCursorManager *gInstance;

/*! @category nsCursorManager(PrivateMethods)
    Private methods for the cursor manager class.
*/
@interface nsCursorManager(PrivateMethods)
/*! @method     getCursor:
    @abstract   Get a reference to the native Mac representation of a cursor.
    @discussion Gets a reference to the Mac native implementation of a cursor.
                If the cursor has been requested before, it is retreived from the cursor cache, otherwise is it created
                and cached.
    @param      aCursor the cursor to get
    @result     the Mac native implementation of the cursor
*/
- (nsMacCursor *) getCursor: (nsCursor) aCursor;

/*! @method     createCursor:
    @abstract   Create a Mac native representation of a cursor.
    @discussion Creates a version of the Mac native representation of this cursor suitable for use on this version of
                Mac OS X
    @param      aCursor the cursor to create
    @result     the Mac native implementation of the cursor
*/
+ (nsMacCursor *) createCursor: (enum nsCursor) aCursor;

/*! @method     createNSCursor:orThemeCursor:
    @abstract   Creates the appropriate cursor implementation from the arguments.
    @discussion Creates a native Mac cursor, using NSCursor if the cursor is available on this version of Mac OS X,
                otherwise falls back to creating a traditional Carbon AppearanceManager ThemeCursor.
    @param      aPantherCursor selector indicating the NSCursor cursor to create if on Panther or later
    @param      aJaguarCursor the ThemeCursor to use as an alternative if running on Jaguar
    @result     the Mac native implementation of the cursor
*/
+ (nsMacCursor *) createNSCursor: (SEL) aPantherCursor orThemeCursor: (ThemeCursor) aJaguarCursor;

/*! @method     createNSCursor:orImageCursor:withHotspot:
    @abstract   Creates the appropriate cursor implementation from the arguments.
    @discussion Creates a native Mac cursor, using NSCursor if the cursor is available on this version of Mac OS X,
    otherwise falls back to creating an NSCursor instance from a custom image and hotspot.
    @param      aPantherCursor selector indicating the NSCursor cursor to create if on Panther or later
    @param      aImageName the name of the image to use for the cursor as the cursor on Jagua
    @param      aPoint the hotspot to use with the image to form a cursor on Jaguar
    @result     the Mac native implementation of the cursor
*/
+ (nsMacCursor *) createNSCursor: (SEL) aPantherCursor orImageCursor: (NSString *) aImageName withHotspot: (NSPoint) aPoint;
@end

/*! @function   isPantherOrLater
    @abstract   Determine whether we are running on Panther (Mac OS X 10.3) or later
    @result     YES if the current operating system version is 10.3 or later, else NO
*/
static BOOL isPantherOrLater();

static BOOL isPantherOrLater() 
{
  static PRBool gInitVer = PR_FALSE;
  static PRBool gOnPantherOrLater = PR_FALSE;
  if(!gInitVer)
  {
    long version;
    OSErr err = ::Gestalt(gestaltSystemVersion, &version);
    gOnPantherOrLater = (err == noErr && version >= 0x00001030);
    gInitVer = PR_TRUE;
  }
  return gOnPantherOrLater;
}

@implementation nsCursorManager

+ (nsCursorManager *) sharedInstance
{
  if (gInstance == nil)
  {
    gInstance = [[nsCursorManager alloc] init];
  }
  return gInstance;
}

+ (void) dispose
{
  [gInstance release];
  gInstance = nil;
}

+ (nsMacCursor *) createCursor: (enum nsCursor) aCursor 
{
  switch(aCursor)
  {
    case eCursor_standard:
      return [nsMacCursor cursorWithCursor: [NSCursor arrowCursor]];
    case eCursor_wait:
      return [nsMacCursor cursorWithThemeCursor: kThemeWatchCursor];
    case eCursor_select:              
      return [nsMacCursor cursorWithCursor: [NSCursor IBeamCursor]];
    case eCursor_hyperlink:
      return [nsCursorManager createNSCursor: @selector(pointingHandCursor) orThemeCursor: kThemePointingHandCursor];                  
    case eCursor_crosshair:
      return [nsCursorManager createNSCursor: @selector(crosshairCursor) orThemeCursor: kThemeCrossCursor];                                        
    case eCursor_move:
      return [nsCursorManager createNSCursor: @selector(openHandCursor) orThemeCursor: kThemeOpenHandCursor];                   
    case eCursor_help:
      return [nsMacCursor cursorWithImageNamed: @"help" hotSpot: NSMakePoint(1,1)];        
    case eCursor_copy:
      return [nsMacCursor cursorWithThemeCursor: kThemeCopyArrowCursor];
    case eCursor_alias:
      return [nsMacCursor cursorWithThemeCursor: kThemeAliasArrowCursor];
    case eCursor_context_menu:
      return [nsMacCursor cursorWithThemeCursor: kThemeContextualMenuArrowCursor];

    case eCursor_cell:
      return [nsMacCursor cursorWithThemeCursor: kThemePlusCursor];
    case eCursor_grab:
      return [nsCursorManager createNSCursor: @selector(openHandCursor) orThemeCursor: kThemeOpenHandCursor];
    case eCursor_grabbing:
      return [nsCursorManager createNSCursor: @selector(closedHandCursor) orThemeCursor: kThemeClosedHandCursor];
    case eCursor_spinning:
      return [nsMacCursor cursorWithResources: 200 lastFrame: 203]; // better than kThemeSpinningCursor        
    case eCursor_count_up:
      return [nsMacCursor cursorWithThemeCursor: kThemeCountingUpHandCursor];
    case eCursor_count_down:
      return [nsMacCursor cursorWithThemeCursor: kThemeCountingDownHandCursor];
    case eCursor_count_up_down:
      return [nsMacCursor cursorWithThemeCursor: kThemeCountingUpAndDownHandCursor];
    case eCursor_zoom_in:
      return [nsMacCursor cursorWithImageNamed: @"zoomIn" hotSpot: NSMakePoint(6,6)];
    case eCursor_zoom_out:
      return [nsMacCursor cursorWithImageNamed: @"zoomOut" hotSpot: NSMakePoint(6,6)];
    case eCursor_vertical_text:
      return [nsMacCursor cursorWithImageNamed: @"vtIBeam" hotSpot: NSMakePoint(7,8)];
    case eCursor_all_scroll:
      return [nsCursorManager createNSCursor: @selector(openHandCursor) orThemeCursor: kThemeOpenHandCursor];                   
    case eCursor_not_allowed:
    case eCursor_no_drop:
      return [nsMacCursor cursorWithThemeCursor: kThemeNotAllowedCursor];

    // Arrow Cursors:
    //North
    case eCursor_n_resize:
    case eCursor_arrow_north:
    case eCursor_arrow_north_plus:
        return [nsCursorManager createNSCursor: @selector(resizeUpCursor) orImageCursor: @"arrowN" withHotspot: NSMakePoint(7,7)];
    //North East
    case eCursor_ne_resize:
        return [nsMacCursor cursorWithImageNamed: @"sizeNE" hotSpot: NSMakePoint(8,7)];
    //East
    case eCursor_e_resize:        
    case eCursor_arrow_east:
    case eCursor_arrow_east_plus:
        return [nsCursorManager createNSCursor: @selector(resizeRightCursor) orThemeCursor: kThemeResizeRightCursor];
    //South East
    case eCursor_se_resize:
        return [nsMacCursor cursorWithImageNamed: @"sizeSE" hotSpot: NSMakePoint(8,8)];
    //South
    case eCursor_s_resize:
    case eCursor_arrow_south:
    case eCursor_arrow_south_plus:
        return [nsCursorManager createNSCursor: @selector(resizeDownCursor) orImageCursor: @"arrowS" withHotspot: NSMakePoint(7,7)];
    //South West
    case eCursor_sw_resize:
        return [nsMacCursor cursorWithImageNamed: @"sizeSW" hotSpot: NSMakePoint(6,8)];
    //West
    case eCursor_w_resize:
    case eCursor_arrow_west:
    case eCursor_arrow_west_plus:
        return [nsCursorManager createNSCursor: @selector(resizeLeftCursor) orThemeCursor: kThemeResizeLeftCursor];
    //North West
    case eCursor_nw_resize:
        return [nsMacCursor cursorWithImageNamed: @"sizeNW" hotSpot: NSMakePoint(7,7)];
    //North & South
    case eCursor_ns_resize:
        return [nsCursorManager createNSCursor: @selector(resizeUpDownCursor) orImageCursor: @"sizeNS" withHotspot: NSMakePoint(7,7)];                         
    //East & West
    case eCursor_ew_resize:
        return [nsCursorManager createNSCursor: @selector(resizeLeftRightCursor) orThemeCursor: kThemeResizeLeftRightCursor];                  
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
}

+ (nsMacCursor *) createNSCursor: (SEL) aPantherCursor orThemeCursor: (ThemeCursor) aJaguarCursor
{
  return isPantherOrLater() ?   [nsMacCursor cursorWithCursor: [NSCursor performSelector: aPantherCursor]] :
                                [nsMacCursor cursorWithThemeCursor: aJaguarCursor];
}

+ (nsMacCursor *) createNSCursor: (SEL) aPantherCursor orImageCursor: (NSString *) aImageName withHotspot: (NSPoint) aPoint
{
  return isPantherOrLater() ?	[nsMacCursor cursorWithCursor: [NSCursor performSelector: aPantherCursor]] :
                                [nsMacCursor cursorWithImageNamed: aImageName hotSpot: aPoint];
}

- (id) init
{
  if ( (self = [super init]) ) 
  {
    mCursors = [[NSMutableDictionary alloc] initWithCapacity: 25];
  }
  return self;
}

- (void) setCursor: (enum nsCursor) aCursor
{
  if (aCursor != mCurrentCursor)
  {
    [[self getCursor: mCurrentCursor] unset];
    [[self getCursor: aCursor] set];
    mCurrentCursor = aCursor;
  }
}

- (nsMacCursor *) getCursor: (enum nsCursor) aCursor
{
  nsMacCursor * result = [mCursors objectForKey: [NSNumber numberWithInt: aCursor]];
  if ( result == nil ) 
  {
    result = [nsCursorManager createCursor: aCursor];
    [mCursors setObject: result forKey: [NSNumber numberWithInt: aCursor]];
  }
  return result;
}

- (void) dealloc
{
  [[self getCursor: mCurrentCursor] unset];
  [mCursors release];    
  [super dealloc];    
}
@end
