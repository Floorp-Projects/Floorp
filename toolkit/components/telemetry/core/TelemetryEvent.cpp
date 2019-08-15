/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Telemetry.h"
#include "TelemetryEvent.h"
#include <prtime.h>
#include <limits>
#include "ipc/TelemetryIPCAccumulator.h"
#include "jsapi.h"
#include "mozilla/Maybe.h"
#include "mozilla/Pair.h"
#include "mozilla/Preferences.h"
#include "mozilla/Services.h"
#include "mozilla/StaticMutex.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/Unused.h"
#include "nsClassHashtable.h"
#include "nsDataHashtable.h"
#include "nsHashKeys.h"
#include "nsIObserverService.h"
#include "nsITelemetry.h"
#include "nsJSUtils.h"
#include "nsPrintfCString.h"
#include "nsTArray.h"
#include "nsUTF8Utils.h"
#include "nsXULAppAPI.h"
#include "TelemetryCommon.h"
#include "TelemetryEventData.h"
#include "TelemetryScalar.h"

using mozilla::ArrayLength;
using mozilla::Maybe;
using mozilla::Nothing;
using mozilla::StaticAutoPtr;
using mozilla::StaticMutex;
using mozilla::StaticMutexAutoLock;
using mozilla::TimeStamp;
using mozilla::Telemetry::ChildEventData;
using mozilla::Telemetry::EventExtraEntry;
using mozilla::Telemetry::LABELS_TELEMETRY_EVENT_RECORDING_ERROR;
using mozilla::Telemetry::LABELS_TELEMETRY_EVENT_REGISTRATION_ERROR;
using mozilla::Telemetry::ProcessID;
using mozilla::Telemetry::Common::AutoHashtable;
using mozilla::Telemetry::Common::CanRecordDataset;
using mozilla::Telemetry::Common::CanRecordInProcess;
using mozilla::Telemetry::Common::CanRecordProduct;
using mozilla::Telemetry::Common::GetCurrentProduct;
using mozilla::Telemetry::Common::GetNameForProcessID;
using mozilla::Telemetry::Common::IsExpiredVersion;
using mozilla::Telemetry::Common::IsInDataset;
using mozilla::Telemetry::Common::IsValidIdentifierString;
using mozilla::Telemetry::Common::LogToBrowserConsole;
using mozilla::Telemetry::Common::MsSinceProcessStart;
using mozilla::Telemetry::Common::ToJSString;

namespace TelemetryIPCAccumulator = mozilla::TelemetryIPCAccumulator;

////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////
//
// Naming: there are two kinds of functions in this file:
//
// * Functions taking a StaticMutexAutoLock: these can only be reached via
//   an interface function (TelemetryEvent::*). They expect the interface
//   function to have acquired |gTelemetryEventsMutex|, so they do not
//   have to be thread-safe.
//
// * Functions named TelemetryEvent::*. This is the external interface.
//   Entries and exits to these functions are serialised using
//   |gTelemetryEventsMutex|.
//
// Avoiding races and deadlocks:
//
// All functions in the external interface (TelemetryEvent::*) are
// serialised using the mutex |gTelemetryEventsMutex|. This means
// that the external interface is thread-safe, and the internal
// functions can ignore thread safety. But it also brings a danger
// of deadlock if any function in the external interface can get back
// to that interface. That is, we will deadlock on any call chain like
// this:
//
// TelemetryEvent::* -> .. any functions .. -> TelemetryEvent::*
//
// To reduce the danger of that happening, observe the following rules:
//
// * No function in TelemetryEvent::* may directly call, nor take the
//   address of, any other function in TelemetryEvent::*.
//
// * No internal function may call, nor take the address
//   of, any function in TelemetryEvent::*.

////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////
//
// PRIVATE TYPES

namespace {

const uint32_t kEventCount =
    static_cast<uint32_t>(mozilla::Telemetry::EventID::EventCount);
// This is a special event id used to mark expired events, to make expiry checks
// cheap at runtime.
const uint32_t kExpiredEventId = std::numeric_limits<uint32_t>::max();
static_assert(kExpiredEventId > kEventCount,
              "Built-in event count should be less than the expired event id.");

// Maximum length of any passed value string, in UTF8 byte sequence length.
const uint32_t kMaxValueByteLength = 80;
// Maximum length of any string value in the extra dictionary, in UTF8 byte
// sequence length.
const uint32_t kMaxExtraValueByteLength = 80;
// Maximum length of dynamic method names, in UTF8 byte sequence length.
const uint32_t kMaxMethodNameByteLength = 20;
// Maximum length of dynamic object names, in UTF8 byte sequence length.
const uint32_t kMaxObjectNameByteLength = 20;
// Maximum length of extra key names, in UTF8 byte sequence length.
const uint32_t kMaxExtraKeyNameByteLength = 15;
// The maximum number of valid extra keys for an event.
const uint32_t kMaxExtraKeyCount = 10;

struct EventKey {
  uint32_t id;
  bool dynamic;
};

struct DynamicEventInfo {
  DynamicEventInfo(const nsACString& category, const nsACString& method,
                   const nsACString& object, nsTArray<nsCString>& extra_keys,
                   bool recordOnRelease, bool builtin)
      : category(category),
        method(method),
        object(object),
        extra_keys(extra_keys),
        recordOnRelease(recordOnRelease),
        builtin(builtin) {}

  DynamicEventInfo(const DynamicEventInfo&) = default;
  DynamicEventInfo& operator=(const DynamicEventInfo&) = delete;

  const nsCString category;
  const nsCString method;
  const nsCString object;
  const nsTArray<nsCString> extra_keys;
  const bool recordOnRelease;
  const bool builtin;

  size_t SizeOfExcludingThis(mozilla::MallocSizeOf aMallocSizeOf) const {
    size_t n = 0;

    n += category.SizeOfExcludingThisIfUnshared(aMallocSizeOf);
    n += method.SizeOfExcludingThisIfUnshared(aMallocSizeOf);
    n += object.SizeOfExcludingThisIfUnshared(aMallocSizeOf);
    n += extra_keys.ShallowSizeOfExcludingThis(aMallocSizeOf);
    for (auto& key : extra_keys) {
      n += key.SizeOfExcludingThisIfUnshared(aMallocSizeOf);
    }

    return n;
  }
};

enum class RecordEventResult {
  Ok,
  UnknownEvent,
  InvalidExtraKey,
  StorageLimitReached,
  ExpiredEvent,
  WrongProcess,
};

typedef nsTArray<EventExtraEntry> ExtraArray;

class EventRecord {
 public:
  EventRecord(double timestamp, const EventKey& key,
              const Maybe<nsCString>& value, const ExtraArray& extra)
      : mTimestamp(timestamp), mEventKey(key), mValue(value), mExtra(extra) {}

  EventRecord(const EventRecord& other) = default;

  EventRecord& operator=(const EventRecord& other) = delete;

  double Timestamp() const { return mTimestamp; }
  const EventKey& GetEventKey() const { return mEventKey; }
  const Maybe<nsCString>& Value() const { return mValue; }
  const ExtraArray& Extra() const { return mExtra; }

  size_t SizeOfExcludingThis(mozilla::MallocSizeOf aMallocSizeOf) const;

 private:
  const double mTimestamp;
  const EventKey mEventKey;
  const Maybe<nsCString> mValue;
  const ExtraArray mExtra;
};

// Implements the methods for EventInfo.
const nsDependentCString EventInfo::method() const {
  return nsDependentCString(&gEventsStringTable[this->method_offset]);
}

const nsDependentCString EventInfo::object() const {
  return nsDependentCString(&gEventsStringTable[this->object_offset]);
}

// Implements the methods for CommonEventInfo.
const nsDependentCString CommonEventInfo::category() const {
  return nsDependentCString(&gEventsStringTable[this->category_offset]);
}

const nsDependentCString CommonEventInfo::expiration_version() const {
  return nsDependentCString(
      &gEventsStringTable[this->expiration_version_offset]);
}

const nsDependentCString CommonEventInfo::extra_key(uint32_t index) const {
  MOZ_ASSERT(index < this->extra_count);
  uint32_t key_index = gExtraKeysTable[this->extra_index + index];
  return nsDependentCString(&gEventsStringTable[key_index]);
}

// Implementation for the EventRecord class.
size_t EventRecord::SizeOfExcludingThis(
    mozilla::MallocSizeOf aMallocSizeOf) const {
  size_t n = 0;

  if (mValue) {
    n += mValue.value().SizeOfExcludingThisIfUnshared(aMallocSizeOf);
  }

  n += mExtra.ShallowSizeOfExcludingThis(aMallocSizeOf);
  for (uint32_t i = 0; i < mExtra.Length(); ++i) {
    n += mExtra[i].key.SizeOfExcludingThisIfUnshared(aMallocSizeOf);
    n += mExtra[i].value.SizeOfExcludingThisIfUnshared(aMallocSizeOf);
  }

  return n;
}

nsCString UniqueEventName(const nsACString& category, const nsACString& method,
                          const nsACString& object) {
  nsCString name;
  name.Append(category);
  name.AppendLiteral("#");
  name.Append(method);
  name.AppendLiteral("#");
  name.Append(object);
  return name;
}

nsCString UniqueEventName(const EventInfo& info) {
  return UniqueEventName(info.common_info.category(), info.method(),
                         info.object());
}

nsCString UniqueEventName(const DynamicEventInfo& info) {
  return UniqueEventName(info.category, info.method, info.object);
}

void TruncateToByteLength(nsCString& str, uint32_t length) {
  // last will be the index of the first byte of the current multi-byte
  // sequence.
  uint32_t last = RewindToPriorUTF8Codepoint(str.get(), length);
  str.Truncate(last);
}

}  // anonymous namespace

////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////
//
// PRIVATE STATE, SHARED BY ALL THREADS

namespace {

// Set to true once this global state has been initialized.
bool gInitDone = false;

bool gCanRecordBase;
bool gCanRecordExtended;

// The EventName -> EventKey cache map.
nsClassHashtable<nsCStringHashKey, EventKey> gEventNameIDMap(kEventCount);

// The CategoryName set.
nsTHashtable<nsCStringHashKey> gCategoryNames;

// This tracks the IDs of the categories for which recording is enabled.
nsTHashtable<nsCStringHashKey> gEnabledCategories;

// The main event storage. Events are inserted here, keyed by process id and
// in recording order.
typedef nsUint32HashKey ProcessIDHashKey;
typedef nsTArray<EventRecord> EventRecordArray;
typedef nsClassHashtable<ProcessIDHashKey, EventRecordArray>
    EventRecordsMapType;

EventRecordsMapType gEventRecords;

// The details on dynamic events that are recorded from addons are registered
// here.
StaticAutoPtr<nsTArray<DynamicEventInfo>> gDynamicEventInfo;

}  // namespace

////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////
//
// PRIVATE: thread-safe helpers for event recording.

namespace {

unsigned int GetDataset(const StaticMutexAutoLock& lock,
                        const EventKey& eventKey) {
  if (!eventKey.dynamic) {
    return gEventInfo[eventKey.id].common_info.dataset;
  }

  if (!gDynamicEventInfo) {
    return nsITelemetry::DATASET_PRERELEASE_CHANNELS;
  }

  return (*gDynamicEventInfo)[eventKey.id].recordOnRelease
             ? nsITelemetry::DATASET_ALL_CHANNELS
             : nsITelemetry::DATASET_PRERELEASE_CHANNELS;
}

nsCString GetCategory(const StaticMutexAutoLock& lock,
                      const EventKey& eventKey) {
  if (!eventKey.dynamic) {
    return gEventInfo[eventKey.id].common_info.category();
  }

  if (!gDynamicEventInfo) {
    return NS_LITERAL_CSTRING("");
  }

  return (*gDynamicEventInfo)[eventKey.id].category;
}

bool CanRecordEvent(const StaticMutexAutoLock& lock, const EventKey& eventKey,
                    ProcessID process) {
  if (!gCanRecordBase) {
    return false;
  }

  if (!CanRecordDataset(GetDataset(lock, eventKey), gCanRecordBase,
                        gCanRecordExtended)) {
    return false;
  }

  // We don't allow specifying a process to record in for dynamic events.
  if (!eventKey.dynamic) {
    const CommonEventInfo& info = gEventInfo[eventKey.id].common_info;

    if (!CanRecordProduct(info.products)) {
      return false;
    }

    if (!CanRecordInProcess(info.record_in_processes, process)) {
      return false;
    }
  }

  return true;
}

bool IsExpired(const EventKey& key) { return key.id == kExpiredEventId; }

EventRecordArray* GetEventRecordsForProcess(const StaticMutexAutoLock& lock,
                                            ProcessID processType,
                                            const EventKey& eventKey) {
  EventRecordArray* eventRecords = nullptr;
  if (!gEventRecords.Get(uint32_t(processType), &eventRecords)) {
    eventRecords = new EventRecordArray();
    gEventRecords.Put(uint32_t(processType), eventRecords);
  }
  return eventRecords;
}

EventKey* GetEventKey(const StaticMutexAutoLock& lock,
                      const nsACString& category, const nsACString& method,
                      const nsACString& object) {
  EventKey* event;
  const nsCString& name = UniqueEventName(category, method, object);
  if (!gEventNameIDMap.Get(name, &event)) {
    return nullptr;
  }
  return event;
}

static bool CheckExtraKeysValid(const EventKey& eventKey,
                                const ExtraArray& extra) {
  nsTHashtable<nsCStringHashKey> validExtraKeys;
  if (!eventKey.dynamic) {
    const CommonEventInfo& common = gEventInfo[eventKey.id].common_info;
    for (uint32_t i = 0; i < common.extra_count; ++i) {
      validExtraKeys.PutEntry(common.extra_key(i));
    }
  } else if (gDynamicEventInfo) {
    const DynamicEventInfo& info = (*gDynamicEventInfo)[eventKey.id];
    for (uint32_t i = 0, len = info.extra_keys.Length(); i < len; ++i) {
      validExtraKeys.PutEntry(info.extra_keys[i]);
    }
  }

  for (uint32_t i = 0; i < extra.Length(); ++i) {
    if (!validExtraKeys.GetEntry(extra[i].key)) {
      return false;
    }
  }

  return true;
}

RecordEventResult RecordEvent(const StaticMutexAutoLock& lock,
                              ProcessID processType, double timestamp,
                              const nsACString& category,
                              const nsACString& method,
                              const nsACString& object,
                              const Maybe<nsCString>& value,
                              const ExtraArray& extra) {
  // Look up the event id.
  EventKey* eventKey = GetEventKey(lock, category, method, object);
  if (!eventKey) {
    mozilla::Telemetry::AccumulateCategorical(
        LABELS_TELEMETRY_EVENT_RECORDING_ERROR::UnknownEvent);
    return RecordEventResult::UnknownEvent;
  }

  // If the event is expired or not enabled for this process, we silently drop
  // this call. We don't want recording for expired probes to be an error so
  // code doesn't have to be removed at a specific time or version. Even logging
  // warnings would become very noisy.
  if (IsExpired(*eventKey)) {
    mozilla::Telemetry::AccumulateCategorical(
        LABELS_TELEMETRY_EVENT_RECORDING_ERROR::Expired);
    return RecordEventResult::ExpiredEvent;
  }

  // Fixup the process id only for non-builtin (e.g. supporting build faster)
  // dynamic events.
  auto dynamicNonBuiltin =
      eventKey->dynamic && !(*gDynamicEventInfo)[eventKey->id].builtin;
  if (dynamicNonBuiltin) {
    processType = ProcessID::Dynamic;
  }

  // Check whether the extra keys passed are valid.
  if (!CheckExtraKeysValid(*eventKey, extra)) {
    mozilla::Telemetry::AccumulateCategorical(
        LABELS_TELEMETRY_EVENT_RECORDING_ERROR::ExtraKey);
    return RecordEventResult::InvalidExtraKey;
  }

  // Check whether we can record this event.
  if (!CanRecordEvent(lock, *eventKey, processType)) {
    return RecordEventResult::Ok;
  }

  // Count the number of times this event has been recorded, even if its
  // category does not have recording enabled.
  TelemetryScalar::SummarizeEvent(UniqueEventName(category, method, object),
                                  processType, dynamicNonBuiltin);

  // Check whether this event's category has recording enabled
  if (!gEnabledCategories.GetEntry(GetCategory(lock, *eventKey))) {
    return RecordEventResult::Ok;
  }

  static bool sEventPingEnabled = mozilla::Preferences::GetBool(
      "toolkit.telemetry.eventping.enabled", true);
  if (!sEventPingEnabled) {
    return RecordEventResult::Ok;
  }

  EventRecordArray* eventRecords =
      GetEventRecordsForProcess(lock, processType, *eventKey);
  eventRecords->AppendElement(EventRecord(timestamp, *eventKey, value, extra));

  // Notify observers when we hit the "event" ping event record limit.
  static uint32_t sEventPingLimit = mozilla::Preferences::GetUint(
      "toolkit.telemetry.eventping.eventLimit", 1000);
  if (eventRecords->Length() == sEventPingLimit) {
    return RecordEventResult::StorageLimitReached;
  }

  return RecordEventResult::Ok;
}

RecordEventResult ShouldRecordChildEvent(const StaticMutexAutoLock& lock,
                                         const nsACString& category,
                                         const nsACString& method,
                                         const nsACString& object) {
  EventKey* eventKey = GetEventKey(lock, category, method, object);
  if (!eventKey) {
    // This event is unknown in this process, but it might be a dynamic event
    // that was registered in the parent process.
    return RecordEventResult::Ok;
  }

  if (IsExpired(*eventKey)) {
    return RecordEventResult::ExpiredEvent;
  }

  const auto processes =
      gEventInfo[eventKey->id].common_info.record_in_processes;
  if (!CanRecordInProcess(processes, XRE_GetProcessType())) {
    return RecordEventResult::WrongProcess;
  }

  return RecordEventResult::Ok;
}

void RegisterEvents(const StaticMutexAutoLock& lock, const nsACString& category,
                    const nsTArray<DynamicEventInfo>& eventInfos,
                    const nsTArray<bool>& eventExpired, bool aBuiltin) {
  MOZ_ASSERT(eventInfos.Length() == eventExpired.Length(),
             "Event data array sizes should match.");

  // Register the new events.
  if (!gDynamicEventInfo) {
    gDynamicEventInfo = new nsTArray<DynamicEventInfo>();
  }

  for (uint32_t i = 0, len = eventInfos.Length(); i < len; ++i) {
    const nsCString& eventName = UniqueEventName(eventInfos[i]);

    // Re-registering events can happen for two reasons and we don't print
    // warnings:
    //
    // * When add-ons update.
    //   We don't support changing their definition, but the expiry might have
    //   changed.
    // * When dynamic builtins ("build faster") events are registered.
    //   The dynamic definition takes precedence then.
    EventKey* existing = nullptr;
    if (!aBuiltin && gEventNameIDMap.Get(eventName, &existing)) {
      if (eventExpired[i]) {
        existing->id = kExpiredEventId;
      }
      continue;
    }

    gDynamicEventInfo->AppendElement(eventInfos[i]);
    uint32_t eventId =
        eventExpired[i] ? kExpiredEventId : gDynamicEventInfo->Length() - 1;
    gEventNameIDMap.Put(eventName, new EventKey{eventId, true});
  }

  // If it is a builtin, add the category name in order to enable it later.
  if (aBuiltin) {
    gCategoryNames.PutEntry(category);
  }

  if (!aBuiltin) {
    // Now after successful registration enable recording for this category
    // (if not a dynamic builtin).
    gEnabledCategories.PutEntry(category);
  }
}

}  // anonymous namespace

////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////
//
// PRIVATE: thread-unsafe helpers for event handling.

namespace {

nsresult SerializeEventsArray(const EventRecordArray& events, JSContext* cx,
                              JS::MutableHandleObject result,
                              unsigned int dataset) {
  // We serialize the events to a JS array.
  JS::RootedObject eventsArray(cx, JS_NewArrayObject(cx, events.Length()));
  if (!eventsArray) {
    return NS_ERROR_FAILURE;
  }

  for (uint32_t i = 0; i < events.Length(); ++i) {
    const EventRecord& record = events[i];

    // Each entry is an array of one of the forms:
    // [timestamp, category, method, object, value]
    // [timestamp, category, method, object, null, extra]
    // [timestamp, category, method, object, value, extra]
    JS::RootedVector<JS::Value> items(cx);

    // Add timestamp.
    JS::Rooted<JS::Value> val(cx);
    if (!items.append(JS::NumberValue(floor(record.Timestamp())))) {
      return NS_ERROR_FAILURE;
    }

    // Add category, method, object.
    auto addCategoryMethodObjectValues = [&](const nsACString& category,
                                             const nsACString& method,
                                             const nsACString& object) -> bool {
      return items.append(JS::StringValue(ToJSString(cx, category))) &&
             items.append(JS::StringValue(ToJSString(cx, method))) &&
             items.append(JS::StringValue(ToJSString(cx, object)));
    };

    const EventKey& eventKey = record.GetEventKey();
    if (!eventKey.dynamic) {
      const EventInfo& info = gEventInfo[eventKey.id];
      if (!addCategoryMethodObjectValues(info.common_info.category(),
                                         info.method(), info.object())) {
        return NS_ERROR_FAILURE;
      }
    } else if (gDynamicEventInfo) {
      const DynamicEventInfo& info = (*gDynamicEventInfo)[eventKey.id];
      if (!addCategoryMethodObjectValues(info.category, info.method,
                                         info.object)) {
        return NS_ERROR_FAILURE;
      }
    }

    // Add the optional string value only when needed.
    // When the value field is empty and extra is not set, we can save a little
    // space that way. We still need to submit a null value if extra is set, to
    // match the form: [ts, category, method, object, null, extra]
    if (record.Value()) {
      if (!items.append(
              JS::StringValue(ToJSString(cx, record.Value().value())))) {
        return NS_ERROR_FAILURE;
      }
    } else if (!record.Extra().IsEmpty()) {
      if (!items.append(JS::NullValue())) {
        return NS_ERROR_FAILURE;
      }
    }

    // Add the optional extra dictionary.
    // To save a little space, only add it when it is not empty.
    if (!record.Extra().IsEmpty()) {
      JS::RootedObject obj(cx, JS_NewPlainObject(cx));
      if (!obj) {
        return NS_ERROR_FAILURE;
      }

      // Add extra key & value entries.
      const ExtraArray& extra = record.Extra();
      for (uint32_t i = 0; i < extra.Length(); ++i) {
        JS::Rooted<JS::Value> value(cx);
        value.setString(ToJSString(cx, extra[i].value));

        if (!JS_DefineProperty(cx, obj, extra[i].key.get(), value,
                               JSPROP_ENUMERATE)) {
          return NS_ERROR_FAILURE;
        }
      }
      val.setObject(*obj);

      if (!items.append(val)) {
        return NS_ERROR_FAILURE;
      }
    }

    // Add the record to the events array.
    JS::RootedObject itemsArray(cx, JS_NewArrayObject(cx, items));
    if (!JS_DefineElement(cx, eventsArray, i, itemsArray, JSPROP_ENUMERATE)) {
      return NS_ERROR_FAILURE;
    }
  }

  result.set(eventsArray);
  return NS_OK;
}

}  // anonymous namespace

////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////
//
// EXTERNALLY VISIBLE FUNCTIONS in namespace TelemetryEvents::

// This is a StaticMutex rather than a plain Mutex (1) so that
// it gets initialised in a thread-safe manner the first time
// it is used, and (2) because it is never de-initialised, and
// a normal Mutex would show up as a leak in BloatView.  StaticMutex
// also has the "OffTheBooks" property, so it won't show as a leak
// in BloatView.
// Another reason to use a StaticMutex instead of a plain Mutex is
// that, due to the nature of Telemetry, we cannot rely on having a
// mutex initialized in InitializeGlobalState. Unfortunately, we
// cannot make sure that no other function is called before this point.
static StaticMutex gTelemetryEventsMutex;

void TelemetryEvent::InitializeGlobalState(bool aCanRecordBase,
                                           bool aCanRecordExtended) {
  StaticMutexAutoLock locker(gTelemetryEventsMutex);
  MOZ_ASSERT(!gInitDone,
             "TelemetryEvent::InitializeGlobalState "
             "may only be called once");

  gCanRecordBase = aCanRecordBase;
  gCanRecordExtended = aCanRecordExtended;

  // Populate the static event name->id cache. Note that the event names are
  // statically allocated and come from the automatically generated
  // TelemetryEventData.h.
  const uint32_t eventCount =
      static_cast<uint32_t>(mozilla::Telemetry::EventID::EventCount);
  for (uint32_t i = 0; i < eventCount; ++i) {
    const EventInfo& info = gEventInfo[i];
    uint32_t eventId = i;

    // If this event is expired or not recorded in this process, mark it with
    // a special event id.
    // This avoids doing repeated checks at runtime.
    if (IsExpiredVersion(info.common_info.expiration_version().get())) {
      eventId = kExpiredEventId;
    }

    gEventNameIDMap.Put(UniqueEventName(info), new EventKey{eventId, false});
    gCategoryNames.PutEntry(info.common_info.category());
  }

  gInitDone = true;
}

void TelemetryEvent::DeInitializeGlobalState() {
  StaticMutexAutoLock locker(gTelemetryEventsMutex);
  MOZ_ASSERT(gInitDone);

  gCanRecordBase = false;
  gCanRecordExtended = false;

  gEventNameIDMap.Clear();
  gCategoryNames.Clear();
  gEnabledCategories.Clear();
  gEventRecords.Clear();

  gDynamicEventInfo = nullptr;

  gInitDone = false;
}

void TelemetryEvent::SetCanRecordBase(bool b) {
  StaticMutexAutoLock locker(gTelemetryEventsMutex);
  gCanRecordBase = b;
}

void TelemetryEvent::SetCanRecordExtended(bool b) {
  StaticMutexAutoLock locker(gTelemetryEventsMutex);
  gCanRecordExtended = b;
}

nsresult TelemetryEvent::RecordChildEvents(
    ProcessID aProcessType,
    const nsTArray<mozilla::Telemetry::ChildEventData>& aEvents) {
  MOZ_ASSERT(XRE_IsParentProcess());
  StaticMutexAutoLock locker(gTelemetryEventsMutex);
  for (uint32_t i = 0; i < aEvents.Length(); ++i) {
    const mozilla::Telemetry::ChildEventData& e = aEvents[i];

    // Timestamps from child processes are absolute. We fix them up here to be
    // relative to the main process start time.
    // This allows us to put events from all processes on the same timeline.
    double relativeTimestamp =
        (e.timestamp - TimeStamp::ProcessCreation()).ToMilliseconds();

    ::RecordEvent(locker, aProcessType, relativeTimestamp, e.category, e.method,
                  e.object, e.value, e.extra);
  }
  return NS_OK;
}

nsresult TelemetryEvent::RecordEvent(const nsACString& aCategory,
                                     const nsACString& aMethod,
                                     const nsACString& aObject,
                                     JS::HandleValue aValue,
                                     JS::HandleValue aExtra, JSContext* cx,
                                     uint8_t optional_argc) {
  // Check value argument.
  if ((optional_argc > 0) && !aValue.isNull() && !aValue.isString()) {
    LogToBrowserConsole(nsIScriptError::warningFlag,
                        NS_LITERAL_STRING("Invalid type for value parameter."));
    mozilla::Telemetry::AccumulateCategorical(
        LABELS_TELEMETRY_EVENT_RECORDING_ERROR::Value);
    return NS_OK;
  }

  // Extract value parameter.
  Maybe<nsCString> value;
  if (aValue.isString()) {
    nsAutoJSString jsStr;
    if (!jsStr.init(cx, aValue)) {
      LogToBrowserConsole(
          nsIScriptError::warningFlag,
          NS_LITERAL_STRING("Invalid string value for value parameter."));
      mozilla::Telemetry::AccumulateCategorical(
          LABELS_TELEMETRY_EVENT_RECORDING_ERROR::Value);
      return NS_OK;
    }

    nsCString str = NS_ConvertUTF16toUTF8(jsStr);
    if (str.Length() > kMaxValueByteLength) {
      LogToBrowserConsole(
          nsIScriptError::warningFlag,
          NS_LITERAL_STRING(
              "Value parameter exceeds maximum string length, truncating."));
      TruncateToByteLength(str, kMaxValueByteLength);
    }
    value = mozilla::Some(str);
  }

  // Check extra argument.
  if ((optional_argc > 1) && !aExtra.isNull() && !aExtra.isObject()) {
    LogToBrowserConsole(nsIScriptError::warningFlag,
                        NS_LITERAL_STRING("Invalid type for extra parameter."));
    mozilla::Telemetry::AccumulateCategorical(
        LABELS_TELEMETRY_EVENT_RECORDING_ERROR::Extra);
    return NS_OK;
  }

  // Extract extra dictionary.
  ExtraArray extra;
  if (aExtra.isObject()) {
    JS::RootedObject obj(cx, &aExtra.toObject());
    JS::Rooted<JS::IdVector> ids(cx, JS::IdVector(cx));
    if (!JS_Enumerate(cx, obj, &ids)) {
      LogToBrowserConsole(nsIScriptError::warningFlag,
                          NS_LITERAL_STRING("Failed to enumerate object."));
      mozilla::Telemetry::AccumulateCategorical(
          LABELS_TELEMETRY_EVENT_RECORDING_ERROR::Extra);
      return NS_OK;
    }

    for (size_t i = 0, n = ids.length(); i < n; i++) {
      nsAutoJSString key;
      if (!key.init(cx, ids[i])) {
        LogToBrowserConsole(
            nsIScriptError::warningFlag,
            NS_LITERAL_STRING(
                "Extra dictionary should only contain string keys."));
        mozilla::Telemetry::AccumulateCategorical(
            LABELS_TELEMETRY_EVENT_RECORDING_ERROR::Extra);
        return NS_OK;
      }

      JS::Rooted<JS::Value> value(cx);
      if (!JS_GetPropertyById(cx, obj, ids[i], &value)) {
        LogToBrowserConsole(nsIScriptError::warningFlag,
                            NS_LITERAL_STRING("Failed to get extra property."));
        mozilla::Telemetry::AccumulateCategorical(
            LABELS_TELEMETRY_EVENT_RECORDING_ERROR::Extra);
        return NS_OK;
      }

      nsAutoJSString jsStr;
      if (!value.isString() || !jsStr.init(cx, value)) {
        LogToBrowserConsole(
            nsIScriptError::warningFlag,
            NS_LITERAL_STRING("Extra properties should have string values."));
        mozilla::Telemetry::AccumulateCategorical(
            LABELS_TELEMETRY_EVENT_RECORDING_ERROR::Extra);
        return NS_OK;
      }

      nsCString str = NS_ConvertUTF16toUTF8(jsStr);
      if (str.Length() > kMaxExtraValueByteLength) {
        LogToBrowserConsole(
            nsIScriptError::warningFlag,
            NS_LITERAL_STRING(
                "Extra value exceeds maximum string length, truncating."));
        TruncateToByteLength(str, kMaxExtraValueByteLength);
      }

      extra.AppendElement(EventExtraEntry{NS_ConvertUTF16toUTF8(key), str});
    }
  }

  // Lock for accessing internal data.
  // While the lock is being held, no complex calls like JS calls can be made,
  // as all of these could record Telemetry, which would result in deadlock.
  RecordEventResult res;
  if (!XRE_IsParentProcess()) {
    {
      StaticMutexAutoLock lock(gTelemetryEventsMutex);
      res = ::ShouldRecordChildEvent(lock, aCategory, aMethod, aObject);
    }

    if (res == RecordEventResult::Ok) {
      TelemetryIPCAccumulator::RecordChildEvent(
          TimeStamp::NowLoRes(), aCategory, aMethod, aObject, value, extra);
    }
  } else {
    StaticMutexAutoLock lock(gTelemetryEventsMutex);

    if (!gInitDone) {
      return NS_ERROR_FAILURE;
    }

    // Get the current time.
    double timestamp = -1;
    if (NS_WARN_IF(NS_FAILED(MsSinceProcessStart(&timestamp)))) {
      return NS_ERROR_FAILURE;
    }

    res = ::RecordEvent(lock, ProcessID::Parent, timestamp, aCategory, aMethod,
                        aObject, value, extra);
  }

  // Trigger warnings or errors where needed.
  switch (res) {
    case RecordEventResult::UnknownEvent: {
      JS_ReportErrorASCII(cx, R"(Unknown event: ["%s", "%s", "%s"])",
                          PromiseFlatCString(aCategory).get(),
                          PromiseFlatCString(aMethod).get(),
                          PromiseFlatCString(aObject).get());
      return NS_OK;
    }
    case RecordEventResult::InvalidExtraKey: {
      nsPrintfCString msg(R"(Invalid extra key for event ["%s", "%s", "%s"].)",
                          PromiseFlatCString(aCategory).get(),
                          PromiseFlatCString(aMethod).get(),
                          PromiseFlatCString(aObject).get());
      LogToBrowserConsole(nsIScriptError::warningFlag,
                          NS_ConvertUTF8toUTF16(msg));
      return NS_OK;
    }
    case RecordEventResult::StorageLimitReached: {
      LogToBrowserConsole(nsIScriptError::warningFlag,
                          NS_LITERAL_STRING("Event storage limit reached."));
      nsCOMPtr<nsIObserverService> serv =
          mozilla::services::GetObserverService();
      if (serv) {
        serv->NotifyObservers(nullptr, "event-telemetry-storage-limit-reached",
                              nullptr);
      }
      return NS_OK;
    }
    default:
      return NS_OK;
  }
}

void TelemetryEvent::RecordEventNative(
    mozilla::Telemetry::EventID aId, const mozilla::Maybe<nsCString>& aValue,
    const mozilla::Maybe<ExtraArray>& aExtra) {
  // Truncate aValue if present and necessary.
  mozilla::Maybe<nsCString> value;
  if (aValue) {
    nsCString valueStr = aValue.ref();
    if (valueStr.Length() > kMaxValueByteLength) {
      TruncateToByteLength(valueStr, kMaxValueByteLength);
    }
    value = mozilla::Some(valueStr);
  }

  // Truncate any over-long extra values.
  ExtraArray extra;
  if (aExtra) {
    extra = aExtra.ref();
    for (auto& item : extra) {
      if (item.value.Length() > kMaxExtraValueByteLength) {
        TruncateToByteLength(item.value, kMaxExtraValueByteLength);
      }
    }
  }

  const EventInfo& info = gEventInfo[static_cast<uint32_t>(aId)];
  const nsCString category(info.common_info.category());
  const nsCString method(info.method());
  const nsCString object(info.object());
  if (!XRE_IsParentProcess()) {
    RecordEventResult res;
    {
      StaticMutexAutoLock lock(gTelemetryEventsMutex);
      res = ::ShouldRecordChildEvent(lock, category, method, object);
    }

    if (res == RecordEventResult::Ok) {
      TelemetryIPCAccumulator::RecordChildEvent(TimeStamp::NowLoRes(), category,
                                                method, object, value, extra);
    }
  } else {
    StaticMutexAutoLock lock(gTelemetryEventsMutex);

    if (!gInitDone) {
      return;
    }

    // Get the current time.
    double timestamp = -1;
    if (NS_WARN_IF(NS_FAILED(MsSinceProcessStart(&timestamp)))) {
      return;
    }

    ::RecordEvent(lock, ProcessID::Parent, timestamp, category, method, object,
                  value, extra);
  }
}

static bool GetArrayPropertyValues(JSContext* cx, JS::HandleObject obj,
                                   const char* property,
                                   nsTArray<nsCString>* results) {
  JS::RootedValue value(cx);
  if (!JS_GetProperty(cx, obj, property, &value)) {
    JS_ReportErrorASCII(cx, R"(Missing required property "%s" for event)",
                        property);
    return false;
  }

  bool isArray = false;
  if (!JS_IsArrayObject(cx, value, &isArray) || !isArray) {
    JS_ReportErrorASCII(cx, R"(Property "%s" for event should be an array)",
                        property);
    return false;
  }

  JS::RootedObject arrayObj(cx, &value.toObject());
  uint32_t arrayLength;
  if (!JS_GetArrayLength(cx, arrayObj, &arrayLength)) {
    return false;
  }

  for (uint32_t arrayIdx = 0; arrayIdx < arrayLength; ++arrayIdx) {
    JS::Rooted<JS::Value> element(cx);
    if (!JS_GetElement(cx, arrayObj, arrayIdx, &element)) {
      return false;
    }

    if (!element.isString()) {
      JS_ReportErrorASCII(
          cx, R"(Array entries for event property "%s" should be strings)",
          property);
      return false;
    }

    nsAutoJSString jsStr;
    if (!jsStr.init(cx, element)) {
      return false;
    }

    results->AppendElement(NS_ConvertUTF16toUTF8(jsStr));
  }

  return true;
}

nsresult TelemetryEvent::RegisterEvents(const nsACString& aCategory,
                                        JS::Handle<JS::Value> aEventData,
                                        bool aBuiltin, JSContext* cx) {
  MOZ_ASSERT(XRE_IsParentProcess(),
             "Events can only be registered in the parent process");

  if (!IsValidIdentifierString(aCategory, 30, true, true)) {
    JS_ReportErrorASCII(
        cx, "Category parameter should match the identifier pattern.");
    mozilla::Telemetry::AccumulateCategorical(
        LABELS_TELEMETRY_EVENT_REGISTRATION_ERROR::Category);
    return NS_ERROR_INVALID_ARG;
  }

  if (!aEventData.isObject()) {
    JS_ReportErrorASCII(cx, "Event data parameter should be an object");
    mozilla::Telemetry::AccumulateCategorical(
        LABELS_TELEMETRY_EVENT_REGISTRATION_ERROR::Other);
    return NS_ERROR_INVALID_ARG;
  }

  JS::RootedObject obj(cx, &aEventData.toObject());
  JS::Rooted<JS::IdVector> eventPropertyIds(cx, JS::IdVector(cx));
  if (!JS_Enumerate(cx, obj, &eventPropertyIds)) {
    mozilla::Telemetry::AccumulateCategorical(
        LABELS_TELEMETRY_EVENT_REGISTRATION_ERROR::Other);
    return NS_ERROR_FAILURE;
  }

  // Collect the event data into local storage first.
  // Only after successfully validating all contained events will we register
  // them into global storage.
  nsTArray<DynamicEventInfo> newEventInfos;
  nsTArray<bool> newEventExpired;

  for (size_t i = 0, n = eventPropertyIds.length(); i < n; i++) {
    nsAutoJSString eventName;
    if (!eventName.init(cx, eventPropertyIds[i])) {
      mozilla::Telemetry::AccumulateCategorical(
          LABELS_TELEMETRY_EVENT_REGISTRATION_ERROR::Other);
      return NS_ERROR_FAILURE;
    }

    if (!IsValidIdentifierString(NS_ConvertUTF16toUTF8(eventName),
                                 kMaxMethodNameByteLength, false, true)) {
      JS_ReportErrorASCII(cx,
                          "Event names should match the identifier pattern.");
      mozilla::Telemetry::AccumulateCategorical(
          LABELS_TELEMETRY_EVENT_REGISTRATION_ERROR::Name);
      return NS_ERROR_INVALID_ARG;
    }

    JS::RootedValue value(cx);
    if (!JS_GetPropertyById(cx, obj, eventPropertyIds[i], &value) ||
        !value.isObject()) {
      mozilla::Telemetry::AccumulateCategorical(
          LABELS_TELEMETRY_EVENT_REGISTRATION_ERROR::Other);
      return NS_ERROR_FAILURE;
    }
    JS::RootedObject eventObj(cx, &value.toObject());

    // Extract the event registration data.
    nsTArray<nsCString> methods;
    nsTArray<nsCString> objects;
    nsTArray<nsCString> extra_keys;
    bool expired = false;
    bool recordOnRelease = false;

    // The methods & objects properties are required.
    if (!GetArrayPropertyValues(cx, eventObj, "methods", &methods)) {
      mozilla::Telemetry::AccumulateCategorical(
          LABELS_TELEMETRY_EVENT_REGISTRATION_ERROR::Other);
      return NS_ERROR_FAILURE;
    }

    if (!GetArrayPropertyValues(cx, eventObj, "objects", &objects)) {
      mozilla::Telemetry::AccumulateCategorical(
          LABELS_TELEMETRY_EVENT_REGISTRATION_ERROR::Other);
      return NS_ERROR_FAILURE;
    }

    // extra_keys is optional.
    bool hasProperty = false;
    if (JS_HasProperty(cx, eventObj, "extra_keys", &hasProperty) &&
        hasProperty) {
      if (!GetArrayPropertyValues(cx, eventObj, "extra_keys", &extra_keys)) {
        mozilla::Telemetry::AccumulateCategorical(
            LABELS_TELEMETRY_EVENT_REGISTRATION_ERROR::Other);
        return NS_ERROR_FAILURE;
      }
    }

    // expired is optional.
    if (JS_HasProperty(cx, eventObj, "expired", &hasProperty) && hasProperty) {
      JS::RootedValue temp(cx);
      if (!JS_GetProperty(cx, eventObj, "expired", &temp) ||
          !temp.isBoolean()) {
        mozilla::Telemetry::AccumulateCategorical(
            LABELS_TELEMETRY_EVENT_REGISTRATION_ERROR::Other);
        return NS_ERROR_FAILURE;
      }

      expired = temp.toBoolean();
    }

    // record_on_release is optional.
    if (JS_HasProperty(cx, eventObj, "record_on_release", &hasProperty) &&
        hasProperty) {
      JS::RootedValue temp(cx);
      if (!JS_GetProperty(cx, eventObj, "record_on_release", &temp) ||
          !temp.isBoolean()) {
        mozilla::Telemetry::AccumulateCategorical(
            LABELS_TELEMETRY_EVENT_REGISTRATION_ERROR::Other);
        return NS_ERROR_FAILURE;
      }

      recordOnRelease = temp.toBoolean();
    }

    // Validate methods.
    for (auto& method : methods) {
      if (!IsValidIdentifierString(method, kMaxMethodNameByteLength, false,
                                   true)) {
        JS_ReportErrorASCII(
            cx, "Method names should match the identifier pattern.");
        mozilla::Telemetry::AccumulateCategorical(
            LABELS_TELEMETRY_EVENT_REGISTRATION_ERROR::Method);
        return NS_ERROR_INVALID_ARG;
      }
    }

    // Validate objects.
    for (auto& object : objects) {
      if (!IsValidIdentifierString(object, kMaxObjectNameByteLength, false,
                                   true)) {
        JS_ReportErrorASCII(
            cx, "Object names should match the identifier pattern.");
        mozilla::Telemetry::AccumulateCategorical(
            LABELS_TELEMETRY_EVENT_REGISTRATION_ERROR::Object);
        return NS_ERROR_INVALID_ARG;
      }
    }

    // Validate extra keys.
    if (extra_keys.Length() > kMaxExtraKeyCount) {
      JS_ReportErrorASCII(cx, "No more than 10 extra keys can be registered.");
      mozilla::Telemetry::AccumulateCategorical(
          LABELS_TELEMETRY_EVENT_REGISTRATION_ERROR::ExtraKeys);
      return NS_ERROR_INVALID_ARG;
    }
    for (auto& key : extra_keys) {
      if (!IsValidIdentifierString(key, kMaxExtraKeyNameByteLength, false,
                                   true)) {
        JS_ReportErrorASCII(
            cx, "Extra key names should match the identifier pattern.");
        mozilla::Telemetry::AccumulateCategorical(
            LABELS_TELEMETRY_EVENT_REGISTRATION_ERROR::ExtraKeys);
        return NS_ERROR_INVALID_ARG;
      }
    }

    // Append event infos to be registered.
    for (auto& method : methods) {
      for (auto& object : objects) {
        // We defer the actual registration here in case any other event
        // description is invalid. In that case we don't need to roll back any
        // partial registration.
        DynamicEventInfo info{aCategory,  method,          object,
                              extra_keys, recordOnRelease, aBuiltin};
        newEventInfos.AppendElement(info);
        newEventExpired.AppendElement(expired);
      }
    }
  }

  {
    StaticMutexAutoLock locker(gTelemetryEventsMutex);
    RegisterEvents(locker, aCategory, newEventInfos, newEventExpired, aBuiltin);
  }

  return NS_OK;
}

nsresult TelemetryEvent::CreateSnapshots(uint32_t aDataset, bool aClear,
                                         uint32_t aEventLimit, JSContext* cx,
                                         uint8_t optional_argc,
                                         JS::MutableHandleValue aResult) {
  if (!XRE_IsParentProcess()) {
    return NS_ERROR_FAILURE;
  }

  // Creating a JS snapshot of the events is a two-step process:
  // (1) Lock the storage and copy the events into function-local storage.
  // (2) Serialize the events into JS.
  // We can't hold a lock for (2) because we will run into deadlocks otherwise
  // from JS recording Telemetry.

  // (1) Extract the events from storage with a lock held.
  nsTArray<mozilla::Pair<const char*, EventRecordArray>> processEvents;
  nsTArray<mozilla::Pair<uint32_t, EventRecordArray>> leftovers;
  {
    StaticMutexAutoLock locker(gTelemetryEventsMutex);

    if (!gInitDone) {
      return NS_ERROR_FAILURE;
    }

    // The snapshotting function is the same for both static and dynamic builtin
    // events. We can use the same function and store the events in the same
    // output storage.
    auto snapshotter = [aDataset, &locker, &processEvents, &leftovers, aClear,
                        optional_argc,
                        aEventLimit](EventRecordsMapType& aProcessStorage) {
      for (auto iter = aProcessStorage.Iter(); !iter.Done(); iter.Next()) {
        const EventRecordArray* eventStorage =
            static_cast<EventRecordArray*>(iter.Data());
        EventRecordArray events;
        EventRecordArray leftoverEvents;

        const uint32_t len = eventStorage->Length();
        for (uint32_t i = 0; i < len; ++i) {
          const EventRecord& record = (*eventStorage)[i];
          if (IsInDataset(GetDataset(locker, record.GetEventKey()), aDataset)) {
            // If we have a limit, adhere to it. If we have a limit and are
            // going to clear, save the leftovers for later.
            if (optional_argc < 2 || events.Length() < aEventLimit) {
              events.AppendElement(record);
            } else if (aClear) {
              leftoverEvents.AppendElement(record);
            }
          }
        }

        if (events.Length()) {
          const char* processName = GetNameForProcessID(ProcessID(iter.Key()));
          processEvents.AppendElement(
              mozilla::MakePair(processName, std::move(events)));
          if (leftoverEvents.Length()) {
            leftovers.AppendElement(
                mozilla::MakePair(iter.Key(), std::move(leftoverEvents)));
          }
        }
      }
    };

    // Take a snapshot of the plain and dynamic builtin events.
    snapshotter(gEventRecords);
    if (aClear) {
      gEventRecords.Clear();
      for (auto pair : leftovers) {
        gEventRecords.Put(pair.first(),
                          new EventRecordArray(std::move(pair.second())));
      }
      leftovers.Clear();
    }
  }

  // (2) Serialize the events to a JS object.
  JS::RootedObject rootObj(cx, JS_NewPlainObject(cx));
  if (!rootObj) {
    return NS_ERROR_FAILURE;
  }

  const uint32_t processLength = processEvents.Length();
  for (uint32_t i = 0; i < processLength; ++i) {
    JS::RootedObject eventsArray(cx);
    if (NS_FAILED(SerializeEventsArray(processEvents[i].second(), cx,
                                       &eventsArray, aDataset))) {
      return NS_ERROR_FAILURE;
    }

    if (!JS_DefineProperty(cx, rootObj, processEvents[i].first(), eventsArray,
                           JSPROP_ENUMERATE)) {
      return NS_ERROR_FAILURE;
    }
  }

  aResult.setObject(*rootObj);
  return NS_OK;
}

/**
 * Resets all the stored events. This is intended to be only used in tests.
 */
void TelemetryEvent::ClearEvents() {
  StaticMutexAutoLock lock(gTelemetryEventsMutex);

  if (!gInitDone) {
    return;
  }

  gEventRecords.Clear();
}

void TelemetryEvent::SetEventRecordingEnabled(const nsACString& category,
                                              bool enabled) {
  StaticMutexAutoLock locker(gTelemetryEventsMutex);

  if (!gCategoryNames.Contains(category)) {
    LogToBrowserConsole(
        nsIScriptError::warningFlag,
        NS_ConvertUTF8toUTF16(
            NS_LITERAL_CSTRING(
                "Unknown category for SetEventRecordingEnabled: ") +
            category));
    return;
  }

  if (enabled) {
    gEnabledCategories.PutEntry(category);
  } else {
    gEnabledCategories.RemoveEntry(category);
  }
}

size_t TelemetryEvent::SizeOfIncludingThis(
    mozilla::MallocSizeOf aMallocSizeOf) {
  StaticMutexAutoLock locker(gTelemetryEventsMutex);
  size_t n = 0;

  auto getSizeOfRecords = [aMallocSizeOf](auto& storageMap) {
    size_t partial = storageMap.ShallowSizeOfExcludingThis(aMallocSizeOf);
    for (auto iter = storageMap.Iter(); !iter.Done(); iter.Next()) {
      EventRecordArray* eventRecords =
          static_cast<EventRecordArray*>(iter.Data());
      partial += eventRecords->ShallowSizeOfIncludingThis(aMallocSizeOf);

      const uint32_t len = eventRecords->Length();
      for (uint32_t i = 0; i < len; ++i) {
        partial += (*eventRecords)[i].SizeOfExcludingThis(aMallocSizeOf);
      }
    }
    return partial;
  };

  n += getSizeOfRecords(gEventRecords);

  n += gEventNameIDMap.ShallowSizeOfExcludingThis(aMallocSizeOf);
  for (auto iter = gEventNameIDMap.ConstIter(); !iter.Done(); iter.Next()) {
    n += iter.Key().SizeOfExcludingThisIfUnshared(aMallocSizeOf);
  }

  n += gCategoryNames.ShallowSizeOfExcludingThis(aMallocSizeOf);
  n += gEnabledCategories.ShallowSizeOfExcludingThis(aMallocSizeOf);

  if (gDynamicEventInfo) {
    n += gDynamicEventInfo->ShallowSizeOfIncludingThis(aMallocSizeOf);
    for (auto& info : *gDynamicEventInfo) {
      n += info.SizeOfExcludingThis(aMallocSizeOf);
    }
  }

  return n;
}
