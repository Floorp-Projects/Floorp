/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.activity;

import android.content.Context;
import android.os.Build;
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

// This test checks basic ad blocking capability
@RunWith(AndroidJUnit4.class)
public class AdBlockingTest {

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
    public void AdBlockTest() throws InterruptedException, UiObjectNotFoundException {

        final long waitingTime = TestHelper.waitingTime;

        UiObject blockAdTrackerEntry = TestHelper.settingsList.getChild(new UiSelector()
                .className("android.widget.LinearLayout")
                .instance(2));
        UiObject blockAdTrackerValue = blockAdTrackerEntry.getChild(new UiSelector()
                .className("android.widget.Switch"));
        UiObject blocked = TestHelper.mDevice.findObject(new UiSelector()
                .className("android.view.View")
                .description(" Ad blocking enabled!")
                .enabled(true));
        UiObject unBlocked = TestHelper.mDevice.findObject(new UiSelector()
                .className("android.view.View")
                .description("No ad blocking detected")
                .enabled(true));
        TestHelper.firstViewBtn.waitForExists(waitingTime);
        TestHelper.firstViewBtn.click();

        // Let's go to https://blockads.fivefilters.org/
        TestHelper.urlBar.waitForExists(waitingTime);
        TestHelper.urlBar.click();

        TestHelper.inlineAutocompleteEditText.waitForExists(waitingTime);
        TestHelper.inlineAutocompleteEditText.clearTextField();
        TestHelper.inlineAutocompleteEditText.setText("https://blockads.fivefilters.org");
        TestHelper.hint.waitForExists(waitingTime);
        TestHelper.pressEnterKey();
        TestHelper.webView.waitForExists(waitingTime);
        blocked.waitForExists(waitingTime);
        assertTrue (blocked.exists());

        /* Go to settings and disable everything */
        TestHelper.menuButton.perform(click());
        TestHelper.browserViewSettingsMenuItem.click();
        TestHelper.settingsHeading.waitForExists(waitingTime);
        blockAdTrackerEntry.click();
        assertTrue(blockAdTrackerValue.getText().equals("OFF"));
        TestHelper.navigateUp.click();
        TestHelper.browserURLbar.waitForExists(waitingTime);

        //Back to the webpage
        // For API 25 (N) devices in BB, it does not allow https connection,
        // and it behaves erratically (Locally below executes with no issues)
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.N) {
            TestHelper.browserURLbar.click();
            TestHelper.inlineAutocompleteEditText.waitForExists(waitingTime);
            TestHelper.inlineAutocompleteEditText.clearTextField();
            TestHelper.inlineAutocompleteEditText.setText("https://blockads.fivefilters.org");
            TestHelper.hint.waitForExists(waitingTime);
            TestHelper.pressEnterKey();
            TestHelper.webView.waitForExists(waitingTime);
            blocked.waitForExists(waitingTime);
            assertTrue(unBlocked.exists());
        }

        //Let's set it back for future tests
        TestHelper.menuButton.perform(click());
        TestHelper.browserViewSettingsMenuItem.click();
        TestHelper.settingsHeading.waitForExists(waitingTime);
        blockAdTrackerEntry.click();
        assertTrue(blockAdTrackerValue.getText().equals("ON"));
        TestHelper.navigateUp.click();
        TestHelper.webView.waitForExists(waitingTime);
        TestHelper.floatingEraseButton.perform(click());
   }
}
