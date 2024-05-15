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
static const char kPHCAvgDelayFirst[] = "memory.phc.avg_delay.first";
static const char kPHCAvgDelayNormal[] = "memory.phc.avg_delay.normal";
static const char kPHCAvgDelayPageRuse[] = "memory.phc.avg_delay.page_reuse";

static const char kPHCAvgDelayContentFirst[] =
    "memory.phc.avg_delay.content.first";
static const char kPHCAvgDelayContentNormal[] =
    "memory.phc.avg_delay.content.normal";
static const char kPHCAvgDelayContentPageRuse[] =
    "memory.phc.avg_delay.content.page_reuse";

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
    // Set PHC probablities before enabling PHC so that the first allocation
    // delay gets used.
    if (XRE_IsContentProcess()) {
      // Content processes can come and go and have their own delays
      // configured.
      SetPHCProbabilities(
          StaticPrefs::memory_phc_avg_delay_content_first(),
          StaticPrefs::memory_phc_avg_delay_content_normal(),
          StaticPrefs::memory_phc_avg_delay_content_page_reuse());
    } else {
      // All other process types tend to exist for the length of the
      // session, they're grouped here.
      SetPHCProbabilities(StaticPrefs::memory_phc_avg_delay_first(),
                          StaticPrefs::memory_phc_avg_delay_normal(),
                          StaticPrefs::memory_phc_avg_delay_page_reuse());
    }

    SetPHCState(Enabled);
    sWasPHCEnabled = true;
  } else {
    SetPHCState(OnlyFree);
  }
}

static void PrefChangeCallback(const char* aPrefName, void* aNull) {
  MOZ_ASSERT((0 == strcmp(aPrefName, kPHCEnabledPref)) ||
             (0 == strcmp(aPrefName, kPHCMinRamMBPref)) ||
             (0 == strcmp(aPrefName, kPHCAvgDelayFirst)) ||
             (0 == strcmp(aPrefName, kPHCAvgDelayNormal)) ||
             (0 == strcmp(aPrefName, kPHCAvgDelayPageRuse)) ||
             (0 == strcmp(aPrefName, kPHCAvgDelayContentFirst)) ||
             (0 == strcmp(aPrefName, kPHCAvgDelayContentNormal)) ||
             (0 == strcmp(aPrefName, kPHCAvgDelayContentPageRuse)));

  UpdatePHCState();
}

void InitPHCState() {
  Preferences::RegisterCallback(PrefChangeCallback, kPHCEnabledPref);
  Preferences::RegisterCallback(PrefChangeCallback, kPHCMinRamMBPref);
  Preferences::RegisterCallback(PrefChangeCallback, kPHCAvgDelayFirst);
  Preferences::RegisterCallback(PrefChangeCallback, kPHCAvgDelayNormal);
  Preferences::RegisterCallback(PrefChangeCallback, kPHCAvgDelayPageRuse);
  Preferences::RegisterCallback(PrefChangeCallback, kPHCAvgDelayContentFirst);
  Preferences::RegisterCallback(PrefChangeCallback, kPHCAvgDelayContentNormal);
  Preferences::RegisterCallback(PrefChangeCallback,
                                kPHCAvgDelayContentPageRuse);
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
