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

import static junit.framework.Assert.assertTrue;
import static org.mozilla.focus.fragment.FirstrunFragment.FIRSTRUN_PREF;
import static org.mozilla.focus.helpers.EspressoHelper.openSettings;
import static org.mozilla.focus.helpers.TestHelper.waitingTime;
import static org.mozilla.focus.web.WebViewProviderKt.ENGINE_PREF_STRING_KEY;

// This test checks all the headings in the Settings menu are there
// https://testrail.stage.mozaws.net/index.php?/cases/view/40064
@RunWith(AndroidJUnit4.class)
public class AccessSettingsTest {

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
        }
    };

    @After
    public void tearDown() {
        mActivityTestRule.getActivity().finishAndRemoveTask();
    }

    @Test
    public void AccessSettingsTest() throws UiObjectNotFoundException {

        UiObject generalHeading = TestHelper.mDevice.findObject(new UiSelector()
                .text("General")
                .resourceId("android:id/title"));

        UiObject privacyHeading = TestHelper.mDevice.findObject(new UiSelector()
                .text("Privacy & Security")
                .resourceId("android:id/title"));

        UiObject searchHeading = TestHelper.mDevice.findObject(new UiSelector()
                .text("Search")
                .resourceId("android:id/title"));

        UiObject mozHeading = TestHelper.mDevice.findObject(new UiSelector()
                .text("Mozilla")
                .resourceId("android:id/title"));

        /* Go to Settings */
        TestHelper.inlineAutocompleteEditText.waitForExists(waitingTime);
        openSettings();
        generalHeading.waitForExists(waitingTime);

        /* Check the first element and other headings are present */
        assertTrue(generalHeading.exists());
        assertTrue(searchHeading.exists());
        assertTrue(privacyHeading.exists());
        assertTrue(mozHeading.exists());
    }
}
