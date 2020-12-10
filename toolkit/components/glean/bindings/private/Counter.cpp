/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/glean/Counter.h"

#include "nsString.h"
#include "mozilla/Components.h"
#include "nsIClassInfoImpl.h"

namespace mozilla::glean {

NS_IMPL_CLASSINFO(GleanCounter, nullptr, 0, {0})
NS_IMPL_ISUPPORTS_CI(GleanCounter, nsIGleanCounter)

NS_IMETHODIMP
GleanCounter::Add(uint32_t aAmount) {
  mCounter.Add(aAmount);
  return NS_OK;
}

NS_IMETHODIMP
GleanCounter::TestGetValue(const nsACString& aStorageName,
                           JS::MutableHandleValue aResult) {
  auto result = mCounter.TestGetValue(aStorageName);
  if (result.isNothing()) {
    aResult.set(JS::UndefinedValue());
  } else {
    aResult.set(JS::Int32Value(result.value()));
  }
  return NS_OK;
}

}  // namespace mozilla::glean
