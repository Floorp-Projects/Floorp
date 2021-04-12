/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/glean/bindings/Timespan.h"

#include "nsString.h"
#include "mozilla/Components.h"
#include "nsIClassInfoImpl.h"

namespace mozilla::glean {

NS_IMPL_CLASSINFO(GleanTimespan, nullptr, 0, {0})
NS_IMPL_ISUPPORTS_CI(GleanTimespan, nsIGleanTimespan)

NS_IMETHODIMP
GleanTimespan::Start() {
  mTimespan.Start();
  return NS_OK;
}

NS_IMETHODIMP
GleanTimespan::Stop() {
  mTimespan.Stop();
  return NS_OK;
}

NS_IMETHODIMP
GleanTimespan::Cancel() {
  mTimespan.Cancel();
  return NS_OK;
}

NS_IMETHODIMP
GleanTimespan::SetRaw(uint32_t aDuration) {
  mTimespan.SetRaw(aDuration);
  return NS_OK;
}

NS_IMETHODIMP
GleanTimespan::TestGetValue(const nsACString& aStorageName,
                            JS::MutableHandleValue aResult) {
  auto result = mTimespan.TestGetValue(aStorageName);
  if (result.isNothing()) {
    aResult.set(JS::UndefinedValue());
  } else {
    aResult.set(JS::DoubleValue(result.value()));
  }
  return NS_OK;
}

}  // namespace mozilla::glean
