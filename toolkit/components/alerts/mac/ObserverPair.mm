/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#import "ObserverPair.h"

#include "nsObjCExceptions.h"

@implementation ObserverPair

- (id) initWithObserver:(nsIObserver *)aObserver window:(nsIDOMWindow *)aWindow
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NIL;

  if ((self = [super init])) {
    NS_ADDREF(observer = aObserver);
    NS_IF_ADDREF(window = aWindow);
  }

  return self;

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
