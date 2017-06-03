/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.telemetry.pingbuilders;

import android.os.Bundle;

import org.mozilla.gecko.telemetry.TelemetryLocalPing;

/**
 * Local ping builder which understands how to process event data.
 * This is a placeholder, to be implemented in Bug 1363924.
 */
public class TelemetrySyncEventPingBuilder extends TelemetryLocalPingBuilder {
    public TelemetrySyncEventPingBuilder fromEventTelemetry(Bundle data) {
        return this;
    }

    @Override
    public TelemetryLocalPing build() {
        throw new UnsupportedOperationException();
    }
}
