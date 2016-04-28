/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.telemetry;

import org.mozilla.gecko.sync.ExtendedJSONObject;
import org.mozilla.gecko.telemetry.stores.TelemetryPingStore;

/**
 * Container for telemetry data and the data necessary to upload it, as
 * returned by a {@link TelemetryPingStore}.
 */
public class TelemetryPingFromStore extends TelemetryPing {
    private final int uniqueID;

    public TelemetryPingFromStore(final String url, final ExtendedJSONObject payload, final int uniqueID) {
        super(url, payload);
        this.uniqueID = uniqueID;
    }

    public int getUniqueID() { return uniqueID; }
}

