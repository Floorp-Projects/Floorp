/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/glean/bindings/Ping.h"

#include "mozilla/Components.h"
#include "nsIClassInfoImpl.h"
#include "nsString.h"

namespace mozilla::glean {

NS_IMPL_CLASSINFO(GleanPing, nullptr, 0, {0})
NS_IMPL_ISUPPORTS_CI(GleanPing, nsIGleanPing)

NS_IMETHODIMP
GleanPing::Submit(const nsACString& aReason) {
  mPing.Submit(aReason);
  return NS_OK;
}

}  // namespace mozilla::glean
