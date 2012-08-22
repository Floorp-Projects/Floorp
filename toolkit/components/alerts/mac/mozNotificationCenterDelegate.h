/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#import <Cocoa/Cocoa.h>

#include "nscore.h"
#include "nsStringGlue.h"
#include "nsNotificationCenterCompat.h"

#define OBSERVER_KEY @"ALERT_OBSERVER"
#define COOKIE_KEY   @"ALERT_COOKIE"

class nsIObserver;
class nsIDOMWindow;

@interface mozNotificationCenterDelegate : NSObject <NSUserNotificationCenterDelegate>
{
@private
  PRUint32 mKey;
  NSMutableDictionary *mObserverDict;
  id<FakeNSUserNotificationCenter> mCenter;
}

/**
 * Dispatches a notification to Notification Center
 *
 * @param aTitle  The title of the notification
 * @param aText   The body of the notification
 * @param aKey    The observer key to use as a lookup (or 0 if no observer)
 * @param aCookie The string to be used as a cookie if there is an observer
 */
- (void)notifyWithTitle:(const nsAString&)aTitle
            description:(const nsAString&)aText
                    key:(PRUint32)aKey
                 cookie:(const nsAString&)aCookie;

/**
 * Adds an nsIObserver that we can query later for dispatching obsevers.
 *
 * @param aObserver The observer we are adding.
 * @return The key it was stored in.
 */
- (PRUint32)addObserver:(nsIObserver*)aObserver;

/**
 * Called when a window was closed and the observers are no longer valid.
 *
 * @param window The window that was closed.
 */
- (void)forgetObserversForWindow:(nsIDOMWindow*)window;

/**
 * Called when all observers should be released.
 */
- (void)forgetObservers;

@end
