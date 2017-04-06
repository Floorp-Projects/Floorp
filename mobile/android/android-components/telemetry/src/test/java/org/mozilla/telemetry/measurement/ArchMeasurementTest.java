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
public class ArchMeasurementTest {
    @Test
    public void testEmptyArray() {
        final ArchMeasurement measurement = spy(new ArchMeasurement());
        doReturn(new String[0]).when(measurement).getSupportedAbis();

        final Object value = measurement.flush();
        assertNotNull(value);
        assertTrue(value instanceof String);

        final String arch = (String) value;
        assertFalse(TextUtils.isEmpty(arch));

        assertEquals("?", arch);
    }

    @Test
    public void testARM() {
        final ArchMeasurement measurement = spy(new ArchMeasurement());
        doReturn(new String[] {
                "armeabi-v7a"
        }).when(measurement).getSupportedAbis();

        assertEquals("arm", measurement.flush());
    }

    @Test
    public void testX86() {
        final ArchMeasurement measurement = spy(new ArchMeasurement());
        doReturn(new String[] {
                "x86"
        }).when(measurement).getSupportedAbis();

        assertEquals("x86", measurement.flush());
    }

    @Test
    public void testMIPS() {
        final ArchMeasurement measurement = spy(new ArchMeasurement());
        doReturn(new String[] {
                "mips"
        }).when(measurement).getSupportedAbis();

        assertEquals("mips", measurement.flush());
    }

    @Test
    public void testARM64() {
        final ArchMeasurement measurement = spy(new ArchMeasurement());
        doReturn(new String[] {
                "arm64-v8a"
        }).when(measurement).getSupportedAbis();

        assertEquals("arm", measurement.flush());
    }

    @Test
    public void testX8664() {
        final ArchMeasurement measurement = spy(new ArchMeasurement());
        doReturn(new String[] {
                "x86_64"
        }).when(measurement).getSupportedAbis();

        assertEquals("x86", measurement.flush());
    }

    @Test
    public void testMIPS64() {
        final ArchMeasurement measurement = spy(new ArchMeasurement());
        doReturn(new String[] {
                "mips64"
        }).when(measurement).getSupportedAbis();

        assertEquals("mips", measurement.flush());
    }

    @Test
    public void testUnknownValues() {
        final ArchMeasurement measurement = spy(new ArchMeasurement());
        doReturn(new String[] {
                "fun",
                "star",
                "blazer"
        }).when(measurement).getSupportedAbis();

        assertEquals("?", measurement.flush());
    }

    @Test
    public void testARMInBetween() {
        final ArchMeasurement measurement = spy(new ArchMeasurement());
        doReturn(new String[] {
                "fun",
                "star",
                "armeabi-v7a",
                "blazer",
                "x86"
        }).when(measurement).getSupportedAbis();

        assertEquals("arm", measurement.flush());
    }

    @Test
    public void testX86InBetween() {
        final ArchMeasurement measurement = spy(new ArchMeasurement());
        doReturn(new String[] {
                "fun",
                "x86_64",
                "star",
                "armeabi-v7a",
                "blazer"
        }).when(measurement).getSupportedAbis();

        assertEquals("x86", measurement.flush());
    }
}
