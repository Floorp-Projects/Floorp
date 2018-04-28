/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsTObserverArray.h"

void
nsTObserverArray_base::AdjustIterators(index_type aModPos,
                                       diff_type aAdjustment)
{
  MOZ_ASSERT(aAdjustment == -1 || aAdjustment == 1, "invalid adjustment");
  Iterator_base* iter = mIterators;
  while (iter) {
    if (iter->mPosition > aModPos) {
      iter->mPosition += aAdjustment;
    }
    iter = iter->mNext;
  }
}

void
nsTObserverArray_base::ClearIterators()
{
  Iterator_base* iter = mIterators;
  while (iter) {
    iter->mPosition = 0;
    iter = iter->mNext;
  }
}
