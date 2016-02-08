/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.telemetry;

import org.mozilla.gecko.sync.ExtendedJSONObject;

/**
 * Container for telemetry data and the data necessary to upload it.
 */
public class TelemetryPing {
    private final String url;
    private final ExtendedJSONObject payload;

    public TelemetryPing(final String url, final ExtendedJSONObject payload) {
        this.url = url;
        this.payload = payload;
    }

    public String getURL() { return url; }
    public ExtendedJSONObject getPayload() { return payload; }
}
