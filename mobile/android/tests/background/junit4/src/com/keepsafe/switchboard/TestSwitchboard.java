package com.keepsafe.switchboard;

import android.content.Context;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mozilla.gecko.background.testhelpers.TestRunner;
import org.robolectric.RuntimeEnvironment;

import java.io.IOException;
import java.util.UUID;

import static org.junit.Assert.*;

@RunWith(TestRunner.class)
public class TestSwitchboard {

    private static final String TEST_JSON = "{\"active-experiment\":{\"isActive\":true,\"values\":null},\"inactive-experiment\":{\"isActive\":false,\"values\":null}}";

    @Before
    public void setUp() throws IOException {
        final Context c = RuntimeEnvironment.application;

        // Avoid hitting the network by setting a config directly.
        Preferences.setDynamicConfigJson(c, TEST_JSON);
    }

    @Test
    public void testDeviceUuidFactory() {
        final Context c = RuntimeEnvironment.application;
        final DeviceUuidFactory df = new DeviceUuidFactory(c);
        final UUID uuid = df.getDeviceUuid();
        assertNotNull("UUID is not null", uuid);
        assertEquals("DeviceUuidFactory always returns the same UUID", df.getDeviceUuid(), uuid);
    }

    @Test
    public void testIsInExperiment() {
        final Context c = RuntimeEnvironment.application;
        assertTrue("Active experiment is active", SwitchBoard.isInExperiment(c, "active-experiment"));
        assertFalse("Inactive experiment is inactive", SwitchBoard.isInExperiment(c, "inactive-experiment"));
    }

}
