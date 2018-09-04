/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsITelemetry.h"
#include "nsVersionComparator.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/Preferences.h"
#include "nsIConsoleService.h"
#include "nsThreadUtils.h"

#include "TelemetryCommon.h"
#include "TelemetryProcessData.h"

#include <cstring>

namespace mozilla {
namespace Telemetry {
namespace Common {

bool
IsExpiredVersion(const char* aExpiration)
{
  MOZ_ASSERT(aExpiration);
  // Note: We intentionally don't construct a static Version object here as we
  // saw odd crashes around this (see bug 1334105).
  return strcmp(aExpiration, "never") && strcmp(aExpiration, "default") &&
    (mozilla::Version(aExpiration) <= MOZ_APP_VERSION);
}

bool
IsInDataset(uint32_t aDataset, uint32_t aContainingDataset)
{
  if (aDataset == aContainingDataset) {
    return true;
  }

  // The "optin on release channel" dataset is a superset of the
  // "optout on release channel one".
  if (aContainingDataset == nsITelemetry::DATASET_RELEASE_CHANNEL_OPTIN &&
      aDataset == nsITelemetry::DATASET_RELEASE_CHANNEL_OPTOUT) {
    return true;
  }

  return false;
}

bool
CanRecordDataset(uint32_t aDataset, bool aCanRecordBase, bool aCanRecordExtended)
{
  // If we are extended telemetry is enabled, we are allowed to record
  // regardless of the dataset.
  if (aCanRecordExtended) {
    return true;
  }

  // If base telemetry data is enabled and we're trying to record base
  // telemetry, allow it.
  if (aCanRecordBase &&
      IsInDataset(aDataset, nsITelemetry::DATASET_RELEASE_CHANNEL_OPTOUT)) {
      return true;
  }

  // We're not recording extended telemetry or this is not the base
  // dataset. Bail out.
  return false;
}

bool
CanRecordInProcess(RecordedProcessType processes, GeckoProcessType processType)
{
  bool recordAllChildren = !!(processes & RecordedProcessType::AllChildren);
  // We can use (1 << ProcessType) due to the way RecordedProcessType is defined.
  bool canRecordProcess =
    !!(processes & static_cast<RecordedProcessType>(1 << processType));

  return canRecordProcess ||
         ((processType != GeckoProcessType_Default) && recordAllChildren);
}

bool
CanRecordInProcess(RecordedProcessType processes, ProcessID processId)
{
  return CanRecordInProcess(processes, GetGeckoProcessType(processId));
}

bool
CanRecordProduct(SupportedProduct aProducts)
{
  return !!(aProducts & GetCurrentProduct());
}

nsresult
MsSinceProcessStart(double* aResult)
{
  bool error;
  *aResult = (TimeStamp::NowLoRes() -
              TimeStamp::ProcessCreation(&error)).ToMilliseconds();
  if (error) {
    return NS_ERROR_NOT_AVAILABLE;
  }
  return NS_OK;
}

void
LogToBrowserConsole(uint32_t aLogLevel, const nsAString& aMsg)
{
  if (!NS_IsMainThread()) {
    nsString msg(aMsg);
    nsCOMPtr<nsIRunnable> task = NS_NewRunnableFunction(
      "Telemetry::Common::LogToBrowserConsole",
      [aLogLevel, msg]() { LogToBrowserConsole(aLogLevel, msg); });
    NS_DispatchToMainThread(task.forget(), NS_DISPATCH_NORMAL);
    return;
  }

  nsCOMPtr<nsIConsoleService> console(do_GetService("@mozilla.org/consoleservice;1"));
  if (!console) {
    NS_WARNING("Failed to log message to console.");
    return;
  }

  nsCOMPtr<nsIScriptError> error(do_CreateInstance(NS_SCRIPTERROR_CONTRACTID));
  error->Init(aMsg, EmptyString(), EmptyString(), 0, 0, aLogLevel,
              "chrome javascript", false /* from private window */);
  console->LogMessage(error);
}

const char*
GetNameForProcessID(ProcessID process)
{
  MOZ_ASSERT(process < ProcessID::Count);
  return ProcessIDToString[static_cast<uint32_t>(process)];
}

ProcessID
GetIDForProcessName(const char* aProcessName)
{
  for (uint32_t id = 0; id < static_cast<uint32_t>(ProcessID::Count); id++) {
    if (!strcmp(GetNameForProcessID(ProcessID(id)), aProcessName)) {
      return ProcessID(id);
    }
  }

  return ProcessID::Count;
}

GeckoProcessType
GetGeckoProcessType(ProcessID process)
{
  MOZ_ASSERT(process < ProcessID::Count);
  return ProcessIDToGeckoProcessType[static_cast<uint32_t>(process)];
}

bool
IsStringCharValid(const char aChar, const bool aAllowInfixPeriod,
                  const bool aAllowInfixUnderscore)
{
  return (aChar >= 'A' && aChar <= 'Z')
      || (aChar >= 'a' && aChar <= 'z')
      || (aChar >= '0' && aChar <= '9')
      || (aAllowInfixPeriod && (aChar == '.'))
      || (aAllowInfixUnderscore && (aChar == '_'));
}

bool
IsValidIdentifierString(const nsACString& aStr, const size_t aMaxLength,
                        const bool aAllowInfixPeriod, const bool aAllowInfixUnderscore)
{
  // Check string length.
  if (aStr.Length() > aMaxLength) {
    return false;
  }

  // Check string characters.
  const char* first = aStr.BeginReading();
  const char* end = aStr.EndReading();

  for (const char* cur = first; cur < end; ++cur) {
      const bool infix = (cur != first) && (cur != (end - 1));
      if (!IsStringCharValid(*cur,
                             aAllowInfixPeriod && infix,
                             aAllowInfixUnderscore && infix)) {
        return false;
      }
  }

  return true;
}

JSString*
ToJSString(JSContext* cx, const nsACString& aStr)
{
  const NS_ConvertUTF8toUTF16 wide(aStr);
  return JS_NewUCStringCopyN(cx, wide.Data(), wide.Length());
}

JSString*
ToJSString(JSContext* cx, const nsAString& aStr)
{
  return JS_NewUCStringCopyN(cx, aStr.Data(), aStr.Length());
}

// Keep knowledge about the current running product.
// Defaults to Firefox and is reset on Android on Telemetry initialization.
SupportedProduct gCurrentProduct = SupportedProduct::Firefox;

void
SetCurrentProduct()
{
#if defined(MOZ_WIDGET_ANDROID)
  bool isGeckoview = Preferences::GetBool("toolkit.telemetry.isGeckoViewMode", false);
  if (isGeckoview) {
    gCurrentProduct = SupportedProduct::Geckoview;
  } else {
    gCurrentProduct = SupportedProduct::Fennec;
  }
#else
  gCurrentProduct = SupportedProduct::Firefox;
#endif
}

SupportedProduct
GetCurrentProduct()
{
  return gCurrentProduct;
}

} // namespace Common
} // namespace Telemetry
} // namespace mozilla
