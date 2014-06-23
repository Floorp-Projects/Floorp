package org.mozilla.gecko.tests;

import android.hardware.Camera;
import android.os.Build;

public class testGetUserMedia extends BaseTest {
    public void testGetUserMedia() {
        String GUM_CAMERA_URL = getAbsoluteUrl("/robocop/robocop_getusermedia2.html");
        String GUM_TAB_URL = getAbsoluteUrl("/robocop/robocop_getusermedia.html");
        // Browser constraint needs HTTPS
        String GUM_TAB_HTTPS_URL = GUM_TAB_URL.replace("http://mochi.test:8888", "https://example.com");

        String GUM_MESSAGE = "Would you like to share your camera and microphone with";
        String GUM_ALLOW = "^Share$";
        String GUM_DENY = "^Don't Share$";

        String GUM_BACK_CAMERA = "Back facing camera";

        String GUM_PAGE_FAILED = "failed gumtest";
        String GUM_PAGE_AUDIO = "audio gumtest";
        String GUM_PAGE_VIDEO = "video gumtest";
        String GUM_PAGE_AUDIOVIDEO = "audiovideo gumtest";

        blockForGeckoReady();

        // Only try GUM test if the device has a camera (emulation).
        if (Camera.getNumberOfCameras() <= 0) {
            return;
        }

        // Tests on Camera page will test camera enumeration code, but
        // the actual cameras don't seem to work on the emulators, so
        // the enumeration is all that gets tested.

        // Test GUM notification showing
        inputAndLoadUrl(GUM_CAMERA_URL);
        waitForText(GUM_MESSAGE);
        // At least one camera detected
        mAsserter.is(mSolo.searchText(GUM_BACK_CAMERA), true, "getUserMedia found a camera");
        mAsserter.is(mSolo.searchText(GUM_MESSAGE), true, "getUserMedia doorhanger has been displayed");
        mSolo.clickOnButton(GUM_DENY);
        verifyPageTitle(GUM_PAGE_FAILED, GUM_CAMERA_URL);

        // Cameras don't work on the testing hardware, so stream a tab
        inputAndLoadUrl(GUM_TAB_HTTPS_URL);
        waitForText(GUM_MESSAGE);
        mSolo.clickOnText("Microphone 1");
        mSolo.clickOnText("No Audio");
        mSolo.clickOnButton(GUM_ALLOW);
        mSolo.clickOnText("gUM Test Page");
        verifyPageTitle(GUM_PAGE_VIDEO, GUM_TAB_HTTPS_URL);

        // Android 2.3 testers fail because of audio issues:
        // E/AudioRecord(  650): Unsupported configuration: sampleRate 44100, format 1, channelCount 1
        // E/libOpenSLES(  650): android_audioRecorder_realize(0x26d7d8) error creating AudioRecord object
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.ICE_CREAM_SANDWICH) {
            return;
        }

        inputAndLoadUrl(GUM_TAB_HTTPS_URL);
        waitForText(GUM_MESSAGE);
        mSolo.clickOnButton(GUM_ALLOW);
        mSolo.clickOnText("gUM Test Page");
        verifyPageTitle(GUM_PAGE_AUDIOVIDEO, GUM_TAB_HTTPS_URL);

        inputAndLoadUrl(GUM_TAB_HTTPS_URL);
        waitForText(GUM_MESSAGE);
        mSolo.clickOnText("Choose a tab to stream");
        mSolo.clickOnText("No Video");
        mSolo.clickOnButton(GUM_ALLOW);
        verifyPageTitle(GUM_PAGE_AUDIO, GUM_TAB_HTTPS_URL);
    }
}
