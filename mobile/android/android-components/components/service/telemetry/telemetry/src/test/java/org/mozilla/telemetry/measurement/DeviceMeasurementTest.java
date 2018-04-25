/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.telemetry.measurement;

import android.text.TextUtils;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RobolectricTestRunner;

import static org.junit.Assert.*;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.spy;

@RunWith(RobolectricTestRunner.class)
public class DeviceMeasurementTest {
    @Test
    public void testFormat() {
        final DeviceMeasurement measurement = spy(new DeviceMeasurement());
        doReturn("Samsung").when(measurement).getManufacturer();
        doReturn("Galaxy S5").when(measurement).getModel();

        final Object value = measurement.flush();
        assertNotNull(value);
        assertTrue(value instanceof String);

        final String device = (String) value;
        assertFalse(TextUtils.isEmpty(device));

        assertEquals("Samsung-Galaxy S5", device);
    }

    @Test
    public void testWithLongManufacturer() {
        final DeviceMeasurement measurement = spy(new DeviceMeasurement());
        doReturn("SuperMegaDevice Corp.").when(measurement).getManufacturer();
        doReturn("Flip One").when(measurement).getModel();

        final Object value = measurement.flush();
        assertNotNull(value);
        assertTrue(value instanceof String);

        final String device = (String) value;
        assertFalse(TextUtils.isEmpty(device));

        assertEquals("SuperMegaDev-Flip One", device);
    }

    @Test
    public void testWithLongModel() {
        final DeviceMeasurement measurement = spy(new DeviceMeasurement());
        doReturn("Banjo").when(measurement).getManufacturer();
        doReturn("Little Cricket Daywalker").when(measurement).getModel();

        final Object value = measurement.flush();
        assertNotNull(value);
        assertTrue(value instanceof String);

        final String device = (String) value;
        assertFalse(TextUtils.isEmpty(device));

        assertEquals("Banjo-Little Cricket Dayw", device);
    }
}