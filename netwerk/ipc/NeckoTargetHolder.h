/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_net_NeckoTargetHolder_h
#define mozilla_net_NeckoTargetHolder_h

#include "nsIEventTarget.h"
#include "nsThreadUtils.h"

namespace mozilla {
namespace net {

// A helper class that implements GetNeckoTarget(). Basically, all e10s child
// channels should inherit this class in order to get a labeled event target.
class NeckoTargetHolder
{
public:
  explicit NeckoTargetHolder(nsIEventTarget *aNeckoTarget)
    : mNeckoTarget(aNeckoTarget)
  {}

protected:
  virtual ~NeckoTargetHolder() = default;
  // Get event target for processing network events.
  virtual already_AddRefed<nsIEventTarget> GetNeckoTarget();

  // EventTarget for labeling networking events.
  nsCOMPtr<nsIEventTarget> mNeckoTarget;
};

} // namespace net
} // namespace mozilla

#endif
