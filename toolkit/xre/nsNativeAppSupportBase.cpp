/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsNativeAppSupportBase.h"

nsNativeAppSupportBase::nsNativeAppSupportBase() {}

nsNativeAppSupportBase::~nsNativeAppSupportBase() {}

NS_IMPL_ISUPPORTS(nsNativeAppSupportBase, nsINativeAppSupport)

// Start answer defaults to OK.
NS_IMETHODIMP
nsNativeAppSupportBase::Start(bool* result) {
  *result = true;
  return NS_OK;
}

NS_IMETHODIMP
nsNativeAppSupportBase::Enable() { return NS_OK; }

NS_IMETHODIMP
nsNativeAppSupportBase::ReOpen() { return NS_OK; }

NS_IMETHODIMP
nsNativeAppSupportBase::OnLastWindowClosing() { return NS_OK; }
