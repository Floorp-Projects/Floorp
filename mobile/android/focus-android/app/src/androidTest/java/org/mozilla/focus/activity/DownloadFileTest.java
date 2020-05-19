/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.activity;

import android.content.Context;
import android.os.Build;

import androidx.preference.PreferenceManager;
import androidx.test.InstrumentationRegistry;
import androidx.test.rule.ActivityTestRule;
import androidx.test.runner.AndroidJUnit4;
import androidx.test.uiautomator.UiObject;
import androidx.test.uiautomator.UiObjectNotFoundException;
import androidx.test.uiautomator.UiSelector;

import org.junit.After;
import org.junit.Assert;
import org.junit.Ignore;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mozilla.focus.helpers.TestHelper;
import org.mozilla.focus.utils.AppConstants;

import java.io.IOException;

import okhttp3.mockwebserver.MockResponse;
import okhttp3.mockwebserver.MockWebServer;

import static org.mozilla.focus.fragment.FirstrunFragment.FIRSTRUN_PREF;
import static org.mozilla.focus.helpers.TestHelper.waitingTime;

@RunWith(AndroidJUnit4.class)
@Ignore("This test was written specifically for WebView and needs to be adapted for GeckoView")
// https://testrail.stage.mozaws.net/index.php?/cases/view/53141
public class DownloadFileTest {
    private static final String TEST_PATH = "/";

    private MockWebServer webServer;

    @Rule
    public ActivityTestRule<MainActivity> mActivityTestRule  = new ActivityTestRule<MainActivity>(MainActivity.class) {
        @Override
        protected void beforeActivityLaunched() {
            super.beforeActivityLaunched();

            Context appContext = InstrumentationRegistry.getInstrumentation()
                    .getTargetContext()
                    .getApplicationContext();

            // This test is for API 25 and greater. see https://github.com/mozilla-mobile/focus-android/issues/2696
            org.junit.Assume.assumeTrue(Build.VERSION.SDK_INT >= Build.VERSION_CODES.N);


            PreferenceManager.getDefaultSharedPreferences(appContext)
                    .edit()
                    .putBoolean(FIRSTRUN_PREF, true)
                    .apply();

            webServer = new MockWebServer();

            try {
                webServer.enqueue(new MockResponse()
                        .setBody(TestHelper.readTestAsset("image_test.html"))
                        .addHeader("Set-Cookie", "sphere=battery; Expires=Wed, 21 Oct 2035 07:28:00 GMT;"));
                webServer.enqueue(new MockResponse()
                        .setBody(TestHelper.readTestAsset("rabbit.jpg")));
                webServer.enqueue(new MockResponse()
                        .setBody(TestHelper.readTestAsset("download.jpg")));
                webServer.enqueue(new MockResponse()
                        .setBody(TestHelper.readTestAsset("download.jpg")));
                webServer.enqueue(new MockResponse()
                        .setBody(TestHelper.readTestAsset("download.jpg")));

                webServer.start();
            } catch (IOException e) {
                throw new AssertionError("Could not start web server", e);
            }
        }

        @Override
        protected void afterActivityFinished() {
            super.afterActivityFinished();

            try {
                webServer.close();
                webServer.shutdown();
            } catch (IOException e) {
                throw new AssertionError("Could not stop web server", e);
            }
        }
    };

    @After
    public void tearDown()  {

        // If notification is still up, this will take it off screen
        TestHelper.pressBackKey();

        mActivityTestRule.getActivity().finishAndRemoveTask();
    }

    @Test
    public void DownloadTest() throws UiObjectNotFoundException  {
        UiObject downloadIcon;

        downloadIcon = TestHelper.mDevice.findObject(new UiSelector()
                .resourceId("download")
                .enabled(true));

        // Load website with service worker
        TestHelper.inlineAutocompleteEditText.waitForExists(waitingTime);
        TestHelper.inlineAutocompleteEditText.clearTextField();
        TestHelper.inlineAutocompleteEditText.setText(webServer.url(TEST_PATH).toString());
        TestHelper.hint.waitForExists(waitingTime);
        TestHelper.pressEnterKey();
        TestHelper.waitForWebContent();
        TestHelper.progressBar.waitUntilGone(waitingTime);

        downloadIcon.click();

        // If permission dialog appears, grant it
        if (TestHelper.permAllowBtn.waitForExists(waitingTime)) {
            TestHelper.permAllowBtn.click();
        }

        TestHelper.downloadTitle.waitForExists(waitingTime);
        Assert.assertTrue(TestHelper.downloadTitle.isEnabled());
        Assert.assertTrue(TestHelper.downloadCancelBtn.isEnabled());
        Assert.assertTrue(TestHelper.downloadBtn.isEnabled());
        Assert.assertEquals(TestHelper.downloadFileName.getText(), "download.jpg");
        Assert.assertEquals(TestHelper.downloadWarning.getText(),
                "Downloaded files will not be deleted when you erase Firefox Focus history.");
        TestHelper.downloadBtn.click();
        TestHelper.completedMsg.waitForExists(waitingTime);
        Assert.assertTrue(TestHelper.completedMsg.isEnabled());
        Assert.assertTrue(TestHelper.completedMsg.getText().contains("finished"));
        TestHelper.mDevice.openNotification();
        TestHelper.mDevice.waitForIdle();
        TestHelper.savedNotification.waitForExists(waitingTime);
        TestHelper.savedNotification.swipeRight(50);
    }
}
