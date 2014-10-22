/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef PROFILER_MARKERS_H
#define PROFILER_MARKERS_H

#include "JSStreamWriter.h"
#include "mozilla/TimeStamp.h"
#include "nsAutoPtr.h"
#include "Units.h"    // For ScreenIntPoint

namespace mozilla {
namespace layers {
class Layer;
} // layers
} // mozilla

/**
 * This is an abstract object that can be implied to supply
 * data to be attached with a profiler marker. Most data inserted
 * into a profile is stored in a circular buffer. This buffer
 * typically wraps around and overwrites most entries. Because
 * of this, this structure is designed to defer the work of
 * prepare the payload only when 'preparePayload' is called.
 *
 * Note when implementing that this object is typically constructed
 * on a particular thread but 'preparePayload' and the destructor
 * is called from the main thread.
 */
class ProfilerMarkerPayload
{
public:
  /**
   * ProfilerMarkerPayload takes ownership of aStack
   */
  explicit ProfilerMarkerPayload(ProfilerBacktrace* aStack = nullptr);
  ProfilerMarkerPayload(const mozilla::TimeStamp& aStartTime,
                        const mozilla::TimeStamp& aEndTime,
                        ProfilerBacktrace* aStack = nullptr);

  /**
   * Called from the main thread
   */
  virtual ~ProfilerMarkerPayload();

  /**
   * Called from the main thread
   */
  void StreamPayload(JSStreamWriter& b) {
    return streamPayload(b);
  }

protected:
  /**
   * Called from the main thread
   */
  void streamCommonProps(const char* aMarkerType, JSStreamWriter& b);

  /**
   * Called from the main thread
   */
  virtual void
  streamPayload(JSStreamWriter& b) = 0;

  void SetStack(ProfilerBacktrace* aStack) { mStack = aStack; }

private:
  mozilla::TimeStamp  mStartTime;
  mozilla::TimeStamp  mEndTime;
  ProfilerBacktrace*  mStack;
};

class ProfilerMarkerTracing : public ProfilerMarkerPayload
{
public:
  ProfilerMarkerTracing(const char* aCategory, TracingMetadata aMetaData);
  ProfilerMarkerTracing(const char* aCategory, TracingMetadata aMetaData, ProfilerBacktrace* aCause);

  const char *GetCategory() const { return mCategory; }
  TracingMetadata GetMetaData() const { return mMetaData; }

protected:
  virtual void
  streamPayload(JSStreamWriter& b) { return streamPayloadImp(b); }

private:
  void streamPayloadImp(JSStreamWriter& b);

private:
  const char *mCategory;
  TracingMetadata mMetaData;
};


#include "gfxASurface.h"
class ProfilerMarkerImagePayload : public ProfilerMarkerPayload
{
public:
  explicit ProfilerMarkerImagePayload(gfxASurface *aImg);

protected:
  virtual void
  streamPayload(JSStreamWriter& b) { return streamPayloadImp(b); }

private:
  void streamPayloadImp(JSStreamWriter& b);

  nsRefPtr<gfxASurface> mImg;
};

class IOMarkerPayload : public ProfilerMarkerPayload
{
public:
  IOMarkerPayload(const char* aSource, const char* aFilename, const mozilla::TimeStamp& aStartTime,
                  const mozilla::TimeStamp& aEndTime,
                  ProfilerBacktrace* aStack);
  ~IOMarkerPayload();

protected:
  virtual void
  streamPayload(JSStreamWriter& b) { return streamPayloadImp(b); }

private:
  void streamPayloadImp(JSStreamWriter& b);

  const char* mSource;
  char* mFilename;
};

/**
 * Contains the translation applied to a 2d layer so we can
 * track the layer position at each frame.
 */
class LayerTranslationPayload : public ProfilerMarkerPayload
{
public:
  LayerTranslationPayload(mozilla::layers::Layer* aLayer,
                          mozilla::gfx::Point aPoint);

protected:
  virtual void
  streamPayload(JSStreamWriter& b) { return streamPayloadImpl(b); }

private:
  void streamPayloadImpl(JSStreamWriter& b);
  mozilla::layers::Layer* mLayer;
  mozilla::gfx::Point mPoint;
};

/**
 * Tracks when touch events are processed by gecko, not when
 * the touch actually occured in gonk/android.
 */
class TouchDataPayload : public ProfilerMarkerPayload
{
public:
  explicit TouchDataPayload(const mozilla::ScreenIntPoint& aPoint);
  virtual ~TouchDataPayload() {}

protected:
  virtual void
  streamPayload(JSStreamWriter& b) { return streamPayloadImpl(b); }

private:
  void streamPayloadImpl(JSStreamWriter& b);
  mozilla::ScreenIntPoint mPoint;
};

/**
 * Tracks when a vsync occurs according to the HardwareComposer.
 */
class VsyncPayload : public ProfilerMarkerPayload
{
public:
  explicit VsyncPayload(mozilla::TimeStamp aVsyncTimestamp);
  virtual ~VsyncPayload() {}

protected:
  virtual void
  streamPayload(JSStreamWriter& b) { return streamPayloadImpl(b); }

private:
  void streamPayloadImpl(JSStreamWriter& b);
  mozilla::TimeStamp mVsyncTimestamp;
};

#endif // PROFILER_MARKERS_H
