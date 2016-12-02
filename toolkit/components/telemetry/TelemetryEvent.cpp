/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <prtime.h>
#include "nsITelemetry.h"
#include "nsHashKeys.h"
#include "nsDataHashtable.h"
#include "nsClassHashtable.h"
#include "nsTArray.h"
#include "mozilla/StaticMutex.h"
#include "mozilla/Unused.h"
#include "mozilla/Maybe.h"
#include "mozilla/StaticPtr.h"
#include "jsapi.h"
#include "nsJSUtils.h"
#include "nsXULAppAPI.h"
#include "nsUTF8Utils.h"

#include "TelemetryCommon.h"
#include "TelemetryEvent.h"
#include "TelemetryEventData.h"

using mozilla::StaticMutex;
using mozilla::StaticMutexAutoLock;
using mozilla::ArrayLength;
using mozilla::Maybe;
using mozilla::Nothing;
using mozilla::Pair;
using mozilla::StaticAutoPtr;
using mozilla::Telemetry::Common::AutoHashtable;
using mozilla::Telemetry::Common::IsExpiredVersion;
using mozilla::Telemetry::Common::CanRecordDataset;
using mozilla::Telemetry::Common::IsInDataset;
using mozilla::Telemetry::Common::MsSinceProcessStart;
using mozilla::Telemetry::Common::LogToBrowserConsole;

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

const uint32_t kEventCount = mozilla::Telemetry::EventID::EventCount;
// This is a special event id used to mark expired events, to make expiry checks
// faster at runtime.
const uint32_t kExpiredEventId = kEventCount + 1;
static_assert(kEventCount < kExpiredEventId, "Should not overflow.");

// This is the hard upper limit on the number of event records we keep in storage.
// If we cross this limit, we will drop any further event recording until elements
// are removed from storage.
const uint32_t kMaxEventRecords = 1000;
// Maximum length of any passed value string, in UTF8 byte sequence length.
const uint32_t kMaxValueByteLength = 80;
// Maximum length of any string value in the extra dictionary, in UTF8 byte sequence length.
const uint32_t kMaxExtraValueByteLength = 80;

typedef nsDataHashtable<nsCStringHashKey, uint32_t> EventMapType;
typedef nsClassHashtable<nsCStringHashKey, nsCString> StringMap;

enum class RecordEventResult {
  Ok,
  UnknownEvent,
  InvalidExtraKey,
  StorageLimitReached,
};

struct ExtraEntry {
  const nsCString key;
  const nsCString value;
};

typedef nsTArray<ExtraEntry> ExtraArray;

class EventRecord {
public:
  EventRecord(double timestamp, uint32_t eventId, const Maybe<nsCString>& value,
              const ExtraArray& extra)
    : mTimestamp(timestamp)
    , mEventId(eventId)
    , mValue(value)
    , mExtra(extra)
  {}

  EventRecord(const EventRecord& other)
    : mTimestamp(other.mTimestamp)
    , mEventId(other.mEventId)
    , mValue(other.mValue)
    , mExtra(other.mExtra)
  {}

  EventRecord& operator=(const EventRecord& other) = delete;

  double Timestamp() const { return mTimestamp; }
  uint32_t EventId() const { return mEventId; }
  const Maybe<nsCString>& Value() const { return mValue; }
  const ExtraArray& Extra() const { return mExtra; }

  size_t SizeOfExcludingThis(mozilla::MallocSizeOf aMallocSizeOf) const;

private:
  const double mTimestamp;
  const uint32_t mEventId;
  const Maybe<nsCString> mValue;
  const ExtraArray mExtra;
};

// Implements the methods for EventInfo.
const char*
EventInfo::method() const
{
  return &gEventsStringTable[this->method_offset];
}

const char*
EventInfo::object() const
{
  return &gEventsStringTable[this->object_offset];
}

// Implements the methods for CommonEventInfo.
const char*
CommonEventInfo::category() const
{
  return &gEventsStringTable[this->category_offset];
}

const char*
CommonEventInfo::expiration_version() const
{
  return &gEventsStringTable[this->expiration_version_offset];
}

const char*
CommonEventInfo::extra_key(uint32_t index) const
{
  MOZ_ASSERT(index < this->extra_count);
  uint32_t key_index = gExtraKeysTable[this->extra_index + index];
  return &gEventsStringTable[key_index];
}

// Implementation for the EventRecord class.
size_t
EventRecord::SizeOfExcludingThis(mozilla::MallocSizeOf aMallocSizeOf) const
{
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

nsCString
UniqueEventName(const nsACString& category, const nsACString& method, const nsACString& object)
{
  nsCString name;
  name.Append(category);
  name.AppendLiteral("#");
  name.Append(method);
  name.AppendLiteral("#");
  name.Append(object);
  return name;
}

nsCString
UniqueEventName(const EventInfo& info)
{
  return UniqueEventName(nsDependentCString(info.common_info.category()),
                         nsDependentCString(info.method()),
                         nsDependentCString(info.object()));
}

bool
IsExpiredDate(uint32_t expires_days_since_epoch) {
  if (expires_days_since_epoch == 0) {
    return false;
  }

  const uint32_t days_since_epoch = PR_Now() / (PRTime(PR_USEC_PER_SEC) * 24 * 60 * 60);
  return expires_days_since_epoch <= days_since_epoch;
}

void
TruncateToByteLength(nsCString& str, uint32_t length)
{
  // last will be the index of the first byte of the current multi-byte sequence.
  uint32_t last = RewindToPriorUTF8Codepoint(str.get(), length);
  str.Truncate(last);
}

} // anonymous namespace

////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////
//
// PRIVATE STATE, SHARED BY ALL THREADS

namespace {

// Set to true once this global state has been initialized.
bool gInitDone = false;

bool gCanRecordBase;
bool gCanRecordExtended;

// The Name -> ID cache map.
EventMapType gEventNameIDMap(kEventCount);

// The main event storage. Events are inserted here in recording order.
StaticAutoPtr<nsTArray<EventRecord>> gEventRecords;

} // namespace

////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////
//
// PRIVATE: thread-unsafe helpers for event recording.

namespace {

bool
CanRecordEvent(const StaticMutexAutoLock& lock, const CommonEventInfo& info)
{
  if (!gCanRecordBase) {
    return false;
  }

  return CanRecordDataset(info.dataset, gCanRecordBase, gCanRecordExtended);
}

RecordEventResult
RecordEvent(const StaticMutexAutoLock& lock, double timestamp,
            const nsACString& category, const nsACString& method,
            const nsACString& object, const Maybe<nsCString>& value,
            const ExtraArray& extra)
{
  // Apply hard limit on event count in storage.
  if (gEventRecords->Length() >= kMaxEventRecords) {
    return RecordEventResult::StorageLimitReached;
  }

  // Look up the event id.
  const nsCString& name = UniqueEventName(category, method, object);
  uint32_t eventId;
  if (!gEventNameIDMap.Get(name, &eventId)) {
    return RecordEventResult::UnknownEvent;
  }

  // If the event is expired, silently drop this call.
  // We don't want recording for expired probes to be an error so code doesn't
  // have to be removed at a specific time or version.
  // Even logging warnings would become very noisy.
  if (eventId == kExpiredEventId) {
    return RecordEventResult::Ok;
  }

  // Check whether we can record this event.
  const CommonEventInfo& common = gEventInfo[eventId].common_info;
  if (!CanRecordEvent(lock, common)) {
    return RecordEventResult::Ok;
  }

  // Check whether the extra keys passed are valid.
  nsTHashtable<nsCStringHashKey> validExtraKeys;
  for (uint32_t i = 0; i < common.extra_count; ++i) {
    validExtraKeys.PutEntry(nsDependentCString(common.extra_key(i)));
  }

  for (uint32_t i = 0; i < extra.Length(); ++i) {
    if (!validExtraKeys.GetEntry(extra[i].key)) {
      return RecordEventResult::InvalidExtraKey;
    }
  }

  // Add event record.
  gEventRecords->AppendElement(EventRecord(timestamp, eventId, value, extra));
  return RecordEventResult::Ok;
}

} // anonymous namespace

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

void
TelemetryEvent::InitializeGlobalState(bool aCanRecordBase, bool aCanRecordExtended)
{
  StaticMutexAutoLock locker(gTelemetryEventsMutex);
  MOZ_ASSERT(!gInitDone, "TelemetryEvent::InitializeGlobalState "
             "may only be called once");

  gCanRecordBase = aCanRecordBase;
  gCanRecordExtended = aCanRecordExtended;

  gEventRecords = new nsTArray<EventRecord>();

  // Populate the static event name->id cache. Note that the event names are
  // statically allocated and come from the automatically generated TelemetryEventData.h.
  const uint32_t eventCount = static_cast<uint32_t>(mozilla::Telemetry::EventID::EventCount);
  for (uint32_t i = 0; i < eventCount; ++i) {
    const EventInfo& info = gEventInfo[i];
    uint32_t eventId = i;

    // If this event is expired, mark it with a special event id.
    // This avoids doing expensive expiry checks at runtime.
    if (IsExpiredVersion(info.common_info.expiration_version()) ||
        IsExpiredDate(info.common_info.expiration_day)) {
      eventId = kExpiredEventId;
    }

    gEventNameIDMap.Put(UniqueEventName(info), eventId);
  }

#ifdef DEBUG
  gEventNameIDMap.MarkImmutable();
#endif
  gInitDone = true;
}

void
TelemetryEvent::DeInitializeGlobalState()
{
  StaticMutexAutoLock locker(gTelemetryEventsMutex);
  MOZ_ASSERT(gInitDone);

  gCanRecordBase = false;
  gCanRecordExtended = false;

  gEventNameIDMap.Clear();
  gEventRecords->Clear();
  gEventRecords = nullptr;

  gInitDone = false;
}

void
TelemetryEvent::SetCanRecordBase(bool b)
{
  StaticMutexAutoLock locker(gTelemetryEventsMutex);
  gCanRecordBase = b;
}

void
TelemetryEvent::SetCanRecordExtended(bool b) {
  StaticMutexAutoLock locker(gTelemetryEventsMutex);
  gCanRecordExtended = b;
}

nsresult
TelemetryEvent::RecordEvent(const nsACString& aCategory, const nsACString& aMethod,
                            const nsACString& aObject, JS::HandleValue aValue,
                            JS::HandleValue aExtra, JSContext* cx,
                            uint8_t optional_argc)
{
  // Currently only recording in the parent process is supported.
  if (!XRE_IsParentProcess()) {
    return NS_OK;
  }

  // Get the current time.
  double timestamp = -1;
  nsresult rv = MsSinceProcessStart(&timestamp);
  if (NS_FAILED(rv)) {
    LogToBrowserConsole(nsIScriptError::warningFlag,
                        NS_LITERAL_STRING("Failed to get time since process start."));
    return NS_OK;
  }

  // Check value argument.
  if ((optional_argc > 0) && !aValue.isNull() && !aValue.isString()) {
    LogToBrowserConsole(nsIScriptError::warningFlag,
                        NS_LITERAL_STRING("Invalid type for value parameter."));
    return NS_OK;
  }

  // Extract value parameter.
  Maybe<nsCString> value;
  if (aValue.isString()) {
    nsAutoJSString jsStr;
    if (!jsStr.init(cx, aValue)) {
      LogToBrowserConsole(nsIScriptError::warningFlag,
                          NS_LITERAL_STRING("Invalid string value for value parameter."));
      return NS_OK;
    }

    nsCString str = NS_ConvertUTF16toUTF8(jsStr);
    if (str.Length() > kMaxValueByteLength) {
      LogToBrowserConsole(nsIScriptError::warningFlag,
                          NS_LITERAL_STRING("Value parameter exceeds maximum string length, truncating."));
      TruncateToByteLength(str, kMaxValueByteLength);
    }
    value = mozilla::Some(str);
  }

  // Check extra argument.
  if ((optional_argc > 1) && !aExtra.isNull() && !aExtra.isObject()) {
    LogToBrowserConsole(nsIScriptError::warningFlag,
                        NS_LITERAL_STRING("Invalid type for extra parameter."));
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
      return NS_OK;
    }

    for (size_t i = 0, n = ids.length(); i < n; i++) {
      nsAutoJSString key;
      if (!key.init(cx, ids[i])) {
        LogToBrowserConsole(nsIScriptError::warningFlag,
                            NS_LITERAL_STRING("Extra dictionary should only contain string keys."));
        return NS_OK;
      }

      JS::Rooted<JS::Value> value(cx);
      if (!JS_GetPropertyById(cx, obj, ids[i], &value)) {
        LogToBrowserConsole(nsIScriptError::warningFlag,
                            NS_LITERAL_STRING("Failed to get extra property."));
        return NS_OK;
      }

      nsAutoJSString jsStr;
      if (!value.isString() || !jsStr.init(cx, value)) {
        LogToBrowserConsole(nsIScriptError::warningFlag,
                            NS_LITERAL_STRING("Extra properties should have string values."));
        return NS_OK;
      }

      nsCString str = NS_ConvertUTF16toUTF8(jsStr);
      if (str.Length() > kMaxExtraValueByteLength) {
        LogToBrowserConsole(nsIScriptError::warningFlag,
                            NS_LITERAL_STRING("Extra value exceeds maximum string length, truncating."));
        TruncateToByteLength(str, kMaxExtraValueByteLength);
      }

      extra.AppendElement(ExtraEntry{NS_ConvertUTF16toUTF8(key), str});
    }
  }

  // Lock for accessing internal data.
  // While the lock is being held, no complex calls like JS calls can be made,
  // as all of these could record Telemetry, which would result in deadlock.
  RecordEventResult res;
  {
    StaticMutexAutoLock lock(gTelemetryEventsMutex);

    if (!gInitDone) {
      return NS_ERROR_FAILURE;
    }

    res = ::RecordEvent(lock, timestamp, aCategory, aMethod, aObject, value, extra);
  }

  // Trigger warnings or errors where needed.
  switch (res) {
    case RecordEventResult::UnknownEvent:
      JS_ReportErrorASCII(cx, "Unknown event.");
      return NS_ERROR_INVALID_ARG;
    case RecordEventResult::InvalidExtraKey:
      LogToBrowserConsole(nsIScriptError::warningFlag,
                          NS_LITERAL_STRING("Invalid extra key for event."));
      return NS_OK;
    case RecordEventResult::StorageLimitReached:
      LogToBrowserConsole(nsIScriptError::warningFlag,
                          NS_LITERAL_STRING("Event storage limit reached."));
      return NS_OK;
    default:
      return NS_OK;
  }
}

nsresult
TelemetryEvent::CreateSnapshots(uint32_t aDataset, bool aClear, JSContext* cx,
                                uint8_t optional_argc, JS::MutableHandleValue aResult)
{
  // Extract the events from storage.
  nsTArray<EventRecord> events;
  {
    StaticMutexAutoLock locker(gTelemetryEventsMutex);

    if (!gInitDone) {
      return NS_ERROR_FAILURE;
    }

    uint32_t len = gEventRecords->Length();
    for (uint32_t i = 0; i < len; ++i) {
      const EventRecord& record = (*gEventRecords)[i];
      const EventInfo& info = gEventInfo[record.EventId()];

      if (IsInDataset(info.common_info.dataset, aDataset)) {
        events.AppendElement(record);
      }
    }

    if (aClear) {
      gEventRecords->Clear();
    }
  }

  // We serialize the events to a JS array.
  JS::RootedObject eventsArray(cx, JS_NewArrayObject(cx, events.Length()));
  if (!eventsArray) {
    return NS_ERROR_FAILURE;
  }

  for (uint32_t i = 0; i < events.Length(); ++i) {
    const EventRecord& record = events[i];
    const EventInfo& info = gEventInfo[record.EventId()];

    // Each entry is an array of one of the forms:
    // [timestamp, category, method, object, value]
    // [timestamp, category, method, object, null, extra]
    // [timestamp, category, method, object, value, extra]
    JS::AutoValueVector items(cx);

    // Add timestamp.
    JS::Rooted<JS::Value> val(cx);
    if (!items.append(JS::NumberValue(floor(record.Timestamp())))) {
      return NS_ERROR_FAILURE;
    }

    // Add category, method, object.
    const char* strings[] = {
      info.common_info.category(),
      info.method(),
      info.object(),
    };
    for (const char* s : strings) {
      const NS_ConvertUTF8toUTF16 wide(s);
      if (!items.append(JS::StringValue(JS_NewUCStringCopyN(cx, wide.Data(), wide.Length())))) {
        return NS_ERROR_FAILURE;
      }
    }

    // Add the optional string value only when needed.
    // When extra is empty and this has no value, we can save a little space.
    if (record.Value()) {
      const NS_ConvertUTF8toUTF16 wide(record.Value().value());
      if (!items.append(JS::StringValue(JS_NewUCStringCopyN(cx, wide.Data(), wide.Length())))) {
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
        const NS_ConvertUTF8toUTF16 wide(extra[i].value);
        JS::Rooted<JS::Value> value(cx);
        value.setString(JS_NewUCStringCopyN(cx, wide.Data(), wide.Length()));

        if (!JS_DefineProperty(cx, obj, extra[i].key.get(), value, JSPROP_ENUMERATE)) {
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

  aResult.setObject(*eventsArray);
  return NS_OK;
}

/**
 * Resets all the stored events. This is intended to be only used in tests.
 */
void
TelemetryEvent::ClearEvents()
{
  StaticMutexAutoLock lock(gTelemetryEventsMutex);

  if (!gInitDone) {
    return;
  }

  gEventRecords->Clear();
}

size_t
TelemetryEvent::SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf)
{
  StaticMutexAutoLock locker(gTelemetryEventsMutex);
  size_t n = 0;

  n += gEventRecords->ShallowSizeOfIncludingThis(aMallocSizeOf);
  for (uint32_t i = 0; i < gEventRecords->Length(); ++i) {
    n += (*gEventRecords)[i].SizeOfExcludingThis(aMallocSizeOf);
  }

  n += gEventNameIDMap.ShallowSizeOfExcludingThis(aMallocSizeOf);
  for (auto iter = gEventNameIDMap.ConstIter(); !iter.Done(); iter.Next()) {
    n += iter.Key().SizeOfExcludingThisIfUnshared(aMallocSizeOf);
  }

  return n;
}
