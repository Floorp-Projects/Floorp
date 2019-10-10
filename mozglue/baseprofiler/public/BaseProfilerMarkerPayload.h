/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef BaseProfilerMarkerPayload_h
#define BaseProfilerMarkerPayload_h

#include "BaseProfiler.h"

#ifndef MOZ_BASE_PROFILER
#  error Do not #include this header when MOZ_BASE_PROFILER is not #defined.
#endif

#include "mozilla/Atomics.h"
#include "mozilla/Attributes.h"
#include "mozilla/BlocksRingBuffer.h"
#include "mozilla/Maybe.h"
#include "mozilla/RefPtr.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/UniquePtrExtensions.h"

namespace mozilla {
namespace baseprofiler {

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
      const Maybe<uint64_t>& aInnerWindowID = Nothing(),
      UniqueProfilerBacktrace aStack = nullptr)
      : mCommonProps{TimeStamp{}, TimeStamp{}, std::move(aStack),
                     aInnerWindowID} {}

  ProfilerMarkerPayload(const TimeStamp& aStartTime, const TimeStamp& aEndTime,
                        const Maybe<uint64_t>& aInnerWindowID = Nothing(),
                        UniqueProfilerBacktrace aStack = nullptr)
      : mCommonProps{aStartTime, aEndTime, std::move(aStack), aInnerWindowID} {}

  virtual ~ProfilerMarkerPayload() {}

  // Compute the number of bytes needed to serialize the `DeserializerTag` and
  // payload, including in the no-payload (nullptr) case.
  static BlocksRingBuffer::Length TagAndSerializationBytes(
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
                              BlocksRingBuffer::EntryWriter& aEntryWriter) {
    if (!aPayload) {
      aEntryWriter.WriteObject(DeserializerTag(0));
      return;
    }
    aPayload->SerializeTagAndPayload(aEntryWriter);
  }

  // Deserialize a payload from an EntryReader, including in the no-payload
  // (nullptr) case.
  static UniquePtr<ProfilerMarkerPayload> DeserializeTagAndPayload(
      mozilla::BlocksRingBuffer::EntryReader& aER) {
    const auto tag = aER.ReadObject<DeserializerTag>();
    Deserializer deserializer = DeserializerForTag(tag);
    return deserializer(aER);
  }

  virtual void StreamPayload(SpliceableJSONWriter& aWriter,
                             const TimeStamp& aProcessStartTime,
                             UniqueStacks& aUniqueStacks) const = 0;

  TimeStamp GetStartTime() const { return mCommonProps.mStartTime; }

 protected:
  // A `Deserializer` is a free function that can read a serialized payload from
  // an `EntryReader` and return a reconstructed `ProfilerMarkerPayload`
  // sub-object (may be null if there was no payload).
  typedef UniquePtr<ProfilerMarkerPayload> (*Deserializer)(
      BlocksRingBuffer::EntryReader&);

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
  static Atomic<DeserializerTagAtomic, ReleaseAcquire,
                recordreplay::Behavior::DontPreserve>
      sDeserializerCount;

  // List of currently-registered deserializers.
  // sDeserializers[0] is a no-payload deserializer.
  static Deserializer sDeserializers[DeserializerMax];

  // Get the `DeserializerTag` for a `Deserializer` (which gets registered on
  // the first call.) Tag 0 means no payload; a null `aDeserializer` gives that
  // 0 tag.
  MFBT_API static DeserializerTag TagForDeserializer(
      Deserializer aDeserializer);

  // Get the `Deserializer` for a given `DeserializerTag`.
  // Tag 0 is reserved as no-payload deserializer (which returns nullptr).
  MFBT_API static Deserializer DeserializerForTag(DeserializerTag aTag);

  struct CommonProps {
    TimeStamp mStartTime;
    TimeStamp mEndTime;
    UniqueProfilerBacktrace mStack;
    Maybe<uint64_t> mInnerWindowID;
  };

  // Deserializers can use this base constructor.
  explicit ProfilerMarkerPayload(CommonProps&& aCommonProps)
      : mCommonProps(std::move(aCommonProps)) {}

  // Serialization/deserialization of common props in ProfilerMarkerPayload.
  MFBT_API BlocksRingBuffer::Length CommonPropsTagAndSerializationBytes() const;
  MFBT_API void SerializeTagAndCommonProps(
      DeserializerTag aDeserializerTag,
      BlocksRingBuffer::EntryWriter& aEntryWriter) const;
  MFBT_API static CommonProps DeserializeCommonProps(
      BlocksRingBuffer::EntryReader& aEntryReader);

  MFBT_API void StreamType(const char* aMarkerType,
                           SpliceableJSONWriter& aWriter) const;

  MFBT_API void StreamCommonProps(const char* aMarkerType,
                                  SpliceableJSONWriter& aWriter,
                                  const TimeStamp& aProcessStartTime,
                                  UniqueStacks& aUniqueStacks) const;

 private:
  // Compute the number of bytes needed to serialize the `DeserializerTag` and
  // payload in `SerializeTagAndPayload` below.
  virtual BlocksRingBuffer::Length TagAndSerializationBytes() const = 0;

  // Serialize the `DeserializerTag` and payload into an EntryWriter.
  // Must be of the exact size given by `TagAndSerializationBytes()`.
  virtual void SerializeTagAndPayload(
      BlocksRingBuffer::EntryWriter& aEntryWriter) const = 0;

  CommonProps mCommonProps;
};

#define DECL_BASE_STREAM_PAYLOAD                                               \
  MFBT_API void StreamPayload(                                                 \
      ::mozilla::baseprofiler::SpliceableJSONWriter& aWriter,                  \
      const ::mozilla::TimeStamp& aProcessStartTime,                           \
      ::mozilla::baseprofiler::UniqueStacks& aUniqueStacks) const override;    \
  static UniquePtr<ProfilerMarkerPayload> Deserialize(                         \
      BlocksRingBuffer::EntryReader& aEntryReader);                            \
  MFBT_API BlocksRingBuffer::Length TagAndSerializationBytes() const override; \
  MFBT_API void SerializeTagAndPayload(                                        \
      BlocksRingBuffer::EntryWriter& aEntryWriter) const override;

// TODO: Increase the coverage of tracing markers that include InnerWindowID
// information
class TracingMarkerPayload : public ProfilerMarkerPayload {
 public:
  MFBT_API TracingMarkerPayload(
      const char* aCategory, TracingKind aKind,
      const Maybe<uint64_t>& aInnerWindowID = Nothing(),
      UniqueProfilerBacktrace aCause = nullptr);

  MFBT_API ~TracingMarkerPayload() override;

  DECL_BASE_STREAM_PAYLOAD

 private:
  MFBT_API TracingMarkerPayload(CommonProps&& aCommonProps,
                                const char* aCategory, TracingKind aKind);

  const char* mCategory;
  TracingKind mKind;
};

class FileIOMarkerPayload : public ProfilerMarkerPayload {
 public:
  MFBT_API FileIOMarkerPayload(const char* aOperation, const char* aSource,
                               const char* aFilename,
                               const TimeStamp& aStartTime,
                               const TimeStamp& aEndTime,
                               UniqueProfilerBacktrace aStack);

  MFBT_API ~FileIOMarkerPayload() override;

  DECL_BASE_STREAM_PAYLOAD

 private:
  MFBT_API FileIOMarkerPayload(CommonProps&& aCommonProps, const char* aSource,
                               UniqueFreePtr<char>&& aOperation,
                               UniqueFreePtr<char>&& aFilename);

  const char* mSource;
  UniqueFreePtr<char> mOperation;
  UniqueFreePtr<char> mFilename;
};

class UserTimingMarkerPayload : public ProfilerMarkerPayload {
 public:
  MFBT_API UserTimingMarkerPayload(const std::string& aName,
                                   const TimeStamp& aStartTime,
                                   const Maybe<uint64_t>& aInnerWindowID);

  MFBT_API UserTimingMarkerPayload(const std::string& aName,
                                   const Maybe<std::string>& aStartMark,
                                   const Maybe<std::string>& aEndMark,
                                   const TimeStamp& aStartTime,
                                   const TimeStamp& aEndTime,
                                   const Maybe<uint64_t>& aInnerWindowID);

  MFBT_API ~UserTimingMarkerPayload() override;

  DECL_BASE_STREAM_PAYLOAD

 private:
  MFBT_API UserTimingMarkerPayload(CommonProps&& aCommonProps,
                                   const char* aEntryType, std::string&& aName,
                                   Maybe<std::string>&& aStartMark,
                                   Maybe<std::string>&& aEndMark);

  // Either "mark" or "measure".
  const char* mEntryType;
  std::string mName;
  Maybe<std::string> mStartMark;
  Maybe<std::string> mEndMark;
};

class HangMarkerPayload : public ProfilerMarkerPayload {
 public:
  MFBT_API HangMarkerPayload(const TimeStamp& aStartTime,
                             const TimeStamp& aEndTime);

  MFBT_API ~HangMarkerPayload() override;

  DECL_BASE_STREAM_PAYLOAD

 private:
  MFBT_API explicit HangMarkerPayload(CommonProps&& aCommonProps);
};

class LongTaskMarkerPayload : public ProfilerMarkerPayload {
 public:
  MFBT_API LongTaskMarkerPayload(const TimeStamp& aStartTime,
                                 const TimeStamp& aEndTime);

  MFBT_API ~LongTaskMarkerPayload() override;

  DECL_BASE_STREAM_PAYLOAD

 private:
  MFBT_API explicit LongTaskMarkerPayload(CommonProps&& aCommonProps);
};

class TextMarkerPayload : public ProfilerMarkerPayload {
 public:
  MFBT_API TextMarkerPayload(const std::string& aText,
                             const TimeStamp& aStartTime);

  MFBT_API TextMarkerPayload(const std::string& aText,
                             const TimeStamp& aStartTime,
                             const TimeStamp& aEndTime);

  MFBT_API TextMarkerPayload(const std::string& aText,
                             const TimeStamp& aStartTime,
                             const Maybe<uint64_t>& aInnerWindowID);

  MFBT_API TextMarkerPayload(const std::string& aText,
                             const TimeStamp& aStartTime,
                             const TimeStamp& aEndTime,
                             const Maybe<uint64_t>& aInnerWindowID,
                             UniqueProfilerBacktrace aCause = nullptr);

  MFBT_API ~TextMarkerPayload() override;

  DECL_BASE_STREAM_PAYLOAD

 private:
  MFBT_API TextMarkerPayload(CommonProps&& aCommonProps, std::string&& aText);

  std::string mText;
};

class LogMarkerPayload : public ProfilerMarkerPayload {
 public:
  MFBT_API LogMarkerPayload(const char* aModule, const char* aText,
                            const TimeStamp& aStartTime);

  MFBT_API ~LogMarkerPayload() override;

  DECL_BASE_STREAM_PAYLOAD

 private:
  MFBT_API LogMarkerPayload(CommonProps&& aCommonProps, std::string&& aModule,
                            std::string&& aText);

  std::string mModule;  // longest known LazyLogModule name is ~24
  std::string mText;
};

}  // namespace baseprofiler

// Serialize a pointed-at ProfilerMarkerPayload, may be null when there are no
// payloads.
template <>
struct BlocksRingBuffer::Serializer<
    const baseprofiler::ProfilerMarkerPayload*> {
  static Length Bytes(const baseprofiler::ProfilerMarkerPayload* aPayload) {
    return baseprofiler::ProfilerMarkerPayload::TagAndSerializationBytes(
        aPayload);
  }

  static void Write(EntryWriter& aEW,
                    const baseprofiler::ProfilerMarkerPayload* aPayload) {
    baseprofiler::ProfilerMarkerPayload::TagAndSerialize(aPayload, aEW);
  }
};

// Serialize a pointed-at ProfilerMarkerPayload, may be null for no payloads.
template <>
struct BlocksRingBuffer::Serializer<
    UniquePtr<baseprofiler::ProfilerMarkerPayload>> {
  static Length Bytes(
      const UniquePtr<baseprofiler::ProfilerMarkerPayload>& aPayload) {
    return baseprofiler::ProfilerMarkerPayload::TagAndSerializationBytes(
        aPayload.get());
  }

  static void Write(
      EntryWriter& aEW,
      const UniquePtr<baseprofiler::ProfilerMarkerPayload>& aPayload) {
    baseprofiler::ProfilerMarkerPayload::TagAndSerialize(aPayload.get(), aEW);
  }
};

// Deserialize a ProfilerMarkerPayload into a UniquePtr, may be null if there
// are no payloads.
template <>
struct BlocksRingBuffer::Deserializer<
    UniquePtr<baseprofiler::ProfilerMarkerPayload>> {
  static void ReadInto(
      EntryReader& aER,
      UniquePtr<baseprofiler::ProfilerMarkerPayload>& aPayload) {
    aPayload = Read(aER);
  }

  static UniquePtr<baseprofiler::ProfilerMarkerPayload> Read(EntryReader& aER) {
    return baseprofiler::ProfilerMarkerPayload::DeserializeTagAndPayload(aER);
  }
};

}  // namespace mozilla

#endif  // BaseProfilerMarkerPayload_h
