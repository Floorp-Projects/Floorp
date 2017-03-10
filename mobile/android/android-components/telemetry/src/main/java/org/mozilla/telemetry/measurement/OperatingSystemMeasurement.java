/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.telemetry.measurement;

public class OperatingSystemMeasurement extends StaticMeasurement {
    private static final String FIELD_NAME = "os";
    private static final String FIELD_VALUE = "Android";

    public OperatingSystemMeasurement() {
        super(FIELD_NAME, FIELD_VALUE);
    }
}
