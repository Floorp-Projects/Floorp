/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.gecko.sync.telemetry;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RobolectricTestRunner;

import java.util.HashMap;

@RunWith(RobolectricTestRunner.class)
public class TelemetryEventCollectorTest {

    @Test
    public void testValidateTelemetryEvent() {
        // object arg bytes len > 20.
        Assert.assertFalse(TelemetryEventCollector.validateTelemetryEvent(repeat("c", 21), "met", null, null));
        Assert.assertFalse(TelemetryEventCollector.validateTelemetryEvent(repeat("©", 11), "met", null, null));

        // method arg bytes len > 20.
        Assert.assertFalse(TelemetryEventCollector.validateTelemetryEvent("obj", repeat("c", 21), null, null));
        Assert.assertFalse(TelemetryEventCollector.validateTelemetryEvent("obj", repeat("©", 11), null, null));

        // val arg bytes len > 80.
        Assert.assertFalse(TelemetryEventCollector.validateTelemetryEvent("obj", "met", repeat("c", 81), null));
        Assert.assertFalse(TelemetryEventCollector.validateTelemetryEvent("obj", "met", repeat("©", 41), null));

        // extra arg len > 10.
        HashMap<String, String> extra = new HashMap<>();
        for (int i = 0; i < 11; i++) {
            extra.put("" + i, "" + i);
        }
        Assert.assertFalse(TelemetryEventCollector.validateTelemetryEvent("obj", "met", "val", extra));

        // extra arg key len > 15.
        extra.clear();
        for (int i = 0; i < 9; i++) {
            extra.put("" + i, "" + i);
        }
        extra.put("HashMapKeyInstanceFactory", "val");
        Assert.assertFalse(TelemetryEventCollector.validateTelemetryEvent("obj", "met", "val", extra));

        // extra arg val bytes len > 80.
        extra.clear();
        for (int i = 0; i < 8; i++) {
            extra.put("" + i, "" + i);
        }
        extra.put("key", repeat("©", 41));
        Assert.assertFalse(TelemetryEventCollector.validateTelemetryEvent("obj", "met", "val", extra));

        // Happy case.
        extra.clear();
        for (int i = 0; i < 10; i++) {
            extra.put(repeat(i + "", 15), repeat("" + i, 80));
        }
        Assert.assertTrue(TelemetryEventCollector.validateTelemetryEvent(repeat("c", 20), repeat("c", 20), repeat("c", 80), extra));
    }

    private static String repeat(String c, int times) {
        return new String(new char[times]).replace("\0", c);
    }
}
