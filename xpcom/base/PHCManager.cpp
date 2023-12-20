/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PHCManager.h"

#include "PHC.h"
#include "mozilla/Literals.h"
#include "mozilla/Preferences.h"
#include "mozilla/StaticPrefs_memory.h"
#include "mozilla/Telemetry.h"
#include "prsystem.h"

namespace mozilla {

using namespace phc;

static const char kPHCEnabledPref[] = "memory.phc.enabled";
static const char kPHCMinRamMBPref[] = "memory.phc.min_ram_mb";

// True if PHC has ever been enabled for this process.
static bool sWasPHCEnabled = false;

static void UpdatePHCState() {
  size_t mem_size = PR_GetPhysicalMemorySize() / (1_MiB);
  size_t min_mem_size = StaticPrefs::memory_phc_min_ram_mb();

  // Only enable PHC if there are at least 8GB of ram.  Note that we use
  // 1000 bytes per kilobyte rather than 1024.  Some 8GB machines will have
  // slightly lower actual RAM available after some hardware devices
  // reserve some.
  if (StaticPrefs::memory_phc_enabled() && mem_size >= min_mem_size) {
    SetPHCState(Enabled);
    sWasPHCEnabled = true;
  } else {
    SetPHCState(OnlyFree);
  }
}

static void PrefChangeCallback(const char* aPrefName, void* aNull) {
  MOZ_ASSERT((0 == strcmp(aPrefName, kPHCEnabledPref)) ||
             (0 == strcmp(aPrefName, kPHCMinRamMBPref)));

  UpdatePHCState();
}

void InitPHCState() {
  Preferences::RegisterCallback(PrefChangeCallback, kPHCEnabledPref);
  Preferences::RegisterCallback(PrefChangeCallback, kPHCMinRamMBPref);
  UpdatePHCState();
}

void ReportPHCTelemetry() {
  if (!sWasPHCEnabled) {
    return;
  }

  MemoryUsage usage;
  PHCMemoryUsage(usage);

  Accumulate(Telemetry::MEMORY_PHC_SLOP, usage.mFragmentationBytes);

  PHCStats stats;
  GetPHCStats(stats);

  Accumulate(Telemetry::MEMORY_PHC_SLOTS_ALLOCATED, stats.mSlotsAllocated);
  Accumulate(Telemetry::MEMORY_PHC_SLOTS_FREED, stats.mSlotsFreed);
  // There are also slots that are unused (neither free nor allocated) they
  // can be calculated by knowing the total number of slots.
}

};  // namespace mozilla
