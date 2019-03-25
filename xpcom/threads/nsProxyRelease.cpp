/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsProxyRelease.h"
#include "nsThreadUtils.h"

namespace detail {

/* static */ void ProxyReleaseChooser<true>::ProxyReleaseISupports(
    const char* aName, nsIEventTarget* aTarget, nsISupports* aDoomed,
    bool aAlwaysProxy) {
  ::detail::ProxyRelease<nsISupports>(aName, aTarget, dont_AddRef(aDoomed),
                                      aAlwaysProxy);
}

}  // namespace detail

extern "C" {

// This function uses C linkage because it's exposed to Rust to support the
// `ThreadPtrHolder` wrapper in the `moz_task` crate.
void NS_ProxyReleaseISupports(const char* aName, nsIEventTarget* aTarget,
                              nsISupports* aDoomed, bool aAlwaysProxy) {
  NS_ProxyRelease(aName, aTarget, dont_AddRef(aDoomed), aAlwaysProxy);
}

}  // extern "C"
