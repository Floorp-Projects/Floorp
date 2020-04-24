/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

#ifndef TelemetryTestHelpers_h_
#define TelemetryTestHelpers_h_

#include "js/TypeDecls.h"
#include "mozilla/TelemetryProcessEnums.h"
#include "nsITelemetry.h"

using mozilla::Telemetry::ProcessID;

namespace TelemetryTestHelpers {

void CheckUintScalar(const char* aName, JSContext* aCx,
                     JS::HandleValue aSnapshot, uint32_t expectedValue);

void CheckBoolScalar(const char* aName, JSContext* aCx,
                     JS::HandleValue aSnapshot, bool expectedValue);

void CheckStringScalar(const char* aName, JSContext* aCx,
                       JS::HandleValue aSnapshot, const char* expectedValue);

void CheckKeyedUintScalar(const char* aName, const char* aKey, JSContext* aCx,
                          JS::HandleValue aSnapshot, uint32_t expectedValue);

void CheckKeyedBoolScalar(const char* aName, const char* aKey, JSContext* aCx,
                          JS::HandleValue aSnapshot, bool expectedValue);

void CheckNumberOfProperties(const char* aName, JSContext* aCx,
                             JS::HandleValue aSnapshot,
                             uint32_t expectedNumProperties);

bool EventPresent(JSContext* aCx, const JS::RootedValue& aSnapshot,
                  const nsACString& aCategory, const nsACString& aMethod,
                  const nsACString& aObject);

void GetEventSnapshot(JSContext* aCx, JS::MutableHandle<JS::Value> aResult,
                      ProcessID aProcessType = ProcessID::Parent);

void GetScalarsSnapshot(bool aKeyed, JSContext* aCx,
                        JS::MutableHandle<JS::Value> aResult,
                        ProcessID aProcessType = ProcessID::Parent);

void GetAndClearHistogram(JSContext* cx, nsCOMPtr<nsITelemetry> mTelemetry,
                          const nsACString& name, bool is_keyed);

void GetProperty(JSContext* cx, const char* name, JS::HandleValue valueIn,
                 JS::MutableHandleValue valueOut);

void GetElement(JSContext* cx, uint32_t index, JS::HandleValue valueIn,
                JS::MutableHandleValue valueOut);

void GetSnapshots(JSContext* cx, nsCOMPtr<nsITelemetry> mTelemetry,
                  const char* name, JS::MutableHandleValue valueOut,
                  bool is_keyed);

void GetOriginSnapshot(JSContext* aCx, JS::MutableHandle<JS::Value> aResult,
                       bool aClear = false);

void GetEncodedOriginStrings(
    JSContext* aCx, const nsCString& aEncoding,
    nsTArray<mozilla::Tuple<nsCString, nsCString>>& aPrioStrings);

}  // namespace TelemetryTestHelpers

#endif
