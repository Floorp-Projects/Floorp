/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/glean/bindings/Datetime.h"

#include "nsString.h"
#include "mozilla/Components.h"
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

#ifndef MOZ_GLEAN_ANDROID
  int32_t offset =
      exploded.tm_params.tp_gmt_offset + exploded.tm_params.tp_dst_offset;
  fog_datetime_set(mId, exploded.tm_year, exploded.tm_month + 1,
                   exploded.tm_mday, exploded.tm_hour, exploded.tm_min,
                   exploded.tm_sec, exploded.tm_usec * 1000, offset);
#endif
}

Maybe<nsCString> DatetimeMetric::TestGetValue(
    const nsACString& aPingName) const {
#ifdef MOZ_GLEAN_ANDROID
  Unused << mId;
  return Nothing();
#else
  if (!fog_datetime_test_has_value(mId, &aPingName)) {
    return Nothing();
  }
  nsCString ret;
  fog_datetime_test_get_value(mId, &aPingName, &ret);
  return Some(ret);
#endif
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
                            JS::MutableHandleValue aResult) {
  auto result = mDatetime.TestGetValue(aStorageName);
  if (result.isNothing()) {
    aResult.set(JS::UndefinedValue());
  } else {
    const NS_ConvertUTF8toUTF16 str(result.value());
    aResult.set(
        JS::StringValue(JS_NewUCStringCopyN(aCx, str.Data(), str.Length())));
  }
  return NS_OK;
}

}  // namespace mozilla::glean
