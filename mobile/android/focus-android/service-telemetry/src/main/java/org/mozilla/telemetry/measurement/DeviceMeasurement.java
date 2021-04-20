/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.telemetry.measurement;

import android.os.Build;
import androidx.annotation.NonNull;
import androidx.annotation.VisibleForTesting;

import org.mozilla.telemetry.util.StringUtils;

public class DeviceMeasurement extends TelemetryMeasurement {
    private static final String FIELD_NAME = "device";

    public DeviceMeasurement() {
        super(FIELD_NAME);
    }

    @Override
    public Object flush() {
        // We limit the device descriptor to 32 characters because it can get long. We give fewer
        // characters to the manufacturer because we're less likely to have manufacturers with
        // similar names than we are for a manufacturer to have two devices with the similar names
        // (e.g. Galaxy S6 vs. Galaxy Note 6).
        return StringUtils.safeSubstring(getManufacturer(), 0, 12) + '-' +  StringUtils.safeSubstring(getModel(), 0, 19);
    }

    @VisibleForTesting String getManufacturer() {
        return Build.MANUFACTURER;
    }

    @VisibleForTesting String getModel() {
        return Build.MODEL;
    }
}
