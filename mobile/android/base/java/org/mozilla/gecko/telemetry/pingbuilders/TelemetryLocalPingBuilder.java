/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.telemetry.pingbuilders;

import org.mozilla.gecko.sync.ExtendedJSONObject;
import org.mozilla.gecko.telemetry.TelemetryLocalPing;

import java.util.UUID;

abstract class TelemetryLocalPingBuilder {
    final ExtendedJSONObject payload = new ExtendedJSONObject();
    final String docID = UUID.randomUUID().toString();

    abstract TelemetryLocalPing build();
}
