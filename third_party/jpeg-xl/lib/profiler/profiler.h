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

#ifndef LIB_PROFILER_PROFILER_H_
#define LIB_PROFILER_PROFILER_H_

// High precision, low overhead time measurements. Returns exact call counts and
// total elapsed time for user-defined 'zones' (code regions, i.e. C++ scopes).
//
// Usage: add this header to BUILD srcs; instrument regions of interest:
// { PROFILER_ZONE("name"); /*code*/ } or
// void FuncToMeasure() { PROFILER_FUNC; /*code*/ }.
// After all threads have exited any zones, invoke PROFILER_PRINT_RESULTS() to
// print call counts and average durations [CPU cycles] to stdout, sorted in
// descending order of total duration.

// If zero, this file has no effect and no measurements will be recorded.
#ifndef PROFILER_ENABLED
#define PROFILER_ENABLED 0
#endif
#if PROFILER_ENABLED

#include <stddef.h>
#include <stdint.h>

#include <hwy/targets.h>

#include "lib/jxl/base/arch_macros.h"  // for JXL_ARCH_*
#include "lib/jxl/base/cache_aligned.h"
#include "lib/jxl/base/compiler_specific.h"
#include "lib/jxl/base/status.h"
#include "lib/profiler/tsc_timer.h"

#if JXL_ARCH_X64 && HWY_TARGET != HWY_SCALAR
#define PROFILER_BUFFER 1
#else
#define PROFILER_BUFFER 0
#endif

namespace jxl {

// Represents zone entry/exit events. Stores a full-resolution timestamp plus
// an offset (representing zone name or identifying exit packets). POD.
class Packet {
 public:
  // If offsets do not fit, UpdateOrAdd will overrun our heap allocation
  // (governed by kMaxZones). We have seen ~100 MiB static binaries.
  static constexpr size_t kOffsetBits = 27;
  static constexpr uintptr_t kOffsetBias = 1ULL << (kOffsetBits - 1);

  // We need full-resolution timestamps; at an effective rate of 4 GHz,
  // this permits 34 second zone durations (for longer durations, split into
  // multiple zones). Wraparound is handled by masking.
  static constexpr size_t kTimestampBits = 64 - kOffsetBits;
  static constexpr uint64_t kTimestampMask = (1ULL << kTimestampBits) - 1;

  static Packet Make(const uint64_t biased_offset, const uint64_t timestamp) {
    JXL_DASSERT(biased_offset < (1ULL << kOffsetBits));

    Packet packet;
    packet.bits_ =
        (biased_offset << kTimestampBits) + (timestamp & kTimestampMask);
    return packet;
  }

  uint64_t Timestamp() const { return bits_ & kTimestampMask; }

  uintptr_t BiasedOffset() const { return (bits_ >> kTimestampBits); }

 private:
  uint64_t bits_;
};
static_assert(sizeof(Packet) == 8, "Wrong Packet size");

class Results;

// Per-thread packet storage, allocated via CacheAligned.
class ThreadSpecific {
  static constexpr size_t kBufferCapacity =
      CacheAligned::kCacheLineSize / sizeof(Packet);

 public:
  // `zone_name` is used to sanity-check offsets fit in kOffsetBits.
  explicit ThreadSpecific(const char* zone_name);
  ~ThreadSpecific();

  // Depends on Zone => defined out of line.
  void ComputeOverhead();

  void WriteEntry(const char* name, const uint64_t timestamp) {
    const uint64_t biased_offset =
        reinterpret_cast<uintptr_t>(name) - string_origin_;
    Write(Packet::Make(biased_offset, timestamp));
  }

  void WriteExit(const uint64_t timestamp) {
    const uint64_t biased_offset = Packet::kOffsetBias;
    Write(Packet::Make(biased_offset, timestamp));
  }

  void AnalyzeRemainingPackets();

  Results& GetResults() { return *results_; }

 private:
  void FlushStorage();
#if PROFILER_BUFFER
  void FlushBuffer();
#endif

  // Write packet to buffer/storage, emptying them as needed.
  void Write(const Packet packet) {
#if PROFILER_BUFFER
    if (buffer_size_ == kBufferCapacity) {  // Full
      FlushBuffer();
    }
    buffer_[buffer_size_] = packet;
    ++buffer_size_;
#else
    if (num_packets_ >= max_packets_) {  // Full
      FlushStorage();
    }
    packets_[num_packets_] = packet;
    ++num_packets_;
#endif  // PROFILER_BUFFER
  }

  // Write-combining buffer to avoid cache pollution. Must be the first
  // non-static member to ensure cache-line alignment.
#if PROFILER_BUFFER
  Packet buffer_[kBufferCapacity];
  size_t buffer_size_ = 0;
#endif

  // Contiguous storage for zone enter/exit packets.
  Packet* const JXL_RESTRICT packets_;
  size_t num_packets_;
  const size_t max_packets_;

  // Cached here because we already read this cache line on zone entry/exit.
  uintptr_t string_origin_;

  Results* results_;
};

// RAII zone enter/exit recorder constructed by the ZONE macro; also
// responsible for initializing ThreadSpecific.
class Zone {
 public:
  // "name" must be a string literal (see StringOrigin).
  JXL_NOINLINE explicit Zone(const char* name) {
    JXL_COMPILER_FENCE;
    ThreadSpecific* JXL_RESTRICT thread_specific = GetThreadSpecific();
    if (JXL_UNLIKELY(thread_specific == nullptr)) {
      thread_specific = InitThreadSpecific(name);
    }

    // (Capture timestamp ASAP, not inside WriteEntry.)
    JXL_COMPILER_FENCE;
    const uint64_t timestamp = TicksBefore();
    thread_specific->WriteEntry(name, timestamp);
  }

  JXL_NOINLINE ~Zone() {
    JXL_COMPILER_FENCE;
    const uint64_t timestamp = TicksAfter();
    GetThreadSpecific()->WriteExit(timestamp);
    JXL_COMPILER_FENCE;
  }

  // Call exactly once after all threads have exited all zones.
  static void PrintResults();

 private:
  // Returns reference to the thread's ThreadSpecific pointer (initially null).
  // Function-local static avoids needing a separate definition.
  static ThreadSpecific*& GetThreadSpecific() {
    static thread_local ThreadSpecific* thread_specific;
    return thread_specific;
  }

  // Non time-critical.
  ThreadSpecific* InitThreadSpecific(const char* zone_name);
};

// Creates a zone starting from here until the end of the current scope.
// Timestamps will be recorded when entering and exiting the zone.
// "name" must be a string literal, which is ensured by merging with "".
#define PROFILER_ZONE(name)        \
  JXL_COMPILER_FENCE;              \
  const ::jxl::Zone zone("" name); \
  JXL_COMPILER_FENCE

// Creates a zone for an entire function (when placed at its beginning).
// Shorter/more convenient than ZONE.
#define PROFILER_FUNC               \
  JXL_COMPILER_FENCE;               \
  const ::jxl::Zone zone(__func__); \
  JXL_COMPILER_FENCE

#define PROFILER_PRINT_RESULTS ::jxl::Zone::PrintResults

}  // namespace jxl

#else  // !PROFILER_ENABLED
#define PROFILER_ZONE(name)
#define PROFILER_FUNC
#define PROFILER_PRINT_RESULTS()
#endif

#endif  // LIB_PROFILER_PROFILER_H_
