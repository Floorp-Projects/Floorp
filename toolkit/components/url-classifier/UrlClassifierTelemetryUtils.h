//* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef UrlClassifierTelemetryUtils_h__
#define UrlClassifierTelemetryUtils_h__

#include "mozilla/TypedEnumBits.h"
#include "nsISupportsImpl.h"

namespace mozilla {
namespace safebrowsing {

// We might need to expand the bucket here if telemetry shows lots of errors
// are neither connection errors nor DNS errors.
uint8_t NetworkErrorToBucket(nsresult rv);

// Map the HTTP response code to a Telemetry bucket
uint32_t HTTPStatusToBucket(uint32_t status);

enum UpdateTimeout {
  eNoTimeout = 0,
  eResponseTimeout = 1,
  eDownloadTimeout = 2,
};

}  // namespace safebrowsing
}  // namespace mozilla

#endif  // UrlClassifierTelemetryUtils_h__
