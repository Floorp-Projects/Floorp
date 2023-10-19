/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PHCManager.h"

#include "PHC.h"
#include "mozilla/Preferences.h"
#include "mozilla/StaticPrefs_memory.h"

namespace mozilla {

using namespace phc;

static const char kPHCPref[] = "memory.phc.enabled";

static PHCState GetPHCStateFromPref() {
  return StaticPrefs::memory_phc_enabled() ? Enabled : OnlyFree;
}

static void PrefChangeCallback(const char* aPrefName, void* aNull) {
  MOZ_ASSERT(0 == strcmp(aPrefName, kPHCPref));

  SetPHCState(GetPHCStateFromPref());
}

void InitPHCState() {
  SetPHCState(GetPHCStateFromPref());

  Preferences::RegisterCallback(PrefChangeCallback, kPHCPref);
}

};  // namespace mozilla
