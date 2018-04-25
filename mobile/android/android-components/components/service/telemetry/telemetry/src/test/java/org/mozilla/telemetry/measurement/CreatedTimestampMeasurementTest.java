/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.telemetry.measurement;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RobolectricTestRunner;

import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;

@RunWith(RobolectricTestRunner.class)
public class CreatedTimestampMeasurementTest {
    @Test
    public void testFormat() throws Exception {
        final CreatedTimestampMeasurement measurement = new CreatedTimestampMeasurement();

        final Object value = measurement.flush();
        assertNotNull(value);
        assertTrue(value instanceof Long);

        final long created = (Long) value;
        assertTrue(created > 0);

        // This is the timestamp of the point in time this test was written. Assuming that time is
        // not going backwards our timestamp created by this class should be bigger. If it is not
        // then either your clock is wrong or you are experiencing a rare event.
        assertTrue(created > 1490727495664L);
    }
}