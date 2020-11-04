/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/glean/Uuid.h"

#include "nsString.h"
#include "mozilla/Components.h"
#include "nsIClassInfoImpl.h"

namespace mozilla {
namespace glean {

NS_IMPL_CLASSINFO(GleanUuid, nullptr, 0, {0})
NS_IMPL_ISUPPORTS_CI(GleanUuid, nsIGleanUuid)

NS_IMETHODIMP
GleanUuid::Set(const nsACString& value, JSContext* cx) {
  this->mUuid.Set(value);
  return NS_OK;
}

NS_IMETHODIMP
GleanUuid::GenerateAndSet(JSContext* cx) {
  this->mUuid.GenerateAndSet();
  return NS_OK;
}

NS_IMETHODIMP
GleanUuid::TestHasValue(const nsACString& aStorageName, JSContext* cx,
                        bool* result) {
  *result = this->mUuid.TestHasValue(PromiseFlatCString(aStorageName).get());
  return NS_OK;
}

NS_IMETHODIMP
GleanUuid::TestGetValue(const nsACString& aStorageName, JSContext* cx,
                        nsACString& result) {
  result.Assign(
      this->mUuid.TestGetValue(PromiseFlatCString(aStorageName).get()));
  return NS_OK;
}

}  // namespace glean
}  // namespace mozilla
