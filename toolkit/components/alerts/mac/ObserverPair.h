/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#import <Cocoa/Cocoa.h>

#include "nsIXPConnect.h"
#include "nsIObserver.h"
#include "nsIDOMWindow.h"
#include "nsIJSContextStack.h"
#include "nsServiceManagerUtils.h"

@interface ObserverPair : NSObject
{
@public
  nsIObserver *observer;
  nsIDOMWindow *window;
}

- (id) initWithObserver:(nsIObserver *)aObserver window:(nsIDOMWindow *)aWindow;
- (void) dealloc;

@end

/**
 * Returns the DOM window that owns the given observer in the case that the
 * observer is implemented in JS and was created in a DOM window's scope.
 *
 * We need this so that we can properly clean up in cases where the window gets
 * closed before the growl timeout/click notifications have fired. Otherwise we
 * leak those windows.
 */
static already_AddRefed<nsIDOMWindow> __attribute__((unused))
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
