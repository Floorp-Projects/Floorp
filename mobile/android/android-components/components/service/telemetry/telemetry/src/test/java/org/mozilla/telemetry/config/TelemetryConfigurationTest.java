/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.telemetry.config;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RobolectricTestRunner;
import org.robolectric.RuntimeEnvironment;

import java.io.File;

import static org.junit.Assert.*;

@RunWith(RobolectricTestRunner.class)
public class TelemetryConfigurationTest {
    @Test
    public void testBuilderSanity() {
        final TelemetryConfiguration configuration = new TelemetryConfiguration(RuntimeEnvironment.application)
                .setAppName("AwesomeApp")
                .setAppVersion("42.0")
                .setBuildId("20170330")
                .setCollectionEnabled(false)
                .setConnectTimeout(5000)
                .setDataDirectory(new File(RuntimeEnvironment.application.getCacheDir(), "telemetry-test"))
                .setInitialBackoffForUpload(120000)
                .setMinimumEventsForUpload(12)
                .setPreferencesImportantForTelemetry("cat", "dog", "duck")
                .setReadTimeout(7000)
                .setServerEndpoint("http://127.0.0.1")
                .setUserAgent("AwesomeTelemetry/29")
                .setUpdateChannel("release")
                .setUploadEnabled(false);

        assertEquals(RuntimeEnvironment.application, configuration.getContext());
        assertEquals("AwesomeApp", configuration.getAppName());
        assertEquals("42.0", configuration.getAppVersion());
        assertEquals("20170330", configuration.getBuildId());
        assertFalse(configuration.isCollectionEnabled());
        assertEquals(5000, configuration.getConnectTimeout());
        assertEquals(new File(RuntimeEnvironment.application.getCacheDir(), "telemetry-test").getAbsolutePath(),
                configuration.getDataDirectory().getAbsolutePath());
        assertEquals(120000, configuration.getInitialBackoffForUpload());
        assertEquals(12, configuration.getMinimumEventsForUpload());
        assertEquals(3, configuration.getPreferencesImportantForTelemetry().size());
        assertTrue(configuration.getPreferencesImportantForTelemetry().contains("cat"));
        assertTrue(configuration.getPreferencesImportantForTelemetry().contains("dog"));
        assertTrue(configuration.getPreferencesImportantForTelemetry().contains("duck"));
        assertEquals(7000, configuration.getReadTimeout());
        assertEquals("http://127.0.0.1", configuration.getServerEndpoint());
        assertEquals("AwesomeTelemetry/29", configuration.getUserAgent());
        assertEquals("release", configuration.getUpdateChannel());
        assertFalse(configuration.isUploadEnabled());
    }
}
