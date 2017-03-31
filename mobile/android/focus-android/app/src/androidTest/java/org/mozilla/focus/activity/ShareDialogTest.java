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

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import static android.support.test.espresso.action.ViewActions.click;
import static junit.framework.Assert.assertTrue;
import static org.mozilla.focus.fragment.FirstrunFragment.FIRSTRUN_PREF;

// This test opens share menu
@RunWith(AndroidJUnit4.class)
public class ShareDialogTest {

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
                    .putBoolean(FIRSTRUN_PREF, false)
                    .apply();
        }
    };

    @Test
    public void shareTest() throws InterruptedException, UiObjectNotFoundException {
        final long waitingTime = TestHelper.waitingTime;

        UiObject shareBtn = TestHelper.mDevice.findObject(new UiSelector()
                .resourceId("org.mozilla.focus.debug:id/share")
                .enabled(true));
        UiObject shareMenuHeader = TestHelper.mDevice.findObject(new UiSelector()
                .resourceId("android:id/title")
                .text("Share with")
                .enabled(true));
        UiObject shareAppList = TestHelper.mDevice.findObject(new UiSelector()
                .resourceId("android:id/resolver_list")
                .enabled(true));

        /* Wait for app to load, and take the First View screenshot */
        TestHelper.firstViewBtn.waitForExists(waitingTime);
        TestHelper.firstViewBtn.click();

        /* Go to a webpage */
        TestHelper.urlBar.waitForExists(waitingTime);
        TestHelper.urlBar.click();
        TestHelper.inlineAutocompleteEditText.waitForExists(waitingTime);
        TestHelper.inlineAutocompleteEditText.clearTextField();
        TestHelper.inlineAutocompleteEditText.setText("mozilla");
        TestHelper.hint.waitForExists(waitingTime);
        TestHelper.pressEnterKey();
        TestHelper.webView.waitForExists(waitingTime);

        /* Select share */
        TestHelper.menuButton.perform(click());
        shareBtn.waitForExists(waitingTime);
        shareBtn.click();

        // For simulators, where apps are not installed, it'll take to message app
        shareMenuHeader.waitForExists(waitingTime);
        assertTrue(shareMenuHeader.exists());
        assertTrue(shareAppList.exists());
        TestHelper.pressBackKey();
    }
}
