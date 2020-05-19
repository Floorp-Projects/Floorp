/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.activity;

import android.content.Context;
import android.content.Intent;

import androidx.preference.PreferenceManager;
import androidx.test.InstrumentationRegistry;
import androidx.test.rule.ActivityTestRule;
import androidx.test.runner.AndroidJUnit4;
import androidx.test.uiautomator.By;
import androidx.test.uiautomator.UiObject;
import androidx.test.uiautomator.UiObjectNotFoundException;
import androidx.test.uiautomator.UiSelector;
import androidx.test.uiautomator.Until;

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

import static junit.framework.Assert.assertTrue;
import static org.hamcrest.core.IsNull.notNullValue;
import static org.junit.Assert.assertThat;
import static org.mozilla.focus.fragment.FirstrunFragment.FIRSTRUN_PREF;
import static org.mozilla.focus.helpers.TestHelper.waitingTime;

// This test opens enters and invalid URL, and Focus should provide an appropriate error message
@RunWith(AndroidJUnit4.class)
@Ignore("This test was written specifically for WebView and needs to be adapted for GeckoView")
public class SwitchContextTest {

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
    public void tearDown() {
        mActivityTestRule.getActivity().finishAndRemoveTask();
    }

    @Test
    public void ForegroundTest() throws  UiObjectNotFoundException {

        // Open a webpage
        TestHelper.inlineAutocompleteEditText.waitForExists(waitingTime);
        Assert.assertTrue(TestHelper.inlineAutocompleteEditText.exists());
        TestHelper.inlineAutocompleteEditText.clearTextField();
        TestHelper.inlineAutocompleteEditText.setText(webServer.url(TEST_PATH).toString());
        TestHelper.hint.waitForExists(waitingTime);
        TestHelper.pressEnterKey();

        // Assert website is loaded
        TestHelper.waitForWebSiteTitleLoad();

        // Switch out of Focus, pull down system bar and select open action
        TestHelper.pressHomeKey();
        TestHelper.openNotification();
        TestHelper.waitForIdle();
        // If simulator has more recent message, the options need to be expanded
        TestHelper.expandNotification();
        TestHelper.notificationOpenItem.click();

        // Verify that it's on the main view, showing the previous browsing session
        TestHelper.browserURLbar.waitForExists(waitingTime);
        assertTrue(TestHelper.browserURLbar.exists());
        TestHelper.waitForWebSiteTitleLoad();
    }

    @Test
    public void eraseAndOpenTest() throws UiObjectNotFoundException {

        // Open a webpage
        TestHelper.inlineAutocompleteEditText.waitForExists(waitingTime);
        Assert.assertTrue(TestHelper.inlineAutocompleteEditText.exists());
        TestHelper.inlineAutocompleteEditText.clearTextField();
        TestHelper.inlineAutocompleteEditText.setText(webServer.url(TEST_PATH).toString());
        TestHelper.hint.waitForExists(waitingTime);
        TestHelper.pressEnterKey();

        // Assert website is loaded
        TestHelper.waitForWebSiteTitleLoad();

        // Switch out of Focus, pull down system bar and select open action
        TestHelper.pressHomeKey();
        TestHelper.openNotification();

        // If simulator has more recent message, the options need to be expanded
        TestHelper.expandNotification();
        TestHelper.notificationEraseOpenItem.click();

        // Verify that it's on the main view, showing the initial view
        TestHelper.erasedMsg.waitForExists(waitingTime);
        assertTrue(TestHelper.erasedMsg.exists());
        assertTrue(TestHelper.inlineAutocompleteEditText.exists());
        assertTrue(TestHelper.initialView.exists());
    }

    @Test
    public void settingsToFocus() throws UiObjectNotFoundException {

        // Initialize UiDevice instance
        final int LAUNCH_TIMEOUT = 5000;
        final String SETTINGS_APP = "com.android.settings";
        final UiObject settings = TestHelper.mDevice.findObject(new UiSelector()
                .packageName(SETTINGS_APP)
                .enabled(true));

        // Open a webpage
        TestHelper.inlineAutocompleteEditText.waitForExists(waitingTime);
        TestHelper.inlineAutocompleteEditText.clearTextField();
        TestHelper.inlineAutocompleteEditText.setText(webServer.url(TEST_PATH).toString());
        TestHelper.hint.waitForExists(waitingTime);
        TestHelper.pressEnterKey();

        // Assert website is loaded
        TestHelper.waitForWebSiteTitleLoad();

        // Switch out of Focus, open settings app
        TestHelper.pressHomeKey();

        // Wait for launcher
        final String launcherPackage = TestHelper.mDevice.getLauncherPackageName();
        assertThat(launcherPackage, notNullValue());
        TestHelper.mDevice.wait(Until.hasObject(By.pkg(launcherPackage).depth(0)),
                LAUNCH_TIMEOUT);

        // Launch the app
        Context context = InstrumentationRegistry.getInstrumentation()
                .getTargetContext()
                .getApplicationContext();
        final Intent intent = context.getPackageManager()
                .getLaunchIntentForPackage(SETTINGS_APP);
        context.startActivity(intent);

        // switch to Focus
        settings.waitForExists(waitingTime);
        assertTrue(settings.exists());
        TestHelper.openNotification();

        // If simulator has more recent message, the options need to be expanded
        TestHelper.expandNotification();
        TestHelper.notificationOpenItem.click();

        // Verify that it's on the main view, showing the previous browsing session
        TestHelper.browserURLbar.waitForExists(waitingTime);
        assertTrue(TestHelper.browserURLbar.exists());
        TestHelper.waitForWebSiteTitleLoad();
    }
}
