/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/widget/nsAutoRollup.h"

namespace mozilla {
namespace widget {

/*static*/ uint32_t nsAutoRollup::sCount = 0;
/*static*/ StaticRefPtr<nsIContent> nsAutoRollup::sLastRollup;

nsAutoRollup::nsAutoRollup()
{
  // remember if sLastRollup was null, and only clear it upon destruction
  // if so. This prevents recursive usage of nsAutoRollup from clearing
  // sLastRollup when it shouldn't.
  mWasClear = !sLastRollup;
  sCount++;
}

nsAutoRollup::~nsAutoRollup()
{
  if (sLastRollup && mWasClear) {
    sLastRollup = nullptr;
  }
  sCount--;
}

/*static*/ void
nsAutoRollup::SetLastRollup(nsIContent* aLastRollup)
{
  // There must be at least one nsAutoRollup on the stack.
  MOZ_ASSERT(sCount);

  sLastRollup = aLastRollup;
}

/*static*/ nsIContent*
nsAutoRollup::GetLastRollup()
{
  return sLastRollup.get();
}

} // namespace widget
} // namespace mozilla
