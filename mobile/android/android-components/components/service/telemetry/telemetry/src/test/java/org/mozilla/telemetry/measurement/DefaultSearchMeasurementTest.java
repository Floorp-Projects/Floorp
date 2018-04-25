/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.telemetry.measurement;

import android.text.TextUtils;

import org.json.JSONObject;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RobolectricTestRunner;

import org.mozilla.telemetry.measurement.DefaultSearchMeasurement.DefaultSearchEngineProvider;

import static org.junit.Assert.*;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;

@RunWith(RobolectricTestRunner.class)
public class DefaultSearchMeasurementTest {
    @Test
    public void testDefault() {
        final DefaultSearchMeasurement measurement = new DefaultSearchMeasurement();

        final Object value = measurement.flush();
        assertNotNull(value);
        assertEquals(JSONObject.NULL, value);
    }

    @Test
    public void testProviderReturningNull() {
        final DefaultSearchEngineProvider provider = mock(DefaultSearchEngineProvider.class);
        doReturn(null).when(provider).getDefaultSearchEngineIdentifier();

        final DefaultSearchMeasurement measurement = new DefaultSearchMeasurement();
        measurement.setDefaultSearchEngineProvider(provider);

        final Object value = measurement.flush();
        assertNotNull(value);
        assertEquals(JSONObject.NULL, value);

        verify(provider).getDefaultSearchEngineIdentifier();
    }

    @Test
    public void testProviderReturningValue() {
        final DefaultSearchEngineProvider provider = mock(DefaultSearchEngineProvider.class);
        doReturn("TestValue0").when(provider).getDefaultSearchEngineIdentifier();

        final DefaultSearchMeasurement measurement = new DefaultSearchMeasurement();
        measurement.setDefaultSearchEngineProvider(provider);

        final Object value = measurement.flush();
        assertNotNull(value);
        assertTrue(value instanceof String);

        final String defaultSearchEngine = (String) value;
        assertFalse(TextUtils.isEmpty(defaultSearchEngine));

        verify(provider).getDefaultSearchEngineIdentifier();
    }
}
