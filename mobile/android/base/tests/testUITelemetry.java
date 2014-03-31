package org.mozilla.gecko.tests;

import org.mozilla.gecko.AppConstants;
import org.mozilla.gecko.PrefsHelper;
import org.mozilla.gecko.Telemetry;

import android.util.Log;

public class testUITelemetry extends JavascriptTest {
    // Prefix used to distinguish test events and sessions from
    // real ones. Used by the javascript part of the test.
    static final String TEST_PREFIX = "TEST-";

    public testUITelemetry() {
        super("testUITelemetry.js");
    }

    @Override
    public void testJavascript() throws Exception {
        blockForGeckoReady();

        // We can't run these tests unless telemetry is turned on --
        // the events will be dropped on the floor.
        Log.i("GeckoTest", "Enabling telemetry.");
        PrefsHelper.setPref(AppConstants.TELEMETRY_PREF_NAME, true);

        Log.i("GeckoTest", "Adding telemetry events.");
        try {
            Telemetry.sendUIEvent(TEST_PREFIX + "enone", "method0");
            Telemetry.startUISession(TEST_PREFIX + "foo");
            Telemetry.sendUIEvent(TEST_PREFIX + "efoo", "method1");
            Telemetry.startUISession(TEST_PREFIX + "foo");
            Telemetry.sendUIEvent(TEST_PREFIX + "efoo", "method2");
            Telemetry.startUISession(TEST_PREFIX + "bar");
            Telemetry.sendUIEvent(TEST_PREFIX + "efoobar", "method3", "foobarextras");
            Telemetry.stopUISession(TEST_PREFIX + "foo", "reasonfoo");
            Telemetry.sendUIEvent(TEST_PREFIX + "ebar", "method4", "barextras");
            Telemetry.stopUISession(TEST_PREFIX + "bar", "reasonbar");
            Telemetry.stopUISession(TEST_PREFIX + "bar", "reasonbar2");
            Telemetry.sendUIEvent(TEST_PREFIX + "enone", "method5");
        } catch (Exception e) {
            Log.e("GeckoTest", "Oops.", e);
        }

        Log.i("GeckoTest", "Running remaining JS test code.");
        super.testJavascript();
    }
}

