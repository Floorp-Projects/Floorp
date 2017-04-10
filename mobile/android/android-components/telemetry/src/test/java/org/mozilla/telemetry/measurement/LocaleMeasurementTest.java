/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.telemetry.measurement;

import android.text.TextUtils;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RobolectricTestRunner;

import java.util.Locale;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;

@RunWith(RobolectricTestRunner.class)
public class LocaleMeasurementTest {
    @Test
    public void testDefaultValue() {
        final LocaleMeasurement measurement = new LocaleMeasurement();

        final Object value = measurement.flush();
        assertNotNull(value);
        assertTrue(value instanceof String);

        final String locale = (String) value;
        assertFalse(TextUtils.isEmpty(locale));
        assertEquals("en-US", locale);
    }

    @Test
    public void testWithChangedLocale() {
        final Locale defaultLocale = Locale.getDefault();

        try {
            Locale.setDefault(new Locale("fy", "NL"));

            final LocaleMeasurement measurement = new LocaleMeasurement();

            final Object value = measurement.flush();
            assertNotNull(value);
            assertTrue(value instanceof String);

            final String locale = (String) value;
            assertFalse(TextUtils.isEmpty(locale));
            assertEquals("fy-NL", locale);
        } finally {
            Locale.setDefault(defaultLocale);
        }
    }

    /**
     * iw_IL -> he-IL
     */
    @Test
    public void testReturnsFixedLanguageTag() {
        final Locale defaultLocale = Locale.getDefault();

        try {
            Locale.setDefault(new Locale("iw", "IL"));

            final LocaleMeasurement measurement = new LocaleMeasurement();

            final Object value = measurement.flush();
            assertNotNull(value);
            assertTrue(value instanceof String);

            final String locale = (String) value;
            assertFalse(TextUtils.isEmpty(locale));
            assertEquals("he-IL", locale);
        } finally {
            Locale.setDefault(defaultLocale);
        }
    }
}