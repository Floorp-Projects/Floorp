/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsMacCursor_h_
#define nsMacCursor_h_

#import <Cocoa/Cocoa.h>
#import "nsIWidget.h"

/*! @class      nsMacCursor
    @abstract   Represents a native Mac cursor.
    @discussion <code>nsMacCursor</code> provides a simple API for creating and
                working with native Macintosh cursors.  Cursors can be created
                used without needing to be aware of the way different cursors
                are implemented, in particular the details of managing an
                animated cursor are hidden.
*/
@interface nsMacCursor : NSObject {
 @private
  NSTimer* mTimer;
 @protected
  nsCursor mType;
  int mFrameCounter;
}

/*! @method     cursorWithCursor:
    @abstract   Create a cursor by specifying a Cocoa <code>NSCursor</code>.
    @discussion Creates a cursor representing the given Cocoa built-in cursor.
    @param      aCursor the <code>NSCursor</code> to use
    @param      aType the corresponding <code>nsCursor</code> constant
    @result     an autoreleased instance of <code>nsMacCursor</code>
                representing the given <code>NSCursor</code>
 */
+ (nsMacCursor*)cursorWithCursor:(NSCursor*)aCursor type:(nsCursor)aType;

/*! @method     cursorWithImageNamed:hotSpot:type:
    @abstract   Create a cursor by specifying the name of an image resource to
                use for the cursor and a hotspot.
    @discussion Creates a cursor by loading the named image using the
                <code>+[NSImage imageNamed:]</code> method.
                <p>The image must be compatible with any restrictions laid down
                by <code>NSCursor</code>. These vary by operating system
                version.</p>
                <p>The hotspot precisely determines the point where the user
                clicks when using the cursor.</p>
    @param      aCursor the name of the image to use for the cursor
    @param      aPoint the point within the cursor to use as the hotspot
    @param      aType the corresponding <code>nsCursor</code> constant
    @result     an autoreleased instance of <code>nsMacCursor</code> that uses
   the given image and hotspot
 */
+ (nsMacCursor*)cursorWithImageNamed:(NSString*)aCursorImage
                             hotSpot:(NSPoint)aPoint
                                type:(nsCursor)aType;

/*! @method     cursorWithFrames:type:
    @abstract   Create an animated cursor by specifying the frames to use for
                the animation.
    @discussion Creates a cursor that will animate by cycling through the given
                frames. Each element of the array must be an instance of
                <code>NSCursor</code>
    @param      aCursorFrames an array of <code>NSCursor</code>, representing
                the frames of an animated cursor, in the order they should be
                played.
    @param      aType the corresponding <code>nsCursor</code> constant
    @result     an autoreleased instance of <code>nsMacCursor</code> that will
                animate the given cursor frames
 */
+ (nsMacCursor*)cursorWithFrames:(NSArray*)aCursorFrames type:(nsCursor)aType;

/*! @method     cocoaCursorWithImageNamed:hotSpot:
    @abstract   Create a Cocoa NSCursor object with a Gecko image resource name
                and a hotspot point.
    @discussion Create a Cocoa NSCursor object with a Gecko image resource name
                and a hotspot point.
    @param      imageName the name of the gecko image resource, "tiff"
                extension is assumed, do not append.
    @param      aPoint the point within the cursor to use as the hotspot
    @result     an autoreleased instance of <code>nsMacCursor</code> that will
                animate the given cursor frames
 */
+ (NSCursor*)cocoaCursorWithImageNamed:(NSString*)imageName
                               hotSpot:(NSPoint)aPoint;

/*! @method     isSet
    @abstract   Determines whether this cursor is currently active.
    @discussion This can be helpful when the Cocoa NSCursor state can be
                influenced without going through nsCursorManager.
    @result     whether the cursor is currently set
 */
- (BOOL)isSet;

/*! @method     set
    @abstract   Set the cursor.
    @discussion Makes this cursor the current cursor. If the cursor is
                animated, the animation is started.
 */
- (void)set;

/*! @method     unset
    @abstract   Unset the cursor. The cursor will return to the default
                (usually the arrow cursor).
    @discussion Unsets the cursor. If the cursor is animated, the animation is
                stopped.
 */
- (void)unset;

/*! @method     isAnimated
    @abstract   Tests whether this cursor is animated.
    @discussion Use this method to determine whether a cursor is animated
    @result     YES if the cursor is animated (has more than one frame), NO if
                it is a simple static cursor.
 */
- (BOOL)isAnimated;

/** @method     cursorType
    @abstract   Get the cursor type for this cursor
    @discussion This method returns the <code>nsCursor</code> constant that
                corresponds to this cursor, which is  equivalent to the CSS
                name for the cursor.
    @result     The nsCursor constant corresponding to this cursor, or
                nsCursor's 'eCursorCount' if the cursor is a custom cursor
                loaded from a URI
 */
- (nsCursor)type;
@end

#endif  // nsMacCursor_h_
