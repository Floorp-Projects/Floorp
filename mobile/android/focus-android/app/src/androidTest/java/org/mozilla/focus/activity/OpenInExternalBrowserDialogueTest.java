/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
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
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mozilla.focus.helpers.TestHelper;

import java.io.IOException;

import okhttp3.mockwebserver.MockResponse;
import okhttp3.mockwebserver.MockWebServer;

import static android.support.test.espresso.action.ViewActions.click;
import static junit.framework.Assert.assertTrue;
import static org.mozilla.focus.fragment.FirstrunFragment.FIRSTRUN_PREF;
import static org.mozilla.focus.helpers.TestHelper.waitingTime;

// This test opens a webpage, and selects "Open in" menu
@RunWith(AndroidJUnit4.class)
public class OpenInExternalBrowserDialogueTest {
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

                webServer.start();
            } catch (IOException e) {
                throw new AssertionError("Could not start web server", e);
            }
        }
    };

    @After
    public void tearDown() {
        mActivityTestRule.getActivity().finishAndRemoveTask();
    }

    @Test
    public void OpenInExternalBrowserDialogueTest() throws UiObjectNotFoundException {

        UiObject openWithBtn = TestHelper.mDevice.findObject(new UiSelector()
                .resourceId(TestHelper.getAppName() + ":id/open_select_browser")
                .enabled(true));

        UiObject openWithTitle = TestHelper.mDevice.findObject(new UiSelector()
                .className("android.widget.TextView")
                .text("Open in…")
                .enabled(true));

        UiObject openWithList = TestHelper.mDevice.findObject(new UiSelector()
                .resourceId(TestHelper.getAppName() + ":id/apps")
                .enabled(true));

        /* Go to mozilla page */
        TestHelper.inlineAutocompleteEditText.waitForExists(waitingTime);
        TestHelper.inlineAutocompleteEditText.clearTextField();
        TestHelper.inlineAutocompleteEditText.setText(webServer.url(TEST_PATH).toString());
        TestHelper.hint.waitForExists(waitingTime);
        TestHelper.pressEnterKey();
        TestHelper.waitForWebContent();

        /* Select Open with from menu, check appearance */
        TestHelper.menuButton.perform(click());
        openWithBtn.waitForExists(waitingTime);
        openWithBtn.click();
        openWithTitle.waitForExists(waitingTime);
        assertTrue(openWithTitle.exists());
        assertTrue(openWithList.exists());
        TestHelper.pressBackKey();
    }
}
