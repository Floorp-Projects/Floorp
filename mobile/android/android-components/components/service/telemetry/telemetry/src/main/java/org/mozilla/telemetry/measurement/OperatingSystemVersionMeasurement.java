/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.telemetry.measurement;

import android.os.Build;

public class OperatingSystemVersionMeasurement extends StaticMeasurement {
    private static final String FIELD_NAME = "osversion";

    public OperatingSystemVersionMeasurement() {
        super(FIELD_NAME, Integer.toString(Build.VERSION.SDK_INT));
    }
}
