/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.telemetry;

import org.mozilla.gecko.sync.ExtendedJSONObject;

/**
 * Container for telemetry data and the data necessary to upload it.
 *
 * The unique ID is used by a Store to manipulate its internal pings. Some ping
 * payloads already contain a unique ID (e.g. sequence number in core ping) and
 * this field can mirror that value.
 *
 * If you want to create one of these, consider extending
 * {@link org.mozilla.gecko.telemetry.pingbuilders.TelemetryPingBuilder}
 * or one of its descendants.
 */
public class TelemetryPing {
    private final String urlPath;
    private final ExtendedJSONObject payload;
    private final int uniqueID;

    public TelemetryPing(final String urlPath, final ExtendedJSONObject payload, final int uniqueID) {
        this.urlPath = urlPath;
        this.payload = payload;
        this.uniqueID = uniqueID;
    }

    public String getURLPath() { return urlPath; }
    public ExtendedJSONObject getPayload() { return payload; }
    public int getUniqueID() { return uniqueID; }
}
