/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.telemetry.measurement;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RobolectricTestRunner;

import static org.junit.Assert.*;

@RunWith(RobolectricTestRunner.class)
public class StaticMeasurementTest {
    @Test
    public void testWithMultipleValues() {
        assertEquals(null, new StaticMeasurement("key", null).flush());

        assertEquals(true, new StaticMeasurement("key", true).flush());
        assertEquals(false, new StaticMeasurement("key", false).flush());

        assertEquals(0, new StaticMeasurement("key", 0).flush());
        assertEquals(1, new StaticMeasurement("key", 1).flush());
        assertEquals(-1, new StaticMeasurement("key", -1).flush());
        assertEquals(2093492, new StaticMeasurement("key", 2093492).flush());
        assertEquals(-99822, new StaticMeasurement("key", -99822).flush());
        assertEquals(Integer.MAX_VALUE, new StaticMeasurement("key", Integer.MAX_VALUE).flush());
        assertEquals(Integer.MIN_VALUE, new StaticMeasurement("key", Integer.MIN_VALUE).flush());

        assertEquals(0L, new StaticMeasurement("key", 0L).flush());
        assertEquals(1L, new StaticMeasurement("key", 1L).flush());
        assertEquals(-1L, new StaticMeasurement("key", -1L).flush());
        assertEquals(2093492L, new StaticMeasurement("key", 2093492L).flush());
        assertEquals(-99822L, new StaticMeasurement("key", -99822L).flush());
        assertEquals(-9872987239847L, new StaticMeasurement("key", -9872987239847L).flush());
        assertEquals(1230948239847239487L, new StaticMeasurement("key", 1230948239847239487L).flush());
        assertEquals(Long.MAX_VALUE, new StaticMeasurement("key", Long.MAX_VALUE).flush());
        assertEquals(Long.MIN_VALUE, new StaticMeasurement("key", Long.MIN_VALUE).flush());

        assertEquals("", new StaticMeasurement("key", "").flush());
        assertEquals("Hello", new StaticMeasurement("key", "Hello").flush());
        assertEquals("löäüß!?", new StaticMeasurement("key", "löäüß!?").flush());

        assertArrayEquals(new String[] { "duck", "dog", "cat"},
                (String[]) new StaticMeasurement("key", new String[] { "duck", "dog", "cat"}).flush());
    }
}
