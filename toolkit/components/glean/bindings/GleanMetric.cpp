/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/glean/bindings/GleanMetric.h"

#include "nsWrapperCache.h"

namespace mozilla::glean {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(GleanMetric, mParent)

NS_IMPL_CYCLE_COLLECTING_ADDREF(GleanMetric)
NS_IMPL_CYCLE_COLLECTING_RELEASE(GleanMetric)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(GleanMetric)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

}  // namespace mozilla::glean
