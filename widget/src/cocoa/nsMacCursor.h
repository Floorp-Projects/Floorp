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

#ifndef nsMacCursor_h_
#define nsMacCursor_h_

#import <Cocoa/Cocoa.h>

/*! @class      nsMacCursor
    @abstract   Represents a native Mac cursor.
    @discussion <code>nsMacCursor</code> provides a simple API for creating and working with native Macintosh cursors.
                Cursors can be created used without needing to be aware of the way different cursors are implemented,
                in particular the details of managing an animated cursor are hidden.
*/
@interface nsMacCursor : NSObject
{
  @private
  NSTimer *mTimer;
  @protected
  int mFrameCounter;    
}

/*! @method     cursorWithCursor:
    @abstract   Create a cursor by specifying a Cocoa <code>NSCursor</code>.
    @discussion Creates a cursor representing the given Cocoa built-in cursor.
    @param      aCursor the <code>NSCursor</code> to use
    @result     an autoreleased instance of <code>nsMacCursor</code> representing the given <code>NSCursor</code>
 */
+ (nsMacCursor *) cursorWithCursor: (NSCursor *) aCursor;

/*! @method     cursorWithImageNamed:hotSpot:
    @abstract   Create a cursor by specifying the name of an image resource to use for the cursor and a hotspot.
    @discussion Creates a cursor by loading the named image using the <code>+[NSImage imageNamed:]</code> method.
                <p>The image must be compatible with any restrictions laid down by <code>NSCursor</code>. These vary
                by operating system version.</p>
                <p>The hotspot precisely determines the point where the user clicks when using the cursor.</p>
    @param      aCursor the name of the image to use for the cursor
    @param      aPoint the point within the cursor to use as the hotspot
    @result     an autoreleased instance of <code>nsMacCursor</code> that uses the given image and hotspot
 */
+ (nsMacCursor *) cursorWithImageNamed: (NSString *) aCursorImage hotSpot: (NSPoint) aPoint;

/*! @method     cursorWithFrames:
    @abstract   Create an animated cursor by specifying the frames to use for the animation.
    @discussion Creates a cursor that will animate by cycling through the given frames. Each element of the array
                must be an instance of <code>NSCursor</code>
    @param      aCursorFrames an array of <code>NSCursor</code>, representing the frames of an animated cursor, in the
                order they should be played.
    @result     an autoreleased instance of <code>nsMacCursor</code> that will animate the given cursor frames
 */
+ (nsMacCursor *) cursorWithFrames: (NSArray *) aCursorFrames;

/*! @method     cocoaCursorWithImageNamed:hotSpot:
    @abstract   Create a Cocoa NSCursor object with a Gecko image resource name and a hotspot point.
    @discussion Create a Cocoa NSCursor object with a Gecko image resource name and a hotspot point.
    @param      imageName the name of the gecko image resource, "tiff" extension is assumed, do not append.
    @param      aPoint the point within the cursor to use as the hotspot
    @result     an autoreleased instance of <code>nsMacCursor</code> that will animate the given cursor frames
 */
+ (NSCursor *) cocoaCursorWithImageNamed: (NSString *) imageName hotSpot: (NSPoint) aPoint;

/*! @method     isSet
    @abstract   Determines whether this cursor is currently active.
    @discussion This can be helpful when the Cocoa NSCursor state can be influenced without going
                through nsCursorManager.
    @result     whether the cursor is currently set
 */
- (BOOL) isSet;

/*! @method     set
    @abstract   Set the cursor.
    @discussion Makes this cursor the current cursor. If the cursor is animated, the animation is started.
 */
- (void) set;

/*! @method     unset
    @abstract   Unset the cursor. The cursor will return to the default (usually the arrow cursor).
    @discussion Unsets the cursor. If the cursor is animated, the animation is stopped.
 */
- (void) unset;

/*! @method     isAnimated
    @abstract   Tests whether this cursor is animated.
    @discussion Use this method to determine whether a cursor is animated
    @result     YES if the cursor is animated (has more than one frame), NO if it is a simple static cursor.
 */
- (BOOL) isAnimated;

@end

#endif // nsMacCursor_h_
