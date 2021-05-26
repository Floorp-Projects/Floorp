// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include "lib/jxl/base/profiler.h"

#if PROFILER_ENABLED

#include <stdio.h>
#include <stdlib.h>
#include <string.h>  // memcpy

#include <algorithm>  // sort
#include <atomic>
#include <cinttypes>  // PRIu64
#include <hwy/cache_control.h>
#include <new>

#include "lib/jxl/base/robust_statistics.h"  // HalfSampleMode

// Optionally use SIMD in StreamCacheLine if available.
#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "lib/profiler/profiler.cc"
#include <hwy/foreach_target.h>
#include <hwy/highway.h>

HWY_BEFORE_NAMESPACE();
namespace profiler {
namespace HWY_NAMESPACE {

// Overwrites `to` without loading it into cache (read-for-ownership).
// Copies 64 bytes from/to naturally aligned addresses.
void StreamCacheLine(const Packet* HWY_RESTRICT from, Packet* HWY_RESTRICT to) {
#if HWY_TARGET == HWY_SCALAR
  hwy::CopyBytes<64>(from, to);
#else
  const HWY_CAPPED(uint64_t, 2) d;
  HWY_FENCE;
  const uint64_t* HWY_RESTRICT from64 = reinterpret_cast<const uint64_t*>(from);
  const auto v0 = Load(d, from64 + 0);
  const auto v1 = Load(d, from64 + 2);
  const auto v2 = Load(d, from64 + 4);
  const auto v3 = Load(d, from64 + 6);
  // Fences prevent the compiler from reordering loads/stores, which may
  // interfere with write-combining.
  HWY_FENCE;
  uint64_t* HWY_RESTRICT to64 = reinterpret_cast<uint64_t*>(to);
  Stream(v0, d, to64 + 0);
  Stream(v1, d, to64 + 2);
  Stream(v2, d, to64 + 4);
  Stream(v3, d, to64 + 6);
  HWY_FENCE;
#endif
}

// NOLINTNEXTLINE(google-readability-namespace-comments)
}  // namespace HWY_NAMESPACE
}  // namespace profiler
HWY_AFTER_NAMESPACE();

#if HWY_ONCE
namespace profiler {

HWY_EXPORT(StreamCacheLine);

namespace {

// How many mebibytes to allocate (if PROFILER_ENABLED) per thread that
// enters at least one zone. Once this buffer is full, the thread will analyze
// packets (two per zone), which introduces observer overhead.
#ifndef PROFILER_THREAD_STORAGE
#define PROFILER_THREAD_STORAGE 32ULL
#endif

#define PROFILER_PRINT_OVERHEAD 0

// Upper bounds for fixed-size data structures (guarded via HWY_ASSERT):
constexpr size_t kMaxDepth = 64;   // Maximum nesting of zones.
constexpr size_t kMaxZones = 256;  // Total number of zones.

// Stack of active (entered but not exited) zones. POD, uninitialized.
// Used to deduct child duration from the parent's self time.
struct ActiveZone {
  const char* name;
  uint64_t entry_timestamp;
  uint64_t child_total;
};

// Totals for all Zones with the same name. POD, must be zero-initialized.
struct ZoneTotals {
  uint64_t total_duration;
  const char* name;
  uint64_t num_calls;
};

template <typename T>
inline T ClampedSubtract(const T minuend, const T subtrahend) {
  if (subtrahend > minuend) {
    return 0;
  }
  return minuend - subtrahend;
}

}  // namespace

// Per-thread call graph (stack) and ZoneTotals for each zone.
class Results {
 public:
  Results() {
    // Zero-initialize all accumulators (avoids a check for num_zones_ == 0).
    memset(zones_, 0, sizeof(zones_));
  }

  // Used for computing overhead when this thread encounters its first Zone.
  // This has no observable effect apart from increasing "analyze_elapsed_".
  uint64_t ZoneDuration(const Packet* packets) {
    HWY_ASSERT(depth_ == 0);
    HWY_ASSERT(num_zones_ == 0);
    AnalyzePackets(packets, 2);
    const uint64_t duration = zones_[0].total_duration;
    zones_[0].num_calls = 0;
    zones_[0].total_duration = 0;
    HWY_ASSERT(depth_ == 0);
    num_zones_ = 0;
    return duration;
  }

  void SetSelfOverhead(const uint64_t self_overhead) {
    self_overhead_ = self_overhead;
  }

  void SetChildOverhead(const uint64_t child_overhead) {
    child_overhead_ = child_overhead;
  }

  // Draw all required information from the packets, which can be discarded
  // afterwards. Called whenever this thread's storage is full.
  void AnalyzePackets(const Packet* HWY_RESTRICT packets,
                      const size_t num_packets) {
    // Ensures prior weakly-ordered streaming stores are globally visible.
    hwy::StoreFence();

    const uint64_t t0 = TicksBefore();

    for (size_t i = 0; i < num_packets; ++i) {
      const uint64_t timestamp = packets[i].timestamp;
      // Entering a zone
      if (packets[i].name != nullptr) {
        HWY_ASSERT(depth_ < kMaxDepth);
        zone_stack_[depth_].name = packets[i].name;
        zone_stack_[depth_].entry_timestamp = timestamp;
        zone_stack_[depth_].child_total = 0;
        ++depth_;
        continue;
      }

      HWY_ASSERT(depth_ != 0);
      const ActiveZone& active = zone_stack_[depth_ - 1];
      const uint64_t duration = timestamp - active.entry_timestamp;
      const uint64_t self_duration = ClampedSubtract(
          duration, self_overhead_ + child_overhead_ + active.child_total);

      UpdateOrAdd(active.name, 1, self_duration);
      --depth_;

      // "Deduct" the nested time from its parent's self_duration.
      if (depth_ != 0) {
        zone_stack_[depth_ - 1].child_total += duration + child_overhead_;
      }
    }

    const uint64_t t1 = TicksAfter();
    analyze_elapsed_ += t1 - t0;
  }

  // Incorporates results from another thread. Call after all threads have
  // exited any zones.
  void Assimilate(const Results& other) {
    const uint64_t t0 = TicksBefore();
    HWY_ASSERT(depth_ == 0);
    HWY_ASSERT(other.depth_ == 0);

    for (size_t i = 0; i < other.num_zones_; ++i) {
      const ZoneTotals& zone = other.zones_[i];
      UpdateOrAdd(zone.name, zone.num_calls, zone.total_duration);
    }
    const uint64_t t1 = TicksAfter();
    analyze_elapsed_ += t1 - t0 + other.analyze_elapsed_;
  }

  // Single-threaded.
  void Print() {
    const uint64_t t0 = TicksBefore();
    MergeDuplicates();

    // Sort by decreasing total (self) cost.
    std::sort(zones_, zones_ + num_zones_,
              [](const ZoneTotals& r1, const ZoneTotals& r2) {
                return r1.total_duration > r2.total_duration;
              });

    uint64_t total_visible_duration = 0;
    for (size_t i = 0; i < num_zones_; ++i) {
      const ZoneTotals& r = zones_[i];
      if (r.name[0] != '@') {
        total_visible_duration += r.total_duration;
        printf("%-40s: %10" PRIu64 " x %15" PRIu64 "= %15" PRIu64 "\n", r.name,
               r.num_calls, r.total_duration / r.num_calls, r.total_duration);
      }
    }

    const uint64_t t1 = TicksAfter();
    analyze_elapsed_ += t1 - t0;
    printf("Total clocks during analysis: %" PRIu64 "\n", analyze_elapsed_);
    printf("Total clocks measured: %" PRIu64 "\n", total_visible_duration);
  }

  // Single-threaded. Clears all results as if no zones had been recorded.
  void Reset() {
    analyze_elapsed_ = 0;
    HWY_ASSERT(depth_ == 0);
    num_zones_ = 0;
    memset(zone_stack_, 0, sizeof(zone_stack_));
    memset(zones_, 0, sizeof(zones_));
  }

 private:
  // Updates ZoneTotals of the same name, or inserts a new one if this thread
  // has not yet seen that name. Uses a self-organizing list data structure,
  // which avoids dynamic memory allocations and is faster than unordered_map.
  void UpdateOrAdd(const char* name, const uint64_t num_calls,
                   const uint64_t duration) {
    // Special case for first zone: (maybe) update, without swapping.
    if (zones_[0].name == name) {
      zones_[0].total_duration += duration;
      zones_[0].num_calls += num_calls;
      return;
    }

    // Look for a zone with the same name.
    for (size_t i = 1; i < num_zones_; ++i) {
      if (zones_[i].name == name) {
        zones_[i].total_duration += duration;
        zones_[i].num_calls += num_calls;
        // Swap with predecessor (more conservative than move to front,
        // but at least as successful).
        std::swap(zones_[i - 1], zones_[i]);
        return;
      }
    }

    // Not found; create a new ZoneTotals.
    HWY_ASSERT(num_zones_ < kMaxZones);
    ZoneTotals* HWY_RESTRICT zone = zones_ + num_zones_;
    zone->name = name;
    zone->num_calls = num_calls;
    zone->total_duration = duration;
    ++num_zones_;
  }

  // Each instantiation of a function template seems to get its own copy of
  // __func__ and GCC doesn't merge them. An N^2 search for duplicates is
  // acceptable because we only expect a few dozen zones.
  void MergeDuplicates() {
    for (size_t i = 0; i < num_zones_; ++i) {
      // Add any subsequent duplicates to num_calls and total_duration.
      for (size_t j = i + 1; j < num_zones_;) {
        if (!strcmp(zones_[i].name, zones_[j].name)) {
          zones_[i].num_calls += zones_[j].num_calls;
          zones_[i].total_duration += zones_[j].total_duration;
          // Fill hole with last item.
          zones_[j] = zones_[--num_zones_];
        } else {  // Name differed, try next ZoneTotals.
          ++j;
        }
      }
    }
  }

  uint64_t analyze_elapsed_ = 0;
  uint64_t self_overhead_ = 0;
  uint64_t child_overhead_ = 0;

  size_t depth_ = 0;      // Number of active zones <= kMaxDepth.
  size_t num_zones_ = 0;  // Number of unique zones <= kMaxZones.

  // After other members to avoid large pointer offsets.
  alignas(64) ActiveZone zone_stack_[kMaxDepth];  // Last = newest
  alignas(64) ZoneTotals zones_[kMaxZones];       // Self-organizing list
};

ThreadSpecific::ThreadSpecific()
    : max_packets_(PROFILER_THREAD_STORAGE << 16),  // MiB / sizeof(Packet)
      packets_(hwy::AllocateAligned<Packet>(max_packets_)),
      num_packets_(0),
      results_(hwy::MakeUniqueAligned<Results>()) {}

ThreadSpecific::~ThreadSpecific() {}

void ThreadSpecific::FlushBuffer() {
  if (num_packets_ + kBufferCapacity > max_packets_) {
    results_->AnalyzePackets(packets_.get(), num_packets_);
    num_packets_ = 0;
  }
  // This buffering halves observer overhead and decreases the overall
  // runtime by about 3%.
  HWY_DYNAMIC_DISPATCH(StreamCacheLine)
  (buffer_, packets_.get() + num_packets_);
  num_packets_ += kBufferCapacity;
  buffer_size_ = 0;
}

void ThreadSpecific::AnalyzeRemainingPackets() {
  // Storage full => empty it.
  if (num_packets_ + buffer_size_ > max_packets_) {
    results_->AnalyzePackets(packets_.get(), num_packets_);
    num_packets_ = 0;
  }

  // Move buffer to storage
  memcpy(packets_.get() + num_packets_, buffer_, buffer_size_ * sizeof(Packet));
  num_packets_ += buffer_size_;
  buffer_size_ = 0;

  results_->AnalyzePackets(packets_.get(), num_packets_);
  num_packets_ = 0;
}

void ThreadSpecific::ComputeOverhead() {
  // Delay after capturing timestamps before/after the actual zone runs. Even
  // with frequency throttling disabled, this has a multimodal distribution,
  // including 32, 34, 48, 52, 59, 62.
  uint64_t self_overhead;
  {
    const size_t kNumSamples = 32;
    uint32_t samples[kNumSamples];
    for (size_t idx_sample = 0; idx_sample < kNumSamples; ++idx_sample) {
      const size_t kNumDurations = 1024;
      uint32_t durations[kNumDurations];

      for (size_t idx_duration = 0; idx_duration < kNumDurations;
           ++idx_duration) {
        {  //
          PROFILER_ZONE("Dummy Zone (never shown)");
        }
        const uint64_t duration = results_->ZoneDuration(buffer_);
        buffer_size_ = 0;
        durations[idx_duration] = static_cast<uint32_t>(duration);
        HWY_ASSERT(num_packets_ == 0);
      }
      jxl::CountingSort(durations, durations + kNumDurations);
      samples[idx_sample] = jxl::HalfSampleMode()(durations, kNumDurations);
    }
    // Median.
    jxl::CountingSort(samples, samples + kNumSamples);
    self_overhead = samples[kNumSamples / 2];
#if PROFILER_PRINT_OVERHEAD
    printf("Overhead: %zu\n", self_overhead);
#endif
    results_->SetSelfOverhead(self_overhead);
  }

  // Delay before capturing start timestamp / after end timestamp.
  const size_t kNumSamples = 32;
  uint32_t samples[kNumSamples];
  for (size_t idx_sample = 0; idx_sample < kNumSamples; ++idx_sample) {
    const size_t kNumDurations = 16;
    uint32_t durations[kNumDurations];
    for (size_t idx_duration = 0; idx_duration < kNumDurations;
         ++idx_duration) {
      const size_t kReps = 10000;
      // Analysis time should not be included => must fit within buffer.
      HWY_ASSERT(kReps * 2 < max_packets_);
      hwy::StoreFence();
      const uint64_t t0 = TicksBefore();
      for (size_t i = 0; i < kReps; ++i) {
        PROFILER_ZONE("Dummy");
      }
      hwy::StoreFence();
      const uint64_t t1 = TicksAfter();
      HWY_ASSERT(num_packets_ + buffer_size_ == kReps * 2);
      buffer_size_ = 0;
      num_packets_ = 0;
      const uint64_t avg_duration = (t1 - t0 + kReps / 2) / kReps;
      durations[idx_duration] =
          static_cast<uint32_t>(ClampedSubtract(avg_duration, self_overhead));
    }
    jxl::CountingSort(durations, durations + kNumDurations);
    samples[idx_sample] = jxl::HalfSampleMode()(durations, kNumDurations);
  }
  jxl::CountingSort(samples, samples + kNumSamples);
  const uint64_t child_overhead = samples[9 * kNumSamples / 10];
#if PROFILER_PRINT_OVERHEAD
  printf("Child overhead: %zu\n", child_overhead);
#endif
  results_->SetChildOverhead(child_overhead);
}

namespace {

// Could be a static member of Zone, but that would expose <atomic> in header.
std::atomic<ThreadSpecific*>& GetHead() {
  static std::atomic<ThreadSpecific*> head_{nullptr};  // Owning
  return head_;
}

}  // namespace

// Thread-safe.
ThreadSpecific* Zone::InitThreadSpecific() {
  ThreadSpecific* thread_specific =
      hwy::MakeUniqueAligned<ThreadSpecific>().release();

  // Insert into unordered list
  std::atomic<ThreadSpecific*>& head = GetHead();
  ThreadSpecific* old_head = head.load(std::memory_order_relaxed);
  thread_specific->SetNext(old_head);
  while (!head.compare_exchange_weak(old_head, thread_specific,
                                     std::memory_order_release,
                                     std::memory_order_relaxed)) {
    thread_specific->SetNext(old_head);
    // TODO(janwas): pause
  }

  // ComputeOverhead also creates a Zone, so this needs to be set before that
  // to prevent infinite recursion.
  GetThreadSpecific() = thread_specific;

  thread_specific->ComputeOverhead();
  return thread_specific;
}

// Single-threaded.
/*static*/ void Zone::PrintResults() {
  ThreadSpecific* head = GetHead().load(std::memory_order_relaxed);
  ThreadSpecific* p = head;
  while (p) {
    p->AnalyzeRemainingPackets();

    // Combine all threads into a single Result.
    if (p != head) {
      head->GetResults().Assimilate(p->GetResults());
      p->GetResults().Reset();
    }

    p = p->GetNext();
  }

  if (head != nullptr) {
    head->GetResults().Print();
    head->GetResults().Reset();
  }
}

}  // namespace profiler

#endif  // HWY_ONCE
#endif  // PROFILER_ENABLED
