/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BaseProfilerMarkerPayload.h"

#include <inttypes.h>

#include "mozilla/Maybe.h"
#include "mozilla/Sprintf.h"

#include "BaseProfiler.h"
#include "BaseProfileJSONWriter.h"
#include "ProfileBufferEntry.h"
#include "ProfilerBacktrace.h"

namespace mozilla {
namespace baseprofiler {

static UniquePtr<ProfilerMarkerPayload> DeserializeNothing(
    mozilla::ProfileBufferEntryReader&) {
  return nullptr;
}

// Number of currently-registered deserializers.
// Starting at 1 for the initial `DeserializeNothing`.
// static
Atomic<ProfilerMarkerPayload::DeserializerTagAtomic, ReleaseAcquire>
    ProfilerMarkerPayload::sDeserializerCount{1};

// Initialize `sDeserializers` with `DeserializeNothing` at index 0, all others
// are nullptrs.
// static
ProfilerMarkerPayload::Deserializer
    ProfilerMarkerPayload::sDeserializers[DeserializerMax] = {
        DeserializeNothing};

// static
ProfilerMarkerPayload::DeserializerTag
ProfilerMarkerPayload::TagForDeserializer(
    ProfilerMarkerPayload::Deserializer aDeserializer) {
  if (!aDeserializer) {
    return 0;
  }
  // Start first search at index 0.
  DeserializerTagAtomic start = 0;
  for (;;) {
    // Read the current count of deserializers.
    const DeserializerTagAtomic tagCount = sDeserializerCount;
    if (tagCount == 0) {
      // Someone else is currently writing into the array, loop around until we
      // get a valid count.
      continue;
    }
    for (DeserializerTagAtomic i = start; i < tagCount; ++i) {
      if (sDeserializers[i] == aDeserializer) {
        // Deserializer already registered, return its tag.
        return static_cast<ProfilerMarkerPayload::DeserializerTag>(i);
      }
    }
    // Not found yet, let's register this new deserializer.
    // Make sure we haven't reached the limit yet.
    MOZ_RELEASE_ASSERT(tagCount < DeserializerMax);
    // Reserve `tagCount` as an index, if not already claimed:
    // If `sDeserializerCount` is still at our previously-read `tagCount`,
    // replace it with a special 0 value to indicate a write.
    if (sDeserializerCount.compareExchange(tagCount, 0)) {
      // Here we own the `tagCount` index, write the deserializer there.
      sDeserializers[tagCount] = aDeserializer;
      // And publish by writing the real new count (1 past our index).
      sDeserializerCount = tagCount + 1;
      return static_cast<ProfilerMarkerPayload::DeserializerTag>(tagCount);
    }
    // Someone else beat us to grab an index, and it could be for the same
    // deserializer! So let's just try searching starting from our recorded
    // `tagCount` (and maybe attempting again to register). It should be rare
    // enough and quick enough that it won't impact performances.
    start = tagCount;
  }
}

// static
ProfilerMarkerPayload::Deserializer ProfilerMarkerPayload::DeserializerForTag(
    ProfilerMarkerPayload::DeserializerTag aTag) {
  MOZ_RELEASE_ASSERT(aTag < DeserializerMax);
  MOZ_RELEASE_ASSERT(aTag < sDeserializerCount);
  return sDeserializers[aTag];
}

static void MOZ_ALWAYS_INLINE WriteTime(SpliceableJSONWriter& aWriter,
                                        const TimeStamp& aProcessStartTime,
                                        const TimeStamp& aTime,
                                        const char* aName) {
  if (!aTime.IsNull()) {
    aWriter.DoubleProperty(aName, (aTime - aProcessStartTime).ToMilliseconds());
  }
}

void ProfilerMarkerPayload::StreamType(const char* aMarkerType,
                                       SpliceableJSONWriter& aWriter) const {
  MOZ_ASSERT(aMarkerType);
  aWriter.StringProperty("type", aMarkerType);
}

ProfileBufferEntryWriter::Length
ProfilerMarkerPayload::CommonPropsTagAndSerializationBytes() const {
  return sizeof(DeserializerTag) +
         ProfileBufferEntryWriter::SumBytes(
             mCommonProps.mStartTime, mCommonProps.mEndTime,
             mCommonProps.mStack, mCommonProps.mInnerWindowID);
}

void ProfilerMarkerPayload::SerializeTagAndCommonProps(
    DeserializerTag aDeserializerTag,
    ProfileBufferEntryWriter& aEntryWriter) const {
  aEntryWriter.WriteObject(aDeserializerTag);
  aEntryWriter.WriteObject(mCommonProps.mStartTime);
  aEntryWriter.WriteObject(mCommonProps.mEndTime);
  aEntryWriter.WriteObject(mCommonProps.mStack);
  aEntryWriter.WriteObject(mCommonProps.mInnerWindowID);
}

// static
ProfilerMarkerPayload::CommonProps
ProfilerMarkerPayload::DeserializeCommonProps(
    ProfileBufferEntryReader& aEntryReader) {
  CommonProps props;
  aEntryReader.ReadIntoObject(props.mStartTime);
  aEntryReader.ReadIntoObject(props.mEndTime);
  aEntryReader.ReadIntoObject(props.mStack);
  aEntryReader.ReadIntoObject(props.mInnerWindowID);
  return props;
}

void ProfilerMarkerPayload::StreamCommonProps(
    const char* aMarkerType, SpliceableJSONWriter& aWriter,
    const TimeStamp& aProcessStartTime, UniqueStacks& aUniqueStacks) const {
  StreamType(aMarkerType, aWriter);
  WriteTime(aWriter, aProcessStartTime, mCommonProps.mStartTime, "startTime");
  WriteTime(aWriter, aProcessStartTime, mCommonProps.mEndTime, "endTime");
  if (mCommonProps.mInnerWindowID) {
    // Here, we are converting uint64_t to double. Both Browsing Context and
    // Inner Window IDs are creating using
    // `nsContentUtils::GenerateProcessSpecificId`, which is specifically
    // designed to only use 53 of the 64 bits to be lossless when passed into
    // and out of JS as a double.
    aWriter.DoubleProperty("innerWindowID", mCommonProps.mInnerWindowID.ref());
  }
  if (mCommonProps.mStack) {
    aWriter.StartObjectProperty("stack");
    {
      mCommonProps.mStack->StreamJSON(aWriter, aProcessStartTime,
                                      aUniqueStacks);
    }
    aWriter.EndObject();
  }
}

TracingMarkerPayload::TracingMarkerPayload(
    const char* aCategory, TracingKind aKind,
    const Maybe<uint64_t>& aInnerWindowID, UniqueProfilerBacktrace aCause)
    : ProfilerMarkerPayload(aInnerWindowID, std::move(aCause)),
      mCategory(aCategory),
      mKind(aKind) {}

TracingMarkerPayload::TracingMarkerPayload(CommonProps&& aCommonProps,
                                           const char* aCategory,
                                           TracingKind aKind)
    : ProfilerMarkerPayload(std::move(aCommonProps)),
      mCategory(aCategory),
      mKind(aKind) {}

TracingMarkerPayload::~TracingMarkerPayload() = default;

ProfileBufferEntryWriter::Length
TracingMarkerPayload::TagAndSerializationBytes() const {
  return CommonPropsTagAndSerializationBytes() +
         ProfileBufferEntryWriter::SumBytes(
             WrapProfileBufferRawPointer(mCategory), mKind);
}

void TracingMarkerPayload::SerializeTagAndPayload(
    ProfileBufferEntryWriter& aEntryWriter) const {
  static const DeserializerTag tag = TagForDeserializer(Deserialize);
  SerializeTagAndCommonProps(tag, aEntryWriter);
  aEntryWriter.WriteObject(WrapProfileBufferRawPointer(mCategory));
  aEntryWriter.WriteObject(mKind);
}

// static
UniquePtr<ProfilerMarkerPayload> TracingMarkerPayload::Deserialize(
    ProfileBufferEntryReader& aEntryReader) {
  ProfilerMarkerPayload::CommonProps props =
      DeserializeCommonProps(aEntryReader);
  const char* category = aEntryReader.ReadObject<const char*>();
  TracingKind kind = aEntryReader.ReadObject<TracingKind>();
  return UniquePtr<ProfilerMarkerPayload>(
      new TracingMarkerPayload(std::move(props), category, kind));
}

void TracingMarkerPayload::StreamPayload(SpliceableJSONWriter& aWriter,
                                         const TimeStamp& aProcessStartTime,
                                         UniqueStacks& aUniqueStacks) const {
  StreamCommonProps("tracing", aWriter, aProcessStartTime, aUniqueStacks);

  if (mCategory) {
    aWriter.StringProperty("category", mCategory);
  }

  if (mKind == TRACING_INTERVAL_START) {
    aWriter.StringProperty("interval", "start");
  } else if (mKind == TRACING_INTERVAL_END) {
    aWriter.StringProperty("interval", "end");
  }
}

FileIOMarkerPayload::FileIOMarkerPayload(const char* aOperation,
                                         const char* aSource,
                                         const char* aFilename,
                                         const TimeStamp& aStartTime,
                                         const TimeStamp& aEndTime,
                                         UniqueProfilerBacktrace aStack)
    : ProfilerMarkerPayload(aStartTime, aEndTime, Nothing(), std::move(aStack)),
      mSource(aSource),
      mOperation(aOperation ? strdup(aOperation) : nullptr),
      mFilename(aFilename ? strdup(aFilename) : nullptr) {
  MOZ_ASSERT(aSource);
}

FileIOMarkerPayload::FileIOMarkerPayload(CommonProps&& aCommonProps,
                                         const char* aSource,
                                         UniqueFreePtr<char>&& aOperation,
                                         UniqueFreePtr<char>&& aFilename)
    : ProfilerMarkerPayload(std::move(aCommonProps)),
      mSource(aSource),
      mOperation(std::move(aOperation)),
      mFilename(std::move(aFilename)) {}

FileIOMarkerPayload::~FileIOMarkerPayload() = default;

ProfileBufferEntryWriter::Length FileIOMarkerPayload::TagAndSerializationBytes()
    const {
  return CommonPropsTagAndSerializationBytes() +
         ProfileBufferEntryWriter::SumBytes(
             WrapProfileBufferRawPointer(mSource), mOperation, mFilename);
}

void FileIOMarkerPayload::SerializeTagAndPayload(
    ProfileBufferEntryWriter& aEntryWriter) const {
  static const DeserializerTag tag = TagForDeserializer(Deserialize);
  SerializeTagAndCommonProps(tag, aEntryWriter);
  aEntryWriter.WriteObject(WrapProfileBufferRawPointer(mSource));
  aEntryWriter.WriteObject(mOperation);
  aEntryWriter.WriteObject(mFilename);
}

// static
UniquePtr<ProfilerMarkerPayload> FileIOMarkerPayload::Deserialize(
    ProfileBufferEntryReader& aEntryReader) {
  ProfilerMarkerPayload::CommonProps props =
      DeserializeCommonProps(aEntryReader);
  auto source = aEntryReader.ReadObject<const char*>();
  auto operation = aEntryReader.ReadObject<UniqueFreePtr<char>>();
  auto filename = aEntryReader.ReadObject<UniqueFreePtr<char>>();
  return UniquePtr<ProfilerMarkerPayload>(new FileIOMarkerPayload(
      std::move(props), source, std::move(operation), std::move(filename)));
}

void FileIOMarkerPayload::StreamPayload(SpliceableJSONWriter& aWriter,
                                        const TimeStamp& aProcessStartTime,
                                        UniqueStacks& aUniqueStacks) const {
  StreamCommonProps("FileIO", aWriter, aProcessStartTime, aUniqueStacks);
  aWriter.StringProperty("operation", mOperation.get());
  aWriter.StringProperty("source", mSource);
  if (mFilename && *mFilename) {
    aWriter.StringProperty("filename", mFilename.get());
  }
}

UserTimingMarkerPayload::UserTimingMarkerPayload(
    const std::string& aName, const TimeStamp& aStartTime,
    const Maybe<uint64_t>& aInnerWindowID)
    : ProfilerMarkerPayload(aStartTime, aStartTime, aInnerWindowID),
      mEntryType("mark"),
      mName(aName) {}

UserTimingMarkerPayload::UserTimingMarkerPayload(
    const std::string& aName, const Maybe<std::string>& aStartMark,
    const Maybe<std::string>& aEndMark, const TimeStamp& aStartTime,
    const TimeStamp& aEndTime, const Maybe<uint64_t>& aInnerWindowID)
    : ProfilerMarkerPayload(aStartTime, aEndTime, aInnerWindowID),
      mEntryType("measure"),
      mName(aName),
      mStartMark(aStartMark),
      mEndMark(aEndMark) {}

UserTimingMarkerPayload::UserTimingMarkerPayload(
    CommonProps&& aCommonProps, const char* aEntryType, std::string&& aName,
    Maybe<std::string>&& aStartMark, Maybe<std::string>&& aEndMark)
    : ProfilerMarkerPayload(std::move(aCommonProps)),
      mEntryType(aEntryType),
      mName(std::move(aName)),
      mStartMark(std::move(aStartMark)),
      mEndMark(std::move(aEndMark)) {}

UserTimingMarkerPayload::~UserTimingMarkerPayload() = default;

ProfileBufferEntryWriter::Length
UserTimingMarkerPayload::TagAndSerializationBytes() const {
  return CommonPropsTagAndSerializationBytes() +
         ProfileBufferEntryWriter::SumBytes(
             WrapProfileBufferRawPointer(mEntryType), mName, mStartMark,
             mEndMark);
}

void UserTimingMarkerPayload::SerializeTagAndPayload(
    ProfileBufferEntryWriter& aEntryWriter) const {
  static const DeserializerTag tag = TagForDeserializer(Deserialize);
  SerializeTagAndCommonProps(tag, aEntryWriter);
  aEntryWriter.WriteObject(WrapProfileBufferRawPointer(mEntryType));
  aEntryWriter.WriteObject(mName);
  aEntryWriter.WriteObject(mStartMark);
  aEntryWriter.WriteObject(mEndMark);
}

// static
UniquePtr<ProfilerMarkerPayload> UserTimingMarkerPayload::Deserialize(
    ProfileBufferEntryReader& aEntryReader) {
  ProfilerMarkerPayload::CommonProps props =
      DeserializeCommonProps(aEntryReader);
  auto entryType = aEntryReader.ReadObject<const char*>();
  auto name = aEntryReader.ReadObject<std::string>();
  auto startMark = aEntryReader.ReadObject<Maybe<std::string>>();
  auto endMark = aEntryReader.ReadObject<Maybe<std::string>>();
  return UniquePtr<ProfilerMarkerPayload>(
      new UserTimingMarkerPayload(std::move(props), entryType, std::move(name),
                                  std::move(startMark), std::move(endMark)));
}

void UserTimingMarkerPayload::StreamPayload(SpliceableJSONWriter& aWriter,
                                            const TimeStamp& aProcessStartTime,
                                            UniqueStacks& aUniqueStacks) const {
  StreamCommonProps("UserTiming", aWriter, aProcessStartTime, aUniqueStacks);
  aWriter.StringProperty("name", mName.c_str());
  aWriter.StringProperty("entryType", mEntryType);

  if (mStartMark.isSome()) {
    aWriter.StringProperty("startMark", mStartMark.value().c_str());
  } else {
    aWriter.NullProperty("startMark");
  }
  if (mEndMark.isSome()) {
    aWriter.StringProperty("endMark", mEndMark.value().c_str());
  } else {
    aWriter.NullProperty("endMark");
  }
}

TextMarkerPayload::TextMarkerPayload(const std::string& aText,
                                     const TimeStamp& aStartTime)
    : ProfilerMarkerPayload(aStartTime, aStartTime), mText(aText) {}

TextMarkerPayload::TextMarkerPayload(const std::string& aText,
                                     const TimeStamp& aStartTime,
                                     const TimeStamp& aEndTime)
    : ProfilerMarkerPayload(aStartTime, aEndTime), mText(aText) {}

TextMarkerPayload::TextMarkerPayload(const std::string& aText,
                                     const TimeStamp& aStartTime,
                                     const Maybe<uint64_t>& aInnerWindowID)
    : ProfilerMarkerPayload(aStartTime, aStartTime, aInnerWindowID),
      mText(aText) {}

TextMarkerPayload::TextMarkerPayload(const std::string& aText,
                                     const TimeStamp& aStartTime,
                                     const TimeStamp& aEndTime,
                                     const Maybe<uint64_t>& aInnerWindowID,
                                     UniqueProfilerBacktrace aCause)
    : ProfilerMarkerPayload(aStartTime, aEndTime, aInnerWindowID,
                            std::move(aCause)),
      mText(aText) {}

TextMarkerPayload::TextMarkerPayload(CommonProps&& aCommonProps,
                                     std::string&& aText)
    : ProfilerMarkerPayload(std::move(aCommonProps)), mText(std::move(aText)) {}

TextMarkerPayload::~TextMarkerPayload() = default;

ProfileBufferEntryWriter::Length TextMarkerPayload::TagAndSerializationBytes()
    const {
  return CommonPropsTagAndSerializationBytes() +
         ProfileBufferEntryWriter::SumBytes(mText);
}

void TextMarkerPayload::SerializeTagAndPayload(
    ProfileBufferEntryWriter& aEntryWriter) const {
  static const DeserializerTag tag = TagForDeserializer(Deserialize);
  SerializeTagAndCommonProps(tag, aEntryWriter);
  aEntryWriter.WriteObject(mText);
}

// static
UniquePtr<ProfilerMarkerPayload> TextMarkerPayload::Deserialize(
    ProfileBufferEntryReader& aEntryReader) {
  ProfilerMarkerPayload::CommonProps props =
      DeserializeCommonProps(aEntryReader);
  auto text = aEntryReader.ReadObject<std::string>();
  return UniquePtr<ProfilerMarkerPayload>(
      new TextMarkerPayload(std::move(props), std::move(text)));
}

void TextMarkerPayload::StreamPayload(SpliceableJSONWriter& aWriter,
                                      const TimeStamp& aProcessStartTime,
                                      UniqueStacks& aUniqueStacks) const {
  StreamCommonProps("Text", aWriter, aProcessStartTime, aUniqueStacks);
  aWriter.StringProperty("name", mText.c_str());
}

LogMarkerPayload::LogMarkerPayload(const char* aModule, const char* aText,
                                   const TimeStamp& aStartTime)
    : ProfilerMarkerPayload(aStartTime, aStartTime),
      mModule(aModule),
      mText(aText) {}

LogMarkerPayload::LogMarkerPayload(CommonProps&& aCommonProps,
                                   std::string&& aModule, std::string&& aText)
    : ProfilerMarkerPayload(std::move(aCommonProps)),
      mModule(std::move(aModule)),
      mText(std::move(aText)) {}

LogMarkerPayload::~LogMarkerPayload() = default;

ProfileBufferEntryWriter::Length LogMarkerPayload::TagAndSerializationBytes()
    const {
  return CommonPropsTagAndSerializationBytes() +
         ProfileBufferEntryWriter::SumBytes(mModule, mText);
}

void LogMarkerPayload::SerializeTagAndPayload(
    ProfileBufferEntryWriter& aEntryWriter) const {
  static const DeserializerTag tag = TagForDeserializer(Deserialize);
  SerializeTagAndCommonProps(tag, aEntryWriter);
  aEntryWriter.WriteObject(mModule);
  aEntryWriter.WriteObject(mText);
}

// static
UniquePtr<ProfilerMarkerPayload> LogMarkerPayload::Deserialize(
    ProfileBufferEntryReader& aEntryReader) {
  ProfilerMarkerPayload::CommonProps props =
      DeserializeCommonProps(aEntryReader);
  auto module = aEntryReader.ReadObject<std::string>();
  auto text = aEntryReader.ReadObject<std::string>();
  return UniquePtr<ProfilerMarkerPayload>(new LogMarkerPayload(
      std::move(props), std::move(module), std::move(text)));
}

void LogMarkerPayload::StreamPayload(SpliceableJSONWriter& aWriter,
                                     const TimeStamp& aProcessStartTime,
                                     UniqueStacks& aUniqueStacks) const {
  StreamCommonProps("Log", aWriter, aProcessStartTime, aUniqueStacks);
  aWriter.StringProperty("name", mText.c_str());
  aWriter.StringProperty("module", mModule.c_str());
}

HangMarkerPayload::HangMarkerPayload(const TimeStamp& aStartTime,
                                     const TimeStamp& aEndTime)
    : ProfilerMarkerPayload(aStartTime, aEndTime) {}

HangMarkerPayload::HangMarkerPayload(CommonProps&& aCommonProps)
    : ProfilerMarkerPayload(std::move(aCommonProps)) {}

HangMarkerPayload::~HangMarkerPayload() = default;

ProfileBufferEntryWriter::Length HangMarkerPayload::TagAndSerializationBytes()
    const {
  return CommonPropsTagAndSerializationBytes();
}

void HangMarkerPayload::SerializeTagAndPayload(
    ProfileBufferEntryWriter& aEntryWriter) const {
  static const DeserializerTag tag = TagForDeserializer(Deserialize);
  SerializeTagAndCommonProps(tag, aEntryWriter);
}

// static
UniquePtr<ProfilerMarkerPayload> HangMarkerPayload::Deserialize(
    ProfileBufferEntryReader& aEntryReader) {
  ProfilerMarkerPayload::CommonProps props =
      DeserializeCommonProps(aEntryReader);
  return UniquePtr<ProfilerMarkerPayload>(
      new HangMarkerPayload(std::move(props)));
}

void HangMarkerPayload::StreamPayload(SpliceableJSONWriter& aWriter,
                                      const TimeStamp& aProcessStartTime,
                                      UniqueStacks& aUniqueStacks) const {
  StreamCommonProps("BHR-detected hang", aWriter, aProcessStartTime,
                    aUniqueStacks);
}

LongTaskMarkerPayload::LongTaskMarkerPayload(const TimeStamp& aStartTime,
                                             const TimeStamp& aEndTime)
    : ProfilerMarkerPayload(aStartTime, aEndTime) {}

LongTaskMarkerPayload::LongTaskMarkerPayload(CommonProps&& aCommonProps)
    : ProfilerMarkerPayload(std::move(aCommonProps)) {}

LongTaskMarkerPayload::~LongTaskMarkerPayload() = default;

ProfileBufferEntryWriter::Length
LongTaskMarkerPayload::TagAndSerializationBytes() const {
  return CommonPropsTagAndSerializationBytes();
}

void LongTaskMarkerPayload::SerializeTagAndPayload(
    ProfileBufferEntryWriter& aEntryWriter) const {
  static const DeserializerTag tag = TagForDeserializer(Deserialize);
  SerializeTagAndCommonProps(tag, aEntryWriter);
}

// static
UniquePtr<ProfilerMarkerPayload> LongTaskMarkerPayload::Deserialize(
    ProfileBufferEntryReader& aEntryReader) {
  ProfilerMarkerPayload::CommonProps props =
      DeserializeCommonProps(aEntryReader);
  return UniquePtr<ProfilerMarkerPayload>(
      new LongTaskMarkerPayload(std::move(props)));
}

void LongTaskMarkerPayload::StreamPayload(SpliceableJSONWriter& aWriter,
                                          const TimeStamp& aProcessStartTime,
                                          UniqueStacks& aUniqueStacks) const {
  StreamCommonProps("MainThreadLongTask", aWriter, aProcessStartTime,
                    aUniqueStacks);
  aWriter.StringProperty("category", "LongTask");
}

}  // namespace baseprofiler
}  // namespace mozilla
