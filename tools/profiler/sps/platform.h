// Copyright (c) 2006-2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef ANDROID
#include <android/log.h>
#else
#define __android_log_print(a, ...)
#endif

#include "mozilla/Util.h"
#include "mozilla/unused.h"
#include <vector>
#define ASSERT(a) MOZ_ASSERT(a)
#ifdef ANDROID
#define ENABLE_SPS_LEAF_DATA
#define LOG(text) __android_log_print(ANDROID_LOG_ERROR, "profiler", "%s", text);
#else
#define LOG(text) printf("Profiler: %s\n", text)
#endif

#include <stdint.h>
typedef uint8 byte;
typedef byte* Address;

class MapEntry {
public:
  MapEntry(unsigned long aStart, unsigned long aEnd, unsigned long aOffset, char *aName)
    : mStart(aStart)
    , mEnd(aEnd)
    , mOffset(aOffset)
    , mName(strdup(aName))
  {}

  MapEntry(const MapEntry& aEntry)
    : mStart(aEntry.mStart)
    , mEnd(aEntry.mEnd)
    , mOffset(aEntry.mOffset)
    , mName(strdup(aEntry.mName))
  {}

  ~MapEntry()
  {
    free(mName);
  }

  unsigned long GetStart() { return mStart; }
  unsigned long GetEnd() { return mEnd; }
  char* GetName() { return mName; }

private:
  unsigned long mStart;
  unsigned long mEnd;
  unsigned long mOffset;
  char *mName;
};

class MapInfo {
public:
  MapInfo() {}

  void AddMapEntry(MapEntry entry)
  {
    mEntries.push_back(entry);
  }

  MapEntry& GetEntry(size_t i)
  {
    return mEntries[i];
  }

  size_t GetSize()
  {
    return mEntries.size();
  }
private:
  std::vector<MapEntry> mEntries;
};

#ifdef ENABLE_SPS_LEAF_DATA
struct MapInfo getmaps(pid_t pid);
#endif

// ----------------------------------------------------------------------------
// Sampler
//
// A sampler periodically samples the state of the VM and optionally
// (if used for profiling) the program counter and stack pointer for
// the thread that created it.

// TickSample captures the information collected for each sample.
class TickSample {
 public:
  TickSample()
      :
        pc(NULL),
        sp(NULL),
        fp(NULL),
        function(NULL),
        frames_count(0) {}
  Address pc;  // Instruction pointer.
  Address sp;  // Stack pointer.
  Address fp;  // Frame pointer.
  Address function;  // The last called JS function.
  static const int kMaxFramesCount = 64;
  Address stack[kMaxFramesCount];  // Call stack.
  int frames_count;  // Number of captured frames.
};

class Sampler {
 public:
  // Initialize sampler.
  explicit Sampler(int interval, bool profiling);
  virtual ~Sampler();

  // Performs stack sampling.
  virtual void SampleStack(TickSample* sample) = 0;

  // This method is called for each sampling period with the current
  // program counter.
  virtual void Tick(TickSample* sample) = 0;

  // Request a save from a signal handler
  virtual void RequestSave() = 0;
  // Process any outstanding request outside a signal handler.
  virtual void HandleSaveRequest() = 0;

  // Start and stop sampler.
  void Start();
  void Stop();

  // Is the sampler used for profiling?
  bool IsProfiling() const { return profiling_; }

  // Is the sampler running in sync with the JS thread? On platforms
  // where the sampler is implemented with a thread that wakes up
  // every now and then, having a synchronous sampler implies
  // suspending/resuming the JS thread.
  bool IsSynchronous() const { return synchronous_; }

  // Whether the sampler is running (that is, consumes resources).
  bool IsActive() const { return active_; }

  class PlatformData;

 private:
  const int interval_;
  const bool profiling_;
  const bool synchronous_;
  bool active_;
  PlatformData* data_;  // Platform specific data.
};

