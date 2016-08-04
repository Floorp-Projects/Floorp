package com.keepsafe.switchboard;

import android.content.Context;

import org.json.JSONException;
import org.json.JSONObject;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mozilla.gecko.background.testhelpers.TestRunner;
import org.mozilla.gecko.Experiments;
import org.mozilla.gecko.util.IOUtils;
import org.robolectric.RuntimeEnvironment;

import java.io.BufferedInputStream;
import java.io.BufferedReader;
import java.io.ByteArrayOutputStream;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.net.URL;
import java.util.List;
import java.util.UUID;

import static org.junit.Assert.*;

@RunWith(TestRunner.class)
public class TestSwitchboard {

    /**
     * Create a JSON response from a JSON file.
     */
    private String readFromFile(String fileName) throws IOException {
        URL url = getClass().getResource("/" + fileName);
        if (url == null) {
            throw new FileNotFoundException(fileName);
        }

        InputStream inputStream = null;
        ByteArrayOutputStream outputStream = null;

        try {
            inputStream = new BufferedInputStream(new FileInputStream(url.getPath()));
            InputStreamReader inputStreamReader = new InputStreamReader(inputStream);
            BufferedReader bufferReader = new BufferedReader(inputStreamReader, 8192);
            String line;
            StringBuilder resultContent = new StringBuilder();
            while ((line = bufferReader.readLine()) != null) {
                resultContent.append(line);
            }
            bufferReader.close();

            return resultContent.toString();

        } finally {
            IOUtils.safeStreamClose(inputStream);
        }
    }

    @Before
    public void setUp() throws IOException {
        final Context c = RuntimeEnvironment.application;
        Preferences.setDynamicConfigJson(c, readFromFile("experiments.json"));
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

    @Test
    public void testMatching() {
        final Context c = RuntimeEnvironment.application;
        assertTrue("is-experiment is matching", SwitchBoard.isInExperiment(c, "is-matching"));
        assertFalse("is-not-matching is not matching", SwitchBoard.isInExperiment(c, "is-not-matching"));
    }

    @Test
    public void testNotExisting() {
        final Context c = RuntimeEnvironment.application;
        assertFalse("F0O does not exists", SwitchBoard.isInExperiment(c, "F0O"));
        assertFalse("BaAaz does not exists", SwitchBoard.hasExperimentValues(c, "BaAaz"));
    }



}
