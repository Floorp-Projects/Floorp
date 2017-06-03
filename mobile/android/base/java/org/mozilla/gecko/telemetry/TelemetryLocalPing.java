/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.telemetry;

import android.support.annotation.Nullable;

import org.mozilla.gecko.sync.ExtendedJSONObject;

/**
 * A "local" ping, which is not intended to be uploaded, but simply stored for later processing.
 * Currently, many instances of local pings are bundled into a Sync Ping at appropriate moments.
 */
public class TelemetryLocalPing implements TelemetryPing {
    private final ExtendedJSONObject payload;
    private final String docID;

    public TelemetryLocalPing(final ExtendedJSONObject payload, final String docID) {
        this.payload = payload;
        this.docID = docID;
    }

    public ExtendedJSONObject getPayload() { return payload; }
    public String getDocID() { return docID; }

    // Following the path of least resistance to avoid decoupling a ping from where it should
    // be uploaded, for a local ping we declare that the path is nullable, and in fact it's always null.
    @Nullable
    public String getURLPath() {
        return null;
    }
}
