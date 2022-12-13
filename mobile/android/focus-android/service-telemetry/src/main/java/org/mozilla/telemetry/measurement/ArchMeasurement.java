/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.telemetry.measurement;

import android.os.Build;
import androidx.annotation.VisibleForTesting;

import java.util.Locale;

public class ArchMeasurement extends TelemetryMeasurement {
    private static final String FIELD_NAME = "arch";

    public ArchMeasurement() {
        super(FIELD_NAME);
    }

    @Override
    public Object flush() {
        return getArchitecture();
    }

    @VisibleForTesting String getArchitecture() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP) {
            return Build.SUPPORTED_ABIS[0];
        } else {
            //noinspection deprecation
            return Build.CPU_ABI;
        }
    }
}
