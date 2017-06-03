/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.telemetry;

import org.mozilla.gecko.sync.ExtendedJSONObject;

/**
 * Container for telemetry data and the data necessary to upload it.
 *
 * The doc ID is used by a Store to manipulate its internal pings and should
 * be the same value found in the urlPath.
 *
 * If you want to create one of these, consider extending
 * {@link org.mozilla.gecko.telemetry.pingbuilders.TelemetryPingBuilder}
 * or one of its descendants.
 */
public class TelemetryOutgoingPing implements TelemetryPing {
    private final ExtendedJSONObject payload;
    private final String docID;
    private final String urlPath;

    public TelemetryOutgoingPing(final String urlPath, final ExtendedJSONObject payload, final String docID) {
        this.payload = payload;
        this.docID = docID;
        this.urlPath = urlPath;
    }

    public String getURLPath() { return urlPath; }

    @Override
    public ExtendedJSONObject getPayload() {
        return payload;
    }

    @Override
    public String getDocID() {
        return docID;
    }
}
