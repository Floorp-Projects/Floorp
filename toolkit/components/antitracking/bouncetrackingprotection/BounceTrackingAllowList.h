/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_BounceTrackingAllowList_h
#define mozilla_BounceTrackingAllowList_h

#include "mozilla/ContentBlockingAllowList.h"

namespace mozilla {

// Extends ContentBlockingAllowListCache to also check for "cookie" allow
// permissions.
class BounceTrackingAllowList final : public ContentBlockingAllowListCache {
 private:
  nsTArray<nsCString> GetAllowListPermissionTypes() override;
  nsresult IsAllowListPermission(nsIPermission* aPermission,
                                 bool* aResult) override;
};

}  // namespace mozilla

#endif
