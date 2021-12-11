/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#ifndef mozilla_FOG_h
#define mozilla_FOG_h

#include "nsIFOG.h"
#include "nsIObserver.h"

namespace mozilla {
class FOG final : public nsIFOG, public nsIObserver {
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIFOG
  NS_DECL_NSIOBSERVER

 public:
  FOG() = default;
  static already_AddRefed<FOG> GetSingleton();

 private:
  ~FOG() = default;
  void Shutdown();
};

};  // namespace mozilla

#endif  // mozilla_FOG_h
