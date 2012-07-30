/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#import "mozGrowlDelegate.h"

#include "nsIObserver.h"
#include "nsIXPConnect.h"
#include "nsIXULAppInfo.h"
#include "nsIStringBundle.h"
#include "nsIJSContextStack.h"
#include "nsIDOMWindow.h"

#include "jsapi.h"
#include "nsCOMPtr.h"
#include "nsObjCExceptions.h"
#include "nsServiceManagerUtils.h"
#include "nsWeakReference.h"

/**
 * Returns the DOM window that owns the given observer in the case that the
 * observer is implemented in JS and was created in a DOM window's scope.
 *
 * We need this so that we can properly clean up in cases where the window gets
 * closed before the growl timeout/click notifications have fired. Otherwise we
 * leak those windows.
 */
static already_AddRefed<nsIDOMWindow>
GetWindowOfObserver(nsIObserver* aObserver)
{
  nsCOMPtr<nsIXPConnectWrappedJS> wrappedJS(do_QueryInterface(aObserver));
  if (!wrappedJS) {
    // We can't do anything with objects that aren't implemented in JS...
    return nullptr;
  }

  JSObject* obj;
  nsresult rv = wrappedJS->GetJSObject(&obj);
  NS_ENSURE_SUCCESS(rv, nullptr);

  nsCOMPtr<nsIThreadJSContextStack> stack =
    do_GetService("@mozilla.org/js/xpc/ContextStack;1", &rv);
  NS_ENSURE_SUCCESS(rv, nullptr);

  JSContext* cx = stack->GetSafeJSContext();
  NS_ENSURE_TRUE(cx, nullptr);

  JSAutoRequest ar(cx);
  JSAutoEnterCompartment ac;
  if (!ac.enter(cx, obj)) {
    return nullptr;
  }

  JSObject* global = JS_GetGlobalForObject(cx, obj);
  NS_ENSURE_TRUE(global, nullptr);

  nsCOMPtr<nsIXPConnect> xpc(do_GetService(nsIXPConnect::GetCID()));
  NS_ENSURE_TRUE(xpc, nullptr);

  nsCOMPtr<nsIXPConnectWrappedNative> wrapper;
  rv = xpc->GetWrappedNativeOfJSObject(cx, global, getter_AddRefs(wrapper));
  NS_ENSURE_SUCCESS(rv, nullptr);

  nsCOMPtr<nsIDOMWindow> window = do_QueryWrappedNative(wrapper);
  NS_ENSURE_TRUE(window, nullptr);

  return window.forget();
}

@interface ObserverPair : NSObject
{
@public
  nsIObserver *observer;
  nsIDOMWindow *window;
}

- (id) initWithObserver:(nsIObserver *)aObserver window:(nsIDOMWindow *)aWindow;
- (void) dealloc;

@end

@implementation ObserverPair

- (id) initWithObserver:(nsIObserver *)aObserver window:(nsIDOMWindow *)aWindow
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NIL;

  if ((self = [super init])) {
    NS_ADDREF(observer = aObserver);
    NS_IF_ADDREF(window = aWindow);
    return self;
  }

  // Safeguard against calling NS_RELEASE on uninitialized memory.
  observer = nullptr;
  window = nullptr;

  return nil;

  NS_OBJC_END_TRY_ABORT_BLOCK_NIL;
}

- (void) dealloc
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  NS_IF_RELEASE(observer);
  NS_IF_RELEASE(window);
  [super dealloc];

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

@end

@implementation mozGrowlDelegate

- (id) init
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NIL;

  if ((self = [super init])) {
    mKey = 0;
    mDict = [[NSMutableDictionary dictionaryWithCapacity: 8] retain];
    
    mNames   = [[NSMutableArray alloc] init];
    mEnabled = [[NSMutableArray alloc] init];
  
    nsresult rv;
    nsCOMPtr<nsIStringBundleService> bundleService =
      do_GetService("@mozilla.org/intl/stringbundle;1", &rv);

    if (NS_SUCCEEDED(rv)) {
      nsCOMPtr<nsIStringBundle> bundle;
      rv = bundleService->CreateBundle(GROWL_STRING_BUNDLE_LOCATION,
                                       getter_AddRefs(bundle));
      
      if (NS_SUCCEEDED(rv)) {
        nsString text;
        rv = bundle->GetStringFromName(NS_LITERAL_STRING("general").get(),
                                       getter_Copies(text));
        
        if (NS_SUCCEEDED(rv)) {
          NSString *s = [NSString stringWithCharacters: text.BeginReading()
                                                length: text.Length()];
          [mNames addObject: s];
          [mEnabled addObject: s];

          return self;
        }
      }
    }

    // Fallback
    [mNames addObject: @"General Notification"];
    [mEnabled addObject: @"General Notification"];
  }

  return self;

  NS_OBJC_END_TRY_ABORT_BLOCK_NIL;
}

- (void) dealloc
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  [mDict release];

  [mNames release];
  [mEnabled release];

  [super dealloc];

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

- (void) addNotificationNames:(NSArray*)aNames
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  [mNames addObjectsFromArray: aNames];

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

- (void) addEnabledNotifications:(NSArray*)aEnabled
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  [mEnabled addObjectsFromArray: aEnabled];

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

+ (void) notifyWithName:(const nsAString&)aName
                  title:(const nsAString&)aTitle
            description:(const nsAString&)aText
               iconData:(NSData*)aImage
                    key:(PRUint32)aKey
                 cookie:(const nsAString&)aCookie
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  NS_ASSERTION(aName.Length(), "No name specified for the alert!");
  
  NSDictionary* clickContext = nil;
  if (aKey) {
    clickContext = [NSDictionary dictionaryWithObjectsAndKeys:
      [NSNumber numberWithUnsignedInt: aKey],
      OBSERVER_KEY,
      [NSArray arrayWithObject:
        [NSString stringWithCharacters: aCookie.BeginReading()
                                length: aCookie.Length()]],
      COOKIE_KEY,
      nil];
  }

  [GrowlApplicationBridge
     notifyWithTitle: [NSString stringWithCharacters: aTitle.BeginReading()
                                              length: aTitle.Length()]
         description: [NSString stringWithCharacters: aText.BeginReading()
                                              length: aText.Length()]
    notificationName: [NSString stringWithCharacters: aName.BeginReading()
                                              length: aName.Length()]
            iconData: aImage
            priority: 0
            isSticky: NO
        clickContext: clickContext];

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

- (PRUint32) addObserver:(nsIObserver *)aObserver
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_RETURN;

  nsCOMPtr<nsIDOMWindow> parentWindow = GetWindowOfObserver(aObserver);

  ObserverPair* pair = [[ObserverPair alloc] initWithObserver: aObserver
                                                       window: parentWindow];
  [pair autorelease];

  [mDict setObject: pair
            forKey: [NSNumber numberWithUnsignedInt: ++mKey]];
  return mKey;

  NS_OBJC_END_TRY_ABORT_BLOCK_RETURN(0);
}

- (NSDictionary *) registrationDictionaryForGrowl
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NIL;

  return [NSDictionary dictionaryWithObjectsAndKeys:
           mNames, GROWL_NOTIFICATIONS_ALL,
           mEnabled, GROWL_NOTIFICATIONS_DEFAULT,
           nil];

  NS_OBJC_END_TRY_ABORT_BLOCK_NIL;
}

- (NSString*) applicationNameForGrowl
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NIL;

  nsresult rv;

  nsCOMPtr<nsIXULAppInfo> appInfo =
    do_GetService("@mozilla.org/xre/app-info;1", &rv);
  NS_ENSURE_SUCCESS(rv, nil);

  nsCAutoString appName;
  rv = appInfo->GetName(appName);
  NS_ENSURE_SUCCESS(rv, nil);

  nsAutoString name = NS_ConvertUTF8toUTF16(appName);
  return [NSString stringWithCharacters: name.BeginReading()
                                 length: name.Length()];

  NS_OBJC_END_TRY_ABORT_BLOCK_NIL;
}

- (void) growlNotificationTimedOut:(id)clickContext
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  NS_ASSERTION([clickContext valueForKey: OBSERVER_KEY] != nil,
               "OBSERVER_KEY not found!");
  NS_ASSERTION([clickContext valueForKey: COOKIE_KEY] != nil,
               "COOKIE_KEY not found!");

  ObserverPair* pair =
    [mDict objectForKey: [clickContext valueForKey: OBSERVER_KEY]];
  nsCOMPtr<nsIObserver> observer = pair ? pair->observer : nullptr;

  [mDict removeObjectForKey: [clickContext valueForKey: OBSERVER_KEY]];
  NSString* cookie = [[clickContext valueForKey: COOKIE_KEY] objectAtIndex: 0];

  if (observer) {
    nsAutoString tmp;
    tmp.SetLength([cookie length]);
    [cookie getCharacters:tmp.BeginWriting()];

    observer->Observe(nullptr, "alertfinished", tmp.get());
  }

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

- (void) growlNotificationWasClicked:(id)clickContext
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  NS_ASSERTION([clickContext valueForKey: OBSERVER_KEY] != nil,
               "OBSERVER_KEY not found!");
  NS_ASSERTION([clickContext valueForKey: COOKIE_KEY] != nil,
               "COOKIE_KEY not found!");

  ObserverPair* pair =
    [mDict objectForKey: [clickContext valueForKey: OBSERVER_KEY]];
  nsCOMPtr<nsIObserver> observer = pair ? pair->observer : nullptr;

  [mDict removeObjectForKey: [clickContext valueForKey: OBSERVER_KEY]];
  NSString* cookie = [[clickContext valueForKey: COOKIE_KEY] objectAtIndex: 0];

  if (observer) {
    nsAutoString tmp;
    tmp.SetLength([cookie length]);
    [cookie getCharacters:tmp.BeginWriting()];

    observer->Observe(nullptr, "alertclickcallback", tmp.get());
    observer->Observe(nullptr, "alertfinished", tmp.get());
  }

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

- (void) forgetObserversForWindow:(nsIDOMWindow*)window
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  NS_ASSERTION(window, "No window!");

  NSMutableArray *keysToRemove = [NSMutableArray array];

  NSEnumerator *keyEnumerator = [[mDict allKeys] objectEnumerator];
  NSNumber *key;
  while ((key = [keyEnumerator nextObject])) {
    ObserverPair *pair = [mDict objectForKey:key];
    if (pair && pair->window == window)
      [keysToRemove addObject:key];
  }

  [mDict removeObjectsForKeys:keysToRemove];

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

- (void) forgetObservers
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  [mDict removeAllObjects];

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

@end
