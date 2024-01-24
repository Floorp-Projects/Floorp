/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsCursorManager_h_
#define nsCursorManager_h_

#import <Foundation/Foundation.h>
#include "nsIWidget.h"
#include "nsMacCursor.h"

/*! @class      nsCursorManager
    @abstract   Singleton service provides access to all cursors available in
   the application.
    @discussion Use <code>nsCusorManager</code> to set the current cursor using
                an XP <code>nsCusor</code> enum value.
                <code>nsCursorManager</code> encapsulates the details of
                setting different types of cursors, animating cursors and
                cleaning up cursors when they are no longer in use.
 */
@interface nsCursorManager : NSObject {
 @private
  NSMutableDictionary* mCursors;
  nsMacCursor* mCurrentMacCursor;
}

/*! @method     setCursor:
    @abstract   Sets the current cursor.
    @discussion Sets the current cursor to the cursor indicated by the XP
                cursor given in the argument. Resources associated with the
                previous cursor are cleaned up.
    @param aCursor the cursor to use
*/
- (nsresult)setNonCustomCursor:(const nsIWidget::Cursor&)aCursor;

// As above, but returns an error if the cursor isn't custom or we couldn't set
// it for some reason.
- (nsresult)setCustomCursor:(const nsIWidget::Cursor&)aCursor
          widgetScaleFactor:(CGFloat)aWidgetScaleFactor
                forceUpdate:(bool)aForceUpdate;

/*! @method     sharedInstance
    @abstract   Get the Singleton instance of the cursor manager.
    @discussion Use this method to obtain a reference to the cursor manager.
    @result a reference to the cursor manager
*/
+ (nsCursorManager*)sharedInstance;

/*! @method     dispose
    @abstract   Releases the shared instance of the cursor manager.
    @discussion Use dispose to clean up the cursor manager and associated
   cursors.
*/
+ (void)dispose;
@end

@interface NSCursor (Undocumented)
// busyButClickableCursor is an undocumented NSCursor API, but has been in use
// since at least OS X 10.4 and through 10.9.
+ (NSCursor*)busyButClickableCursor;
@end

#endif  // nsCursorManager_h_
