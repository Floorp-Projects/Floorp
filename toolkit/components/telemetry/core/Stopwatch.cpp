/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/telemetry/Stopwatch.h"

#include "TelemetryHistogram.h"
#include "TelemetryUserInteraction.h"

#include "js/MapAndSet.h"
#include "js/WeakMap.h"
#include "mozilla/dom/ScriptSettings.h"
#include "mozilla/BackgroundHangMonitor.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/HangAnnotations.h"
#include "mozilla/ProfilerMarkers.h"
#include "mozilla/DataMutex.h"
#include "mozilla/TimeStamp.h"
#include "nsHashKeys.h"
#include "nsContentUtils.h"
#include "nsPrintfCString.h"
#include "nsQueryObject.h"
#include "nsString.h"
#include "xpcpublic.h"

using mozilla::DataMutex;
using mozilla::dom::AutoJSAPI;

#define USER_INTERACTION_VALUE_MAX_LENGTH 50  // bytes

static inline nsQueryObject<nsISupports> do_QueryReflector(
    JSObject* aReflector) {
  // None of the types we query to are implemented by Window or Location.
  nsCOMPtr<nsISupports> reflector = xpc::ReflectorToISupportsStatic(aReflector);
  return do_QueryObject(reflector);
}

static inline nsQueryObject<nsISupports> do_QueryReflector(
    const JS::Value& aReflector) {
  return do_QueryReflector(&aReflector.toObject());
}

static void LogError(JSContext* aCx, const nsCString& aMessage) {
  // This is a bit of a hack to report an error with the current JS caller's
  // location. We create an AutoJSAPI object bound to the current caller
  // global, report a JS error, and then let AutoJSAPI's destructor report the
  // error.
  //
  // Unfortunately, there isn't currently a more straightforward way to do
  // this from C++.
  JS::Rooted<JSObject*> global(aCx, JS::CurrentGlobalOrNull(aCx));

  AutoJSAPI jsapi;
  if (jsapi.Init(global)) {
    JS_ReportErrorUTF8(jsapi.cx(), "%s", aMessage.get());
  }
}

namespace mozilla::telemetry {

class Timer final : public mozilla::LinkedListElement<RefPtr<Timer>> {
 public:
  NS_INLINE_DECL_REFCOUNTING(Timer)

  Timer() = default;

  void Start(bool aInSeconds) {
    mStartTime = TimeStamp::Now();
    mInSeconds = aInSeconds;
  }

  bool Started() { return !mStartTime.IsNull(); }

  uint32_t Elapsed() {
    auto delta = TimeStamp::Now() - mStartTime;
    return mInSeconds ? delta.ToSeconds() : delta.ToMilliseconds();
  }

  TimeStamp& StartTime() { return mStartTime; }

  bool& InSeconds() { return mInSeconds; }

  /**
   * Note that these values will want to be read from the
   * BackgroundHangAnnotator thread. Callers should take a lock
   * on Timers::mBHRAnnotationTimers before calling this.
   */
  void SetBHRAnnotation(const nsAString& aBHRAnnotationKey,
                        const nsACString& aBHRAnnotationValue) {
    mBHRAnnotationKey = aBHRAnnotationKey;
    mBHRAnnotationValue = aBHRAnnotationValue;
  }

  const nsString& GetBHRAnnotationKey() const { return mBHRAnnotationKey; }
  const nsCString& GetBHRAnnotationValue() const { return mBHRAnnotationValue; }

 private:
  ~Timer() = default;
  TimeStamp mStartTime{};
  nsString mBHRAnnotationKey;
  nsCString mBHRAnnotationValue;
  bool mInSeconds;
};

#define TIMER_KEYS_IID                               \
  {                                                  \
    0xef707178, 0x1544, 0x46e2, {                    \
      0xa3, 0xf5, 0x98, 0x38, 0xba, 0x60, 0xfd, 0x8f \
    }                                                \
  }

class TimerKeys final : public nsISupports {
 public:
  NS_DECL_ISUPPORTS
  NS_DECLARE_STATIC_IID_ACCESSOR(TIMER_KEYS_IID)

  Timer* Get(const nsAString& aKey, bool aCreate = true);

  already_AddRefed<Timer> GetAndDelete(const nsAString& aKey) {
    RefPtr<Timer> timer;
    mTimers.Remove(aKey, getter_AddRefs(timer));
    return timer.forget();
  }

  bool Delete(const nsAString& aKey) { return mTimers.Remove(aKey); }

 private:
  ~TimerKeys() = default;

  nsRefPtrHashtable<nsStringHashKey, Timer> mTimers;
};

NS_DEFINE_STATIC_IID_ACCESSOR(TimerKeys, TIMER_KEYS_IID)

NS_IMPL_ISUPPORTS(TimerKeys, TimerKeys)

Timer* TimerKeys::Get(const nsAString& aKey, bool aCreate) {
  if (aCreate) {
    return mTimers.GetOrInsertNew(aKey);
  }
  return mTimers.GetWeak(aKey);
}

class Timers final : public BackgroundHangAnnotator {
 public:
  Timers();

  static Timers& Singleton();

  NS_INLINE_DECL_REFCOUNTING(Timers)

  JSObject* Get(JSContext* aCx, const nsAString& aHistogram,
                bool aCreate = true);

  TimerKeys* Get(JSContext* aCx, const nsAString& aHistogram,
                 JS::Handle<JSObject*> aObj, bool aCreate = true);

  Timer* Get(JSContext* aCx, const nsAString& aHistogram,
             JS::Handle<JSObject*> aObj, const nsAString& aKey,
             bool aCreate = true);

  already_AddRefed<Timer> GetAndDelete(JSContext* aCx,
                                       const nsAString& aHistogram,
                                       JS::Handle<JSObject*> aObj,
                                       const nsAString& aKey);

  bool Delete(JSContext* aCx, const nsAString& aHistogram,
              JS::Handle<JSObject*> aObj, const nsAString& aKey);

  int32_t TimeElapsed(JSContext* aCx, const nsAString& aHistogram,
                      JS::Handle<JSObject*> aObj, const nsAString& aKey,
                      bool aCanceledOkay = false);

  bool Start(JSContext* aCx, const nsAString& aHistogram,
             JS::Handle<JSObject*> aObj, const nsAString& aKey,
             bool aInSeconds = false);

  int32_t Finish(JSContext* aCx, const nsAString& aHistogram,
                 JS::Handle<JSObject*> aObj, const nsAString& aKey,
                 bool aCanceledOkay = false);

  bool& SuppressErrors() { return mSuppressErrors; }

  bool StartUserInteraction(JSContext* aCx, const nsAString& aUserInteraction,
                            const nsACString& aValue,
                            JS::Handle<JSObject*> aObj);
  bool RunningUserInteraction(JSContext* aCx, const nsAString& aUserInteraction,
                              JS::Handle<JSObject*> aObj);
  bool UpdateUserInteraction(JSContext* aCx, const nsAString& aUserInteraction,
                             const nsACString& aValue,
                             JS::Handle<JSObject*> aObj);
  bool FinishUserInteraction(JSContext* aCx, const nsAString& aUserInteraction,
                             JS::Handle<JSObject*> aObj,
                             const dom::Optional<nsACString>& aAdditionalText);
  bool CancelUserInteraction(JSContext* aCx, const nsAString& aUserInteraction,
                             JS::Handle<JSObject*> aObj);

  void AnnotateHang(BackgroundHangAnnotations& aAnnotations) final;

 private:
  ~Timers();

  JS::PersistentRooted<JSObject*> mTimers;
  DataMutex<mozilla::LinkedList<RefPtr<Timer>>> mBHRAnnotationTimers;
  bool mSuppressErrors = false;

  static StaticRefPtr<Timers> sSingleton;
};

StaticRefPtr<Timers> Timers::sSingleton;

/* static */ Timers& Timers::Singleton() {
  if (!sSingleton) {
    sSingleton = new Timers();
    ClearOnShutdown(&sSingleton);
  }
  return *sSingleton;
}

Timers::Timers()
    : mTimers(dom::RootingCx()), mBHRAnnotationTimers("BHRAnnotationTimers") {
  AutoJSAPI jsapi;
  MOZ_ALWAYS_TRUE(jsapi.Init(xpc::PrivilegedJunkScope()));

  mTimers = JS::NewMapObject(jsapi.cx());
  MOZ_RELEASE_ASSERT(mTimers);

  BackgroundHangMonitor::RegisterAnnotator(*this);
}

Timers::~Timers() {
  // We use a scope here to prevent a deadlock with the mutex that locks
  // inside of ::UnregisterAnnotator.
  {
    auto annotationTimers = mBHRAnnotationTimers.Lock();
    annotationTimers->clear();
  }
  BackgroundHangMonitor::UnregisterAnnotator(*this);
}

JSObject* Timers::Get(JSContext* aCx, const nsAString& aHistogram,
                      bool aCreate) {
  JSAutoRealm ar(aCx, mTimers);

  JS::Rooted<JS::Value> histogram(aCx);
  JS::Rooted<JS::Value> objs(aCx);

  if (!xpc::NonVoidStringToJsval(aCx, aHistogram, &histogram) ||
      !JS::MapGet(aCx, mTimers, histogram, &objs)) {
    return nullptr;
  }
  if (!objs.isObject()) {
    if (aCreate) {
      objs = JS::ObjectOrNullValue(JS::NewWeakMapObject(aCx));
    }
    if (!objs.isObject() || !JS::MapSet(aCx, mTimers, histogram, objs)) {
      return nullptr;
    }
  }

  return &objs.toObject();
}

TimerKeys* Timers::Get(JSContext* aCx, const nsAString& aHistogram,
                       JS::Handle<JSObject*> aObj, bool aCreate) {
  JSAutoRealm ar(aCx, mTimers);

  JS::Rooted<JSObject*> objs(aCx, Get(aCx, aHistogram, aCreate));
  if (!objs) {
    return nullptr;
  }

  // If no object is passed, use mTimers as a stand-in for a null object
  // (which cannot be used as a weak map key).
  JS::Rooted<JSObject*> obj(aCx, aObj ? aObj : mTimers);
  if (!JS_WrapObject(aCx, &obj)) {
    return nullptr;
  }

  RefPtr<TimerKeys> keys;
  JS::Rooted<JS::Value> keysObj(aCx);
  JS::Rooted<JS::Value> objVal(aCx, JS::ObjectValue(*obj));
  if (!JS::GetWeakMapEntry(aCx, objs, objVal, &keysObj)) {
    return nullptr;
  }
  if (!keysObj.isObject()) {
    if (aCreate) {
      keys = new TimerKeys();
      Unused << nsContentUtils::WrapNative(aCx, keys, &keysObj);
    }
    if (!keysObj.isObject() ||
        !JS::SetWeakMapEntry(aCx, objs, objVal, keysObj)) {
      return nullptr;
    }
  }

  keys = do_QueryReflector(keysObj);
  return keys;
}

Timer* Timers::Get(JSContext* aCx, const nsAString& aHistogram,
                   JS::Handle<JSObject*> aObj, const nsAString& aKey,
                   bool aCreate) {
  if (RefPtr<TimerKeys> keys = Get(aCx, aHistogram, aObj, aCreate)) {
    return keys->Get(aKey, aCreate);
  }
  return nullptr;
}

already_AddRefed<Timer> Timers::GetAndDelete(JSContext* aCx,
                                             const nsAString& aHistogram,
                                             JS::Handle<JSObject*> aObj,
                                             const nsAString& aKey) {
  if (RefPtr<TimerKeys> keys = Get(aCx, aHistogram, aObj, false)) {
    return keys->GetAndDelete(aKey);
  }
  return nullptr;
}

bool Timers::Delete(JSContext* aCx, const nsAString& aHistogram,
                    JS::Handle<JSObject*> aObj, const nsAString& aKey) {
  if (RefPtr<TimerKeys> keys = Get(aCx, aHistogram, aObj, false)) {
    return keys->Delete(aKey);
  }
  return false;
}

int32_t Timers::TimeElapsed(JSContext* aCx, const nsAString& aHistogram,
                            JS::Handle<JSObject*> aObj, const nsAString& aKey,
                            bool aCanceledOkay) {
  RefPtr<Timer> timer = Get(aCx, aHistogram, aObj, aKey, false);
  if (!timer) {
    if (!aCanceledOkay && !mSuppressErrors) {
      LogError(aCx, nsPrintfCString(
                        "TelemetryStopwatch: requesting elapsed time for "
                        "nonexisting stopwatch. Histogram: \"%s\", key: \"%s\"",
                        NS_ConvertUTF16toUTF8(aHistogram).get(),
                        NS_ConvertUTF16toUTF8(aKey).get()));
    }
    return -1;
  }

  return timer->Elapsed();
}

bool Timers::Start(JSContext* aCx, const nsAString& aHistogram,
                   JS::Handle<JSObject*> aObj, const nsAString& aKey,
                   bool aInSeconds) {
  if (RefPtr<Timer> timer = Get(aCx, aHistogram, aObj, aKey)) {
    if (timer->Started()) {
      if (!mSuppressErrors) {
        LogError(aCx,
                 nsPrintfCString(
                     "TelemetryStopwatch: key \"%s\" was already initialized",
                     NS_ConvertUTF16toUTF8(aHistogram).get()));
      }
      Delete(aCx, aHistogram, aObj, aKey);
    } else {
      timer->Start(aInSeconds);
      return true;
    }
  }
  return false;
}

int32_t Timers::Finish(JSContext* aCx, const nsAString& aHistogram,
                       JS::Handle<JSObject*> aObj, const nsAString& aKey,
                       bool aCanceledOkay) {
  RefPtr<Timer> timer = GetAndDelete(aCx, aHistogram, aObj, aKey);
  if (!timer) {
    if (!aCanceledOkay && !mSuppressErrors) {
      LogError(aCx, nsPrintfCString(
                        "TelemetryStopwatch: finishing nonexisting stopwatch. "
                        "Histogram: \"%s\", key: \"%s\"",
                        NS_ConvertUTF16toUTF8(aHistogram).get(),
                        NS_ConvertUTF16toUTF8(aKey).get()));
    }
    return -1;
  }

  int32_t delta = timer->Elapsed();
  NS_ConvertUTF16toUTF8 histogram(aHistogram);
  nsresult rv;
  if (!aKey.IsVoid()) {
    NS_ConvertUTF16toUTF8 key(aKey);
    rv = TelemetryHistogram::Accumulate(histogram.get(), key, delta);
  } else {
    rv = TelemetryHistogram::Accumulate(histogram.get(), delta);
  }
  if (profiler_thread_is_being_profiled_for_markers()) {
    nsCString markerText = histogram;
    if (!aKey.IsVoid()) {
      markerText.AppendLiteral(":");
      markerText.Append(NS_ConvertUTF16toUTF8(aKey));
    }
    PROFILER_MARKER_TEXT("TelemetryStopwatch", OTHER,
                         MarkerTiming::IntervalUntilNowFrom(timer->StartTime()),
                         markerText);
  }
  if (NS_FAILED(rv) && rv != NS_ERROR_NOT_AVAILABLE && !mSuppressErrors) {
    LogError(aCx, nsPrintfCString(
                      "TelemetryStopwatch: failed to update the Histogram "
                      "\"%s\", using key: \"%s\"",
                      NS_ConvertUTF16toUTF8(aHistogram).get(),
                      NS_ConvertUTF16toUTF8(aKey).get()));
  }
  return NS_SUCCEEDED(rv) ? delta : -1;
}

bool Timers::StartUserInteraction(JSContext* aCx,
                                  const nsAString& aUserInteraction,
                                  const nsACString& aValue,
                                  JS::Handle<JSObject*> aObj) {
  MOZ_ASSERT(NS_IsMainThread());

  // Ensure that this ID maps to a UserInteraction that can be recorded
  // for this product.
  if (!TelemetryUserInteraction::CanRecord(aUserInteraction)) {
    if (!mSuppressErrors) {
      LogError(aCx, nsPrintfCString(
                        "UserInteraction with name \"%s\" cannot be recorded.",
                        NS_ConvertUTF16toUTF8(aUserInteraction).get()));
    }
    return false;
  }

  if (aValue.Length() > USER_INTERACTION_VALUE_MAX_LENGTH) {
    if (!mSuppressErrors) {
      LogError(aCx,
               nsPrintfCString(
                   "UserInteraction with name \"%s\" cannot be recorded with"
                   "a value of length greater than %d (%s)",
                   NS_ConvertUTF16toUTF8(aUserInteraction).get(),
                   USER_INTERACTION_VALUE_MAX_LENGTH,
                   PromiseFlatCString(aValue).get()));
    }
    return false;
  }

  if (RefPtr<Timer> timer = Get(aCx, aUserInteraction, aObj, VoidString())) {
    auto annotationTimers = mBHRAnnotationTimers.Lock();

    if (timer->Started()) {
      if (!mSuppressErrors) {
        LogError(aCx,
                 nsPrintfCString(
                     "UserInteraction with name \"%s\" was already initialized",
                     NS_ConvertUTF16toUTF8(aUserInteraction).get()));
      }
      timer->removeFrom(*annotationTimers);
      Delete(aCx, aUserInteraction, aObj, VoidString());
      timer = Get(aCx, aUserInteraction, aObj, VoidString());

      nsAutoString clobberText(aUserInteraction);
      clobberText.AppendLiteral(u" (clobbered)");
      timer->SetBHRAnnotation(clobberText, aValue);
    } else {
      timer->SetBHRAnnotation(aUserInteraction, aValue);
    }

    annotationTimers->insertBack(timer);
    timer->Start(false);
    return true;
  }
  return false;
}

bool Timers::RunningUserInteraction(JSContext* aCx,
                                    const nsAString& aUserInteraction,
                                    JS::Handle<JSObject*> aObj) {
  if (RefPtr<Timer> timer =
          Get(aCx, aUserInteraction, aObj, VoidString(), false /* aCreate */)) {
    return timer->Started();
  }
  return false;
}

bool Timers::UpdateUserInteraction(JSContext* aCx,
                                   const nsAString& aUserInteraction,
                                   const nsACString& aValue,
                                   JS::Handle<JSObject*> aObj) {
  MOZ_ASSERT(NS_IsMainThread());

  // Ensure that this ID maps to a UserInteraction that can be recorded
  // for this product.
  if (!TelemetryUserInteraction::CanRecord(aUserInteraction)) {
    if (!mSuppressErrors) {
      LogError(aCx, nsPrintfCString(
                        "UserInteraction with name \"%s\" cannot be recorded.",
                        NS_ConvertUTF16toUTF8(aUserInteraction).get()));
    }
    return false;
  }

  auto lock = mBHRAnnotationTimers.Lock();
  if (RefPtr<Timer> timer = Get(aCx, aUserInteraction, aObj, VoidString())) {
    if (!timer->Started()) {
      if (!mSuppressErrors) {
        LogError(aCx, nsPrintfCString(
                          "UserInteraction with id \"%s\" was not initialized",
                          NS_ConvertUTF16toUTF8(aUserInteraction).get()));
      }
      return false;
    }
    timer->SetBHRAnnotation(aUserInteraction, aValue);
    return true;
  }
  return false;
}

bool Timers::FinishUserInteraction(
    JSContext* aCx, const nsAString& aUserInteraction,
    JS::Handle<JSObject*> aObj,
    const dom::Optional<nsACString>& aAdditionalText) {
  MOZ_ASSERT(NS_IsMainThread());

  // Ensure that this ID maps to a UserInteraction that can be recorded
  // for this product.
  if (!TelemetryUserInteraction::CanRecord(aUserInteraction)) {
    if (!mSuppressErrors) {
      LogError(aCx, nsPrintfCString(
                        "UserInteraction with id \"%s\" cannot be recorded.",
                        NS_ConvertUTF16toUTF8(aUserInteraction).get()));
    }
    return false;
  }

  RefPtr<Timer> timer = GetAndDelete(aCx, aUserInteraction, aObj, VoidString());
  if (!timer) {
    if (!mSuppressErrors) {
      LogError(aCx, nsPrintfCString(
                        "UserInteraction: finishing nonexisting stopwatch. "
                        "name: \"%s\"",
                        NS_ConvertUTF16toUTF8(aUserInteraction).get()));
    }
    return false;
  }

  if (profiler_thread_is_being_profiled_for_markers()) {
    nsAutoCString markerText(timer->GetBHRAnnotationValue());
    if (aAdditionalText.WasPassed()) {
      markerText.Append(",");
      markerText.Append(aAdditionalText.Value());
    }

    PROFILER_MARKER_TEXT(NS_ConvertUTF16toUTF8(aUserInteraction), OTHER,
                         MarkerTiming::IntervalUntilNowFrom(timer->StartTime()),
                         markerText);
  }

  // The Timer will be held alive by the RefPtr that's still in the LinkedList,
  // so the automatic removal from the LinkedList from the LinkedListElement
  // destructor will not occur. We must remove it manually from the LinkedList
  // instead.
  {
    auto annotationTimers = mBHRAnnotationTimers.Lock();
    timer->removeFrom(*annotationTimers);
  }

  return true;
}

bool Timers::CancelUserInteraction(JSContext* aCx,
                                   const nsAString& aUserInteraction,
                                   JS::Handle<JSObject*> aObj) {
  MOZ_ASSERT(NS_IsMainThread());

  // Ensure that this ID maps to a UserInteraction that can be recorded
  // for this product.
  if (!TelemetryUserInteraction::CanRecord(aUserInteraction)) {
    if (!mSuppressErrors) {
      LogError(aCx, nsPrintfCString(
                        "UserInteraction with id \"%s\" cannot be recorded.",
                        NS_ConvertUTF16toUTF8(aUserInteraction).get()));
    }
    return false;
  }

  RefPtr<Timer> timer = GetAndDelete(aCx, aUserInteraction, aObj, VoidString());
  if (!timer) {
    if (!mSuppressErrors) {
      LogError(aCx, nsPrintfCString(
                        "UserInteraction: cancelling nonexisting stopwatch. "
                        "name: \"%s\"",
                        NS_ConvertUTF16toUTF8(aUserInteraction).get()));
    }
    return false;
  }

  // The Timer will be held alive by the RefPtr that's still in the LinkedList,
  // so the automatic removal from the LinkedList from the LinkedListElement
  // destructor will not occur. We must remove it manually from the LinkedList
  // instead.
  {
    auto annotationTimers = mBHRAnnotationTimers.Lock();
    timer->removeFrom(*annotationTimers);
  }

  return true;
}

void Timers::AnnotateHang(mozilla::BackgroundHangAnnotations& aAnnotations) {
  auto annotationTimers = mBHRAnnotationTimers.Lock();
  for (Timer* bhrAnnotationTimer : *annotationTimers) {
    aAnnotations.AddAnnotation(bhrAnnotationTimer->GetBHRAnnotationKey(),
                               bhrAnnotationTimer->GetBHRAnnotationValue());
  }
}

/* static */
bool Stopwatch::Start(const dom::GlobalObject& aGlobal,
                      const nsAString& aHistogram, JS::Handle<JSObject*> aObj,
                      const dom::TelemetryStopwatchOptions& aOptions) {
  return StartKeyed(aGlobal, aHistogram, VoidString(), aObj, aOptions);
}
/* static */
bool Stopwatch::StartKeyed(const dom::GlobalObject& aGlobal,
                           const nsAString& aHistogram, const nsAString& aKey,
                           JS::Handle<JSObject*> aObj,
                           const dom::TelemetryStopwatchOptions& aOptions) {
  return Timers::Singleton().Start(aGlobal.Context(), aHistogram, aObj, aKey,
                                   aOptions.mInSeconds);
}

/* static */
bool Stopwatch::Running(const dom::GlobalObject& aGlobal,
                        const nsAString& aHistogram,
                        JS::Handle<JSObject*> aObj) {
  return RunningKeyed(aGlobal, aHistogram, VoidString(), aObj);
}

/* static */
bool Stopwatch::RunningKeyed(const dom::GlobalObject& aGlobal,
                             const nsAString& aHistogram, const nsAString& aKey,
                             JS::Handle<JSObject*> aObj) {
  return TimeElapsedKeyed(aGlobal, aHistogram, aKey, aObj, true) != -1;
}

/* static */
int32_t Stopwatch::TimeElapsed(const dom::GlobalObject& aGlobal,
                               const nsAString& aHistogram,
                               JS::Handle<JSObject*> aObj, bool aCanceledOkay) {
  return TimeElapsedKeyed(aGlobal, aHistogram, VoidString(), aObj,
                          aCanceledOkay);
}

/* static */
int32_t Stopwatch::TimeElapsedKeyed(const dom::GlobalObject& aGlobal,
                                    const nsAString& aHistogram,
                                    const nsAString& aKey,
                                    JS::Handle<JSObject*> aObj,
                                    bool aCanceledOkay) {
  return Timers::Singleton().TimeElapsed(aGlobal.Context(), aHistogram, aObj,
                                         aKey, aCanceledOkay);
}

/* static */
bool Stopwatch::Finish(const dom::GlobalObject& aGlobal,
                       const nsAString& aHistogram, JS::Handle<JSObject*> aObj,
                       bool aCanceledOkay) {
  return FinishKeyed(aGlobal, aHistogram, VoidString(), aObj, aCanceledOkay);
}

/* static */
bool Stopwatch::FinishKeyed(const dom::GlobalObject& aGlobal,
                            const nsAString& aHistogram, const nsAString& aKey,
                            JS::Handle<JSObject*> aObj, bool aCanceledOkay) {
  return Timers::Singleton().Finish(aGlobal.Context(), aHistogram, aObj, aKey,
                                    aCanceledOkay) != -1;
}

/* static */
bool Stopwatch::Cancel(const dom::GlobalObject& aGlobal,
                       const nsAString& aHistogram,
                       JS::Handle<JSObject*> aObj) {
  return CancelKeyed(aGlobal, aHistogram, VoidString(), aObj);
}

/* static */
bool Stopwatch::CancelKeyed(const dom::GlobalObject& aGlobal,
                            const nsAString& aHistogram, const nsAString& aKey,
                            JS::Handle<JSObject*> aObj) {
  return Timers::Singleton().Delete(aGlobal.Context(), aHistogram, aObj, aKey);
}

/* static */
void Stopwatch::SetTestModeEnabled(const dom::GlobalObject& aGlobal,
                                   bool aTesting) {
  Timers::Singleton().SuppressErrors() = aTesting;
}

/* static */
bool UserInteractionStopwatch::Start(const dom::GlobalObject& aGlobal,
                                     const nsAString& aUserInteraction,
                                     const nsACString& aValue,
                                     JS::Handle<JSObject*> aObj) {
  if (!NS_IsMainThread()) {
    return false;
  }
  return Timers::Singleton().StartUserInteraction(
      aGlobal.Context(), aUserInteraction, aValue, aObj);
}

/* static */
bool UserInteractionStopwatch::Running(const dom::GlobalObject& aGlobal,
                                       const nsAString& aUserInteraction,
                                       JS::Handle<JSObject*> aObj) {
  if (!NS_IsMainThread()) {
    return false;
  }
  return Timers::Singleton().RunningUserInteraction(aGlobal.Context(),
                                                    aUserInteraction, aObj);
}

/* static */
bool UserInteractionStopwatch::Update(const dom::GlobalObject& aGlobal,
                                      const nsAString& aUserInteraction,
                                      const nsACString& aValue,
                                      JS::Handle<JSObject*> aObj) {
  if (!NS_IsMainThread()) {
    return false;
  }
  return Timers::Singleton().UpdateUserInteraction(
      aGlobal.Context(), aUserInteraction, aValue, aObj);
}

/* static */
bool UserInteractionStopwatch::Cancel(const dom::GlobalObject& aGlobal,
                                      const nsAString& aUserInteraction,
                                      JS::Handle<JSObject*> aObj) {
  if (!NS_IsMainThread()) {
    return false;
  }
  return Timers::Singleton().CancelUserInteraction(aGlobal.Context(),
                                                   aUserInteraction, aObj);
}

/* static */
bool UserInteractionStopwatch::Finish(
    const dom::GlobalObject& aGlobal, const nsAString& aUserInteraction,
    JS::Handle<JSObject*> aObj,
    const dom::Optional<nsACString>& aAdditionalText) {
  if (!NS_IsMainThread()) {
    return false;
  }
  return Timers::Singleton().FinishUserInteraction(
      aGlobal.Context(), aUserInteraction, aObj, aAdditionalText);
}

}  // namespace mozilla::telemetry
