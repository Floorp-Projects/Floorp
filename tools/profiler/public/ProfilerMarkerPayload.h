/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ProfilerMarkerPayload_h
#define ProfilerMarkerPayload_h

#include "mozilla/Atomics.h"
#include "mozilla/Attributes.h"
#include "mozilla/ipc/ProtocolUtils.h"
#include "mozilla/Maybe.h"
#include "mozilla/net/TimingStruct.h"
#include "mozilla/Preferences.h"
#include "mozilla/ProfileBufferEntrySerialization.h"
#include "mozilla/RefPtr.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/UniquePtrExtensions.h"

#include "nsString.h"
#include "nsCRTGlue.h"
#include "GeckoProfiler.h"

#include "js/Utility.h"
#include "js/AllocationRecording.h"
#include "js/ProfilingFrameIterator.h"
#include "gfxASurface.h"
#include "mozilla/ServoTraversalStatistics.h"

namespace mozilla {
namespace layers {
class Layer;
}  // namespace layers
}  // namespace mozilla

class SpliceableJSONWriter;
class UniqueStacks;

// This is an abstract class that can be implemented to supply data to be
// attached with a profiler marker.
//
// When subclassing this, note that the destructor can be called on any thread,
// i.e. not necessarily on the thread that created the object.
class ProfilerMarkerPayload {
 public:
  explicit ProfilerMarkerPayload(
      const mozilla::Maybe<uint64_t>& aInnerWindowID = mozilla::Nothing(),
      UniqueProfilerBacktrace aStack = nullptr)
      : mCommonProps{mozilla::TimeStamp{}, mozilla::TimeStamp{},
                     std::move(aStack), aInnerWindowID} {}

  ProfilerMarkerPayload(
      const mozilla::TimeStamp& aStartTime, const mozilla::TimeStamp& aEndTime,
      const mozilla::Maybe<uint64_t>& aInnerWindowID = mozilla::Nothing(),
      UniqueProfilerBacktrace aStack = nullptr)
      : mCommonProps{aStartTime, aEndTime, std::move(aStack), aInnerWindowID} {}

  virtual ~ProfilerMarkerPayload() = default;

  // Compute the number of bytes needed to serialize the `DeserializerTag` and
  // payload, including in the no-payload (nullptr) case.
  static mozilla::ProfileBufferEntryWriter::Length TagAndSerializationBytes(
      const ProfilerMarkerPayload* aPayload) {
    if (!aPayload) {
      return sizeof(DeserializerTag);
    }
    return aPayload->TagAndSerializationBytes();
  }

  // Serialize the payload into an EntryWriter, including in the no-payload
  // (nullptr) case. Must be of the exact size given by
  // `TagAndSerializationBytes(aPayload)`.
  static void TagAndSerialize(const ProfilerMarkerPayload* aPayload,
                              mozilla::ProfileBufferEntryWriter& aEntryWriter) {
    if (!aPayload) {
      aEntryWriter.WriteObject(DeserializerTag(0));
      return;
    }
    aPayload->SerializeTagAndPayload(aEntryWriter);
  }

  // Deserialize a payload from an EntryReader, including in the no-payload
  // (nullptr) case.
  static mozilla::UniquePtr<ProfilerMarkerPayload> DeserializeTagAndPayload(
      mozilla::ProfileBufferEntryReader& aER) {
    const auto tag = aER.ReadObject<DeserializerTag>();
    Deserializer deserializer = DeserializerForTag(tag);
    return deserializer(aER);
  }

  virtual void StreamPayload(SpliceableJSONWriter& aWriter,
                             const mozilla::TimeStamp& aProcessStartTime,
                             UniqueStacks& aUniqueStacks) const = 0;

  mozilla::TimeStamp GetStartTime() const { return mCommonProps.mStartTime; }

 protected:
  // A `Deserializer` is a free function that can read a serialized payload from
  // an `EntryReader` and return a reconstructed `ProfilerMarkerPayload`
  // sub-object (may be null if there was no payload).
  typedef mozilla::UniquePtr<ProfilerMarkerPayload> (*Deserializer)(
      mozilla::ProfileBufferEntryReader&);

  // A `DeserializerTag` will be added before the payload, to help select the
  // correct deserializer when reading back the payload.
  using DeserializerTag = unsigned char;

  // This needs to be big enough to handle all possible sub-types of
  // ProfilerMarkerPayload.
  static constexpr DeserializerTag DeserializerMax = 32;

  // We need an atomic type that can hold a `DeserializerTag`. (Atomic doesn't
  // work with too-small types.)
  using DeserializerTagAtomic = int;

  // Number of currently-registered deserializers.
  static mozilla::Atomic<DeserializerTagAtomic, mozilla::ReleaseAcquire>
      sDeserializerCount;

  // List of currently-registered deserializers.
  // sDeserializers[0] is a no-payload deserializer.
  static Deserializer sDeserializers[DeserializerMax];

  // Get the `DeserializerTag` for a `Deserializer` (which gets registered on
  // the first call.) Tag 0 means no payload; a null `aDeserializer` gives that
  // 0 tag.
  static DeserializerTag TagForDeserializer(Deserializer aDeserializer);

  // Get the `Deserializer` for a given `DeserializerTag`.
  // Tag 0 is reserved as no-payload deserializer (which returns nullptr).
  static Deserializer DeserializerForTag(DeserializerTag aTag);

  struct CommonProps {
    mozilla::TimeStamp mStartTime;
    mozilla::TimeStamp mEndTime;
    UniqueProfilerBacktrace mStack;
    mozilla::Maybe<uint64_t> mInnerWindowID;
  };

  // Deserializers can use this base constructor.
  explicit ProfilerMarkerPayload(CommonProps&& aCommonProps)
      : mCommonProps(std::move(aCommonProps)) {}

  // Serialization/deserialization of common props in ProfilerMarkerPayload.
  mozilla::ProfileBufferEntryWriter::Length
  CommonPropsTagAndSerializationBytes() const;
  void SerializeTagAndCommonProps(
      DeserializerTag aDeserializerTag,
      mozilla::ProfileBufferEntryWriter& aEntryWriter) const;
  static CommonProps DeserializeCommonProps(
      mozilla::ProfileBufferEntryReader& aEntryReader);

  void StreamType(const char* aMarkerType, SpliceableJSONWriter& aWriter) const;

  void StreamCommonProps(const char* aMarkerType, SpliceableJSONWriter& aWriter,
                         const mozilla::TimeStamp& aProcessStartTime,
                         UniqueStacks& aUniqueStacks) const;

 private:
  // Compute the number of bytes needed to serialize payload in
  // `SerializeTagAndPayload` below.
  virtual mozilla::ProfileBufferEntryWriter::Length TagAndSerializationBytes()
      const = 0;

  // Serialize the payload into an EntryWriter.
  // Must be of the exact size given by `TagAndSerializationBytes()`.
  virtual void SerializeTagAndPayload(
      mozilla::ProfileBufferEntryWriter& aEntryWriter) const = 0;

  CommonProps mCommonProps;
};

#define DECL_STREAM_PAYLOAD                                                    \
  void StreamPayload(SpliceableJSONWriter& aWriter,                            \
                     const mozilla::TimeStamp& aProcessStartTime,              \
                     UniqueStacks& aUniqueStacks) const override;              \
  static mozilla::UniquePtr<ProfilerMarkerPayload> Deserialize(                \
      mozilla::ProfileBufferEntryReader& aEntryReader);                        \
  mozilla::ProfileBufferEntryWriter::Length TagAndSerializationBytes()         \
      const override;                                                          \
  void SerializeTagAndPayload(mozilla::ProfileBufferEntryWriter& aEntryWriter) \
      const override;

// TODO: Increase the coverage of tracing markers that include InnerWindowID
// information
class TracingMarkerPayload : public ProfilerMarkerPayload {
 public:
  TracingMarkerPayload(
      const char* aCategory, TracingKind aKind,
      const mozilla::Maybe<uint64_t>& aInnerWindowID = mozilla::Nothing(),
      UniqueProfilerBacktrace aCause = nullptr)
      : ProfilerMarkerPayload(aInnerWindowID, std::move(aCause)),
        mCategory(aCategory),
        mKind(aKind) {}

  DECL_STREAM_PAYLOAD

 protected:
  TracingMarkerPayload(CommonProps&& aCommonProps, const char* aCategory,
                       TracingKind aKind)
      : ProfilerMarkerPayload(std::move(aCommonProps)),
        mCategory(aCategory),
        mKind(aKind) {}

  // May be used by derived classes.
  void SerializeTagAndPayload(
      DeserializerTag aDeserializerTag,
      mozilla::ProfileBufferEntryWriter& aEntryWriter) const;

 private:
  const char* mCategory;
  TracingKind mKind;
};

class FileIOMarkerPayload : public ProfilerMarkerPayload {
 public:
  FileIOMarkerPayload(const char* aOperation, const char* aSource,
                      const char* aFilename,
                      const mozilla::TimeStamp& aStartTime,
                      const mozilla::TimeStamp& aEndTime,
                      UniqueProfilerBacktrace aStack)
      : ProfilerMarkerPayload(aStartTime, aEndTime, mozilla::Nothing(),
                              std::move(aStack)),
        mSource(aSource),
        mOperation(aOperation ? strdup(aOperation) : nullptr),
        mFilename(aFilename ? strdup(aFilename) : nullptr) {
    MOZ_ASSERT(aSource);
  }

  DECL_STREAM_PAYLOAD

 private:
  FileIOMarkerPayload(CommonProps&& aCommonProps, const char* aSource,
                      mozilla::UniqueFreePtr<char>&& aOperation,
                      mozilla::UniqueFreePtr<char>&& aFilename)
      : ProfilerMarkerPayload(std::move(aCommonProps)),
        mSource(aSource),
        mOperation(std::move(aOperation)),
        mFilename(std::move(aFilename)) {}

  const char* mSource;
  mozilla::UniqueFreePtr<char> mOperation;
  mozilla::UniqueFreePtr<char> mFilename;
};

class DOMEventMarkerPayload : public TracingMarkerPayload {
 public:
  DOMEventMarkerPayload(const nsAString& aEventType,
                        const mozilla::TimeStamp& aTimeStamp,
                        const char* aCategory, TracingKind aKind,
                        const mozilla::Maybe<uint64_t>& aInnerWindowID)
      : TracingMarkerPayload(aCategory, aKind, aInnerWindowID),
        mTimeStamp(aTimeStamp),
        mEventType(aEventType) {}

  DECL_STREAM_PAYLOAD

 private:
  DOMEventMarkerPayload(CommonProps&& aCommonProps, const char* aCategory,
                        TracingKind aKind, mozilla::TimeStamp aTimeStamp,
                        nsString aEventType)
      : TracingMarkerPayload(std::move(aCommonProps), aCategory, aKind),
        mTimeStamp(aTimeStamp),
        mEventType(aEventType) {}

  mozilla::TimeStamp mTimeStamp;
  nsString mEventType;
};

class PrefMarkerPayload : public ProfilerMarkerPayload {
 public:
  PrefMarkerPayload(const char* aPrefName,
                    const mozilla::Maybe<mozilla::PrefValueKind>& aPrefKind,
                    const mozilla::Maybe<mozilla::PrefType>& aPrefType,
                    const nsCString& aPrefValue,
                    const mozilla::TimeStamp& aPrefAccessTime)
      : ProfilerMarkerPayload(aPrefAccessTime, aPrefAccessTime),
        mPrefAccessTime(aPrefAccessTime),
        mPrefName(aPrefName),
        mPrefKind(aPrefKind),
        mPrefType(aPrefType),
        mPrefValue(aPrefValue) {}

  DECL_STREAM_PAYLOAD

 private:
  PrefMarkerPayload(CommonProps&& aCommonProps,
                    mozilla::TimeStamp aPrefAccessTime, nsCString&& aPrefName,
                    mozilla::Maybe<mozilla::PrefValueKind>&& aPrefKind,
                    mozilla::Maybe<mozilla::PrefType>&& aPrefType,
                    nsCString&& aPrefValue)
      : ProfilerMarkerPayload(std::move(aCommonProps)),
        mPrefAccessTime(aPrefAccessTime),
        mPrefName(aPrefName),
        mPrefKind(aPrefKind),
        mPrefType(aPrefType),
        mPrefValue(aPrefValue) {}

  mozilla::TimeStamp mPrefAccessTime;
  nsCString mPrefName;
  // Nothing means this is a shared preference. Something, on the other hand,
  // holds an actual PrefValueKind indicating either a Default or User
  // preference.
  mozilla::Maybe<mozilla::PrefValueKind> mPrefKind;
  // Nothing means that the mPrefName preference was not found. Something
  // contains the type of the preference.
  mozilla::Maybe<mozilla::PrefType> mPrefType;
  nsCString mPrefValue;
};

class UserTimingMarkerPayload : public ProfilerMarkerPayload {
 public:
  UserTimingMarkerPayload(const nsAString& aName,
                          const mozilla::TimeStamp& aStartTime,
                          const mozilla::Maybe<uint64_t>& aInnerWindowID)
      : ProfilerMarkerPayload(aStartTime, aStartTime, aInnerWindowID),
        mEntryType("mark"),
        mName(aName) {}

  UserTimingMarkerPayload(const nsAString& aName,
                          const mozilla::Maybe<nsString>& aStartMark,
                          const mozilla::Maybe<nsString>& aEndMark,
                          const mozilla::TimeStamp& aStartTime,
                          const mozilla::TimeStamp& aEndTime,
                          const mozilla::Maybe<uint64_t>& aInnerWindowID)
      : ProfilerMarkerPayload(aStartTime, aEndTime, aInnerWindowID),
        mEntryType("measure"),
        mName(aName),
        mStartMark(aStartMark),
        mEndMark(aEndMark) {}

  DECL_STREAM_PAYLOAD

 private:
  UserTimingMarkerPayload(CommonProps&& aCommonProps, const char* aEntryType,
                          nsString&& aName,
                          mozilla::Maybe<nsString>&& aStartMark,
                          mozilla::Maybe<nsString>&& aEndMark)
      : ProfilerMarkerPayload(std::move(aCommonProps)),
        mEntryType(aEntryType),
        mName(std::move(aName)),
        mStartMark(std::move(aStartMark)),
        mEndMark(std::move(aEndMark)) {}

  // Either "mark" or "measure".
  const char* mEntryType;
  nsString mName;
  mozilla::Maybe<nsString> mStartMark;
  mozilla::Maybe<nsString> mEndMark;
};

// Contains the translation applied to a 2d layer so we can track the layer
// position at each frame.
class LayerTranslationMarkerPayload : public ProfilerMarkerPayload {
 public:
  LayerTranslationMarkerPayload(mozilla::layers::Layer* aLayer,
                                mozilla::gfx::Point aPoint,
                                mozilla::TimeStamp aStartTime)
      : ProfilerMarkerPayload(aStartTime, aStartTime),
        mLayer(aLayer),
        mPoint(aPoint) {}

  DECL_STREAM_PAYLOAD

 private:
  LayerTranslationMarkerPayload(CommonProps&& aCommonProps,
                                mozilla::layers::Layer* aLayer,
                                mozilla::gfx::Point aPoint)
      : ProfilerMarkerPayload(std::move(aCommonProps)),
        mLayer(aLayer),
        mPoint(aPoint) {}

  mozilla::layers::Layer* mLayer;
  mozilla::gfx::Point mPoint;
};

#include "Units.h"  // For ScreenIntPoint

// Tracks when a vsync occurs according to the HardwareComposer.
class VsyncMarkerPayload : public ProfilerMarkerPayload {
 public:
  explicit VsyncMarkerPayload(mozilla::TimeStamp aVsyncTimestamp)
      : ProfilerMarkerPayload(aVsyncTimestamp, aVsyncTimestamp) {}

  DECL_STREAM_PAYLOAD

 private:
  explicit VsyncMarkerPayload(CommonProps&& aCommonProps)
      : ProfilerMarkerPayload(std::move(aCommonProps)) {}
};

class NetworkMarkerPayload : public ProfilerMarkerPayload {
 public:
  NetworkMarkerPayload(int64_t aID, const char* aURI, NetworkLoadType aType,
                       const mozilla::TimeStamp& aStartTime,
                       const mozilla::TimeStamp& aEndTime, int32_t aPri,
                       int64_t aCount,
                       mozilla::net::CacheDisposition aCacheDisposition,
                       uint64_t aInnerWindowID,
                       const mozilla::net::TimingStruct* aTimings = nullptr,
                       const char* aRedirectURI = nullptr,
                       UniqueProfilerBacktrace aSource = nullptr)
      : ProfilerMarkerPayload(aStartTime, aEndTime,
                              mozilla::Some(aInnerWindowID),
                              std::move(aSource)),
        mID(aID),
        mURI(aURI ? strdup(aURI) : nullptr),
        mRedirectURI(aRedirectURI && (strlen(aRedirectURI) > 0)
                         ? strdup(aRedirectURI)
                         : nullptr),
        mType(aType),
        mPri(aPri),
        mCount(aCount),
        mCacheDisposition(aCacheDisposition) {
    if (aTimings) {
      mTimings = *aTimings;
    }
  }

  DECL_STREAM_PAYLOAD

 private:
  NetworkMarkerPayload(CommonProps&& aCommonProps, int64_t aID,
                       mozilla::UniqueFreePtr<char>&& aURI,
                       mozilla::UniqueFreePtr<char>&& aRedirectURI,
                       NetworkLoadType aType, int32_t aPri, int64_t aCount,
                       mozilla::net::TimingStruct aTimings,
                       mozilla::net::CacheDisposition aCacheDisposition)
      : ProfilerMarkerPayload(std::move(aCommonProps)),
        mID(aID),
        mURI(std::move(aURI)),
        mRedirectURI(std::move(aRedirectURI)),
        mType(aType),
        mPri(aPri),
        mCount(aCount),
        mTimings(aTimings),
        mCacheDisposition(aCacheDisposition) {}

  int64_t mID;
  mozilla::UniqueFreePtr<char> mURI;
  mozilla::UniqueFreePtr<char> mRedirectURI;
  NetworkLoadType mType;
  int32_t mPri;
  int64_t mCount;
  mozilla::net::TimingStruct mTimings;
  mozilla::net::CacheDisposition mCacheDisposition;
};

class ScreenshotPayload : public ProfilerMarkerPayload {
 public:
  explicit ScreenshotPayload(mozilla::TimeStamp aTimeStamp,
                             nsCString&& aScreenshotDataURL,
                             const mozilla::gfx::IntSize& aWindowSize,
                             uintptr_t aWindowIdentifier)
      : ProfilerMarkerPayload(aTimeStamp, mozilla::TimeStamp()),
        mScreenshotDataURL(std::move(aScreenshotDataURL)),
        mWindowSize(aWindowSize),
        mWindowIdentifier(aWindowIdentifier) {}

  DECL_STREAM_PAYLOAD

 private:
  ScreenshotPayload(CommonProps&& aCommonProps, nsCString&& aScreenshotDataURL,
                    mozilla::gfx::IntSize aWindowSize,
                    uintptr_t aWindowIdentifier)
      : ProfilerMarkerPayload(std::move(aCommonProps)),
        mScreenshotDataURL(std::move(aScreenshotDataURL)),
        mWindowSize(aWindowSize),
        mWindowIdentifier(aWindowIdentifier) {}

  nsCString mScreenshotDataURL;
  mozilla::gfx::IntSize mWindowSize;
  uintptr_t mWindowIdentifier;
};

class GCSliceMarkerPayload : public ProfilerMarkerPayload {
 public:
  GCSliceMarkerPayload(const mozilla::TimeStamp& aStartTime,
                       const mozilla::TimeStamp& aEndTime,
                       JS::UniqueChars&& aTimingJSON)
      : ProfilerMarkerPayload(aStartTime, aEndTime),
        mTimingJSON(std::move(aTimingJSON)) {}

  DECL_STREAM_PAYLOAD

 private:
  GCSliceMarkerPayload(CommonProps&& aCommonProps,
                       JS::UniqueChars&& aTimingJSON)
      : ProfilerMarkerPayload(std::move(aCommonProps)),
        mTimingJSON(std::move(aTimingJSON)) {}

  JS::UniqueChars mTimingJSON;
};

class GCMajorMarkerPayload : public ProfilerMarkerPayload {
 public:
  GCMajorMarkerPayload(const mozilla::TimeStamp& aStartTime,
                       const mozilla::TimeStamp& aEndTime,
                       JS::UniqueChars&& aTimingJSON)
      : ProfilerMarkerPayload(aStartTime, aEndTime),
        mTimingJSON(std::move(aTimingJSON)) {}

  DECL_STREAM_PAYLOAD

 private:
  GCMajorMarkerPayload(CommonProps&& aCommonProps,
                       JS::UniqueChars&& aTimingJSON)
      : ProfilerMarkerPayload(std::move(aCommonProps)),
        mTimingJSON(std::move(aTimingJSON)) {}

  JS::UniqueChars mTimingJSON;
};

class GCMinorMarkerPayload : public ProfilerMarkerPayload {
 public:
  GCMinorMarkerPayload(const mozilla::TimeStamp& aStartTime,
                       const mozilla::TimeStamp& aEndTime,
                       JS::UniqueChars&& aTimingData)
      : ProfilerMarkerPayload(aStartTime, aEndTime),
        mTimingData(std::move(aTimingData)) {}

  DECL_STREAM_PAYLOAD

 private:
  GCMinorMarkerPayload(CommonProps&& aCommonProps,
                       JS::UniqueChars&& aTimingData)
      : ProfilerMarkerPayload(std::move(aCommonProps)),
        mTimingData(std::move(aTimingData)) {}

  JS::UniqueChars mTimingData;
};

class HangMarkerPayload : public ProfilerMarkerPayload {
 public:
  HangMarkerPayload(const mozilla::TimeStamp& aStartTime,
                    const mozilla::TimeStamp& aEndTime)
      : ProfilerMarkerPayload(aStartTime, aEndTime) {}

  DECL_STREAM_PAYLOAD
 private:
  explicit HangMarkerPayload(CommonProps&& aCommonProps)
      : ProfilerMarkerPayload(std::move(aCommonProps)) {}
};

class StyleMarkerPayload : public ProfilerMarkerPayload {
 public:
  StyleMarkerPayload(const mozilla::TimeStamp& aStartTime,
                     const mozilla::TimeStamp& aEndTime,
                     UniqueProfilerBacktrace aCause,
                     const mozilla::ServoTraversalStatistics& aStats,
                     const mozilla::Maybe<uint64_t>& aInnerWindowID)
      : ProfilerMarkerPayload(aStartTime, aEndTime, aInnerWindowID,
                              std::move(aCause)),
        mStats(aStats) {}

  DECL_STREAM_PAYLOAD

 private:
  StyleMarkerPayload(CommonProps&& aCommonProps,
                     mozilla::ServoTraversalStatistics aStats)
      : ProfilerMarkerPayload(std::move(aCommonProps)), mStats(aStats) {}

  mozilla::ServoTraversalStatistics mStats;
};

class LongTaskMarkerPayload : public ProfilerMarkerPayload {
 public:
  LongTaskMarkerPayload(const mozilla::TimeStamp& aStartTime,
                        const mozilla::TimeStamp& aEndTime)
      : ProfilerMarkerPayload(aStartTime, aEndTime) {}

  DECL_STREAM_PAYLOAD

 private:
  explicit LongTaskMarkerPayload(CommonProps&& aCommonProps)
      : ProfilerMarkerPayload(std::move(aCommonProps)) {}
};

class TimingMarkerPayload : public ProfilerMarkerPayload {
 public:
  TimingMarkerPayload(const mozilla::TimeStamp& aStartTime,
                      const mozilla::TimeStamp& aEndTime)
      : ProfilerMarkerPayload(aStartTime, aEndTime) {}

  DECL_STREAM_PAYLOAD

 private:
  TimingMarkerPayload(CommonProps&& aCommonProps)
      : ProfilerMarkerPayload(std::move(aCommonProps)) {}
};

class TextMarkerPayload : public ProfilerMarkerPayload {
 public:
  TextMarkerPayload(const nsACString& aText,
                    const mozilla::TimeStamp& aStartTime)
      : ProfilerMarkerPayload(aStartTime, aStartTime), mText(aText) {}

  TextMarkerPayload(const nsACString& aText,
                    const mozilla::TimeStamp& aStartTime,
                    const mozilla::TimeStamp& aEndTime)
      : ProfilerMarkerPayload(aStartTime, aEndTime), mText(aText) {}

  TextMarkerPayload(const nsACString& aText,
                    const mozilla::TimeStamp& aStartTime,
                    const mozilla::Maybe<uint64_t>& aInnerWindowID)
      : ProfilerMarkerPayload(aStartTime, aStartTime, aInnerWindowID),
        mText(aText) {}

  TextMarkerPayload(const nsACString& aText,
                    const mozilla::TimeStamp& aStartTime,
                    const mozilla::TimeStamp& aEndTime,
                    const mozilla::Maybe<uint64_t>& aInnerWindowID,
                    UniqueProfilerBacktrace aCause = nullptr)
      : ProfilerMarkerPayload(aStartTime, aEndTime, aInnerWindowID,
                              std::move(aCause)),
        mText(aText) {}

  DECL_STREAM_PAYLOAD

 private:
  TextMarkerPayload(CommonProps&& aCommonProps, nsCString&& aText)
      : ProfilerMarkerPayload(std::move(aCommonProps)),
        mText(std::move(aText)) {}

  nsCString mText;
};

class LogMarkerPayload : public ProfilerMarkerPayload {
 public:
  LogMarkerPayload(const char* aModule, const char* aText,
                   const mozilla::TimeStamp& aStartTime)
      : ProfilerMarkerPayload(aStartTime, aStartTime),
        mModule(aModule),
        mText(aText) {}

  LogMarkerPayload(const char* aModule, const char* aText,
                   const mozilla::TimeStamp& aStartTime,
                   const mozilla::TimeStamp& aEndTime)
      : ProfilerMarkerPayload(aStartTime, aEndTime),
        mModule(aModule),
        mText(aText) {}

  DECL_STREAM_PAYLOAD

 private:
  LogMarkerPayload(CommonProps&& aCommonProps, nsAutoCStringN<32>&& aModule,
                   nsCString&& aText)
      : ProfilerMarkerPayload(std::move(aCommonProps)),
        mModule(std::move(aModule)),
        mText(std::move(aText)) {}

  nsAutoCStringN<32> mModule;  // longest known LazyLogModule name is ~24
  nsCString mText;
};

class JsAllocationMarkerPayload : public ProfilerMarkerPayload {
 public:
  JsAllocationMarkerPayload(const mozilla::TimeStamp& aStartTime,
                            JS::RecordAllocationInfo&& aInfo,
                            UniqueProfilerBacktrace aStack)
      : ProfilerMarkerPayload(aStartTime, aStartTime, mozilla::Nothing(),
                              std::move(aStack)),
        // Copy the strings, and take ownership of them.
        mTypeName(aInfo.typeName ? NS_xstrdup(aInfo.typeName) : nullptr),
        mClassName(aInfo.className ? strdup(aInfo.className) : nullptr),
        mDescriptiveTypeName(aInfo.descriptiveTypeName
                                 ? NS_xstrdup(aInfo.descriptiveTypeName)
                                 : nullptr),
        // The coarseType points to a string literal, so does not need to be
        // duplicated.
        mCoarseType(aInfo.coarseType),
        mSize(aInfo.size),
        mInNursery(aInfo.inNursery) {}

  DECL_STREAM_PAYLOAD

 private:
  JsAllocationMarkerPayload(
      CommonProps&& aCommonProps,
      mozilla::UniqueFreePtr<const char16_t>&& aTypeName,
      mozilla::UniqueFreePtr<const char>&& aClassName,
      mozilla::UniqueFreePtr<const char16_t>&& aDescriptiveTypeName,
      const char* aCoarseType, uint64_t aSize, bool aInNursery)
      : ProfilerMarkerPayload(std::move(aCommonProps)),
        mTypeName(std::move(aTypeName)),
        mClassName(std::move(aClassName)),
        mDescriptiveTypeName(std::move(aDescriptiveTypeName)),
        mCoarseType(aCoarseType),
        mSize(aSize),
        mInNursery(aInNursery) {}

  mozilla::UniqueFreePtr<const char16_t> mTypeName;
  mozilla::UniqueFreePtr<const char> mClassName;
  mozilla::UniqueFreePtr<const char16_t> mDescriptiveTypeName;
  // Points to a string literal, so does not need to be freed.
  const char* mCoarseType;

  // The size in bytes of the allocation.
  uint64_t mSize;

  // Whether or not the allocation is in the nursery or not.
  bool mInNursery;
};

// This payload is for collecting information about native allocations. There is
// a memory hook into malloc and other memory functions that can sample a subset
// of the allocations. This information is then stored in this payload.
class NativeAllocationMarkerPayload : public ProfilerMarkerPayload {
 public:
  NativeAllocationMarkerPayload(const mozilla::TimeStamp& aStartTime,
                                int64_t aSize, uintptr_t aMemoryAddress,
                                int aThreadId, UniqueProfilerBacktrace aStack)
      : ProfilerMarkerPayload(aStartTime, aStartTime, mozilla::Nothing(),
                              std::move(aStack)),
        mSize(aSize),
        mMemoryAddress(aMemoryAddress),
        mThreadId(aThreadId) {}

  DECL_STREAM_PAYLOAD

 private:
  NativeAllocationMarkerPayload(CommonProps&& aCommonProps, int64_t aSize,
                                uintptr_t aMemoryAddress, int aThreadId)
      : ProfilerMarkerPayload(std::move(aCommonProps)),
        mSize(aSize),
        mMemoryAddress(aMemoryAddress),
        mThreadId(aThreadId) {}

  // The size in bytes of the allocation. If the number is negative then it
  // represents a de-allocation.
  int64_t mSize;
  // The memory address of the allocation or de-allocation.
  uintptr_t mMemoryAddress;

  int mThreadId;
};

class IPCMarkerPayload : public ProfilerMarkerPayload {
 public:
  IPCMarkerPayload(int32_t aOtherPid, int32_t aMessageSeqno,
                   IPC::Message::msgid_t aMessageType, mozilla::ipc::Side aSide,
                   mozilla::ipc::MessageDirection aDirection, bool aSync,
                   const mozilla::TimeStamp& aStartTime)
      : ProfilerMarkerPayload(aStartTime, aStartTime),
        mOtherPid(aOtherPid),
        mMessageSeqno(aMessageSeqno),
        mMessageType(aMessageType),
        mSide(aSide),
        mDirection(aDirection),
        mSync(aSync) {}

  DECL_STREAM_PAYLOAD

 private:
  IPCMarkerPayload(CommonProps&& aCommonProps, int32_t aOtherPid,
                   int32_t aMessageSeqno, IPC::Message::msgid_t aMessageType,
                   mozilla::ipc::Side aSide,
                   mozilla::ipc::MessageDirection aDirection, bool aSync)
      : ProfilerMarkerPayload(std::move(aCommonProps)),
        mOtherPid(aOtherPid),
        mMessageSeqno(aMessageSeqno),
        mMessageType(aMessageType),
        mSide(aSide),
        mDirection(aDirection),
        mSync(aSync) {}

  int32_t mOtherPid;
  int32_t mMessageSeqno;
  IPC::Message::msgid_t mMessageType;
  mozilla::ipc::Side mSide;
  mozilla::ipc::MessageDirection mDirection;
  bool mSync;
};

namespace mozilla {

// Serialize a pointed-at ProfilerMarkerPayload, may be null when there are no
// payloads.
template <>
struct ProfileBufferEntryWriter::Serializer<const ProfilerMarkerPayload*> {
  static Length Bytes(const ProfilerMarkerPayload* aPayload) {
    return ProfilerMarkerPayload::TagAndSerializationBytes(aPayload);
  }

  static void Write(ProfileBufferEntryWriter& aEW,
                    const ProfilerMarkerPayload* aPayload) {
    ProfilerMarkerPayload::TagAndSerialize(aPayload, aEW);
  }
};

// Serialize a pointed-at ProfilerMarkerPayload, may be null when there are no
// payloads.
template <>
struct ProfileBufferEntryWriter::Serializer<UniquePtr<ProfilerMarkerPayload>> {
  static Length Bytes(const UniquePtr<ProfilerMarkerPayload>& aPayload) {
    return ProfilerMarkerPayload::TagAndSerializationBytes(aPayload.get());
  }

  static void Write(ProfileBufferEntryWriter& aEW,
                    const UniquePtr<ProfilerMarkerPayload>& aPayload) {
    ProfilerMarkerPayload::TagAndSerialize(aPayload.get(), aEW);
  }
};

// Deserialize a ProfilerMarkerPayload into a UniquePtr, may be null if there
// are no payloads.
template <>
struct ProfileBufferEntryReader::Deserializer<
    UniquePtr<ProfilerMarkerPayload>> {
  static void ReadInto(ProfileBufferEntryReader& aER,
                       UniquePtr<ProfilerMarkerPayload>& aPayload) {
    aPayload = Read(aER);
  }

  static UniquePtr<ProfilerMarkerPayload> Read(ProfileBufferEntryReader& aER) {
    return ProfilerMarkerPayload::DeserializeTagAndPayload(aER);
  }
};

}  // namespace mozilla

#endif  // ProfilerMarkerPayload_h
