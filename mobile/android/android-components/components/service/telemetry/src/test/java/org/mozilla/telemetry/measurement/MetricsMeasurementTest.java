/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.telemetry.measurement;

import org.json.JSONObject;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RobolectricTestRunner;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;

@RunWith(RobolectricTestRunner.class)
public class MetricsMeasurementTest {
    @Test
    public void testFlushingEmptyMeasurement() {
        final MetricsMeasurement measurement = new MetricsMeasurement(new JSONObject());

        final Object value = measurement.flush();

        assertNotNull(value);
        assertTrue(value instanceof JSONObject);

        final JSONObject jsonObject = (JSONObject) value;

        assertEquals(0, jsonObject.length());
    }
}