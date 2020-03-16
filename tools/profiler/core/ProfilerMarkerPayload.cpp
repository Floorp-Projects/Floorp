/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ProfilerMarkerPayload.h"

#include "GeckoProfiler.h"
#include "ProfileBufferEntry.h"
#include "ProfileJSONWriter.h"
#include "ProfilerBacktrace.h"

#include "gfxASurface.h"
#include "Layers.h"
#include "mozilla/Maybe.h"
#include "mozilla/net/HttpBaseChannel.h"
#include "mozilla/Preferences.h"
#include "mozilla/ProfileBufferEntrySerializationGeckoExtensions.h"
#include "mozilla/Sprintf.h"

#include <inttypes.h>

using namespace mozilla;

static UniquePtr<ProfilerMarkerPayload> DeserializeNothing(
    ProfileBufferEntryReader&) {
  return nullptr;
}

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

ProfileBufferEntryWriter::Length
TracingMarkerPayload::TagAndSerializationBytes() const {
  return CommonPropsTagAndSerializationBytes() +
         ProfileBufferEntryWriter::SumBytes(
             WrapProfileBufferRawPointer(mCategory), mKind);
}

void TracingMarkerPayload::SerializeTagAndPayload(
    ProfileBufferEntryWriter& aEntryWriter) const {
  static const DeserializerTag tag = TagForDeserializer(Deserialize);
  SerializeTagAndPayload(tag, aEntryWriter);
}

void TracingMarkerPayload::SerializeTagAndPayload(
    DeserializerTag aDeserializerTag,
    ProfileBufferEntryWriter& aEntryWriter) const {
  SerializeTagAndCommonProps(aDeserializerTag, aEntryWriter);
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
  if (mFilename) {
    aWriter.StringProperty("filename", mFilename.get());
  }
}

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
  auto name = aEntryReader.ReadObject<nsString>();
  auto startMark = aEntryReader.ReadObject<Maybe<nsString>>();
  auto endMark = aEntryReader.ReadObject<Maybe<nsString>>();
  return UniquePtr<ProfilerMarkerPayload>(
      new UserTimingMarkerPayload(std::move(props), entryType, std::move(name),
                                  std::move(startMark), std::move(endMark)));
}

void UserTimingMarkerPayload::StreamPayload(SpliceableJSONWriter& aWriter,
                                            const TimeStamp& aProcessStartTime,
                                            UniqueStacks& aUniqueStacks) const {
  StreamCommonProps("UserTiming", aWriter, aProcessStartTime, aUniqueStacks);
  aWriter.StringProperty("name", NS_ConvertUTF16toUTF8(mName).get());
  aWriter.StringProperty("entryType", mEntryType);

  if (mStartMark.isSome()) {
    aWriter.StringProperty("startMark",
                           NS_ConvertUTF16toUTF8(mStartMark.value()).get());
  } else {
    aWriter.NullProperty("startMark");
  }
  if (mEndMark.isSome()) {
    aWriter.StringProperty("endMark",
                           NS_ConvertUTF16toUTF8(mEndMark.value()).get());
  } else {
    aWriter.NullProperty("endMark");
  }
}

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
  auto text = aEntryReader.ReadObject<nsCString>();
  return UniquePtr<ProfilerMarkerPayload>(
      new TextMarkerPayload(std::move(props), std::move(text)));
}

void TextMarkerPayload::StreamPayload(SpliceableJSONWriter& aWriter,
                                      const TimeStamp& aProcessStartTime,
                                      UniqueStacks& aUniqueStacks) const {
  StreamCommonProps("Text", aWriter, aProcessStartTime, aUniqueStacks);
  aWriter.StringProperty("name", mText.get());
}

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
  auto module = aEntryReader.ReadObject<nsAutoCStringN<32>>();
  auto text = aEntryReader.ReadObject<nsCString>();
  return UniquePtr<ProfilerMarkerPayload>(new LogMarkerPayload(
      std::move(props), std::move(module), std::move(text)));
}

void LogMarkerPayload::StreamPayload(SpliceableJSONWriter& aWriter,
                                     const TimeStamp& aProcessStartTime,
                                     UniqueStacks& aUniqueStacks) const {
  StreamCommonProps("Log", aWriter, aProcessStartTime, aUniqueStacks);
  aWriter.StringProperty("name", mText.get());
  aWriter.StringProperty("module", mModule.get());
}

ProfileBufferEntryWriter::Length
DOMEventMarkerPayload::TagAndSerializationBytes() const {
  return TracingMarkerPayload::TagAndSerializationBytes() +
         ProfileBufferEntryWriter::SumBytes(mTimeStamp, mEventType);
}

void DOMEventMarkerPayload::SerializeTagAndPayload(
    ProfileBufferEntryWriter& aEntryWriter) const {
  static const DeserializerTag tag = TagForDeserializer(Deserialize);
  // Let our parent class serialize our tag with its payload.
  TracingMarkerPayload::SerializeTagAndPayload(tag, aEntryWriter);
  // Then write our extra data.
  aEntryWriter.WriteObject(mTimeStamp);
  aEntryWriter.WriteObject(mEventType);
}

// static
UniquePtr<ProfilerMarkerPayload> DOMEventMarkerPayload::Deserialize(
    ProfileBufferEntryReader& aEntryReader) {
  ProfilerMarkerPayload::CommonProps props =
      DeserializeCommonProps(aEntryReader);
  const char* category = aEntryReader.ReadObject<const char*>();
  TracingKind kind = aEntryReader.ReadObject<TracingKind>();
  auto timeStamp = aEntryReader.ReadObject<TimeStamp>();
  auto eventType = aEntryReader.ReadObject<nsString>();
  return UniquePtr<ProfilerMarkerPayload>(new DOMEventMarkerPayload(
      std::move(props), category, kind, timeStamp, std::move(eventType)));
}

void DOMEventMarkerPayload::StreamPayload(SpliceableJSONWriter& aWriter,
                                          const TimeStamp& aProcessStartTime,
                                          UniqueStacks& aUniqueStacks) const {
  TracingMarkerPayload::StreamPayload(aWriter, aProcessStartTime,
                                      aUniqueStacks);

  WriteTime(aWriter, aProcessStartTime, mTimeStamp, "timeStamp");
  aWriter.StringProperty("eventType", NS_ConvertUTF16toUTF8(mEventType).get());
}

ProfileBufferEntryWriter::Length PrefMarkerPayload::TagAndSerializationBytes()
    const {
  return CommonPropsTagAndSerializationBytes() +
         ProfileBufferEntryWriter::SumBytes(mPrefAccessTime, mPrefName,
                                            mPrefKind, mPrefType, mPrefValue);
}

void PrefMarkerPayload::SerializeTagAndPayload(
    ProfileBufferEntryWriter& aEntryWriter) const {
  static const DeserializerTag tag = TagForDeserializer(Deserialize);
  SerializeTagAndCommonProps(tag, aEntryWriter);
  aEntryWriter.WriteObject(mPrefAccessTime);
  aEntryWriter.WriteObject(mPrefName);
  aEntryWriter.WriteObject(mPrefKind);
  aEntryWriter.WriteObject(mPrefType);
  aEntryWriter.WriteObject(mPrefValue);
}

// static
UniquePtr<ProfilerMarkerPayload> PrefMarkerPayload::Deserialize(
    ProfileBufferEntryReader& aEntryReader) {
  ProfilerMarkerPayload::CommonProps props =
      DeserializeCommonProps(aEntryReader);
  auto prefAccessTime = aEntryReader.ReadObject<TimeStamp>();
  auto prefName = aEntryReader.ReadObject<nsCString>();
  auto prefKind = aEntryReader.ReadObject<Maybe<PrefValueKind>>();
  auto prefType = aEntryReader.ReadObject<Maybe<PrefType>>();
  auto prefValue = aEntryReader.ReadObject<nsCString>();
  return UniquePtr<ProfilerMarkerPayload>(new PrefMarkerPayload(
      std::move(props), prefAccessTime, std::move(prefName),
      std::move(prefKind), std::move(prefType), std::move(prefValue)));
}

static const char* PrefValueKindToString(const Maybe<PrefValueKind>& aKind) {
  if (aKind) {
    return *aKind == PrefValueKind::Default ? "Default" : "User";
  }
  return "Shared";
}

static const char* PrefTypeToString(const Maybe<PrefType>& type) {
  if (type) {
    switch (*type) {
      case PrefType::None:
        return "None";
      case PrefType::Int:
        return "Int";
      case PrefType::Bool:
        return "Bool";
      case PrefType::String:
        return "String";
      default:
        MOZ_ASSERT_UNREACHABLE("Unknown preference type.");
    }
  }
  return "Preference not found";
}

void PrefMarkerPayload::StreamPayload(SpliceableJSONWriter& aWriter,
                                      const TimeStamp& aProcessStartTime,
                                      UniqueStacks& aUniqueStacks) const {
  StreamCommonProps("PreferenceRead", aWriter, aProcessStartTime,
                    aUniqueStacks);
  WriteTime(aWriter, aProcessStartTime, mPrefAccessTime, "prefAccessTime");
  aWriter.StringProperty("prefName", mPrefName.get());
  aWriter.StringProperty("prefKind", PrefValueKindToString(mPrefKind));
  aWriter.StringProperty("prefType", PrefTypeToString(mPrefType));
  aWriter.StringProperty("prefValue", mPrefValue.get());
}

ProfileBufferEntryWriter::Length
LayerTranslationMarkerPayload::TagAndSerializationBytes() const {
  return CommonPropsTagAndSerializationBytes() +
         ProfileBufferEntryWriter::SumBytes(WrapProfileBufferRawPointer(mLayer),
                                            mPoint);
}

void LayerTranslationMarkerPayload::SerializeTagAndPayload(
    ProfileBufferEntryWriter& aEntryWriter) const {
  static const DeserializerTag tag = TagForDeserializer(Deserialize);
  SerializeTagAndCommonProps(tag, aEntryWriter);
  aEntryWriter.WriteObject(WrapProfileBufferRawPointer(mLayer));
  aEntryWriter.WriteObject(mPoint);
}

// static
UniquePtr<ProfilerMarkerPayload> LayerTranslationMarkerPayload::Deserialize(
    ProfileBufferEntryReader& aEntryReader) {
  ProfilerMarkerPayload::CommonProps props =
      DeserializeCommonProps(aEntryReader);
  auto layer = aEntryReader.ReadObject<layers::Layer*>();
  auto point = aEntryReader.ReadObject<gfx::Point>();
  return UniquePtr<ProfilerMarkerPayload>(
      new LayerTranslationMarkerPayload(std::move(props), layer, point));
}

void LayerTranslationMarkerPayload::StreamPayload(
    SpliceableJSONWriter& aWriter, const TimeStamp& aProcessStartTime,
    UniqueStacks& aUniqueStacks) const {
  StreamType("LayerTranslation", aWriter);
  const size_t bufferSize = 32;
  char buffer[bufferSize];
  SprintfLiteral(buffer, "%p", mLayer);

  aWriter.StringProperty("layer", buffer);
  aWriter.IntProperty("x", mPoint.x);
  aWriter.IntProperty("y", mPoint.y);
}

ProfileBufferEntryWriter::Length VsyncMarkerPayload::TagAndSerializationBytes()
    const {
  return CommonPropsTagAndSerializationBytes();
}

void VsyncMarkerPayload::SerializeTagAndPayload(
    ProfileBufferEntryWriter& aEntryWriter) const {
  static const DeserializerTag tag = TagForDeserializer(Deserialize);
  SerializeTagAndCommonProps(tag, aEntryWriter);
}

// static
UniquePtr<ProfilerMarkerPayload> VsyncMarkerPayload::Deserialize(
    ProfileBufferEntryReader& aEntryReader) {
  ProfilerMarkerPayload::CommonProps props =
      DeserializeCommonProps(aEntryReader);
  return UniquePtr<ProfilerMarkerPayload>(
      new VsyncMarkerPayload(std::move(props)));
}

void VsyncMarkerPayload::StreamPayload(SpliceableJSONWriter& aWriter,
                                       const TimeStamp& aProcessStartTime,
                                       UniqueStacks& aUniqueStacks) const {
  StreamType("VsyncTimestamp", aWriter);
}

ProfileBufferEntryWriter::Length
NetworkMarkerPayload::TagAndSerializationBytes() const {
  return CommonPropsTagAndSerializationBytes() +
         ProfileBufferEntryWriter::SumBytes(mID, mURI, mRedirectURI, mType,
                                            mPri, mCount, mTimings,
                                            mCacheDisposition);
}

void NetworkMarkerPayload::SerializeTagAndPayload(
    ProfileBufferEntryWriter& aEntryWriter) const {
  static const DeserializerTag tag = TagForDeserializer(Deserialize);
  SerializeTagAndCommonProps(tag, aEntryWriter);
  aEntryWriter.WriteObject(mID);
  aEntryWriter.WriteObject(mURI);
  aEntryWriter.WriteObject(mRedirectURI);
  aEntryWriter.WriteObject(mType);
  aEntryWriter.WriteObject(mPri);
  aEntryWriter.WriteObject(mCount);
  aEntryWriter.WriteObject(mTimings);
  aEntryWriter.WriteObject(mCacheDisposition);
}

// static
UniquePtr<ProfilerMarkerPayload> NetworkMarkerPayload::Deserialize(
    ProfileBufferEntryReader& aEntryReader) {
  ProfilerMarkerPayload::CommonProps props =
      DeserializeCommonProps(aEntryReader);
  auto id = aEntryReader.ReadObject<int64_t>();
  auto uri = aEntryReader.ReadObject<UniqueFreePtr<char>>();
  auto redirectURI = aEntryReader.ReadObject<UniqueFreePtr<char>>();
  auto type = aEntryReader.ReadObject<NetworkLoadType>();
  auto pri = aEntryReader.ReadObject<int32_t>();
  auto count = aEntryReader.ReadObject<int64_t>();
  auto timings = aEntryReader.ReadObject<net::TimingStruct>();
  auto cacheDisposition = aEntryReader.ReadObject<net::CacheDisposition>();
  return UniquePtr<ProfilerMarkerPayload>(new NetworkMarkerPayload(
      std::move(props), id, std::move(uri), std::move(redirectURI), type, pri,
      count, timings, cacheDisposition));
}

static const char* GetNetworkState(NetworkLoadType aType) {
  switch (aType) {
    case NetworkLoadType::LOAD_START:
      return "STATUS_START";
    case NetworkLoadType::LOAD_STOP:
      return "STATUS_STOP";
    case NetworkLoadType::LOAD_REDIRECT:
      return "STATUS_REDIRECT";
  }
  return "";
}

static const char* GetCacheState(net::CacheDisposition aCacheDisposition) {
  switch (aCacheDisposition) {
    case net::kCacheUnresolved:
      return "Unresolved";
    case net::kCacheHit:
      return "Hit";
    case net::kCacheHitViaReval:
      return "HitViaReval";
    case net::kCacheMissedViaReval:
      return "MissedViaReval";
    case net::kCacheMissed:
      return "Missed";
    case net::kCacheUnknown:
    default:
      return nullptr;
  }
}

void NetworkMarkerPayload::StreamPayload(SpliceableJSONWriter& aWriter,
                                         const TimeStamp& aProcessStartTime,
                                         UniqueStacks& aUniqueStacks) const {
  StreamCommonProps("Network", aWriter, aProcessStartTime, aUniqueStacks);
  aWriter.IntProperty("id", mID);
  const char* typeString = GetNetworkState(mType);
  const char* cacheString = GetCacheState(mCacheDisposition);
  // want to use aUniqueStacks.mUniqueStrings->WriteElement(aWriter,
  // typeString);
  aWriter.StringProperty("status", typeString);
  if (cacheString) {
    aWriter.StringProperty("cache", cacheString);
  }
  aWriter.IntProperty("pri", mPri);
  if (mCount > 0) {
    aWriter.IntProperty("count", mCount);
  }
  if (mURI) {
    aWriter.StringProperty("URI", mURI.get());
  }
  if (mRedirectURI) {
    aWriter.StringProperty("RedirectURI", mRedirectURI.get());
  }
  if (mType != NetworkLoadType::LOAD_START) {
    WriteTime(aWriter, aProcessStartTime, mTimings.domainLookupStart,
              "domainLookupStart");
    WriteTime(aWriter, aProcessStartTime, mTimings.domainLookupEnd,
              "domainLookupEnd");
    WriteTime(aWriter, aProcessStartTime, mTimings.connectStart,
              "connectStart");
    WriteTime(aWriter, aProcessStartTime, mTimings.tcpConnectEnd,
              "tcpConnectEnd");
    WriteTime(aWriter, aProcessStartTime, mTimings.secureConnectionStart,
              "secureConnectionStart");
    WriteTime(aWriter, aProcessStartTime, mTimings.connectEnd, "connectEnd");
    WriteTime(aWriter, aProcessStartTime, mTimings.requestStart,
              "requestStart");
    WriteTime(aWriter, aProcessStartTime, mTimings.responseStart,
              "responseStart");
    WriteTime(aWriter, aProcessStartTime, mTimings.responseEnd, "responseEnd");
  }
}

ProfileBufferEntryWriter::Length ScreenshotPayload::TagAndSerializationBytes()
    const {
  return CommonPropsTagAndSerializationBytes() +
         ProfileBufferEntryWriter::SumBytes(mScreenshotDataURL, mWindowSize,
                                            mWindowIdentifier);
}

void ScreenshotPayload::SerializeTagAndPayload(
    ProfileBufferEntryWriter& aEntryWriter) const {
  static const DeserializerTag tag = TagForDeserializer(Deserialize);
  SerializeTagAndCommonProps(tag, aEntryWriter);
  aEntryWriter.WriteObject(mScreenshotDataURL);
  aEntryWriter.WriteObject(mWindowSize);
  aEntryWriter.WriteObject(mWindowIdentifier);
}

// static
UniquePtr<ProfilerMarkerPayload> ScreenshotPayload::Deserialize(
    ProfileBufferEntryReader& aEntryReader) {
  ProfilerMarkerPayload::CommonProps props =
      DeserializeCommonProps(aEntryReader);
  auto screenshotDataURL = aEntryReader.ReadObject<nsCString>();
  auto windowSize = aEntryReader.ReadObject<gfx::IntSize>();
  auto windowIdentifier = aEntryReader.ReadObject<uintptr_t>();
  return UniquePtr<ProfilerMarkerPayload>(
      new ScreenshotPayload(std::move(props), std::move(screenshotDataURL),
                            windowSize, windowIdentifier));
}

void ScreenshotPayload::StreamPayload(SpliceableJSONWriter& aWriter,
                                      const TimeStamp& aProcessStartTime,
                                      UniqueStacks& aUniqueStacks) const {
  StreamType("CompositorScreenshot", aWriter);
  aUniqueStacks.mUniqueStrings->WriteProperty(aWriter, "url",
                                              mScreenshotDataURL.get());

  char hexWindowID[32];
  SprintfLiteral(hexWindowID, "0x%" PRIXPTR, mWindowIdentifier);
  aWriter.StringProperty("windowID", hexWindowID);
  aWriter.DoubleProperty("windowWidth", mWindowSize.width);
  aWriter.DoubleProperty("windowHeight", mWindowSize.height);
}

ProfileBufferEntryWriter::Length
GCSliceMarkerPayload::TagAndSerializationBytes() const {
  return CommonPropsTagAndSerializationBytes() +
         ProfileBufferEntryWriter::SumBytes(mTimingJSON);
}

void GCSliceMarkerPayload::SerializeTagAndPayload(
    ProfileBufferEntryWriter& aEntryWriter) const {
  static const DeserializerTag tag = TagForDeserializer(Deserialize);
  SerializeTagAndCommonProps(tag, aEntryWriter);
  aEntryWriter.WriteObject(mTimingJSON);
}

// static
UniquePtr<ProfilerMarkerPayload> GCSliceMarkerPayload::Deserialize(
    ProfileBufferEntryReader& aEntryReader) {
  ProfilerMarkerPayload::CommonProps props =
      DeserializeCommonProps(aEntryReader);
  auto timingJSON = aEntryReader.ReadObject<JS::UniqueChars>();
  return UniquePtr<ProfilerMarkerPayload>(
      new GCSliceMarkerPayload(std::move(props), std::move(timingJSON)));
}

void GCSliceMarkerPayload::StreamPayload(SpliceableJSONWriter& aWriter,
                                         const TimeStamp& aProcessStartTime,
                                         UniqueStacks& aUniqueStacks) const {
  MOZ_ASSERT(mTimingJSON);
  StreamCommonProps("GCSlice", aWriter, aProcessStartTime, aUniqueStacks);
  if (mTimingJSON) {
    aWriter.SplicedJSONProperty("timings", mTimingJSON.get());
  } else {
    aWriter.NullProperty("timings");
  }
}

ProfileBufferEntryWriter::Length
GCMajorMarkerPayload::TagAndSerializationBytes() const {
  return CommonPropsTagAndSerializationBytes() +
         ProfileBufferEntryWriter::SumBytes(mTimingJSON);
}

void GCMajorMarkerPayload::SerializeTagAndPayload(
    ProfileBufferEntryWriter& aEntryWriter) const {
  static const DeserializerTag tag = TagForDeserializer(Deserialize);
  SerializeTagAndCommonProps(tag, aEntryWriter);
  aEntryWriter.WriteObject(mTimingJSON);
}

// static
UniquePtr<ProfilerMarkerPayload> GCMajorMarkerPayload::Deserialize(
    ProfileBufferEntryReader& aEntryReader) {
  ProfilerMarkerPayload::CommonProps props =
      DeserializeCommonProps(aEntryReader);
  auto timingJSON = aEntryReader.ReadObject<JS::UniqueChars>();
  return UniquePtr<ProfilerMarkerPayload>(
      new GCMajorMarkerPayload(std::move(props), std::move(timingJSON)));
}

void GCMajorMarkerPayload::StreamPayload(SpliceableJSONWriter& aWriter,
                                         const TimeStamp& aProcessStartTime,
                                         UniqueStacks& aUniqueStacks) const {
  MOZ_ASSERT(mTimingJSON);
  StreamCommonProps("GCMajor", aWriter, aProcessStartTime, aUniqueStacks);
  if (mTimingJSON) {
    aWriter.SplicedJSONProperty("timings", mTimingJSON.get());
  } else {
    aWriter.NullProperty("timings");
  }
}

ProfileBufferEntryWriter::Length
GCMinorMarkerPayload::TagAndSerializationBytes() const {
  return CommonPropsTagAndSerializationBytes() +
         ProfileBufferEntryWriter::SumBytes(mTimingData);
}

void GCMinorMarkerPayload::SerializeTagAndPayload(
    ProfileBufferEntryWriter& aEntryWriter) const {
  static const DeserializerTag tag = TagForDeserializer(Deserialize);
  SerializeTagAndCommonProps(tag, aEntryWriter);
  aEntryWriter.WriteObject(mTimingData);
}

// static
UniquePtr<ProfilerMarkerPayload> GCMinorMarkerPayload::Deserialize(
    ProfileBufferEntryReader& aEntryReader) {
  ProfilerMarkerPayload::CommonProps props =
      DeserializeCommonProps(aEntryReader);
  auto timingData = aEntryReader.ReadObject<JS::UniqueChars>();
  return UniquePtr<ProfilerMarkerPayload>(
      new GCMinorMarkerPayload(std::move(props), std::move(timingData)));
}

void GCMinorMarkerPayload::StreamPayload(SpliceableJSONWriter& aWriter,
                                         const TimeStamp& aProcessStartTime,
                                         UniqueStacks& aUniqueStacks) const {
  MOZ_ASSERT(mTimingData);
  StreamCommonProps("GCMinor", aWriter, aProcessStartTime, aUniqueStacks);
  if (mTimingData) {
    aWriter.SplicedJSONProperty("nursery", mTimingData.get());
  } else {
    aWriter.NullProperty("nursery");
  }
}

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

ProfileBufferEntryWriter::Length StyleMarkerPayload::TagAndSerializationBytes()
    const {
  return CommonPropsTagAndSerializationBytes() +
         ProfileBufferEntryWriter::SumBytes(mStats);
}

void StyleMarkerPayload::SerializeTagAndPayload(
    ProfileBufferEntryWriter& aEntryWriter) const {
  static const DeserializerTag tag = TagForDeserializer(Deserialize);
  SerializeTagAndCommonProps(tag, aEntryWriter);
  aEntryWriter.WriteObject(mStats);
}

// static
UniquePtr<ProfilerMarkerPayload> StyleMarkerPayload::Deserialize(
    ProfileBufferEntryReader& aEntryReader) {
  ProfilerMarkerPayload::CommonProps props =
      DeserializeCommonProps(aEntryReader);
  auto stats = aEntryReader.ReadObject<ServoTraversalStatistics>();
  return UniquePtr<ProfilerMarkerPayload>(
      new StyleMarkerPayload(std::move(props), stats));
}

void StyleMarkerPayload::StreamPayload(SpliceableJSONWriter& aWriter,
                                       const TimeStamp& aProcessStartTime,
                                       UniqueStacks& aUniqueStacks) const {
  StreamCommonProps("Styles", aWriter, aProcessStartTime, aUniqueStacks);
  aWriter.StringProperty("category", "Paint");
  aWriter.IntProperty("elementsTraversed", mStats.mElementsTraversed);
  aWriter.IntProperty("elementsStyled", mStats.mElementsStyled);
  aWriter.IntProperty("elementsMatched", mStats.mElementsMatched);
  aWriter.IntProperty("stylesShared", mStats.mStylesShared);
  aWriter.IntProperty("stylesReused", mStats.mStylesReused);
}

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

ProfileBufferEntryWriter::Length
JsAllocationMarkerPayload::TagAndSerializationBytes() const {
  return CommonPropsTagAndSerializationBytes() +
         ProfileBufferEntryWriter::SumBytes(mTypeName, mClassName,
                                            mDescriptiveTypeName, mCoarseType,
                                            mSize, mInNursery);
}

void JsAllocationMarkerPayload::SerializeTagAndPayload(
    ProfileBufferEntryWriter& aEntryWriter) const {
  static const DeserializerTag tag = TagForDeserializer(Deserialize);
  SerializeTagAndCommonProps(tag, aEntryWriter);
  aEntryWriter.WriteObject(mTypeName);
  aEntryWriter.WriteObject(mClassName);
  aEntryWriter.WriteObject(mDescriptiveTypeName);
  aEntryWriter.WriteObject(WrapProfileBufferRawPointer(mCoarseType));
  aEntryWriter.WriteObject(mSize);
  aEntryWriter.WriteObject(mInNursery);
}

// static
UniquePtr<ProfilerMarkerPayload> JsAllocationMarkerPayload::Deserialize(
    ProfileBufferEntryReader& aEntryReader) {
  ProfilerMarkerPayload::CommonProps props =
      DeserializeCommonProps(aEntryReader);
  auto typeName = aEntryReader.ReadObject<UniqueFreePtr<const char16_t>>();
  auto className = aEntryReader.ReadObject<UniqueFreePtr<const char>>();
  auto descriptiveTypeName =
      aEntryReader.ReadObject<UniqueFreePtr<const char16_t>>();
  auto coarseType = aEntryReader.ReadObject<const char*>();
  auto size = aEntryReader.ReadObject<uint64_t>();
  auto inNursery = aEntryReader.ReadObject<bool>();
  return UniquePtr<ProfilerMarkerPayload>(new JsAllocationMarkerPayload(
      std::move(props), std::move(typeName), std::move(className),
      std::move(descriptiveTypeName), coarseType, size, inNursery));
}

void JsAllocationMarkerPayload::StreamPayload(
    SpliceableJSONWriter& aWriter, const TimeStamp& aProcessStartTime,
    UniqueStacks& aUniqueStacks) const {
  StreamCommonProps("JS allocation", aWriter, aProcessStartTime, aUniqueStacks);

  if (mClassName) {
    aWriter.StringProperty("className", mClassName.get());
  }
  if (mTypeName) {
    aWriter.StringProperty("typeName",
                           NS_ConvertUTF16toUTF8(mTypeName.get()).get());
  }
  if (mDescriptiveTypeName) {
    aWriter.StringProperty(
        "descriptiveTypeName",
        NS_ConvertUTF16toUTF8(mDescriptiveTypeName.get()).get());
  }
  aWriter.StringProperty("coarseType", mCoarseType);
  aWriter.IntProperty("size", mSize);
  aWriter.BoolProperty("inNursery", mInNursery);
}

ProfileBufferEntryWriter::Length
NativeAllocationMarkerPayload::TagAndSerializationBytes() const {
  return CommonPropsTagAndSerializationBytes() +
         ProfileBufferEntryWriter::SumBytes(mSize, mThreadId, mMemoryAddress);
}

void NativeAllocationMarkerPayload::SerializeTagAndPayload(
    ProfileBufferEntryWriter& aEntryWriter) const {
  static const DeserializerTag tag = TagForDeserializer(Deserialize);
  SerializeTagAndCommonProps(tag, aEntryWriter);
  aEntryWriter.WriteObject(mSize);
  aEntryWriter.WriteObject(mMemoryAddress);
  aEntryWriter.WriteObject(mThreadId);
}

// static
UniquePtr<ProfilerMarkerPayload> NativeAllocationMarkerPayload::Deserialize(
    ProfileBufferEntryReader& aEntryReader) {
  ProfilerMarkerPayload::CommonProps props =
      DeserializeCommonProps(aEntryReader);
  auto size = aEntryReader.ReadObject<int64_t>();
  auto memoryAddress = aEntryReader.ReadObject<uintptr_t>();
  auto threadId = aEntryReader.ReadObject<int>();
  return UniquePtr<ProfilerMarkerPayload>(new NativeAllocationMarkerPayload(
      std::move(props), size, memoryAddress, threadId));
}

void NativeAllocationMarkerPayload::StreamPayload(
    SpliceableJSONWriter& aWriter, const TimeStamp& aProcessStartTime,
    UniqueStacks& aUniqueStacks) const {
  StreamCommonProps("Native allocation", aWriter, aProcessStartTime,
                    aUniqueStacks);
  aWriter.IntProperty("size", mSize);
  aWriter.IntProperty("memoryAddress", static_cast<int64_t>(mMemoryAddress));
  aWriter.IntProperty("threadId", mThreadId);
}

ProfileBufferEntryWriter::Length IPCMarkerPayload::TagAndSerializationBytes()
    const {
  return CommonPropsTagAndSerializationBytes() +
         ProfileBufferEntryWriter::SumBytes(
             mOtherPid, mMessageSeqno, mMessageType, mSide, mDirection, mSync);
}

void IPCMarkerPayload::SerializeTagAndPayload(
    ProfileBufferEntryWriter& aEntryWriter) const {
  static const DeserializerTag tag = TagForDeserializer(Deserialize);
  SerializeTagAndCommonProps(tag, aEntryWriter);
  aEntryWriter.WriteObject(mOtherPid);
  aEntryWriter.WriteObject(mMessageSeqno);
  aEntryWriter.WriteObject(mMessageType);
  aEntryWriter.WriteObject(mSide);
  aEntryWriter.WriteObject(mDirection);
  aEntryWriter.WriteObject(mSync);
}

// static
UniquePtr<ProfilerMarkerPayload> IPCMarkerPayload::Deserialize(
    ProfileBufferEntryReader& aEntryReader) {
  ProfilerMarkerPayload::CommonProps props =
      DeserializeCommonProps(aEntryReader);
  auto otherPid = aEntryReader.ReadObject<int32_t>();
  auto messageSeqno = aEntryReader.ReadObject<int32_t>();
  auto messageType = aEntryReader.ReadObject<IPC::Message::msgid_t>();
  auto mSide = aEntryReader.ReadObject<ipc::Side>();
  auto mDirection = aEntryReader.ReadObject<ipc::MessageDirection>();
  auto mSync = aEntryReader.ReadObject<bool>();
  return UniquePtr<ProfilerMarkerPayload>(
      new IPCMarkerPayload(std::move(props), otherPid, messageSeqno,
                           messageType, mSide, mDirection, mSync));
}

void IPCMarkerPayload::StreamPayload(SpliceableJSONWriter& aWriter,
                                     const TimeStamp& aProcessStartTime,
                                     UniqueStacks& aUniqueStacks) const {
  using namespace mozilla::ipc;
  StreamCommonProps("IPC", aWriter, aProcessStartTime, aUniqueStacks);
  aWriter.IntProperty("otherPid", mOtherPid);
  aWriter.IntProperty("messageSeqno", mMessageSeqno);
  aWriter.StringProperty("messageType",
                         IPC::StringFromIPCMessageType(mMessageType));
  aWriter.StringProperty("side", mSide == ParentSide ? "parent" : "child");
  aWriter.StringProperty("direction", mDirection == MessageDirection::eSending
                                          ? "sending"
                                          : "receiving");
  aWriter.BoolProperty("sync", mSync);
}
