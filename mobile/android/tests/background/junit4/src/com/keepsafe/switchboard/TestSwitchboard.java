package com.keepsafe.switchboard;

import android.content.Context;

import org.json.JSONException;
import org.json.JSONObject;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mozilla.gecko.background.testhelpers.TestRunner;
import org.mozilla.gecko.util.Experiments;
import org.robolectric.RuntimeEnvironment;

import java.io.IOException;
import java.util.List;
import java.util.UUID;

import static org.junit.Assert.*;

@RunWith(TestRunner.class)
public class TestSwitchboard {

    private static final String TEST_JSON = "{\"active-experiment\":{\"isActive\":true,\"values\":{\"foo\": true}},\"inactive-experiment\":{\"isActive\":false,\"values\":null}}";

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
        assertTrue("active-experiment is active", SwitchBoard.isInExperiment(c, "active-experiment"));
        assertFalse("inactive-experiment is inactive", SwitchBoard.isInExperiment(c, "inactive-experiment"));
    }

    @Test
    public void testExperimentValues() throws JSONException {
        final Context c = RuntimeEnvironment.application;
        assertTrue("active-experiment has values", SwitchBoard.hasExperimentValues(c, "active-experiment"));
        assertFalse("inactive-experiment doesn't have values", SwitchBoard.hasExperimentValues(c, "inactive-experiment"));

        final JSONObject values = SwitchBoard.getExperimentValuesFromJson(c, "active-experiment");
        assertNotNull("active-experiment values are not null", values);
        assertTrue("\"foo\" extra value is true", values.getBoolean("foo"));
    }

    @Test
    public void testGetActiveExperiments() {
        final Context c = RuntimeEnvironment.application;
        final List<String> experiments = SwitchBoard.getActiveExperiments(c);
        assertNotNull("List of active experiments is not null", experiments);

        assertTrue("List of active experiments contains active-experiment", experiments.contains("active-experiment"));
        assertFalse("List of active experiments does not contain inactive-experiment", experiments.contains("inactive-experiment"));
    }

    @Test
    public void testOverride() {
        final Context c = RuntimeEnvironment.application;

        Experiments.setOverride(c, "active-experiment", false);
        assertFalse("active-experiment is not active because of override", SwitchBoard.isInExperiment(c, "active-experiment"));
        assertFalse("List of active experiments does not contain active-experiment", SwitchBoard.getActiveExperiments(c).contains("active-experiment"));

        Experiments.clearOverride(c, "active-experiment");
        assertTrue("active-experiment is active after override is cleared", SwitchBoard.isInExperiment(c, "active-experiment"));
        assertTrue("List of active experiments contains active-experiment again", SwitchBoard.getActiveExperiments(c).contains("active-experiment"));

        Experiments.setOverride(c, "inactive-experiment", true);
        assertTrue("inactive-experiment is active because of override", SwitchBoard.isInExperiment(c, "inactive-experiment"));
        assertTrue("List of active experiments contains inactive-experiment", SwitchBoard.getActiveExperiments(c).contains("inactive-experiment"));

        Experiments.clearOverride(c, "inactive-experiment");
        assertFalse("inactive-experiment is inactive after override is cleared", SwitchBoard.isInExperiment(c, "inactive-experiment"));
        assertFalse("List of active experiments does not contain inactive-experiment again", SwitchBoard.getActiveExperiments(c).contains("inactive-experiment"));
    }

}
