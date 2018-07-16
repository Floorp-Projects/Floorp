/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.activity;

import android.content.Context;
import android.content.Intent;
import android.preference.PreferenceManager;
import android.support.test.InstrumentationRegistry;
import android.support.test.rule.ActivityTestRule;
import android.support.test.runner.AndroidJUnit4;
import android.support.test.uiautomator.By;
import android.support.test.uiautomator.UiObjectNotFoundException;
import android.support.test.uiautomator.Until;

import org.junit.After;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mozilla.focus.helpers.TestHelper;

import java.io.IOException;

import okhttp3.mockwebserver.MockResponse;
import okhttp3.mockwebserver.MockWebServer;

import static android.support.test.espresso.action.ViewActions.click;
import static junit.framework.Assert.assertFalse;
import static junit.framework.Assert.assertTrue;
import static org.hamcrest.core.IsNull.notNullValue;
import static org.junit.Assert.assertThat;
import static org.mozilla.focus.fragment.FirstrunFragment.FIRSTRUN_PREF;
import static org.mozilla.focus.helpers.TestHelper.waitingTime;

// This test erases URL and checks for message
// https://testrail.stage.mozaws.net/index.php?/cases/view/40068
@RunWith(AndroidJUnit4.class)
public class EraseAllUserDataTest {
    private static final String TEST_PATH = "/";
    private MockWebServer webServer;

    @Rule
    public ActivityTestRule<MainActivity> mActivityTestRule
            = new ActivityTestRule<MainActivity>(MainActivity.class) {

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
                        .setBody(TestHelper.readTestAsset("plain_test.html")));
                webServer.enqueue(new MockResponse()
                        .setBody(TestHelper.readTestAsset("plain_test.html")));
                webServer.enqueue(new MockResponse()
                        .setBody(TestHelper.readTestAsset("plain_test.html")));

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
    public void TrashTest() throws UiObjectNotFoundException {

        // Open a webpage
        TestHelper.inlineAutocompleteEditText.waitForExists(waitingTime);
        TestHelper.inlineAutocompleteEditText.clearTextField();
        TestHelper.inlineAutocompleteEditText.setText(webServer.url(TEST_PATH).toString());
        TestHelper.hint.waitForExists(waitingTime);
        TestHelper.pressEnterKey();
        TestHelper.waitForWebContent();

        // Press erase button, and check for message and return to the main page
        TestHelper.floatingEraseButton.perform(click());
        TestHelper.erasedMsg.waitForExists(waitingTime);
        assertTrue(TestHelper.erasedMsg.exists());
        assertTrue(TestHelper.inlineAutocompleteEditText.exists());
    }

    @Test
    public void systemBarTest() throws UiObjectNotFoundException {
        // Open a webpage
        TestHelper.inlineAutocompleteEditText.waitForExists(waitingTime);
        TestHelper.inlineAutocompleteEditText.clearTextField();
        TestHelper.inlineAutocompleteEditText.setText(webServer.url(TEST_PATH).toString());
        TestHelper.hint.waitForExists(waitingTime);
        TestHelper.pressEnterKey();
        TestHelper.waitForWebContent();
        TestHelper.menuButton.perform(click());
        TestHelper.blockCounterItem.waitForExists(waitingTime);

        // Pull down system bar and select delete browsing history
        TestHelper.openNotification();
        TestHelper.notificationBarDeleteItem.waitForExists(waitingTime);
        TestHelper.notificationBarDeleteItem.click();
        TestHelper.erasedMsg.waitForExists(waitingTime);
        assertTrue(TestHelper.erasedMsg.exists());
        assertTrue(TestHelper.inlineAutocompleteEditText.exists());
        assertFalse(TestHelper.menulist.exists());
    }

    @Test
    public void systemBarHomeViewTest() throws UiObjectNotFoundException  {

        // Initialize UiDevice instance
        final int LAUNCH_TIMEOUT = 5000;

        // Open a webpage
        TestHelper.inlineAutocompleteEditText.waitForExists(waitingTime);
        TestHelper.inlineAutocompleteEditText.clearTextField();
        TestHelper.inlineAutocompleteEditText.setText(webServer.url(TEST_PATH).toString());
        TestHelper.hint.waitForExists(waitingTime);
        TestHelper.pressEnterKey();
        TestHelper.waitForWebContent();
        // Switch out of Focus, pull down system bar and select delete browsing history
        TestHelper.pressHomeKey();
        TestHelper.openNotification();
        TestHelper.notificationBarDeleteItem.waitForExists(waitingTime);
        TestHelper.notificationBarDeleteItem.click();

        // Wait for launcher
        final String launcherPackage = TestHelper.mDevice.getLauncherPackageName();
        assertThat(launcherPackage, notNullValue());
        TestHelper.mDevice.wait(Until.hasObject(By.pkg(launcherPackage).depth(0)),
                LAUNCH_TIMEOUT);

        // Launch the app
        mActivityTestRule.launchActivity(new Intent(Intent.ACTION_MAIN));
        // Verify that it's on the main view, not showing the previous browsing session
        TestHelper.inlineAutocompleteEditText.waitForExists(waitingTime);
        assertTrue(TestHelper.inlineAutocompleteEditText.exists());
    }
}
