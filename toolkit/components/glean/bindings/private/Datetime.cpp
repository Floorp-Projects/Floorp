/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/glean/bindings/Datetime.h"

#include "jsapi.h"
#include "js/Date.h"
#include "nsString.h"
#include "mozilla/Components.h"
#include "mozilla/ResultVariant.h"
#include "mozilla/glean/bindings/ScalarGIFFTMap.h"
#include "mozilla/glean/fog_ffi_generated.h"
#include "nsIClassInfoImpl.h"
#include "prtime.h"

namespace mozilla::glean {

namespace impl {

void DatetimeMetric::Set(const PRExplodedTime* aValue) const {
  PRExplodedTime exploded;
  if (!aValue) {
    PR_ExplodeTime(PR_Now(), PR_LocalTimeParameters, &exploded);
  } else {
    exploded = *aValue;
  }

  auto id = ScalarIdForMetric(mId);
  if (id) {
    const uint32_t buflen = 64;  // More than enough for now.
    char buf[buflen];
    uint32_t written = PR_FormatTime(buf, buflen, "%FT%T%z", &exploded);
    if (written > 2 && written < 64) {
      // Format's still not quite there. Gotta put a `:` between timezone
      // hours and minutes
      buf[written] = '\0';
      buf[written - 1] = buf[written - 2];
      buf[written - 2] = buf[written - 3];
      buf[written - 3] = ':';
      Telemetry::ScalarSet(id.extract(), NS_ConvertASCIItoUTF16(buf));
    }
  }

  int32_t offset =
      exploded.tm_params.tp_gmt_offset + exploded.tm_params.tp_dst_offset;
  FogDatetime dt{exploded.tm_year,
                 static_cast<uint32_t>(exploded.tm_month + 1),
                 static_cast<uint32_t>(exploded.tm_mday),
                 static_cast<uint32_t>(exploded.tm_hour),
                 static_cast<uint32_t>(exploded.tm_min),
                 static_cast<uint32_t>(exploded.tm_sec),
                 static_cast<uint32_t>(exploded.tm_usec * 1000),
                 offset};
  fog_datetime_set(mId, &dt);
}

Result<Maybe<PRExplodedTime>, nsCString> DatetimeMetric::TestGetValue(
    const nsACString& aPingName) const {
  nsCString err;
  if (fog_datetime_test_get_error(mId, &err)) {
    return Err(err);
  }
  if (!fog_datetime_test_has_value(mId, &aPingName)) {
    return Maybe<PRExplodedTime>();
  }
  FogDatetime ret{0};
  fog_datetime_test_get_value(mId, &aPingName, &ret);
  PRExplodedTime pret{0};
  pret.tm_year = static_cast<PRInt16>(ret.year);
  pret.tm_month = static_cast<PRInt32>(ret.month - 1);
  pret.tm_mday = static_cast<PRInt32>(ret.day);
  pret.tm_hour = static_cast<PRInt32>(ret.hour);
  pret.tm_min = static_cast<PRInt32>(ret.minute);
  pret.tm_sec = static_cast<PRInt32>(ret.second);
  pret.tm_usec = static_cast<PRInt32>(ret.nano / 1000);  // truncated is fine
  pret.tm_params.tp_gmt_offset = static_cast<PRInt32>(ret.offset_seconds);
  return Some(std::move(pret));
}

}  // namespace impl

NS_IMPL_CLASSINFO(GleanDatetime, nullptr, 0, {0})
NS_IMPL_ISUPPORTS_CI(GleanDatetime, nsIGleanDatetime)

NS_IMETHODIMP
GleanDatetime::Set(PRTime aValue, uint8_t aOptionalArgc) {
  if (aOptionalArgc == 0) {
    mDatetime.Set();
  } else {
    PRExplodedTime exploded;
    PR_ExplodeTime(aValue, PR_LocalTimeParameters, &exploded);
    mDatetime.Set(&exploded);
  }

  return NS_OK;
}

NS_IMETHODIMP
GleanDatetime::TestGetValue(const nsACString& aStorageName, JSContext* aCx,
                            JS::MutableHandle<JS::Value> aResult) {
  auto result = mDatetime.TestGetValue(aStorageName);
  if (result.isErr()) {
    aResult.set(JS::UndefinedValue());
    LogToBrowserConsole(nsIScriptError::errorFlag,
                        NS_ConvertUTF8toUTF16(result.unwrapErr()));
    return NS_ERROR_LOSS_OF_SIGNIFICANT_DATA;
  }
  auto optresult = result.unwrap();
  if (optresult.isNothing()) {
    aResult.set(JS::UndefinedValue());
  } else {
    double millis =
        static_cast<double>(PR_ImplodeTime(optresult.ptr())) / PR_USEC_PER_MSEC;
    JS::Rooted<JSObject*> root(aCx,
                               JS::NewDateObject(aCx, JS::TimeClip(millis)));
    aResult.setObject(*root);
  }
  return NS_OK;
}

}  // namespace mozilla::glean
