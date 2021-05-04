// Copyright (c) the JPEG XL Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef LIB_PROFILER_TSC_TIMER_H_
#define LIB_PROFILER_TSC_TIMER_H_

// High-resolution (~10 ns) timestamps, using fences to prevent reordering and
// ensure exactly the desired regions are measured.

#include <stdint.h>

#include <ctime>
#include <hwy/cache_control.h>  // LoadFence

#include "lib/jxl/base/arch_macros.h"  // for JXL_ARCH_*
#include "lib/jxl/base/compiler_specific.h"

namespace jxl {

// TicksBefore/After return absolute timestamps and must be placed immediately
// before and after the region to measure. The functions are distinct because
// they use different fences.
//
// Background: RDTSC is not 'serializing'; earlier instructions may complete
// after it, and/or later instructions may complete before it. 'Fences' ensure
// regions' elapsed times are independent of such reordering. The only
// documented unprivileged serializing instruction is CPUID, which acts as a
// full fence (no reordering across it in either direction). Unfortunately
// the latency of CPUID varies wildly (perhaps made worse by not initializing
// its EAX input). Because it cannot reliably be deducted from the region's
// elapsed time, it must not be included in the region to measure (i.e.
// between the two RDTSC).
//
// The newer RDTSCP is sometimes described as serializing, but it actually
// only serves as a half-fence with release semantics. Although all
// instructions in the region will complete before the final timestamp is
// captured, subsequent instructions may leak into the region and increase the
// elapsed time. Inserting another fence after the final RDTSCP would prevent
// such reordering without affecting the measured region.
//
// Fortunately, such a fence exists. The LFENCE instruction is only documented
// to delay later loads until earlier loads are visible. However, Intel's
// reference manual says it acts as a full fence (waiting until all earlier
// instructions have completed, and delaying later instructions until it
// completes). AMD assigns the same behavior to MFENCE.
//
// We need a fence before the initial RDTSC to prevent earlier instructions
// from leaking into the region, and arguably another after RDTSC to avoid
// region instructions from completing before the timestamp is recorded.
// When surrounded by fences, the additional RDTSCP half-fence provides no
// benefit, so the initial timestamp can be recorded via RDTSC, which has
// lower overhead than RDTSCP because it does not read TSC_AUX. In summary,
// we define Before = LFENCE/RDTSC/LFENCE; After = RDTSCP/LFENCE.
//
// Using Before+Before leads to higher variance and overhead than After+After.
// However, After+After includes an LFENCE in the region measurements, which
// adds a delay dependent on earlier loads. The combination of Before+After
// is faster than Before+Before and more consistent than Stop+Stop because
// the first LFENCE already delayed subsequent loads before the measured
// region. This combination seems not to have been considered in prior work:
// http://akaros.cs.berkeley.edu/lxr/akaros/kern/arch/x86/rdtsc_test.c
//
// Note: performance counters can measure 'exact' instructions-retired or
// (unhalted) cycle counts. The RDPMC instruction is not serializing and also
// requires fences. Unfortunately, it is not accessible on all OSes and we
// prefer to avoid kernel-mode drivers. Performance counters are also affected
// by several under/over-count errata, so we use the TSC instead.

// Returns a 64-bit timestamp in unit of 'ticks'; to convert to seconds,
// divide by InvariantTicksPerSecond. Although 32-bit ticks are faster to read,
// they overflow too quickly to measure long regions.
static inline uint64_t TicksBefore() {
  uint64_t t;
#if JXL_ARCH_PPC
  asm volatile("mfspr %0, %1" : "=r"(t) : "i"(268));
#elif JXL_ARCH_X64 && JXL_COMPILER_MSVC
  hwy::LoadFence();
  JXL_COMPILER_FENCE;
  t = __rdtsc();
  hwy::LoadFence();
  JXL_COMPILER_FENCE;
#elif JXL_ARCH_X64 && (JXL_COMPILER_CLANG || JXL_COMPILER_GCC)
  asm volatile(
      "lfence\n\t"
      "rdtsc\n\t"
      "shl $32, %%rdx\n\t"
      "or %%rdx, %0\n\t"
      "lfence"
      : "=a"(t)
      :
      // "memory" avoids reordering. rdx = TSC >> 32.
      // "cc" = flags modified by SHL.
      : "rdx", "memory", "cc");
#else
  // Fall back to OS - unsure how to reliably query cntvct_el0 frequency.
  timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  t = ts.tv_sec * 1000000000LL + ts.tv_nsec;
#endif
  return t;
}

static inline uint64_t TicksAfter() {
  uint64_t t;
#if JXL_ARCH_X64 && JXL_COMPILER_MSVC
  JXL_COMPILER_FENCE;
  unsigned aux;
  t = __rdtscp(&aux);
  hwy::LoadFence();
  JXL_COMPILER_FENCE;
#elif JXL_ARCH_X64 && (JXL_COMPILER_CLANG || JXL_COMPILER_GCC)
  // Use inline asm because __rdtscp generates code to store TSC_AUX (ecx).
  asm volatile(
      "rdtscp\n\t"
      "shl $32, %%rdx\n\t"
      "or %%rdx, %0\n\t"
      "lfence"
      : "=a"(t)
      :
      // "memory" avoids reordering. rcx = TSC_AUX. rdx = TSC >> 32.
      // "cc" = flags modified by SHL.
      : "rcx", "rdx", "memory", "cc");
#else
  t = TicksBefore();  // no difference on other platforms.
#endif
  return t;
}

}  // namespace jxl

#endif  // LIB_PROFILER_TSC_TIMER_H_
