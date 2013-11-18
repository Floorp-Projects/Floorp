package org.mozilla.gecko.tests;

import org.mozilla.gecko.*;

import android.app.Activity;
import android.hardware.Camera;
import android.os.Build;

public class testGetUserMedia extends BaseTest {
    @Override
    protected int getTestType() {
        return TEST_MOCHITEST;
    }

    public void testGetUserMedia() {
        String GUM_URL = getAbsoluteUrl("/robocop/robocop_getusermedia.html");

        String GUM_MESSAGE = "Would you like to share your camera and microphone with";
        String GUM_ALLOW = "Share";
        String GUM_DENY = "Don't share";

        blockForGeckoReady();

        // Only try GUM test if the device has a camera. If there's a working Camera,
        // we'll assume there is a working audio device as well.
        // getNumberOfCameras is Gingerbread/9+
        // We could avoid that requirement by trying to open a Camera but we
        // already know our 2.2/Tegra test devices don't have them.
        if (Build.VERSION.SDK_INT >= 9) {
            if (Camera.getNumberOfCameras() > 0) {
                // Test GUM notification
                inputAndLoadUrl(GUM_URL);
                waitForText(GUM_MESSAGE);
                mAsserter.is(mSolo.searchText(GUM_MESSAGE), true, "GetUserMedia doorhanger has been displayed");
            }
        }
    }
}
