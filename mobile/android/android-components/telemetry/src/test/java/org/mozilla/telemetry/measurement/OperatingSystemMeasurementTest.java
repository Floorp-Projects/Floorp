/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.telemetry.measurement;

import android.text.TextUtils;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RobolectricTestRunner;

import static org.junit.Assert.*;

@RunWith(RobolectricTestRunner.class)
public class OperatingSystemMeasurementTest {
    @Test
    public void testValue() {
        final OperatingSystemMeasurement measurement = new OperatingSystemMeasurement();

        final Object value = measurement.flush();
        assertNotNull(value);
        assertTrue(value instanceof String);

        final String os = (String) value;
        assertFalse(TextUtils.isEmpty(os));
        assertEquals("Android", os);
    }
}
