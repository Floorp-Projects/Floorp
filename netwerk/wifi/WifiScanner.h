/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#pragma once

#include "nsTArray.h"
#include "mozilla/RefPtr.h"

class nsIWifiAccessPoint;

namespace mozilla {

class WifiScanner {
 public:
  /**
   * GetAccessPointsFromWLAN
   *
   * Scans the available wireless interfaces for nearby access points and
   * populates the supplied collection with them
   *
   * @param accessPoints The collection to populate with available APs
   * @return NS_OK on success, failure codes on failure
   */
  virtual nsresult GetAccessPointsFromWLAN(
      nsTArray<RefPtr<nsIWifiAccessPoint>>& accessPoints) = 0;

  virtual ~WifiScanner() = default;
};

}  // namespace mozilla
