// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#ifndef LIB_PROFILER_PROFILER_H_
#define LIB_PROFILER_PROFILER_H_

// High precision, low overhead time measurements. Returns exact call counts and
// total elapsed time for user-defined 'zones' (code regions, i.e. C++ scopes).
//
// Usage: instrument regions of interest: { PROFILER_ZONE("name"); /*code*/ } or
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

#include <hwy/aligned_allocator.h>
#include <hwy/base.h>

#include "lib/profiler/tsc_timer.h"

#if HWY_COMPILER_MSVC
#define PROFILER_PUBLIC
#else
#define PROFILER_PUBLIC __attribute__((visibility("default")))
#endif

namespace profiler {

// Represents zone entry/exit events. POD.
#pragma pack(push, 1)
struct Packet {
  // Computing a hash or string table is likely too expensive, and offsets
  // from other libraries' string literals can be too large to combine them and
  // a full-resolution timestamp into 64 bits.
  uint64_t timestamp;
  const char* name;  // nullptr for exit packets
#if UINTPTR_MAX <= 0xFFFFFFFFu
  uint32_t padding;
#endif
};
#pragma pack(pop)
static_assert(sizeof(Packet) == 16, "Wrong Packet size");

class Results;  // pImpl

// Per-thread packet storage, dynamically allocated and aligned.
class ThreadSpecific {
  static constexpr size_t kBufferCapacity = 64 / sizeof(Packet);

 public:
  PROFILER_PUBLIC explicit ThreadSpecific();
  PROFILER_PUBLIC ~ThreadSpecific();

  // Depends on Zone => defined out of line.
  PROFILER_PUBLIC void ComputeOverhead();

  HWY_INLINE void WriteEntry(const char* name) { Write(name, TicksBefore()); }
  HWY_INLINE void WriteExit() { Write(nullptr, TicksAfter()); }

  PROFILER_PUBLIC void AnalyzeRemainingPackets();

  // Accessors instead of public member for well-defined data layout.
  void SetNext(ThreadSpecific* next) { next_ = next; }
  ThreadSpecific* GetNext() const { return next_; }

  Results& GetResults() { return *results_; }

 private:
  PROFILER_PUBLIC void FlushBuffer();

  // Write packet to buffer/storage, emptying them as needed.
  void Write(const char* name, const uint64_t timestamp) {
    if (buffer_size_ == kBufferCapacity) {  // Full
      FlushBuffer();
    }
    buffer_[buffer_size_].name = name;
    buffer_[buffer_size_].timestamp = timestamp;
    ++buffer_size_;
  }

  // Write-combining buffer to avoid cache pollution. Must be the first
  // non-static member to ensure cache-line alignment.
  Packet buffer_[kBufferCapacity];
  size_t buffer_size_ = 0;

  // Contiguous storage for zone enter/exit packets.
  const size_t max_packets_;
  hwy::AlignedFreeUniquePtr<Packet[]> packets_;
  size_t num_packets_;

  // Linked list of all threads.
  ThreadSpecific* next_ = nullptr;  // Owned, never released.

  hwy::AlignedUniquePtr<Results> results_;
};

// RAII zone enter/exit recorder constructed by PROFILER_ZONE; also
// responsible for initializing ThreadSpecific.
class Zone {
 public:
  HWY_NOINLINE explicit Zone(const char* name) {
    HWY_FENCE;
    ThreadSpecific* HWY_RESTRICT thread_specific = GetThreadSpecific();
    if (HWY_UNLIKELY(thread_specific == nullptr)) {
      thread_specific = InitThreadSpecific();
    }

    thread_specific->WriteEntry(name);
  }

  HWY_NOINLINE ~Zone() { GetThreadSpecific()->WriteExit(); }

  // Call exactly once after all threads have exited all zones.
  PROFILER_PUBLIC static void PrintResults();

 private:
  // Returns reference to the thread's ThreadSpecific pointer (initially null).
  // Function-local static avoids needing a separate definition.
  static ThreadSpecific*& GetThreadSpecific() {
    static thread_local ThreadSpecific* thread_specific;
    return thread_specific;
  }

  // Non time-critical.
  PROFILER_PUBLIC ThreadSpecific* InitThreadSpecific();
};

// Creates a zone starting from here until the end of the current scope.
// Timestamps will be recorded when entering and exiting the zone.
// To ensure the name pointer remains valid, we require it to be a string
// literal (by merging with ""). We also compare strings by address.
#define PROFILER_ZONE(name)             \
  HWY_FENCE;                            \
  const ::profiler::Zone zone("" name); \
  HWY_FENCE

// Creates a zone for an entire function (when placed at its beginning).
// Shorter/more convenient than ZONE.
#define PROFILER_FUNC                    \
  HWY_FENCE;                             \
  const ::profiler::Zone zone(__func__); \
  HWY_FENCE

#define PROFILER_PRINT_RESULTS ::profiler::Zone::PrintResults

}  // namespace profiler

#else  // !PROFILER_ENABLED
#define PROFILER_ZONE(name)
#define PROFILER_FUNC
#define PROFILER_PRINT_RESULTS()
#endif

#endif  // LIB_PROFILER_PROFILER_H_
