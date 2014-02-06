/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef PROFILER_MARKERS_H
#define PROFILER_MARKERS_H

#include "JSCustomObjectBuilder.h"
#include "JSObjectBuilder.h"
#include "mozilla/TimeStamp.h"
#include "nsAutoPtr.h"

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
  ProfilerMarkerPayload(ProfilerBacktrace* aStack = nullptr);
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
  template<typename Builder>
  typename Builder::Object PreparePayload(Builder& b)
  {
    return preparePayload(b);
  }

protected:
  /**
   * Called from the main thread
   */
  template<typename Builder>
  void prepareCommonProps(const char* aMarkerType, Builder& aBuilder,
                          typename Builder::ObjectHandle aObject);

  /**
   * Called from the main thread
   */
  virtual JSCustomObjectBuilder::Object
  preparePayload(JSCustomObjectBuilder& b) = 0;

  /**
   * Called from the main thread
   */
  virtual JSObjectBuilder::Object
  preparePayload(JSObjectBuilder& b) = 0;

private:
  mozilla::TimeStamp  mStartTime;
  mozilla::TimeStamp  mEndTime;
  ProfilerBacktrace*  mStack;
};

class ProfilerMarkerTracing : public ProfilerMarkerPayload
{
public:
  ProfilerMarkerTracing(const char* aCategory, TracingMetadata aMetaData);

  const char *GetCategory() const { return mCategory; }
  TracingMetadata GetMetaData() const { return mMetaData; }

protected:
  virtual JSCustomObjectBuilder::Object
  preparePayload(JSCustomObjectBuilder& b) { return preparePayloadImp(b); }
  virtual JSObjectBuilder::Object
  preparePayload(JSObjectBuilder& b) { return preparePayloadImp(b); }

private:
  template<typename Builder>
  typename Builder::Object preparePayloadImp(Builder& b);

private:
  const char *mCategory;
  TracingMetadata mMetaData;
};


class gfxASurface;
class ProfilerMarkerImagePayload : public ProfilerMarkerPayload
{
public:
  ProfilerMarkerImagePayload(gfxASurface *aImg);

protected:
  virtual JSCustomObjectBuilder::Object
  preparePayload(JSCustomObjectBuilder& b) { return preparePayloadImp(b); }
  virtual JSObjectBuilder::Object
  preparePayload(JSObjectBuilder& b) { return preparePayloadImp(b); }

private:
  template<typename Builder>
  typename Builder::Object preparePayloadImp(Builder& b);

  nsRefPtr<gfxASurface> mImg;
};

class IOMarkerPayload : public ProfilerMarkerPayload
{
public:
  IOMarkerPayload(const char* aSource, const mozilla::TimeStamp& aStartTime,
                  const mozilla::TimeStamp& aEndTime,
                  ProfilerBacktrace* aStack);

protected:
  virtual JSCustomObjectBuilder::Object
  preparePayload(JSCustomObjectBuilder& b) { return preparePayloadImp(b); }
  virtual JSObjectBuilder::Object
  preparePayload(JSObjectBuilder& b) { return preparePayloadImp(b); }

private:
  template<typename Builder>
  typename Builder::Object preparePayloadImp(Builder& b);

  const char* mSource;
};

#endif // PROFILER_MARKERS_H
