/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.telemetry.ping;

import org.mozilla.telemetry.config.TelemetryConfiguration;
import org.mozilla.telemetry.measurement.*;

/**
 * A telemetry ping builder for events of type "mobile-event".
 *
 * See the schema for more details:
 *   https://github.com/mozilla-services/mozilla-pipeline-schemas/blob/master/schemas/telemetry/mobile-event/mobile-event.1.schema.json
 */
public class TelemetryMobileEventPingBuilder extends TelemetryPingBuilder {
    public static final String TYPE = "mobile-event";
    private static final int VERSION = 1;

    private EventsMeasurement eventsMeasurement;
    private ExperimentsMapMeasurement experimentsMapMeasurement;

    public TelemetryMobileEventPingBuilder(TelemetryConfiguration configuration) {
        super(configuration, TYPE, VERSION);

        addMeasurement(new ProcessStartTimestampMeasurement(configuration));
        addMeasurement(new SequenceMeasurement(configuration, this));
        addMeasurement(new LocaleMeasurement());
        addMeasurement(new OperatingSystemMeasurement());
        addMeasurement(new OperatingSystemVersionMeasurement());
        addMeasurement(new CreatedTimestampMeasurement());
        addMeasurement(new TimezoneOffsetMeasurement());
        addMeasurement(new SettingsMeasurement(configuration));
        addMeasurement(eventsMeasurement = new EventsMeasurement(configuration));
        addMeasurement(experimentsMapMeasurement = new ExperimentsMapMeasurement());
    }

    public EventsMeasurement getEventsMeasurement() {
        return eventsMeasurement;
    }

    public ExperimentsMapMeasurement getExperimentsMapMeasurement() {
        return experimentsMapMeasurement;
    }

    @Override
    public boolean canBuild() {
        return eventsMeasurement.getEventCount() >= getConfiguration().getMinimumEventsForUpload();
    }

    @Override
    protected String getUploadPath(final String documentId) {
        return super.getUploadPath(documentId) + "?v=4";
    }
}
