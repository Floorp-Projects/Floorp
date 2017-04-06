/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.telemetry.measurement;

import android.os.Build;
import android.support.annotation.VisibleForTesting;

import java.util.Locale;

public class ArchMeasurement extends TelemetryMeasurement {
    private static final String FIELD_NAME = "arch";

    private static final String VALUE_X86 = "x86";
    private static final String VALUE_ARM = "arm";
    private static final String VALUE_MIPS = "mips";
    private static final String VALUE_UNKNOWN = "?";

    public ArchMeasurement() {
        super(FIELD_NAME);
    }

    @Override
    public Object flush() {
        for (String abi : getSupportedAbis()) {
            abi = abi.toLowerCase(Locale.US);

            if (abi.contains(VALUE_X86)) {
                return VALUE_X86;
            } else if (abi.contains(VALUE_ARM)) {
                return VALUE_ARM;
            } else if (abi.contains(VALUE_MIPS)) {
                return VALUE_MIPS;
            }
        }

        return VALUE_UNKNOWN;
    }

    @VisibleForTesting String[] getSupportedAbis() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP) {
            return Build.SUPPORTED_ABIS;
        } else {
            //noinspection deprecation
            return new String[] { Build.CPU_ABI };
        }
    }
}
