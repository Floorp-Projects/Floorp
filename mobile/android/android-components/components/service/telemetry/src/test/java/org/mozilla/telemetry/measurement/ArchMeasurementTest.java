/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.telemetry.measurement;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RobolectricTestRunner;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.spy;

@RunWith(RobolectricTestRunner.class)
public class ArchMeasurementTest {
    @Test
    public void testValue() {
        final ArchMeasurement measurement = spy(new ArchMeasurement());
        doReturn("x86_64").when(measurement).getArchitecture();

        final Object value = measurement.flush();
        assertNotNull(value);
        assertTrue(value instanceof String);

        final String architecture = (String) value;
        assertEquals("x86_64", architecture);
    }

    @Test
    public void testDefaultValue() {
        final ArchMeasurement measurement = spy(new ArchMeasurement());
        assertEquals("armeabi-v7a", measurement.flush());
    }
}
