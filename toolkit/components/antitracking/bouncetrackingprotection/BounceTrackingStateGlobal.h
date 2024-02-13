/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_BounceTrackingStateGlobal_h
#define mozilla_BounceTrackingStateGlobal_h

#include "nsCycleCollectionParticipant.h"
#include "nsTHashMap.h"
#include "nsISupports.h"

namespace mozilla {

/**
 * This class holds the global state maps which are used to keep track of
 * potential bounce trackers and user activations.
 * @see BounceTrackingState for the per browser / tab state.
 */
class BounceTrackingStateGlobal final {
 public:
  NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(BounceTrackingStateGlobal);
  NS_DECL_CYCLE_COLLECTION_NATIVE_CLASS(BounceTrackingStateGlobal);

  // Map of site hosts to moments. The moments represent the most recent wall
  // clock time at which the user activated a top-level document on the
  // associated site host.
  nsTHashMap<nsCStringHashKey, PRTime> mUserActivation{};

  // Map of site hosts to moments. The moments represent the first wall clock
  // time since the last execution of the bounce tracking timer at which a page
  // on the given site host performed an action that could indicate stateful
  // bounce tracking took place.
  nsTHashMap<nsCStringHashKey, PRTime> mBounceTrackers{};

 private:
  ~BounceTrackingStateGlobal() = default;
};

}  // namespace mozilla

#endif
