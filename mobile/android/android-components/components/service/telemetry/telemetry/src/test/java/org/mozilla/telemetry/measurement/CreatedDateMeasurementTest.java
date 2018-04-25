/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.telemetry.measurement;

import android.text.TextUtils;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RobolectricTestRunner;

import java.text.SimpleDateFormat;
import java.util.Date;
import java.util.Locale;

import static org.junit.Assert.*;

@RunWith(RobolectricTestRunner.class)
public class CreatedDateMeasurementTest {
    @Test
    public void testFormat() throws Exception {
        final CreatedDateMeasurement measurement = new CreatedDateMeasurement();

        final Object value = measurement.flush();
        assertNotNull(value);
        assertTrue(value instanceof String);

        final String created = (String) value;
        assertFalse(TextUtils.isEmpty(created));

        final SimpleDateFormat format = new SimpleDateFormat("yyyy-MM-dd", Locale.US);
        final Date date = format.parse(created);
        assertNotNull(date);
    }
}