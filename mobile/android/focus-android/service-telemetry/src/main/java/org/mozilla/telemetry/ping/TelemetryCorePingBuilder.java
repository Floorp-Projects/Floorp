/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.telemetry.ping;

import org.mozilla.telemetry.config.TelemetryConfiguration;
import org.mozilla.telemetry.measurement.ArchMeasurement;
import org.mozilla.telemetry.measurement.CreatedDateMeasurement;
import org.mozilla.telemetry.measurement.DefaultSearchMeasurement;
import org.mozilla.telemetry.measurement.DeviceMeasurement;
import org.mozilla.telemetry.measurement.ExperimentsMeasurement;
import org.mozilla.telemetry.measurement.FirstRunProfileDateMeasurement;
import org.mozilla.telemetry.measurement.LocaleMeasurement;
import org.mozilla.telemetry.measurement.OperatingSystemMeasurement;
import org.mozilla.telemetry.measurement.OperatingSystemVersionMeasurement;
import org.mozilla.telemetry.measurement.SearchesMeasurement;
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
    private DefaultSearchMeasurement defaultSearchMeasurement;
    private SearchesMeasurement searchesMeasurement;
    private ExperimentsMeasurement experimentsMeasurement;

    public TelemetryCorePingBuilder(TelemetryConfiguration configuration) {
        super(configuration, TYPE, VERSION);

        addMeasurement(new SequenceMeasurement(configuration, this));
        addMeasurement(new LocaleMeasurement());
        addMeasurement(new OperatingSystemMeasurement());
        addMeasurement(new OperatingSystemVersionMeasurement());
        addMeasurement(new DeviceMeasurement());
        addMeasurement(new ArchMeasurement());
        addMeasurement(new FirstRunProfileDateMeasurement(configuration));
        addMeasurement(defaultSearchMeasurement = new DefaultSearchMeasurement());
        addMeasurement(new CreatedDateMeasurement());
        addMeasurement(new TimezoneOffsetMeasurement());
        addMeasurement(sessionCountMeasurement = new SessionCountMeasurement(configuration));
        addMeasurement(sessionDurationMeasurement = new SessionDurationMeasurement(configuration));
        addMeasurement(searchesMeasurement = new SearchesMeasurement(configuration));
        addMeasurement(experimentsMeasurement = new ExperimentsMeasurement());
    }

    public SessionCountMeasurement getSessionCountMeasurement() {
        return sessionCountMeasurement;
    }

    public SessionDurationMeasurement getSessionDurationMeasurement() {
        return sessionDurationMeasurement;
    }

    public SearchesMeasurement getSearchesMeasurement() {
        return searchesMeasurement;
    }

    public DefaultSearchMeasurement getDefaultSearchMeasurement() {
        return defaultSearchMeasurement;
    }

    public ExperimentsMeasurement getExperimentsMeasurement() {
        return experimentsMeasurement;
    }

    @Override
    protected String getUploadPath(final String documentId) {
        return super.getUploadPath(documentId) + "?v=4";
    }
}
