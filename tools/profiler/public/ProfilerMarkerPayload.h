/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ProfilerMarkerPayload_h
#define ProfilerMarkerPayload_h

#include "mozilla/Attributes.h"
#include "mozilla/RefPtr.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/UniquePtrExtensions.h"

#include "nsString.h"
#include "GeckoProfiler.h"

#include "js/Utility.h"
#include "gfxASurface.h"

namespace mozilla {
namespace layers {
class Layer;
} // namespace layers
} // namespace mozilla

class SpliceableJSONWriter;
class UniqueStacks;

// This is an abstract class that can be implemented to supply data to be
// attached with a profiler marker.
//
// When subclassing this, note that the destructor can be called on any thread,
// i.e. not necessarily on the thread that created the object.
class ProfilerMarkerPayload
{
public:
  explicit ProfilerMarkerPayload(UniqueProfilerBacktrace aStack = nullptr);
  ProfilerMarkerPayload(const mozilla::TimeStamp& aStartTime,
                        const mozilla::TimeStamp& aEndTime,
                        UniqueProfilerBacktrace aStack = nullptr);

  virtual ~ProfilerMarkerPayload() {}

  virtual void StreamPayload(SpliceableJSONWriter& aWriter,
                             const mozilla::TimeStamp& aProcessStartTime,
                             UniqueStacks& aUniqueStacks) = 0;

  mozilla::TimeStamp GetStartTime() const { return mStartTime; }

protected:
  void StreamCommonProps(const char* aMarkerType,
                         SpliceableJSONWriter& aWriter,
                         const mozilla::TimeStamp& aProcessStartTime,
                         UniqueStacks& aUniqueStacks);

  void SetStack(UniqueProfilerBacktrace aStack)
  {
    mStack = mozilla::Move(aStack);
  }

private:
  mozilla::TimeStamp mStartTime;
  mozilla::TimeStamp mEndTime;
  UniqueProfilerBacktrace mStack;
};

class TracingMarkerPayload : public ProfilerMarkerPayload
{
public:
  TracingMarkerPayload(const char* aCategory, TracingKind aKind,
                       UniqueProfilerBacktrace aCause = nullptr);

  virtual void StreamPayload(SpliceableJSONWriter& aWriter,
                             const mozilla::TimeStamp& aProcessStartTime,
                             UniqueStacks& aUniqueStacks) override;

private:
  const char *mCategory;
  TracingKind mKind;
};

class IOMarkerPayload : public ProfilerMarkerPayload
{
public:
  IOMarkerPayload(const char* aSource, const char* aFilename,
                  const mozilla::TimeStamp& aStartTime,
                  const mozilla::TimeStamp& aEndTime,
                  UniqueProfilerBacktrace aStack);

  virtual void StreamPayload(SpliceableJSONWriter& aWriter,
                             const mozilla::TimeStamp& aProcessStartTime,
                             UniqueStacks& aUniqueStacks) override;

private:
  const char* mSource;
  mozilla::UniqueFreePtr<char> mFilename;
};

class DOMEventMarkerPayload : public ProfilerMarkerPayload
{
public:
  DOMEventMarkerPayload(const nsAString& aType, uint16_t aPhase,
                        const mozilla::TimeStamp& aStartTime,
                        const mozilla::TimeStamp& aEndTime);

  virtual void StreamPayload(SpliceableJSONWriter& aWriter,
                             const mozilla::TimeStamp& aProcessStartTime,
                             UniqueStacks& aUniqueStacks) override;

private:
  nsString mType;
  uint16_t mPhase;
};

class UserTimingMarkerPayload : public ProfilerMarkerPayload
{
public:
  UserTimingMarkerPayload(const nsAString& aName,
                          const mozilla::TimeStamp& aStartTime);
  UserTimingMarkerPayload(const nsAString& aName,
                          const mozilla::TimeStamp& aStartTime,
                          const mozilla::TimeStamp& aEndTime);

  virtual void StreamPayload(SpliceableJSONWriter& aWriter,
                             const mozilla::TimeStamp& aProcessStartTime,
                             UniqueStacks& aUniqueStacks) override;

private:
  // Either "mark" or "measure".
  const char* mEntryType;
  nsString mName;
};

// Contains the translation applied to a 2d layer so we can track the layer
// position at each frame.
class LayerTranslationMarkerPayload : public ProfilerMarkerPayload
{
public:
  LayerTranslationMarkerPayload(mozilla::layers::Layer* aLayer,
                                mozilla::gfx::Point aPoint);

  virtual void StreamPayload(SpliceableJSONWriter& aWriter,
                             const mozilla::TimeStamp& aProcessStartTime,
                             UniqueStacks& aUniqueStacks) override;

private:
  mozilla::layers::Layer* mLayer;
  mozilla::gfx::Point mPoint;
};

#include "Units.h"    // For ScreenIntPoint

// Tracks when a vsync occurs according to the HardwareComposer.
class VsyncMarkerPayload : public ProfilerMarkerPayload
{
public:
  explicit VsyncMarkerPayload(mozilla::TimeStamp aVsyncTimestamp);

  virtual void StreamPayload(SpliceableJSONWriter& aWriter,
                             const mozilla::TimeStamp& aProcessStartTime,
                             UniqueStacks& aUniqueStacks) override;

private:
  mozilla::TimeStamp mVsyncTimestamp;
};

class GPUMarkerPayload : public ProfilerMarkerPayload
{
public:
  GPUMarkerPayload(const mozilla::TimeStamp& aCpuTimeStart,
                   const mozilla::TimeStamp& aCpuTimeEnd,
                   uint64_t aGpuTimeStart,
                   uint64_t aGpuTimeEnd);

  virtual void StreamPayload(SpliceableJSONWriter& aWriter,
                             const mozilla::TimeStamp& aProcessStartTime,
                             UniqueStacks& aUniqueStacks) override;

private:
  mozilla::TimeStamp mCpuTimeStart;
  mozilla::TimeStamp mCpuTimeEnd;
  uint64_t mGpuTimeStart;
  uint64_t mGpuTimeEnd;
};

class GCSliceMarkerPayload : public ProfilerMarkerPayload
{
public:
  GCSliceMarkerPayload(const mozilla::TimeStamp& aStartTime,
                       const mozilla::TimeStamp& aEndTime,
                       JS::UniqueChars&& aTimingJSON)
   : ProfilerMarkerPayload(aStartTime, aEndTime, nullptr),
     mTimingJSON(mozilla::Move(aTimingJSON))
  {}

  void StreamPayload(SpliceableJSONWriter& aWriter,
                     const mozilla::TimeStamp& aProcessStartTime,
                     UniqueStacks& aUniqueStacks) override;

private:
  JS::UniqueChars mTimingJSON;
};

class GCMajorMarkerPayload : public ProfilerMarkerPayload
{
public:
  GCMajorMarkerPayload(const mozilla::TimeStamp& aStartTime,
                       const mozilla::TimeStamp& aEndTime,
                       JS::UniqueChars&& aTimingJSON)
   : ProfilerMarkerPayload(aStartTime, aEndTime, nullptr),
     mTimingJSON(mozilla::Move(aTimingJSON))
  {}

  void StreamPayload(SpliceableJSONWriter& aWriter,
                     const mozilla::TimeStamp& aProcessStartTime,
                     UniqueStacks& aUniqueStacks) override;

private:
  JS::UniqueChars mTimingJSON;
};

class GCMinorMarkerPayload : public ProfilerMarkerPayload
{
public:
  GCMinorMarkerPayload(const mozilla::TimeStamp& aStartTime,
                       const mozilla::TimeStamp& aEndTime,
                       JS::UniqueChars&& aTimingData)
   : ProfilerMarkerPayload(aStartTime, aEndTime, nullptr),
     mTimingData(mozilla::Move(aTimingData))
  {}

  void StreamPayload(SpliceableJSONWriter& aWriter,
                     const mozilla::TimeStamp& aProcessStartTime,
                     UniqueStacks& aUniqueStacks) override;

private:
  JS::UniqueChars mTimingData;
};

#endif // ProfilerMarkerPayload_h
