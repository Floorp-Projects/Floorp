package org.mozilla.gecko.tests;

import org.mozilla.gecko.Telemetry;

import android.util.Log;

public class testUITelemetry extends JavascriptTest {
    public testUITelemetry() {
        super("testUITelemetry.js");
    }

    @Override
    public void testJavascript() throws Exception {
        blockForGeckoReady();
        Log.i("GeckoTest", "Adding telemetry events.");

        try {
            Telemetry.sendUIEvent("enone", "method0");
            Telemetry.startUISession("foo");
            Telemetry.sendUIEvent("efoo", "method1");
            Telemetry.startUISession("foo");
            Telemetry.sendUIEvent("efoo", "method2");
            Telemetry.startUISession("bar");
            Telemetry.sendUIEvent("efoobar", "method3", "foobarextras");
            Telemetry.stopUISession("foo", "reasonfoo");
            Telemetry.sendUIEvent("ebar", "method4", "barextras");
            Telemetry.stopUISession("bar", "reasonbar");
            Telemetry.stopUISession("bar", "reasonbar2");
            Telemetry.sendUIEvent("enone", "method5");
        } catch (Exception e) {
            Log.e("GeckoTest", "Oops.", e);
        }

        Log.i("GeckoTest", "Running remaining JS test code.");
        super.testJavascript();
    }
}

