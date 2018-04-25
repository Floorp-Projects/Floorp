/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.telemetry.measurement;

import org.json.JSONObject;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mozilla.telemetry.config.TelemetryConfiguration;
import org.robolectric.RobolectricTestRunner;
import org.robolectric.RuntimeEnvironment;

import static org.junit.Assert.*;

@RunWith(RobolectricTestRunner.class)
public class SearchesMeasurementTest {
    @Test
    public void testDefault() {
        final TelemetryConfiguration configuration = new TelemetryConfiguration(RuntimeEnvironment.application);
        final SearchesMeasurement measurement = new SearchesMeasurement(configuration);

        final Object value = measurement.flush();
        assertNotNull(value);
        assertTrue(value instanceof JSONObject);

        final JSONObject searches = (JSONObject) value;
        assertTrue(searches.length() == 0);
    }

    @Test
    public void testRecording() throws Exception {
        final TelemetryConfiguration configuration = new TelemetryConfiguration(RuntimeEnvironment.application);
        final SearchesMeasurement measurement = new SearchesMeasurement(configuration);

        { // Record a bunch of things
            measurement.recordSearch(SearchesMeasurement.LOCATION_ACTIONBAR, "google");
            measurement.recordSearch(SearchesMeasurement.LOCATION_ACTIONBAR, "yahoo");
            measurement.recordSearch(SearchesMeasurement.LOCATION_ACTIONBAR, "wikipedia-es");
            measurement.recordSearch(SearchesMeasurement.LOCATION_ACTIONBAR, "wikipedia-es");

            measurement.recordSearch(SearchesMeasurement.LOCATION_SUGGESTION, "google");
            measurement.recordSearch(SearchesMeasurement.LOCATION_SUGGESTION, "yahoo");

            measurement.recordSearch(SearchesMeasurement.LOCATION_ACTIONBAR, "wikipedia-es");
            measurement.recordSearch(SearchesMeasurement.LOCATION_ACTIONBAR, "yahoo-mx");
            measurement.recordSearch(SearchesMeasurement.LOCATION_ACTIONBAR, "yahoo");

            measurement.recordSearch(SearchesMeasurement.LOCATION_SUGGESTION, "google");
            measurement.recordSearch(SearchesMeasurement.LOCATION_SUGGESTION, "google");

            final JSONObject searches = (JSONObject) measurement.flush();
            assertEquals(6, searches.length());

            assertTrue(searches.has("actionbar.google"));
            assertTrue(searches.has("actionbar.wikipedia-es"));
            assertTrue(searches.has("actionbar.yahoo-mx"));
            assertTrue(searches.has("actionbar.yahoo"));

            assertTrue(searches.has("suggestion.yahoo"));
            assertTrue(searches.has("suggestion.google"));

            assertEquals(1, searches.getInt("actionbar.google"));
            assertEquals(2, searches.getInt("actionbar.yahoo"));
            assertEquals(3, searches.getInt("actionbar.wikipedia-es"));
            assertEquals(1, searches.getInt("actionbar.yahoo-mx"));

            assertEquals(1, searches.getInt("suggestion.yahoo"));
            assertEquals(3, searches.getInt("suggestion.google"));
        }
        { // After flushing all values should be reset
            final JSONObject searches = (JSONObject) measurement.flush();
            assertEquals(0, searches.length());
        }
        { // New measurements should not pick up old values
            measurement.recordSearch(SearchesMeasurement.LOCATION_SUGGESTION, "google");
            measurement.recordSearch(SearchesMeasurement.LOCATION_SUGGESTION, "yahoo");

            final JSONObject searches = (JSONObject) measurement.flush();
            assertEquals(2, searches.length());

            assertTrue(searches.has("suggestion.yahoo"));
            assertTrue(searches.has("suggestion.google"));

            assertEquals(1, searches.getInt("suggestion.yahoo"));
            assertEquals(1, searches.getInt("suggestion.google"));
        }
    }
}
