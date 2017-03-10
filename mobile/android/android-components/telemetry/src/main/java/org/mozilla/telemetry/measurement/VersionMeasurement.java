/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.telemetry.measurement;

public class VersionMeasurement extends StaticMeasurement {
    private static final String FIELD_NAME = "v";

    public VersionMeasurement(int version) {
        super(FIELD_NAME, version);

        if (version <= 0) {
            throw new IllegalArgumentException("Version should be a positive integer > 0");
        }
    }
}
