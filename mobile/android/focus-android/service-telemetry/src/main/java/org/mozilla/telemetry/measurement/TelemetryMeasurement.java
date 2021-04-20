/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.telemetry.measurement;

public abstract class TelemetryMeasurement {
    private final String fieldName;

    public TelemetryMeasurement(String fieldName) {
        this.fieldName = fieldName;
    }

    public String getFieldName() {
        return fieldName;
    }

    /**
     * Flush this measurement in order for serializing a ping. Calling this method should create
     * an Object representing the current state of this measurement. Optionally this measurement
     * might be reset.
     *
     * For example a TelemetryMeasurement implementation for the OS version of the device might
     * just return a String like "7.0.1".
     * However a TelemetryMeasurement implementation for counting the usage of search engines might
     * return a HashMap mapping search engine names to search counts. Additionally those counts will
     * be reset after flushing.
     */
    public abstract Object flush();
}
