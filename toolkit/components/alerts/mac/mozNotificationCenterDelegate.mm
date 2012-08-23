/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#import "mozNotificationCenterDelegate.h"
#import "ObserverPair.h"

#include "nsIObserver.h"
#include "nsIXULAppInfo.h"
#include "nsIStringBundle.h"
#include "nsIDOMWindow.h"
#include "nsObjCExceptions.h"

// Note that all calls to the Notification Center API are not made normally.
// We do this to maintain backwards compatibility.

id<FakeNSUserNotificationCenter> GetNotificationCenter() {
  Class c = NSClassFromString(@"NSUserNotificationCenter");
  return [c performSelector:@selector(defaultUserNotificationCenter)];
}

@implementation mozNotificationCenterDelegate

- (id)init
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NIL;

  if ((self = [super init])) {
    mCenter = [GetNotificationCenter() retain];
    mKey = 0;
    mObserverDict = [[NSMutableDictionary dictionaryWithCapacity:8] retain];
  }

  return self;

  NS_OBJC_END_TRY_ABORT_BLOCK_NIL;
}

- (void)dealloc
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  [mObserverDict release];
  [mCenter release];

  [super dealloc];

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

-(void) notifyWithTitle:(const nsAString&)aTitle
            description:(const nsAString&)aText
                    key:(PRUint32)aKey
                 cookie:(const nsAString&)aCookie;
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  Class unClass = NSClassFromString(@"NSUserNotification");
  id<FakeNSUserNotification> notification = [[unClass alloc] init];
  notification.title = [NSString stringWithCharacters:aTitle.BeginReading()
                                               length:aTitle.Length()];
  notification.informativeText = [NSString stringWithCharacters:aText.BeginReading()
                                                         length:aText.Length()];
  NSDictionary* clickContext = nil;
  if (aKey) {
    clickContext = [NSDictionary dictionaryWithObjectsAndKeys:
                    [NSNumber numberWithUnsignedInt:aKey],
                    OBSERVER_KEY,
                    [NSArray arrayWithObject:
                     [NSString stringWithCharacters:aCookie.BeginReading()
                                             length:aCookie.Length()]],
                    COOKIE_KEY,
                    nil];
  }
  notification.userInfo = clickContext;
  notification.soundName = NSUserNotificationDefaultSoundName;
  notification.hasActionButton = NO;

  [GetNotificationCenter() deliverNotification:notification];

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

- (PRUint32) addObserver:(nsIObserver *)aObserver
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_RETURN;

  nsCOMPtr<nsIDOMWindow> parentWindow = GetWindowOfObserver(aObserver);

  ObserverPair* pair = [[ObserverPair alloc] initWithObserver:aObserver
                                                       window:parentWindow];

  [mObserverDict setObject:pair forKey:[NSNumber numberWithUnsignedInt:++mKey]];

  [pair release];

  return mKey;

  NS_OBJC_END_TRY_ABORT_BLOCK_RETURN(0);
}

- (void) forgetObserversForWindow:(nsIDOMWindow*)window
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  NS_ASSERTION(window, "No window!");

  NSMutableArray *keysToRemove = [NSMutableArray array];

  NSEnumerator *keyEnumerator = [[mObserverDict allKeys] objectEnumerator];
  NSNumber *key;
  while ((key = [keyEnumerator nextObject])) {
    ObserverPair *pair = [mObserverDict objectForKey:key];
    if (pair && pair->window == window)
      [keysToRemove addObject:key];
  }

  [mObserverDict removeObjectsForKeys:keysToRemove];

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

- (void) forgetObservers
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  [mObserverDict removeAllObjects];

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

// Implement NSUserNotificationCenterDelegate

- (void)userNotificationCenter:(id<FakeNSUserNotificationCenter>)center
        didDeliverNotification:(id<FakeNSUserNotification>)notification
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  // Internally we have two notifications, "alertclickcallback" and "alertfinished".
  // Notification Center does not tell us when the alert finished, but we always need
  // to send alert finished, or otherwise consider it finished in order to free up
  // observers. This is a conundrum. For now we'll do the unfortunately conservative
  // thing and not support actions so that we can consistently end alerts when they
  // are shown.

  NSDictionary* userInfo = notification.userInfo;

  NS_ASSERTION([userInfo valueForKey:OBSERVER_KEY] != nil,
               "OBSERVER_KEY not found!");
  NS_ASSERTION([userInfo valueForKey:COOKIE_KEY] != nil,
               "COOKIE_KEY not found!");

  ObserverPair* pair = [mObserverDict objectForKey:[userInfo valueForKey:OBSERVER_KEY]];
  nsCOMPtr<nsIObserver> observer = pair ? pair->observer : nullptr;
  [mObserverDict removeObjectForKey:[userInfo valueForKey:OBSERVER_KEY]];

  if (observer) {
    NSString* cookie = [[userInfo valueForKey:COOKIE_KEY] objectAtIndex:0];

    nsAutoString tmp;
    tmp.SetLength([cookie length]);
    [cookie getCharacters:tmp.BeginWriting()];

    observer->Observe(nullptr, "alertfinished", tmp.get());
  }

  // We don't want alerts to persist in the notification center because they
  // are likely to refer to potentially disappearing contexts like a download.
  //
  // However, since we don't react to clicks right now, it is OK to let them
  // persist.
  //[mCenter removeDeliveredNotification:notification];

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

- (void)userNotificationCenter:(id<FakeNSUserNotificationCenter>)center
       didActivateNotification:(id<FakeNSUserNotification>)notification
{
  // We don't support activation right now because it'd be sent after
  // we send 'alertfinished' to observers. This is unfortunate but necessary
  // because we can't reliably know when a notification is finished, we
  // can only know when it is shown or activated.
}

- (BOOL)userNotificationCenter:(id<FakeNSUserNotificationCenter>)center
     shouldPresentNotification:(id<FakeNSUserNotification>)notification
{
  // Respect OS choice.
  return NO;
}

@end
