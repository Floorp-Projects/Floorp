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
 * The Original Code is Growl implementation of nsIAlertsService.
 *
 * The Initial Developer of the Original Code is
 *   Shawn Wilsher <me@shawnwilsher.com>.
 * Portions created by the Initial Developer are Copyright (C) 2006-2007
 * the Initial Developer. All Rights Reserved.
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

#import "GrowlApplicationBridge.h"
#include "nsIObserver.h"

#import <Cocoa/Cocoa.h>

// XXX should this be localized?  Bug 378527
#define NOTIFICATION_NAME @"Application Notice"
#define OBSERVER_KEY      @"ALERT_OBSERVER"
#define COOKIE_KEY        @"ALERT_COOKIE"

@interface mozGrowlDelegate : NSObject <GrowlApplicationBridgeDelegate>
{
@private
  PRUint32 mKey;
  NSMutableDictionary* mDict;
}

/**
 * Dispatches a notification to Growl
 *
 * @param aTitle  The title of the notification
 * @param aText   The body of the notification
 * @param aImage  The image data, or [NSData data] if no image
 * @param aKey    The observer key to use as a lookup (or 0 if no observer)
 * @param aCookie The string to be used as a cookie if there is an observer
 */
+ (void) notifyWithTitle:(const nsAString&)aTitle
             description:(const nsAString&)aText
                iconData:(NSData*)aImage
                     key:(PRUint32)aKey
                  cookie:(const nsAString&)aCookie;

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

@end
