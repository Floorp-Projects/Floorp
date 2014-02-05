package org.mozilla.gecko.tests;

import android.os.Build;

import org.json.JSONArray;
import org.json.JSONObject;

import org.mozilla.gecko.*;

/**
 * Tests that Flash is working
 * - loads a page containing a Flash plugin
 * - verifies it rendered properly
 */
public class testAdobeFlash extends PixelTest {
    @Override
    protected int getTestType() {
        return TEST_MOCHITEST;
    }

    public void testLoad() {
        // This test only works on ICS and higher
        if (Build.VERSION.SDK_INT < 15) {
            blockForGeckoReady();
            return;
        }

        // Enable plugins
        JSONObject jsonPref = new JSONObject();
        try {
            jsonPref.put("name", "plugin.enable");
            jsonPref.put("type", "string");
            jsonPref.put("value", "1");
            mActions.sendGeckoEvent("Preferences:Set", jsonPref.toString());

            // Wait for confirmation of the pref change before proceeding with the test.
            final String[] prefNames = { "plugin.default.state" };
            final int ourRequestId = 0x7358;
            Actions.RepeatedEventExpecter eventExpecter = mActions.expectGeckoEvent("Preferences:Data");
            mActions.sendPreferencesGetEvent(ourRequestId, prefNames);

            JSONObject data = null;
            int requestId = -1;

            // Wait until we get the correct "Preferences:Data" event
            while (requestId != ourRequestId) {
                data = new JSONObject(eventExpecter.blockForEventData());
                requestId = data.getInt("requestId");
            }
            eventExpecter.unregisterListener();
        } catch (Exception ex) {
            mAsserter.ok(false, "exception in testAdobeFlash", ex.toString());
        }

        blockForGeckoReady();

        String url = getAbsoluteUrl(StringHelper.ROBOCOP_ADOBE_FLASH_URL);
        PaintedSurface painted = loadAndGetPainted(url);

        mAsserter.ispixel(painted.getPixelAt(0, 0), 0, 0xff, 0, "Pixel at 0, 0");
        mAsserter.ispixel(painted.getPixelAt(50, 50), 0, 0xff, 0, "Pixel at 50, 50");
        mAsserter.ispixel(painted.getPixelAt(101, 0), 0xff, 0xff, 0xff, "Pixel at 101, 0");
        mAsserter.ispixel(painted.getPixelAt(0, 101), 0xff, 0xff, 0xff, "Pixel at 0, 101");

    }
}
