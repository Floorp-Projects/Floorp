/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.telemetry.measurement;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RobolectricTestRunner;

import static org.junit.Assert.*;

@RunWith(RobolectricTestRunner.class)
public class VersionMeasurementTest {
    @Test
    public void testValue() {
        final VersionMeasurement measurement = new VersionMeasurement(1);

        final Object value = measurement.flush();
        assertNotNull(value);
        assertTrue(value instanceof Integer);

        final int version = (int) value;
        assertEquals(1, version);
    }

    @Test
    public void testDifferentValues() {
        assertEquals(2, (int) new VersionMeasurement(2).flush());
        assertEquals(5000, (int) new VersionMeasurement(5000).flush());
        assertEquals(Integer.MAX_VALUE, (int) new VersionMeasurement(Integer.MAX_VALUE).flush());
    }

    @Test(expected = IllegalArgumentException.class)
    public void testZeroShouldThrowException() {
        new VersionMeasurement(0);
    }

    @Test(expected = IllegalArgumentException.class)
    public void testNegativeVersionShouldThrowException() {
        new VersionMeasurement(-1);
    }
}
