/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.telemetry.measurement;

/**
 * The field 'created' from CreatedTimestampMeasurement will be deprecated for the `createdTimestamp` field
 */
public class CreatedTimestampMeasurementNew extends TelemetryMeasurement {
    private static final String FIELD_NAME = "createdTimestamp";

    public CreatedTimestampMeasurementNew() {
        super(FIELD_NAME);
    }

    @Override
    public Object flush() {
        return System.currentTimeMillis();
    }
}
