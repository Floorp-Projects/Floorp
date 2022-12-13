/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.telemetry.storage;

import org.mozilla.telemetry.ping.TelemetryPing;

public interface TelemetryStorage {
    interface TelemetryStorageCallback {
        boolean onTelemetryPingLoaded(String path, String serializedPing);
    }

    void store(TelemetryPing ping);

    boolean process(String pingType, TelemetryStorageCallback callback);

    int countStoredPings(String pingType);
}
