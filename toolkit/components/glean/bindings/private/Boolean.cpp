/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/glean/bindings/Boolean.h"

#include "nsString.h"
#include "mozilla/Components.h"
#include "nsIClassInfoImpl.h"

namespace mozilla {
namespace glean {

NS_IMPL_CLASSINFO(GleanBoolean, nullptr, 0, {0})
NS_IMPL_ISUPPORTS_CI(GleanBoolean, nsIGleanBoolean)

NS_IMETHODIMP
GleanBoolean::Set(bool aValue) {
  mBoolean.Set(aValue);
  return NS_OK;
}

NS_IMETHODIMP
GleanBoolean::TestGetValue(const nsACString& aStorageName,
                           JS::MutableHandleValue aResult) {
  auto result = mBoolean.TestGetValue(aStorageName);
  if (result.isNothing()) {
    aResult.set(JS::UndefinedValue());
  } else {
    aResult.set(JS::BooleanValue(result.value()));
  }
  return NS_OK;
}

}  // namespace glean
}  // namespace mozilla
