/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_glean_GleanEvent_h
#define mozilla_glean_GleanEvent_h

#include "nsIGleanMetrics.h"
#include "mozilla/glean/bindings/EventGIFFTMap.h"
#include "mozilla/glean/fog_ffi_generated.h"
#include "mozilla/Tuple.h"
#include "nsString.h"
#include "nsTArray.h"

namespace mozilla::glean {

// forward declaration
class GleanEvent;

namespace impl {

/**
 * Represents the recorded data for a single event
 */
struct RecordedEvent {
 public:
  uint64_t mTimestamp;
  nsCString mCategory;
  nsCString mName;

  nsTArray<Tuple<nsCString, nsCString>> mExtra;
};

template <class T>
class EventMetric {
  friend class mozilla::glean::GleanEvent;

 public:
  constexpr explicit EventMetric(uint32_t id) : mId(id) {}

  /**
   * Record an event.
   *
   * @param aExtras The list of (extra key, value) pairs. Allowed extra keys are
   *                defined in the metric definition.
   *                If the wrong keys are used or values are too large
   *                an error is report and no event is recorded.
   */
  void Record(const Span<const Tuple<T, nsCString>>& aExtras = {}) const {
    auto id = EventIdForMetric(mId);
    if (id) {
      Maybe<CopyableTArray<Telemetry::EventExtraEntry>> telExtras;
      if (!aExtras.IsEmpty()) {
        CopyableTArray<Telemetry::EventExtraEntry> extras;
        for (auto& entry : aExtras) {
          auto extraString = ExtraStringForKey(Get<0>(entry));
          extras.EmplaceBack(
              Telemetry::EventExtraEntry{extraString, Get<1>(entry)});
        }
        telExtras = Some(extras);
      }
      Telemetry::RecordEvent(id.extract(), Nothing(), telExtras);
    }
#ifndef MOZ_GLEAN_ANDROID
    static_assert(sizeof(T) <= sizeof(int32_t),
                  "Extra keys need to fit into 32 bits");

    nsTArray<int32_t> extraKeys;
    nsTArray<nsCString> extraValues;
    for (auto& entry : aExtras) {
      extraKeys.AppendElement(static_cast<int32_t>(mozilla::Get<0>(entry)));
      extraValues.AppendElement(mozilla::Get<1>(entry));
    }

    fog_event_record(mId, &extraKeys, &extraValues);
#endif
  }

  /**
   * **Test-only API**
   *
   * Get a list of currently stored events for this event metric.
   *
   * This function will attempt to await the last parent-process task (if any)
   * writing to the the metric's storage engine before returning a value.
   * This function will not wait for data from child processes.
   *
   * This doesn't clear the stored value.
   * Parent process only. Panics in child processes.
   *
   * @param aPingName The (optional) name of the ping to retrieve the metric
   *        for. Defaults to the first value in `send_in_pings`.
   *
   * @return value of the stored metric, or Nothing() if there is no value.
   */
  Maybe<nsTArray<RecordedEvent>> TestGetValue(
      const nsACString& aPingName = nsCString()) const {
#ifdef MOZ_GLEAN_ANDROID
    Unused << mId;
    return Nothing();
#else
    if (!fog_event_test_has_value(mId, &aPingName)) {
      return Nothing();
    }

    nsTArray<FfiRecordedEvent> events;
    fog_event_test_get_value(mId, &aPingName, &events);

    nsTArray<RecordedEvent> result;
    for (auto event : events) {
      auto ev = result.AppendElement();
      ev->mTimestamp = event.timestamp;
      ev->mCategory.Append(event.category);
      ev->mName.Assign(event.name);

      // SAFETY:
      // `event.extra` is a valid pointer to an array of length `2 *
      // event.extra_len`.
      ev->mExtra.SetCapacity(event.extra_len);
      for (unsigned int i = 0; i < event.extra_len; i++) {
        // keys & values are interleaved.
        auto key = event.extra[2 * i];
        auto value = event.extra[2 * i + 1];
        ev->mExtra.AppendElement(MakeTuple(key, value));
      }
      // Event extras are now copied, we can free the array.
      fog_event_free_event_extra(event.extra, event.extra_len);
    }
    return Some(std::move(result));
#endif
  }

 private:
  static const nsCString ExtraStringForKey(T aKey);

  const uint32_t mId;
};

}  // namespace impl

enum class NoExtraKeys;

class GleanEvent final : public nsIGleanEvent {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIGLEANEVENT

  explicit GleanEvent(uint32_t id) : mEvent(id){};

 private:
  virtual ~GleanEvent() = default;

  const impl::EventMetric<NoExtraKeys> mEvent;
};

}  // namespace mozilla::glean

#endif /* mozilla_glean_GleanEvent.h */
