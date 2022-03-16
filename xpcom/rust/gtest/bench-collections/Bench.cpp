/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Overview
// --------
// This file measures the speed of various implementations of C++ and Rust
// collections (hash tables, etc.) used within the codebase. There are a small
// number of benchmarks for each collection type; each benchmark tests certain
// operations (insertion, lookup, iteration, etc.) More benchmarks could easily
// be envisioned, but this small number is enough to characterize the major
// differences between implementations, while keeping the file size and
// complexity low.
//
// Details
// -------
// The file uses `MOZ_GTEST_BENCH_F` so that results are integrated into
// PerfHerder. It is also designed so that individual test benchmarks can be
// run under a profiler.
//
// The C++ code uses `MOZ_RELEASE_ASSERT` extensively to check values and
// ensure operations aren't optimized away by the compiler. The Rust code uses
// `assert!()`. These should be roughly equivalent, but aren't guaranteed to be
// the same. As a result, the intra-C++ comparisons should be reliable, and the
// intra-Rust comparisons should be reliable, but the C++ vs. Rust comparisons
// may be less reliable.
//
// Note that the Rust implementations run very slowly without --enable-release.
//
// Profiling
// ---------
// If you want to measure a particular implementation under a profiler such as
// Callgrind, do something like this:
//
//   MOZ_RUN_GTEST=1 GTEST_FILTER='*BenchCollections*$IMPL*'
//       valgrind --tool=callgrind --callgrind-out-file=clgout
//       $OBJDIR/dist/bin/firefox -unittest
//   callgrind_annotate --auto=yes clgout > clgann
//
// where $IMPL is part of an implementation name in a test (e.g. "PLDHash",
// "MozHash") and $OBJDIR is an objdir containing a --enable-release build.
//
// Note that multiple processes are spawned, so `clgout` gets overwritten
// multiple times, but the last process to write its profiling data to file is
// the one of interest. (Alternatively, use --callgrind-out-file=clgout.%p to
// get separate output files for each process, with a PID suffix.)

#include "gtest/gtest.h"
#include "gtest/MozGTestBench.h"  // For MOZ_GTEST_BENCH
#include "mozilla/AllocPolicy.h"
#include "mozilla/ArrayUtils.h"
#include "mozilla/HashFunctions.h"
#include "mozilla/HashTable.h"
#include "mozilla/StaticMutex.h"
#include "mozilla/TimeStamp.h"
#include "PLDHashTable.h"
#include <unordered_set>

using namespace mozilla;

// This function gives a pseudo-random sequence with the following properties:
// - Deterministic and platform-independent.
// - No duplicates in the first VALS_LEN results, which is useful for ensuring
//   the tables get to a particular size, and also for guaranteeing lookups
//   that fail.
static uintptr_t MyRand() {
  static uintptr_t s = 0;
  s = s * 1103515245 + 12345;
  return s;
}

// Keep this in sync with Params in bench.rs.
struct Params {
  const char* mConfigName;
  size_t mNumInserts;            // Insert this many unique keys
  size_t mNumSuccessfulLookups;  // Does mNumInserts lookups each time
  size_t mNumFailingLookups;     // Does mNumInserts lookups each time
  size_t mNumIterations;         // Iterates the full table each time
  bool mRemoveInserts;           // Remove all entries at end?
};

// We don't use std::unordered_{set,map}, but it's an interesting thing to
// benchmark against.
//
// Keep this in sync with all the other Bench_*() functions.
static void Bench_Cpp_unordered_set(const Params* aParams, void** aVals,
                                    size_t aLen) {
  std::unordered_set<void*> hs;

  for (size_t j = 0; j < aParams->mNumInserts; j++) {
    hs.insert(aVals[j]);
  }

  for (size_t i = 0; i < aParams->mNumSuccessfulLookups; i++) {
    for (size_t j = 0; j < aParams->mNumInserts; j++) {
      MOZ_RELEASE_ASSERT(hs.find(aVals[j]) != hs.end());
    }
  }

  for (size_t i = 0; i < aParams->mNumFailingLookups; i++) {
    for (size_t j = aParams->mNumInserts; j < aParams->mNumInserts * 2; j++) {
      MOZ_RELEASE_ASSERT(hs.find(aVals[j]) == hs.end());
    }
  }

  for (size_t i = 0; i < aParams->mNumIterations; i++) {
    size_t n = 0;
    for (const auto& elem : hs) {
      (void)elem;
      n++;
    }
    MOZ_RELEASE_ASSERT(aParams->mNumInserts == n);
    MOZ_RELEASE_ASSERT(hs.size() == n);
  }

  if (aParams->mRemoveInserts) {
    for (size_t j = 0; j < aParams->mNumInserts; j++) {
      MOZ_RELEASE_ASSERT(hs.erase(aVals[j]) == 1);
    }
    MOZ_RELEASE_ASSERT(hs.size() == 0);
  } else {
    MOZ_RELEASE_ASSERT(hs.size() == aParams->mNumInserts);
  }
}

// Keep this in sync with all the other Bench_*() functions.
static void Bench_Cpp_PLDHashTable(const Params* aParams, void** aVals,
                                   size_t aLen) {
  PLDHashTable hs(PLDHashTable::StubOps(), sizeof(PLDHashEntryStub));

  for (size_t j = 0; j < aParams->mNumInserts; j++) {
    auto entry = static_cast<PLDHashEntryStub*>(hs.Add(aVals[j]));
    MOZ_RELEASE_ASSERT(!entry->key);
    entry->key = aVals[j];
  }

  for (size_t i = 0; i < aParams->mNumSuccessfulLookups; i++) {
    for (size_t j = 0; j < aParams->mNumInserts; j++) {
      MOZ_RELEASE_ASSERT(hs.Search(aVals[j]));
    }
  }

  for (size_t i = 0; i < aParams->mNumFailingLookups; i++) {
    for (size_t j = aParams->mNumInserts; j < aParams->mNumInserts * 2; j++) {
      MOZ_RELEASE_ASSERT(!hs.Search(aVals[j]));
    }
  }

  for (size_t i = 0; i < aParams->mNumIterations; i++) {
    size_t n = 0;
    for (auto iter = hs.Iter(); !iter.Done(); iter.Next()) {
      n++;
    }
    MOZ_RELEASE_ASSERT(aParams->mNumInserts == n);
    MOZ_RELEASE_ASSERT(hs.EntryCount() == n);
  }

  if (aParams->mRemoveInserts) {
    for (size_t j = 0; j < aParams->mNumInserts; j++) {
      hs.Remove(aVals[j]);
    }
    MOZ_RELEASE_ASSERT(hs.EntryCount() == 0);
  } else {
    MOZ_RELEASE_ASSERT(hs.EntryCount() == aParams->mNumInserts);
  }
}

// Keep this in sync with all the other Bench_*() functions.
static void Bench_Cpp_MozHashSet(const Params* aParams, void** aVals,
                                 size_t aLen) {
  mozilla::HashSet<void*, mozilla::DefaultHasher<void*>, MallocAllocPolicy> hs;

  for (size_t j = 0; j < aParams->mNumInserts; j++) {
    MOZ_RELEASE_ASSERT(hs.put(aVals[j]));
  }

  for (size_t i = 0; i < aParams->mNumSuccessfulLookups; i++) {
    for (size_t j = 0; j < aParams->mNumInserts; j++) {
      MOZ_RELEASE_ASSERT(hs.has(aVals[j]));
    }
  }

  for (size_t i = 0; i < aParams->mNumFailingLookups; i++) {
    for (size_t j = aParams->mNumInserts; j < aParams->mNumInserts * 2; j++) {
      MOZ_RELEASE_ASSERT(!hs.has(aVals[j]));
    }
  }

  for (size_t i = 0; i < aParams->mNumIterations; i++) {
    size_t n = 0;
    for (auto iter = hs.iter(); !iter.done(); iter.next()) {
      n++;
    }
    MOZ_RELEASE_ASSERT(aParams->mNumInserts == n);
    MOZ_RELEASE_ASSERT(hs.count() == n);
  }

  if (aParams->mRemoveInserts) {
    for (size_t j = 0; j < aParams->mNumInserts; j++) {
      hs.remove(aVals[j]);
    }
    MOZ_RELEASE_ASSERT(hs.count() == 0);
  } else {
    MOZ_RELEASE_ASSERT(hs.count() == aParams->mNumInserts);
  }
}

extern "C" {
void Bench_Rust_HashSet(const Params* params, void** aVals, size_t aLen);
void Bench_Rust_FnvHashSet(const Params* params, void** aVals, size_t aLen);
void Bench_Rust_FxHashSet(const Params* params, void** aVals, size_t aLen);
}

static const size_t VALS_LEN = 131072;

// Each benchmark measures a different aspect of performance.
// Note that no "Inserts" value can exceed VALS_LEN.
// Also, if any failing lookups are done, Inserts must be <= VALS_LEN/2.
const Params gParamsList[] = {
    // clang-format off
  //                           Successful Failing              Remove
  //                 Inserts   lookups    lookups  Iterations  inserts
  { "succ_lookups",  1024,     5000,      0,       0,          false },
  { "fail_lookups",  1024,     0,         5000,    0,          false },
  { "insert_remove", VALS_LEN, 0,         0,       0,          true  },
  { "iterate",       1024,     0,         0,       5000,       false },
    // clang-format on
};

class BenchCollections : public ::testing::Test {
 protected:
  void SetUp() override {
    StaticMutexAutoLock lock(sValsMutex);

    if (!sVals) {
      sVals = (void**)malloc(VALS_LEN * sizeof(void*));
      for (size_t i = 0; i < VALS_LEN; i++) {
        // This leaves the high 32 bits zero on 64-bit platforms, but that
        // should still be enough randomness to get typical behaviour.
        sVals[i] = reinterpret_cast<void*>(uintptr_t(MyRand()));
      }
    }

    printf("\n");
    for (size_t i = 0; i < ArrayLength(gParamsList); i++) {
      const Params* params = &gParamsList[i];
      printf("%14s", params->mConfigName);
    }
    printf("%14s\n", "total");
  }

 public:
  void BenchImpl(void (*aBench)(const Params*, void**, size_t)) {
    StaticMutexAutoLock lock(sValsMutex);

    double total = 0;
    for (size_t i = 0; i < ArrayLength(gParamsList); i++) {
      const Params* params = &gParamsList[i];
      TimeStamp t1 = TimeStamp::Now();
      aBench(params, sVals, VALS_LEN);
      TimeStamp t2 = TimeStamp::Now();
      double t = (t2 - t1).ToMilliseconds();
      printf("%11.1f ms", t);
      total += t;
    }
    printf("%11.1f ms\n", total);
  }

 private:
  // Random values used in the benchmarks.
  static void** sVals;

  // A mutex that protects all benchmark operations, ensuring that two
  // benchmarks never run concurrently.
  static StaticMutex sValsMutex;
};

void** BenchCollections::sVals;
StaticMutex BenchCollections::sValsMutex;

MOZ_GTEST_BENCH_F(BenchCollections, unordered_set,
                  [this] { BenchImpl(Bench_Cpp_unordered_set); });

MOZ_GTEST_BENCH_F(BenchCollections, PLDHash,
                  [this] { BenchImpl(Bench_Cpp_PLDHashTable); });

MOZ_GTEST_BENCH_F(BenchCollections, MozHash,
                  [this] { BenchImpl(Bench_Cpp_MozHashSet); });

MOZ_GTEST_BENCH_F(BenchCollections, RustHash,
                  [this] { BenchImpl(Bench_Rust_HashSet); });

MOZ_GTEST_BENCH_F(BenchCollections, RustFnvHash,
                  [this] { BenchImpl(Bench_Rust_FnvHashSet); });

MOZ_GTEST_BENCH_F(BenchCollections, RustFxHash,
                  [this] { BenchImpl(Bench_Rust_FxHashSet); });
