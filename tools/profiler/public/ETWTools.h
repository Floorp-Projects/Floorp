/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ETWTools_h
#define ETWTools_h

#include "mozilla/BaseProfilerMarkers.h"
#include "mozilla/TimeStamp.h"
#include "nsString.h"

#if defined(XP_WIN) && !defined(RUST_BINDGEN) && !defined(__MINGW32__)
#  include "mozilla/ProfilerState.h"

#  include <windows.h>
#  include <TraceLoggingProvider.h>
#  include <vector>

namespace ETW {

extern std::atomic<ULONGLONG> gETWCollectionMask;

// Forward-declare the g_hMyComponentProvider variable that you will use for
// tracing in this component
TRACELOGGING_DECLARE_PROVIDER(kFirefoxTraceLoggingProvider);

void Init();
void Shutdown();

// This describes the base fields for all markers (information extracted from
// MarkerOptions.
struct BaseMarkerDescription {
  using MS = mozilla::MarkerSchema;
  static constexpr MS::PayloadField PayloadFields[] = {
      {"StartTime", MS::InputType::TimeStamp, "Start Time"},
      {"EndTime", MS::InputType::TimeStamp, "End Time"},
      {"InnerWindowId", MS::InputType::Uint64, "Inner Window ID"},
      {"CategoryPair", MS::InputType::Uint32, "Category Pair"}};
};

// This is the MarkerType object for markers with no statically declared type,
// their name is written dynamically.
struct SimpleMarkerType {
  using MS = mozilla::MarkerSchema;
  static constexpr const char* Name = "SimpleMarker";
  static constexpr MS::PayloadField PayloadFields[] = {
      {"MarkerName", MS::InputType::CString, "Simple Marker Name"}};
};

// This gets the space required in the Tlg static struct to pack the fields.
template <typename T>
constexpr std::size_t GetPackingSpace() {
  size_t length = 0;
  for (size_t i = 0; i < std::size(T::PayloadFields); i++) {
    length += std::string_view{T::PayloadFields[i].Key}.size() + 1;
    length += sizeof(uint8_t);
  }
  return length;
}

// Convert our InputType to Tlgs input type.
constexpr uint8_t GetTlgInputType(mozilla::MarkerSchema::InputType aInput) {
  using InputType = mozilla::MarkerSchema::InputType;
  switch (aInput) {
    case InputType::Boolean:
      return TlgInUINT8;
    case InputType::Uint32:
      return TlgInUINT32;
    case InputType::Uint64:
    case InputType::TimeStamp:
    case InputType::TimeDuration:
      return TlgInUINT64;
    case InputType::CString:
      return TlgInANSISTRING;
    case InputType::String:
      return TlgInUNICODESTRING;
    default:
      return 0;
  }
}

// This class represents the format ETW TraceLogging uses to describe its
// metadata. Since we have an abstraction layer we need to statically
// declare this ourselves and we cannot rely on TraceLogging's official
// macros. This does mean if TraceLogging ships big changes (on an SDK update)
// we may need to adapt.
__pragma(pack(push, 1)) _tlgEvtTagDecl(0);
template <typename T>
struct StaticMetaData {
  _tlgEventMetadata_t metaData;
  _tlgEvtTagType _tlgEvtTag;
  char name[std::string_view{T::Name}.size() + 1];
  char fieldStorage[GetPackingSpace<BaseMarkerDescription>() +
                    GetPackingSpace<T>()];

  // constexpr constructor
  constexpr StaticMetaData()
      : metaData{_TlgBlobEvent4,
                 11,  // WINEVENT_CHANNEL_TRACELOGGING,
                 5,   // Verbose
                 0,
                 0,
                 sizeof(StaticMetaData) - _tlg_EVENT_METADATA_PREAMBLE - 1},
        _tlgEvtTag(_tlgEvtTagInit) {
    for (uint32_t i = 0; i < std::string_view{T::Name}.size() + 1; i++) {
      name[i] = T::Name[i];
    }

    size_t pos = 0;
    for (uint32_t i = 0; i < std::size(BaseMarkerDescription::PayloadFields);
         i++) {
      for (size_t c = 0;
           c < std::string_view{BaseMarkerDescription::PayloadFields[i].Key}
                       .size() +
                   1;
           c++) {
        fieldStorage[pos++] = BaseMarkerDescription::PayloadFields[i].Key[c];
      }
      fieldStorage[pos++] =
          GetTlgInputType(BaseMarkerDescription::PayloadFields[i].InputTy);
    }
    for (uint32_t i = 0; i < std::size(T::PayloadFields); i++) {
      for (size_t c = 0;
           c < std::string_view{T::PayloadFields[i].Key}.size() + 1; c++) {
        fieldStorage[pos++] = T::PayloadFields[i].Key[c];
      }
      fieldStorage[pos++] = GetTlgInputType(T::PayloadFields[i].InputTy);
    }
  }
};
__pragma(pack(pop));

// This defines the amount of storage available on the stack to store POD
// values.
const size_t kStackStorage = 512;

struct PayloadBuffer {
  EVENT_DATA_DESCRIPTOR* mDescriptors = nullptr;
  size_t mOffset = 0;
  std::array<char, kStackStorage> mStorage;
};

// This processes POD objects and stores them in a temporary buffer.
// Theoretically we could probably avoid these assignments when passed a POD
// variable we know is going to be alive but that would require some more
// template magic.
template <typename T>
static void CreateDataDescForPayload(PayloadBuffer& aBuffer,
                                     EVENT_DATA_DESCRIPTOR& aDescriptor,
                                     const T& aPayload) {
  static_assert(std::is_pod<T>::value,
                "Writing a non-POD payload requires template specialization.");

  // Ensure we never overflow our stack buffer.
  MOZ_RELEASE_ASSERT((aBuffer.mOffset + sizeof(T)) < kStackStorage);

  T* storedValue =
      reinterpret_cast<T*>(aBuffer.mStorage.data() + aBuffer.mOffset);
  *storedValue = aPayload;
  aBuffer.mOffset += sizeof(T);

  EventDataDescCreate(&aDescriptor, storedValue, sizeof(T));
}

template <>
inline void CreateDataDescForPayload<mozilla::ProfilerString8View>(
    PayloadBuffer& aBuffer, EVENT_DATA_DESCRIPTOR& aDescriptor,
    const mozilla::ProfilerString8View& aPayload) {
  EventDataDescCreate(&aDescriptor, aPayload.StringView().data(),
                      aPayload.StringView().size() + 1);
}

template <>
inline void CreateDataDescForPayload<mozilla::TimeStamp>(
    PayloadBuffer& aBuffer, EVENT_DATA_DESCRIPTOR& aDescriptor,
    const mozilla::TimeStamp& aPayload) {
  if (aPayload.RawQueryPerformanceCounterValue().isNothing()) {
    // This should never happen?
    EventDataDescCreate(&aDescriptor, nullptr, 0);
    return;
  }

  CreateDataDescForPayload(aBuffer, aDescriptor,
                           aPayload.RawQueryPerformanceCounterValue().value());
}

template <>
inline void CreateDataDescForPayload<mozilla::TimeDuration>(
    PayloadBuffer& aBuffer, EVENT_DATA_DESCRIPTOR& aDescriptor,
    const mozilla::TimeDuration& aPayload) {
  CreateDataDescForPayload(aBuffer, aDescriptor, aPayload.ToMilliseconds());
}

// For reasons that are beyond me if this isn't marked inline it generates an
// unused function warning despite being a template specialization.
template <typename T>
inline void CreateDataDescForPayload(PayloadBuffer& aBuffer,
                                     EVENT_DATA_DESCRIPTOR& aDescriptor,
                                     const nsTString<T>& aPayload) {
  EventDataDescCreate(&aDescriptor, aPayload.BeginReading(),
                      (aPayload.Length() + 1) * sizeof(T));
}
template <typename T>
inline void CreateDataDescForPayload(PayloadBuffer& aBuffer,
                                     EVENT_DATA_DESCRIPTOR& aDescriptor,
                                     const nsTSubstring<T>& aPayload) {
  EventDataDescCreate(&aDescriptor, aPayload.BeginReading(),
                      (aPayload.Length() + 1) * sizeof(T));
}

// Template specialization that writes out empty data descriptors for an empty
// Maybe<T>
template <typename T>
void CreateDataDescForPayload(PayloadBuffer& aBuffer,
                              EVENT_DATA_DESCRIPTOR& aDescriptor,
                              const mozilla::Maybe<T>& aPayload) {
  if (aPayload.isNothing()) {
    EventDataDescCreate(&aDescriptor, nullptr, 0);
  } else {
    CreateDataDescForPayload(aBuffer, aDescriptor, *aPayload);
  }
}

template <typename T, typename = void>
struct MarkerSupportsETW : std::false_type {};
template <typename T>
struct MarkerSupportsETW<T, std::void_t<decltype(T::PayloadFields)>>
    : std::true_type {};

template <typename T, typename = void>
struct MarkerHasTranslator : std::false_type {};
template <typename T>
struct MarkerHasTranslator<
    T, std::void_t<decltype(T::TranslateMarkerInputToSchema)>>
    : std::true_type {};

struct BaseEventStorage {
  uint64_t mStartTime;
  uint64_t mEndTime;
  uint64_t mWindowID;
  uint32_t mCategoryPair;
};

static inline void StoreBaseEventDataDesc(
    BaseEventStorage& aStorage, EVENT_DATA_DESCRIPTOR* aDescriptors,
    const mozilla::MarkerCategory& aCategory,
    const mozilla::MarkerOptions& aOptions) {
  if (!aOptions.IsTimingUnspecified()) {
    aStorage.mStartTime =
        aOptions.Timing().StartTime().RawQueryPerformanceCounterValue().value();
    aStorage.mEndTime =
        aOptions.Timing().EndTime().RawQueryPerformanceCounterValue().value();
  }
  if (!aOptions.InnerWindowId().IsUnspecified()) {
    aStorage.mWindowID = aOptions.InnerWindowId().Id();
  }
  aStorage.mCategoryPair = uint32_t(aCategory.CategoryPair());
  EventDataDescCreate(&aDescriptors[2], &aStorage.mStartTime, sizeof(uint64_t));
  EventDataDescCreate(&aDescriptors[3], &aStorage.mEndTime, sizeof(uint64_t));
  EventDataDescCreate(&aDescriptors[4], &aStorage.mWindowID, sizeof(uint64_t));
  EventDataDescCreate(&aDescriptors[5], &aStorage.mCategoryPair,
                      sizeof(uint32_t));
}

// This is used for markers with no explicit type or markers which have not
// been converted to the updated schema yet.
static inline void EmitETWMarker(const mozilla::ProfilerString8View& aName,
                                 const mozilla::MarkerCategory& aCategory,
                                 const mozilla::MarkerOptions& aOptions = {}) {
  if (!(gETWCollectionMask &
        uint64_t(mozilla::MarkerSchema::ETWMarkerGroup::Generic))) {
    return;
  }

  static const __declspec(allocate(_tlgSegMetadataEvents)) __declspec(
      align(1)) constexpr StaticMetaData<SimpleMarkerType>
      staticData;

  std::array<EVENT_DATA_DESCRIPTOR, 7> descriptors = {};

  // This is storage space allocated on the stack for POD values.
  BaseEventStorage dataStorage = {};

  StoreBaseEventDataDesc(dataStorage, descriptors.data(), aCategory,
                         std::move(aOptions));

  EventDataDescCreate(&descriptors[6], aName.StringView().data(),
                      aName.StringView().size() + 1);
  _tlgWriteTransfer(kFirefoxTraceLoggingProvider, &staticData.metaData.Channel,
                    NULL, NULL, descriptors.size(), descriptors.data());
}

template <typename MarkerType, typename... PayloadArguments>
static inline void EmitETWMarker(const mozilla::ProfilerString8View& aName,
                                 const mozilla::MarkerCategory& aCategory,
                                 const mozilla::MarkerOptions& aOptions,
                                 MarkerType aMarkerType,
                                 const PayloadArguments&... aPayloadArguments) {
  // If our MarkerType has not been made to support ETW, emit only the base
  // event. Avoid attempting to compile the rest of the function.
  if constexpr (!MarkerSupportsETW<MarkerType>::value) {
    return EmitETWMarker(aName, aCategory, aOptions);
  } else {
    if (!(gETWCollectionMask & uint64_t(MarkerType::Group))) {
      return;
    }

    static const __declspec(allocate(_tlgSegMetadataEvents)) __declspec(
        align(1)) constexpr StaticMetaData<MarkerType>
        staticData;

    // Allocate the exact amount of descriptors required by this event.
    std::array<EVENT_DATA_DESCRIPTOR,
               2 + std::size(MarkerType::PayloadFields) +
                   std::size(BaseMarkerDescription::PayloadFields)>
        descriptors = {};

    // Memory allocated on the stack for storing intermediate values.
    BaseEventStorage dataStorage = {};
    PayloadBuffer buffer;

    StoreBaseEventDataDesc(dataStorage, descriptors.data(), aCategory,
                           aOptions);

    if constexpr (MarkerHasTranslator<MarkerType>::value) {
      // When this function is implemented the arguments are passed back to the
      // MarkerType object which is expected to call OutputMarkerSchema with
      // the correct argument format.
      buffer.mDescriptors = descriptors.data() + 2 +
                            std::size(BaseMarkerDescription::PayloadFields);
      MarkerType::TranslateMarkerInputToSchema(&buffer, aPayloadArguments...);
    } else {
      const size_t argCount = sizeof...(PayloadArguments);
      static_assert(
          argCount == std::size(MarkerType::PayloadFields),
          "Number and type of fields must be equal to number and type of "
          "payload arguments. If this is not the case a "
          "TranslateMarkerInputToSchema function must be defined.");
      size_t i = 2 + std::size(BaseMarkerDescription::PayloadFields);
      (CreateDataDescForPayload(buffer, descriptors[i++], aPayloadArguments),
       ...);
    }

    _tlgWriteTransfer(kFirefoxTraceLoggingProvider,
                      &staticData.metaData.Channel, NULL, NULL,
                      descriptors.size(), descriptors.data());
  }
}

// This function allows markers to specify a translator function for when
// their arguments to profiler_add_marker do not exactly match the schema or
// when they need to make other adjustments to the data.
template <typename MarkerType, typename... PayloadArguments>
void OutputMarkerSchema(void* aContext, MarkerType aMarkerType,
                        const PayloadArguments&... aPayloadArguments) {
  const size_t argCount = sizeof...(PayloadArguments);
  static_assert(argCount == std::size(MarkerType::PayloadFields),
                "Number and type of fields must be equal to number and type of "
                "payload arguments.");

  PayloadBuffer* buffer = static_cast<PayloadBuffer*>(aContext);
  size_t i = 0;
  (CreateDataDescForPayload(*buffer, buffer->mDescriptors[i++],
                            aPayloadArguments),
   ...);
}
}  // namespace ETW

#else
namespace ETW {
static inline void Init() {}
static inline void Shutdown() {}
static inline void EmitETWMarker(const mozilla::ProfilerString8View& aName,
                                 const mozilla::MarkerCategory& aCategory,
                                 const mozilla::MarkerOptions& aOptions = {}) {}
template <typename MarkerType, typename... PayloadArguments>
static inline void EmitETWMarker(const mozilla::ProfilerString8View& aName,
                                 const mozilla::MarkerCategory& aCategory,
                                 const mozilla::MarkerOptions& aOptions,
                                 MarkerType aMarkerType,
                                 const PayloadArguments&... aPayloadArguments) {
}
template <typename MarkerType, typename... PayloadArguments>
void OutputMarkerSchema(void* aContext, MarkerType aMarkerType,
                        const PayloadArguments&... aPayloadArguments) {}
}  // namespace ETW
#endif

#endif  // ETWTools_h
