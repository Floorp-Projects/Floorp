/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.activity;

import android.content.Context;
import android.preference.PreferenceManager;
import android.support.test.InstrumentationRegistry;
import android.support.test.rule.ActivityTestRule;
import android.support.test.runner.AndroidJUnit4;
import android.support.test.uiautomator.UiObject;
import android.support.test.uiautomator.UiObjectNotFoundException;
import android.support.test.uiautomator.UiSelector;

import org.junit.After;
import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mozilla.focus.helpers.TestHelper;

import java.io.IOException;

import okhttp3.mockwebserver.MockResponse;
import okhttp3.mockwebserver.MockWebServer;

import static org.mozilla.focus.fragment.FirstrunFragment.FIRSTRUN_PREF;
import static org.mozilla.focus.helpers.TestHelper.waitingTime;

@RunWith(AndroidJUnit4.class)
public class DownloadTest {
    private static final String TEST_PATH = "/";

    private Context appContext;
    private MockWebServer webServer;

    @Rule
    public ActivityTestRule<MainActivity> mActivityTestRule  = new ActivityTestRule<MainActivity>(MainActivity.class) {
        @Override
        protected void beforeActivityLaunched() {
            super.beforeActivityLaunched();

            appContext = InstrumentationRegistry.getInstrumentation()
                    .getTargetContext()
                    .getApplicationContext();

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
    public void tearDown() throws Exception {
        mActivityTestRule.getActivity().finishAndRemoveTask();
    }

    private UiObject downloadIcon = TestHelper.mDevice.findObject(new UiSelector()
            .description("download icon")
            .enabled(true));
    private UiObject downloadTitle = TestHelper.mDevice.findObject(new UiSelector()
            .resourceId("org.mozilla.focus.debug:id/title_template")
            .enabled(true));
    private UiObject downloadFileName = TestHelper.mDevice.findObject(new UiSelector()
            .resourceId("org.mozilla.focus.debug:id/download_dialog_file_name")
            .enabled(true));
    private UiObject downloadWarning = TestHelper.mDevice.findObject(new UiSelector()
            .resourceId("org.mozilla.focus.debug:id/download_dialog_warning")
            .enabled(true));
    private UiObject downloadCancelBtn = TestHelper.mDevice.findObject(new UiSelector()
            .resourceId("org.mozilla.focus.debug:id/download_dialog_cancel")
            .enabled(true));
    private UiObject downloadBtn = TestHelper.mDevice.findObject(new UiSelector()
            .resourceId("org.mozilla.focus.debug:id/download_dialog_download")
            .enabled(true));
    private UiObject completedMsg = TestHelper.mDevice.findObject(new UiSelector()
            .resourceId("org.mozilla.focus.debug:id/snackbar_text")
            .enabled(true));

    @Test
    public void DownloadFileTest() throws InterruptedException, UiObjectNotFoundException, IOException {

        // Load website with service worker
        TestHelper.inlineAutocompleteEditText.waitForExists(waitingTime);
        TestHelper.inlineAutocompleteEditText.clearTextField();
        TestHelper.inlineAutocompleteEditText.setText(webServer.url(TEST_PATH).toString());
        TestHelper.hint.waitForExists(waitingTime);
        TestHelper.pressEnterKey();
        TestHelper.waitForWebSiteTitleLoad();

        // Find download icon tap it
        Assert.assertTrue(downloadIcon.isClickable());
        downloadIcon.click();
        downloadTitle.waitForExists(waitingTime);
        Assert.assertTrue(downloadTitle.isEnabled());
        Assert.assertTrue(downloadCancelBtn.isEnabled());
        Assert.assertTrue(downloadBtn.isEnabled());
        Assert.assertEquals(downloadFileName.getText(), "download.jpg");
        Assert.assertEquals(downloadWarning.getText(),
                "Downloaded files will not be deleted when you erase Firefox Focus history.");
        downloadBtn.click();
        completedMsg.waitForExists(waitingTime);
        Assert.assertTrue(completedMsg.isEnabled());
        Assert.assertTrue(completedMsg.getText().contains("finished"));
        TestHelper.mDevice.openNotification();

        TestHelper.savedNotification.waitForExists(waitingTime);
        TestHelper.savedNotification.swipeRight(50);
        TestHelper.pressBackKey();
    }
}
