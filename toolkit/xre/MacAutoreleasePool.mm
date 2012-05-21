/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MacAutoreleasePool.h"
#include "nsDebug.h"

#import <Foundation/Foundation.h>

using mozilla::MacAutoreleasePool;

MacAutoreleasePool::MacAutoreleasePool()
{
  mPool = [[NSAutoreleasePool alloc] init];
  NS_ASSERTION(mPool != nsnull, "failed to create pool, objects will leak");
}

MacAutoreleasePool::~MacAutoreleasePool() {
  [mPool release];
}
