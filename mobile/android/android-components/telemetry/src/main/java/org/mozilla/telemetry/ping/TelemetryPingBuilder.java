/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.telemetry.ping;

import android.support.annotation.NonNull;
import android.support.annotation.RestrictTo;
import android.support.annotation.VisibleForTesting;

import org.mozilla.telemetry.config.TelemetryConfiguration;
import org.mozilla.telemetry.measurement.ClientIdMeasurement;
import org.mozilla.telemetry.measurement.TelemetryMeasurement;
import org.mozilla.telemetry.measurement.VersionMeasurement;

import java.util.LinkedHashMap;
import java.util.LinkedList;
import java.util.List;
import java.util.Map;
import java.util.UUID;

public abstract class TelemetryPingBuilder {
    private final String type;
    private final List<TelemetryMeasurement> measurements;

    private TelemetryConfiguration configuration;

    public TelemetryPingBuilder(@NonNull TelemetryConfiguration configuration, @NonNull String type, int version) {
        this.configuration = configuration;
        this.type = type;
        this.measurements = new LinkedList<>();

        // All pings contain a version and a client id
        addMeasurement(new VersionMeasurement(version));
        addMeasurement(new ClientIdMeasurement(configuration));
    }

    public TelemetryConfiguration getConfiguration() {
        return configuration;
    }

    @NonNull
    public String getType() {
        return type;
    }

    protected void addMeasurement(TelemetryMeasurement measurement) {
        measurements.add(measurement);
    }

    public boolean canBuild() {
        return true;
    }

    public TelemetryPing build() {
        final String documentId = generateDocumentId();

        return new TelemetryPing(
                getType(),
                documentId,
                getUploadPath(documentId),
                flushMeasurements());
    }

    protected abstract String getUploadPath(final String documentId);

    private Map<String, Object> flushMeasurements() {
        final Map<String, Object> measurementResults = new LinkedHashMap<>();

        for (TelemetryMeasurement measurement : measurements) {
            measurementResults.put(measurement.getFieldName(), measurement.flush());
        }

        return measurementResults;
    }

    @VisibleForTesting
    @RestrictTo(RestrictTo.Scope.LIBRARY)
    public String generateDocumentId() {
        return UUID.randomUUID().toString();
    }
}
