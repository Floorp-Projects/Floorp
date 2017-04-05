/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.telemetry.ping;

import org.mozilla.telemetry.config.TelemetryConfiguration;
import org.mozilla.telemetry.measurement.CreatedDateMeasurement;
import org.mozilla.telemetry.measurement.DeviceMeasurement;
import org.mozilla.telemetry.measurement.LocaleMeasurement;
import org.mozilla.telemetry.measurement.OperatingSystemMeasurement;
import org.mozilla.telemetry.measurement.OperatingSystemVersionMeasurement;
import org.mozilla.telemetry.measurement.SequenceMeasurement;
import org.mozilla.telemetry.measurement.SessionCountMeasurement;
import org.mozilla.telemetry.measurement.SessionDurationMeasurement;
import org.mozilla.telemetry.measurement.TimezoneOffsetMeasurement;

/**
 * This mobile-specific ping is intended to provide the most critical data in a concise format,
 * allowing for frequent uploads.
 *
 * Since this ping is used to measure retention, it should be sent each time the app is opened.
 *
 * https://gecko.readthedocs.io/en/latest/toolkit/components/telemetry/telemetry/data/core-ping.html
 */
public class TelemetryCorePingBuilder extends TelemetryPingBuilder {
    public static final String TYPE = "core";
    private static final int VERSION = 7;

    private SessionCountMeasurement sessionCountMeasurement;
    private SessionDurationMeasurement sessionDurationMeasurement;

    public TelemetryCorePingBuilder(TelemetryConfiguration configuration) {
        super(configuration, TYPE, VERSION);

        addMeasurement(new SequenceMeasurement(configuration, this));
        addMeasurement(new LocaleMeasurement());
        addMeasurement(new OperatingSystemMeasurement());
        addMeasurement(new OperatingSystemVersionMeasurement());
        addMeasurement(new DeviceMeasurement());

        // TODO: "arch": <string>, // e.g. "arm", "x86" (Issue #10)
        // TODO: "profileDate": <pos integer>, // Profile creation date in days since UNIX epoch. (Issue #11)
        // TODO: "defaultSearch": <string>, // Identifier of the default search engine, e.g. "yahoo". (Issue #12)
        // TODO: "distributionId": <string>, // Distribution identifier (optional) (Issue #13)

        addMeasurement(new CreatedDateMeasurement());
        addMeasurement(new TimezoneOffsetMeasurement());
        addMeasurement(sessionCountMeasurement = new SessionCountMeasurement(configuration));
        addMeasurement(sessionDurationMeasurement = new SessionDurationMeasurement(configuration));

        // TODO: "searches": <object>, // Optional, object of search use counts in the format: { "engine.source": <pos integer> } e.g.: { "yahoo.suggestion": 3, "other.listitem": 1 } (Issue #14)
        // TODO: "experiments": [<string>, /* … */], // Optional, array of identifiers for the active experiments (Issue #15)
    }

    public SessionCountMeasurement getSessionCountMeasurement() {
        return sessionCountMeasurement;
    }

    public SessionDurationMeasurement getSessionDurationMeasurement() {
        return sessionDurationMeasurement;
    }
}
