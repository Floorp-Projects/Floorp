/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PreloadService.h"

namespace mozilla {

bool PreloadService::RegisterPreload(PreloadHashKey* aKey,
                                     PreloaderBase* aPreload) {
  if (PreloadExists(aKey)) {
    return false;
  }

  mPreloads.Put(aKey, RefPtr{aPreload});
  return true;
}

void PreloadService::DeregisterPreload(PreloadHashKey* aKey) {
  mPreloads.Remove(aKey);
}

void PreloadService::ClearAllPreloads() { mPreloads.Clear(); }

bool PreloadService::PreloadExists(PreloadHashKey* aKey) {
  bool found;
  mPreloads.GetWeak(aKey, &found);
  return found;
}

already_AddRefed<PreloaderBase> PreloadService::LookupPreload(
    PreloadHashKey* aKey) const {
  return mPreloads.Get(aKey);
}

}  // namespace mozilla
