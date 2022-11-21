/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AboutWindowsMessages.h"

#include "mozilla/ClearOnShutdown.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/StaticPtr.h"
#include "nsThreadUtils.h"

namespace mozilla {

static StaticRefPtr<AboutWindowsMessages> sSingleton;

NS_IMPL_ISUPPORTS(AboutWindowsMessages, nsIAboutWindowsMessages);

/*static*/
already_AddRefed<AboutWindowsMessages> AboutWindowsMessages::GetSingleton() {
  if (!sSingleton) {
    sSingleton = new AboutWindowsMessages;
    ClearOnShutdown(&sSingleton);
  }

  return do_AddRef(sSingleton);
}
}  // namespace mozilla
