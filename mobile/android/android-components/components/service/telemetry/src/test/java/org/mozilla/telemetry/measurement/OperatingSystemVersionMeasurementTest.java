/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.telemetry.measurement;

import android.text.TextUtils;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RobolectricTestRunner;
import org.robolectric.annotation.Config;

import static org.junit.Assert.*;

@RunWith(RobolectricTestRunner.class)
public class OperatingSystemVersionMeasurementTest {
    @Test
    public void testValue() {
        final OperatingSystemVersionMeasurement measurement = new OperatingSystemVersionMeasurement();

        final Object value = measurement.flush();
        assertNotNull(value);
        assertTrue(value instanceof String);

        final String version = (String) value;
        assertFalse(TextUtils.isEmpty(version));
        assertEquals("28", version);
    }

    @Test
    @Config(sdk = 23)
    public void testAPI23() {
        final OperatingSystemVersionMeasurement measurement = new OperatingSystemVersionMeasurement();

        final Object value = measurement.flush();
        assertNotNull(value);
        assertTrue(value instanceof String);

        final String version = (String) value;
        assertFalse(TextUtils.isEmpty(version));
        assertEquals("23", version);
    }

    @Test
    @Config(sdk = 21)
    public void testAPI21() {
        final OperatingSystemVersionMeasurement measurement = new OperatingSystemVersionMeasurement();

        final Object value = measurement.flush();
        assertNotNull(value);
        assertTrue(value instanceof String);

        final String version = (String) value;
        assertFalse(TextUtils.isEmpty(version));
        assertEquals("21", version);
    }
}
