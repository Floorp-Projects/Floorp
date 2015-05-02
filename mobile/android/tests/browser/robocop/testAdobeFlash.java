/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.tests;

import org.json.JSONObject;
import org.mozilla.gecko.PaintedSurface;

import android.os.Build;

/**
 * Tests that Flash is working
 * - loads a page containing a Flash plugin
 * - verifies it rendered properly
 */
public class testAdobeFlash extends PixelTest {
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
            setPreferenceAndWaitForChange(jsonPref);
        } catch (Exception ex) {
            mAsserter.ok(false, "exception in testAdobeFlash", ex.toString());
        }

        blockForGeckoReady();

        String url = getAbsoluteUrl(mStringHelper.ROBOCOP_ADOBE_FLASH_URL);
        PaintedSurface painted = loadAndGetPainted(url);

        mAsserter.ispixel(painted.getPixelAt(0, 0), 0, 0xff, 0, "Pixel at 0, 0");
        mAsserter.ispixel(painted.getPixelAt(50, 50), 0, 0xff, 0, "Pixel at 50, 50");
        mAsserter.ispixel(painted.getPixelAt(101, 0), 0xff, 0xff, 0xff, "Pixel at 101, 0");
        mAsserter.ispixel(painted.getPixelAt(0, 101), 0xff, 0xff, 0xff, "Pixel at 0, 101");

    }
}
