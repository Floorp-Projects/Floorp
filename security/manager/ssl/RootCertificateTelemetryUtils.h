/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef RootCertificateTelemetryUtils_h
#define RootCertificateTelemetryUtils_h

#include "mozilla/Telemetry.h"
#include "certt.h"

namespace mozilla { namespace psm {

// Note: New CAs will show up as UNKNOWN_ROOT until
// RootHashes.inc is updated to include them. 0 is reserved by
// genRootCAHashes.js for the unknowns.
#define ROOT_CERTIFICATE_UNKNOWN  0
#define ROOT_CERTIFICATE_HASH_FAILURE -1

int32_t
RootCABinNumber(const SECItem* cert);

void
AccumulateTelemetryForRootCA(mozilla::Telemetry::ID probe, const CERTCertificate* cert);

} // namespace psm
} // namespace mozilla

#endif // RootCertificateTelemetryUtils_h
