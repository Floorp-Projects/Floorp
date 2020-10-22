/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/glean/Timespan.h"

#include "nsString.h"
#include "mozilla/Components.h"
#include "nsIClassInfoImpl.h"

namespace mozilla {
namespace glean {

NS_IMPL_CLASSINFO(GleanTimespan, nullptr, 0, {0})
NS_IMPL_ISUPPORTS_CI(GleanTimespan, nsIGleanTimespan)

NS_IMETHODIMP
GleanTimespan::Start(JSContext* cx) {
  this->mTimespan.Start();
  return NS_OK;
}

NS_IMETHODIMP
GleanTimespan::Stop(JSContext* cx) {
  this->mTimespan.Stop();
  return NS_OK;
}

NS_IMETHODIMP
GleanTimespan::TestHasValue(const nsACString& aStorageName, JSContext* cx,
                            bool* result) {
  *result =
      this->mTimespan.TestHasValue(PromiseFlatCString(aStorageName).get());
  return NS_OK;
}

NS_IMETHODIMP
GleanTimespan::TestGetValue(const nsACString& aStorageName, JSContext* cx,
                            int64_t* result) {
  *result =
      this->mTimespan.TestGetValue(PromiseFlatCString(aStorageName).get());
  return NS_OK;
}

}  // namespace glean
}  // namespace mozilla
