/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.telemetry;

import org.mozilla.gecko.sync.ExtendedJSONObject;

/**
 * Gathers telemetry details about an individual sync stage.
 * Implementation note: there are no getters/setters to avoid unnecessary verboseness.
 * This data expected to be write-only from within SyncStages, and read-only from TelemetryCollector.
 * Although there shouldn't be concurrent access, it's possible that we'll be reading/writing these
 * values from different threads - hence `volatile` to ensure visibility.
 */
public class TelemetryStageCollector {
    private final TelemetryCollector syncCollector;

    public volatile long started = 0L;
    public volatile long finished = 0L;
    public volatile int inbound = 0;
    public volatile int inboundStored = 0;
    public volatile int inboundFailed = 0;
    public volatile int outbound = 0;
    public volatile int outboundStored = 0;
    public volatile int outboundFailed = 0;
    public volatile int reconciled = 0;
    public volatile ExtendedJSONObject error = null;

    public TelemetryStageCollector(TelemetryCollector syncCollector) {
        this.syncCollector = syncCollector;
    }

    public TelemetryCollector getSyncCollector() {
        return this.syncCollector;
    }
}
