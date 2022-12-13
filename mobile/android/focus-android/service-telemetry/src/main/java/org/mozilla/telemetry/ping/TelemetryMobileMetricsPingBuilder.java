/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.telemetry.ping;

import org.json.JSONObject;
import org.mozilla.telemetry.config.TelemetryConfiguration;
import org.mozilla.telemetry.measurement.ArchMeasurement;
import org.mozilla.telemetry.measurement.CreatedDateMeasurementNew;
import org.mozilla.telemetry.measurement.CreatedTimestampMeasurementNew;
import org.mozilla.telemetry.measurement.DeviceMeasurement;
import org.mozilla.telemetry.measurement.FirstRunProfileDateMeasurement;
import org.mozilla.telemetry.measurement.LocaleMeasurement;
import org.mozilla.telemetry.measurement.MetricsMeasurement;
import org.mozilla.telemetry.measurement.OperatingSystemMeasurement;
import org.mozilla.telemetry.measurement.OperatingSystemVersionMeasurement;
import org.mozilla.telemetry.measurement.ProcessStartTimestampMeasurement;
import org.mozilla.telemetry.measurement.SequenceMeasurement;
import org.mozilla.telemetry.measurement.TimezoneOffsetMeasurement;

/**
 * A telemetry ping builder for events of type "mobile-metrics".
 *
 * See the schema for more details:
 *   https://github.com/mozilla-services/mozilla-pipeline-schemas/blob/master/schemas/telemetry/mobile-metrics/mobile-metrics.1.schema.json
 */
public class TelemetryMobileMetricsPingBuilder extends TelemetryPingBuilder {
    public static final String TYPE = "mobile-metrics";
    private static final int VERSION = 1;

    public TelemetryMobileMetricsPingBuilder(JSONObject snapshots, TelemetryConfiguration configuration) {
        super(configuration, TYPE, VERSION);

        addMeasurement(new ProcessStartTimestampMeasurement(configuration));
        addMeasurement(new SequenceMeasurement(configuration, this));
        addMeasurement(new LocaleMeasurement());
        addMeasurement(new DeviceMeasurement());
        addMeasurement(new ArchMeasurement());
        addMeasurement(new FirstRunProfileDateMeasurement(configuration));
        addMeasurement(new OperatingSystemMeasurement());
        addMeasurement(new OperatingSystemVersionMeasurement());
        addMeasurement(new CreatedDateMeasurementNew());
        addMeasurement(new CreatedTimestampMeasurementNew());
        addMeasurement(new TimezoneOffsetMeasurement());
        addMeasurement(new MetricsMeasurement(snapshots));
    }

    @Override
    protected String getUploadPath(final String documentId) {
        return super.getUploadPath(documentId) + "?v=4";
    }
}
