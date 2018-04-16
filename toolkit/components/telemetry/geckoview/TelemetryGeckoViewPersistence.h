/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


#ifndef GeckoViewTelemetryPersistence_h__
#define GeckoViewTelemetryPersistence_h__

namespace TelemetryGeckoViewPersistence {

/**
 * Initializes the GeckoView persistence.
 * This loads any measure that was previously persisted and then kicks
 * off the persistence timer that regularly serializes telemetry measurements
 * to the disk (off the main thread).
 *
 * Note: while this code should only be used in GeckoView, it's also
 * compiled on other platforms for test-coverage.
 */
void InitPersistence();

/**
 * Shuts down the GeckoView persistence.
 */
void DeInitPersistence();

/**
 * Clears any GeckoView persisted data.
 * This physically deletes persisted data files.
 */
void ClearPersistenceData();

} // namespace TelemetryGeckoViewPersistence

#endif // GeckoViewTelemetryPersistence_h__
