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
import org.mozilla.focus.utils.AppConstants;

import java.io.IOException;

import okhttp3.mockwebserver.MockResponse;
import okhttp3.mockwebserver.MockWebServer;

import static junit.framework.Assert.assertTrue;
import static org.junit.Assert.assertFalse;
import static org.mozilla.focus.fragment.FirstrunFragment.FIRSTRUN_PREF;
import static org.mozilla.focus.helpers.EspressoHelper.openSettings;
import static org.mozilla.focus.helpers.TestHelper.waitingTime;
import static org.mozilla.focus.web.WebViewProviderKt.ENGINE_PREF_STRING_KEY;

@RunWith(AndroidJUnit4.class)
public class SettingsBlockToggleTest {
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

            // This test runs on both GV and WV.
            // Klar is used to test Geckoview. make sure it's set to Gecko
            if (AppConstants.INSTANCE.isKlarBuild() && !AppConstants.INSTANCE.isGeckoBuild()) {
                PreferenceManager.getDefaultSharedPreferences(appContext)
                        .edit()
                        .putBoolean(ENGINE_PREF_STRING_KEY, true)
                        .apply();
            }

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
    public void SettingsToggleTest() throws UiObjectNotFoundException {

        UiObject privacyHeading = TestHelper.mDevice.findObject(new UiSelector()
                .text("Privacy & Security")
                .resourceId("android:id/title"));
        UiObject blockAdTrackerEntry = TestHelper.settingsMenu.getChild(new UiSelector()
                .className("android.widget.LinearLayout")
                .instance(1));
        UiObject blockAdTrackerSwitch = blockAdTrackerEntry.getChild(new UiSelector()
                .resourceId(TestHelper.getAppName() + ":id/switchWidget"));

        UiObject blockAnalyticTrackerEntry = TestHelper.settingsMenu.getChild(new UiSelector()
                .className("android.widget.LinearLayout")
                .instance(2));
        UiObject blockAnalyticTrackerSwitch = blockAnalyticTrackerEntry.getChild(new UiSelector()
                .resourceId(TestHelper.getAppName() + ":id/switchWidget"));

        UiObject blockSocialTrackerEntry = TestHelper.settingsMenu.getChild(new UiSelector()
                .className("android.widget.LinearLayout")
                .instance(4));
        UiObject blockSocialTrackerSwitch = blockSocialTrackerEntry.getChild(new UiSelector()
                .resourceId(TestHelper.getAppName() + ":id/switchWidget"));

        // Let's go to an actual URL which is http://
        TestHelper.inlineAutocompleteEditText.waitForExists(waitingTime);
        TestHelper.inlineAutocompleteEditText.clearTextField();
        TestHelper.inlineAutocompleteEditText.setText(webServer.url(TEST_PATH).toString());
        TestHelper.hint.waitForExists(waitingTime);
        TestHelper.pressEnterKey();
        TestHelper.waitForWebContent();
        assertTrue (TestHelper.browserURLbar.getText().contains(webServer.url(TEST_PATH).toString()));
        assertTrue (!TestHelper.lockIcon.exists());

        /* Go to settings and disable everything */
        openSettings();
        privacyHeading.waitForExists(waitingTime);
        privacyHeading.click();

        String blockAdTrackerValue = blockAdTrackerSwitch.getText();
        String blockAnalyticTrackerValue = blockAnalyticTrackerSwitch.getText();
        String blockSocialTrackerValue = blockSocialTrackerSwitch.getText();

        // Turn off and back on
        blockAdTrackerEntry.click();
        blockAnalyticTrackerEntry.click();
        blockSocialTrackerEntry.click();

        assertFalse(blockAdTrackerSwitch.getText().equals(blockAdTrackerValue));
        assertFalse(blockAnalyticTrackerSwitch.getText().equals(blockAnalyticTrackerValue));
        assertFalse(blockSocialTrackerSwitch.getText().equals(blockSocialTrackerValue));

        blockAdTrackerEntry.click();
        blockAnalyticTrackerEntry.click();
        blockSocialTrackerEntry.click();

        assertTrue(blockAdTrackerSwitch.getText().equals(blockAdTrackerValue));
        assertTrue(blockAnalyticTrackerSwitch.getText().equals(blockAnalyticTrackerValue));
        assertTrue(blockSocialTrackerSwitch.getText().equals(blockSocialTrackerValue));

        //Back to the webpage
        TestHelper.pressBackKey();
        TestHelper.pressBackKey();
        TestHelper.waitForWebContent();
        assertTrue (TestHelper.browserURLbar.getText().contains(webServer.url(TEST_PATH).toString()));
        assertTrue (!TestHelper.lockIcon.exists());
    }
}
