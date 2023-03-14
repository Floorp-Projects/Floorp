/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsCursorManager_h_
#define nsCursorManager_h_

#import <Cocoa/Cocoa.h>

#include "nsIWidget.h"

@interface nsCursorManager : NSObject {
 @private
  NSMutableDictionary* mCursors;
  NSCursor* mCurrentCursor;
  nsCursor mCurrentCursorType;
}

// Set a cursor.
// Returns an error if the cursor isn't custom or we couldn't set
// it for some reason.
- (nsresult)setCustomCursor:(const nsIWidget::Cursor&)aCursor
          widgetScaleFactor:(CGFloat)aWidgetScaleFactor;
// Sets non-custom cursors and can be used as a fallback if setting
// a custom cursor did not succeed.
- (void)setNonCustomCursor:(const nsIWidget::Cursor&)aCursor;

+ (nsCursorManager*)sharedInstance;
@end

@interface NSCursor (Undocumented)
// busyButClickableCursor is an undocumented NSCursor API, but has been in use since
// at least OS X 10.4 and through 10.9.
+ (NSCursor*)busyButClickableCursor;
@end

#endif  // nsCursorManager_h_
