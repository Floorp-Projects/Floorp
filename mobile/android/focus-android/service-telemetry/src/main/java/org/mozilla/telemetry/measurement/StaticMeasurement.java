/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.telemetry.measurement;

public class StaticMeasurement extends TelemetryMeasurement {
    private final Object value;

    public StaticMeasurement(String fieldName, Object value) {
        super(fieldName);

        this.value = value;
    }

    @Override
    public Object flush() {
        return value;
    }
}
