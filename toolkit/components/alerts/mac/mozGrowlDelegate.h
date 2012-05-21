/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#import "GrowlApplicationBridge.h"

#include "nscore.h"
#include "nsStringGlue.h"

#import <Cocoa/Cocoa.h>

#define GROWL_STRING_BUNDLE_LOCATION \
  "chrome://alerts/locale/notificationNames.properties"

#define OBSERVER_KEY      @"ALERT_OBSERVER"
#define COOKIE_KEY        @"ALERT_COOKIE"

class nsIObserver;
class nsIDOMWindow;

@interface mozGrowlDelegate : NSObject <GrowlApplicationBridgeDelegate>
{
@private
  PRUint32 mKey;
  NSMutableDictionary *mDict;
  NSMutableArray *mNames;
  NSMutableArray *mEnabled;
}

/**
 * Dispatches a notification to Growl
 *
 * @param aName   The notification name to dispatch
 * @param aTitle  The title of the notification
 * @param aText   The body of the notification
 * @param aImage  The image data, or [NSData data] if no image
 * @param aKey    The observer key to use as a lookup (or 0 if no observer)
 * @param aCookie The string to be used as a cookie if there is an observer
 */
+ (void) notifyWithName:(const nsAString&)aName
                  title:(const nsAString&)aTitle
            description:(const nsAString&)aText
               iconData:(NSData*)aImage
                    key:(PRUint32)aKey
                 cookie:(const nsAString&)aCookie;

/**
 * Adds notifications to the registration dictionary.
 *
 * @param aNames An NSArray of names of notifications.
 */
- (void) addNotificationNames:(NSArray*)aNames;

/**
 * Adds enabled notifications to the registration dictionary.
 *
 * @param aEnabled An NSArray of names of enabled notifications.
 */
- (void) addEnabledNotifications:(NSArray*)aEnabled;

/**
 * Adds an nsIObserver that we can query later for dispatching obsevers.
 *
 * @param aObserver The observer we are adding.
 * @return The key it was stored in.
 */
- (PRUint32) addObserver:(nsIObserver*)aObserver;

/**
 * Gives Growl the application name.
 */
- (NSString*) applicationNameForGrowl;

/**
 * Gives Growl the complete list of notifications this application will ever
 * send, and also which notifications should be enabled by default.
 */
- (NSDictionary *) registrationDictionaryForGrowl;

/**
 * Informs us that a Growl notification timed out.
 *
 * @param clickContext The object passed back from growl.
 */
- (void) growlNotificationTimedOut:(id)clickContext;

/**
 * Informs us that a Growl notification was clicked.
 *
 * @param clickContext The object passed back from growl.
 */
- (void) growlNotificationWasClicked:(id)clickContext;

/**
 * Called when a window was closed and the observers are no longer valid.
 *
 * @param window The window that was closed.
 */
- (void) forgetObserversForWindow:(nsIDOMWindow*)window;

/**
 * Called when all observers should be released.
 */
- (void) forgetObservers;

@end
