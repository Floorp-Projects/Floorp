/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.telemetry;

import org.mozilla.gecko.sync.ExtendedJSONObject;

/**
 * Container for telemetry data and the data necessary to upload it.
 *
 * If you want to create one of these, consider extending
 * {@link TelemetryPingBuilder} or one of its descendants.
 */
public class TelemetryPing {
    private final String urlPath;
    private final ExtendedJSONObject payload;

    public TelemetryPing(final String urlPath, final ExtendedJSONObject payload) {
        this.urlPath = urlPath;
        this.payload = payload;
    }

    public String getURLPath() { return urlPath; }
    public ExtendedJSONObject getPayload() { return payload; }
}
