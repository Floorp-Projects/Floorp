package org.mozilla.gecko.tests;

import android.widget.Spinner;
import android.view.View;
import com.jayway.android.robotium.solo.Condition;
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
        String GUM_SELECT_TAB = "Choose a tab to stream";

        String GUM_PAGE_TITLE = "gUM Test Page";
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
        mAsserter.is(mSolo.searchText(GUM_MESSAGE), true, "getUserMedia doorhanger has been displayed");
        waitForSpinner();
        // At least one camera detected
        mAsserter.is(mSolo.searchText(GUM_BACK_CAMERA), true, "getUserMedia found a camera");
        mSolo.clickOnButton(GUM_DENY);
        waitForTextDismissed(GUM_MESSAGE);
        mAsserter.is(mSolo.searchText(GUM_MESSAGE), false, "getUserMedia doorhanger hidden after dismissal");
        verifyPageTitle(GUM_PAGE_FAILED, GUM_CAMERA_URL);

        // Cameras don't work on the testing hardware, so stream a tab
        inputAndLoadUrl(GUM_TAB_HTTPS_URL);
        waitForText(GUM_MESSAGE);
        mAsserter.is(mSolo.searchText(GUM_MESSAGE), true, "getUserMedia doorhanger has been displayed");
        waitForSpinner();
        mAsserter.is(mSolo.searchText(GUM_SELECT_TAB), true, "Video source selection available");
        mAsserter.is(mSolo.searchText("MICROPHONE TO USE"), true, "Microphone selection available");
        mAsserter.is(mSolo.searchText("Microphone 1"), true, "Microphone 1 available");
        mSolo.clickOnText("Microphone 1");
        waitForText("No Audio");
        mAsserter.is(mSolo.searchText("No Audio"), true, "No 'No Audio' selection available");
        mSolo.clickOnText("No Audio");
        waitForTextDismissed("Microphone 1");
        mAsserter.is(mSolo.searchText("Microphone 1"), false, "Audio selection hidden after dismissal");
        mAsserter.is(mSolo.searchText(GUM_ALLOW), true, "Share button available after selection");
        mSolo.clickOnButton(GUM_ALLOW);
        waitForTextDismissed(GUM_MESSAGE);
        mAsserter.is(mSolo.searchText(GUM_MESSAGE), false, "getUserMedia doorhanger hidden after dismissal");
        waitForText(GUM_SELECT_TAB);
        mAsserter.is(mSolo.searchText(GUM_SELECT_TAB), true, "Tab selection dialog displayed");
        mSolo.clickOnText(GUM_PAGE_TITLE);
        waitForTextDismissed(GUM_SELECT_TAB);
        mAsserter.is(mSolo.searchText(GUM_SELECT_TAB), false, "Tab selection dialog hidden");
        verifyPageTitle(GUM_PAGE_VIDEO, GUM_TAB_HTTPS_URL);

        // Android 2.3 testers fail because of audio issues:
        // E/AudioRecord(  650): Unsupported configuration: sampleRate 44100, format 1, channelCount 1
        // E/libOpenSLES(  650): android_audioRecorder_realize(0x26d7d8) error creating AudioRecord object
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.ICE_CREAM_SANDWICH) {
            return;
        }

        inputAndLoadUrl(GUM_TAB_HTTPS_URL);
        waitForText(GUM_MESSAGE);
        mAsserter.is(mSolo.searchText(GUM_MESSAGE), true, "getUserMedia doorhanger has been displayed");

        waitForSpinner();
        mAsserter.is(mSolo.searchText(GUM_SELECT_TAB), true, "Video source selection available");
        mSolo.clickOnButton(GUM_ALLOW);
        waitForTextDismissed(GUM_MESSAGE);
        waitForText(GUM_SELECT_TAB);
        mAsserter.is(mSolo.searchText(GUM_SELECT_TAB), true, "Tab selection dialog displayed");
        mSolo.clickOnText(GUM_PAGE_TITLE);
        waitForTextDismissed(GUM_SELECT_TAB);
        mAsserter.is(mSolo.searchText(GUM_SELECT_TAB), false, "Tab selection dialog hidden");
        verifyPageTitle(GUM_PAGE_AUDIOVIDEO, GUM_TAB_HTTPS_URL);

        inputAndLoadUrl(GUM_TAB_HTTPS_URL);
        waitForText(GUM_MESSAGE);
        mAsserter.is(mSolo.searchText(GUM_MESSAGE), true, "getUserMedia doorhanger has been displayed");

        waitForSpinner();
        mAsserter.is(mSolo.searchText(GUM_SELECT_TAB), true, "Video source selection available");
        mSolo.clickOnText(GUM_SELECT_TAB);
        waitForText("No Video");
        mAsserter.is(mSolo.searchText("No Video"), true, "'No video' source selection available");
        mSolo.clickOnText("No Video");
        waitForTextDismissed(GUM_SELECT_TAB);
        mSolo.clickOnButton(GUM_ALLOW);
        waitForTextDismissed(GUM_MESSAGE);
        mAsserter.is(mSolo.searchText(GUM_MESSAGE), false, "getUserMedia doorhanger hidden after dismissal");
        verifyPageTitle(GUM_PAGE_AUDIO, GUM_TAB_HTTPS_URL);
    }

    // wait for a Spinner view that is clickable
    private void waitForSpinner() {
        waitForCondition(new Condition() {
            @Override
            public boolean isSatisfied() {
                for (Spinner view : mSolo.getCurrentViews(Spinner.class)) {
                    if (view.isClickable() &&
                        view.getVisibility() == View.VISIBLE &&
                        view.getWidth() > 0 &&
                        view.getHeight() > 0) {
                        return true;
                    }
                }
                return false;
            }
        }, MAX_WAIT_MS);
    }

    // wait until the specified text is *not* displayed
    private void waitForTextDismissed(final String text) {
        waitForCondition(new Condition() {
            @Override
            public boolean isSatisfied() {
                return !mSolo.searchText(text);
            }
        }, MAX_WAIT_MS);
    }
}
