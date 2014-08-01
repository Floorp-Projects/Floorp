/* -*- Mode: C++; tab-width: 40; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __POWERWAKELOCK_H_
#define __POWERWAKELOCK_H_

#include "mozilla/StaticPtr.h"
#include "nsISupportsImpl.h"

namespace mozilla {
namespace hal_impl {
class PowerWakelock
{
public:
  NS_INLINE_DECL_REFCOUNTING(PowerWakelock);
  PowerWakelock();
private:
  ~PowerWakelock();
};
extern StaticRefPtr <PowerWakelock> gPowerWakelock;
} // hal_impl
} // mozilla
#endif /* __POWERWAKELOCK_H_ */
