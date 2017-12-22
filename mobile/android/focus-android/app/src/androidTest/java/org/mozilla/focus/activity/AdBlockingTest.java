/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.activity;

import android.content.Context;
import android.preference.PreferenceManager;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.Suppress;
import android.support.test.rule.ActivityTestRule;
import android.support.test.uiautomator.UiObject;
import android.support.test.uiautomator.UiObjectNotFoundException;
import android.support.test.uiautomator.UiSelector;

import org.junit.After;
import org.junit.Rule;
import org.junit.Test;
import org.mozilla.focus.helpers.TestHelper;

import static android.support.test.espresso.action.ViewActions.click;
import static junit.framework.Assert.assertTrue;
import static junit.framework.TestCase.assertFalse;
import static org.mozilla.focus.helpers.TestHelper.waitingTime;
import static org.mozilla.focus.fragment.FirstrunFragment.FIRSTRUN_PREF;

// This test checks basic ad blocking capability
// Disabled with @Suppress, because this does not run in Google Test Cloud properly.
// When executed locally, it runs fine
@Suppress
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
                    .putBoolean(FIRSTRUN_PREF, true)
                    .apply();
        }
    };

    @After
    public void tearDown() throws Exception {
        mActivityTestRule.getActivity().finishAndRemoveTask();
    }

    @Test
    public void AdBlockTest() throws InterruptedException, UiObjectNotFoundException {

        UiObject blockAdTrackerEntry = TestHelper.settingsList.getChild(new UiSelector()
                .className("android.widget.LinearLayout")
                .instance(2));
        UiObject blockAdTrackerValue = blockAdTrackerEntry.getChild(new UiSelector()
                .className("android.widget.Switch"));
        UiObject unBlocked = TestHelper.mDevice.findObject(new UiSelector()
                .className("android.view.View")
                .resourceId("ad_iframe"));

        // Let's go to ads-blocker.com/testing/

        TestHelper.inlineAutocompleteEditText.waitForExists(waitingTime);
        TestHelper.inlineAutocompleteEditText.clearTextField();
        TestHelper.inlineAutocompleteEditText.setText("ads-blocker.com/testing/");
        TestHelper.hint.waitForExists(waitingTime);
        TestHelper.pressEnterKey();
        assertTrue(TestHelper.webView.waitForExists(waitingTime));

        int dHeight = TestHelper.mDevice.getDisplayHeight();
        int dWidth = TestHelper.mDevice.getDisplayWidth();
        int xScrollPosition = dWidth / 2;
        int yScrollStop = dHeight / 4;
        TestHelper.mDevice.swipe(
                xScrollPosition,
                yScrollStop,
                xScrollPosition,
                0,
                20);

        unBlocked.waitForExists(waitingTime);
        assertFalse(unBlocked.exists());
        TestHelper.mDevice.swipe(
                xScrollPosition,
                dHeight / 2,
                xScrollPosition,
                dHeight / 2 + 200,
                20);

        /* Go to settings and disable everything */
        TestHelper.menuButton.perform(click());
        TestHelper.browserViewSettingsMenuItem.click();
        TestHelper.settingsHeading.waitForExists(waitingTime);
        blockAdTrackerEntry.click();
        assertTrue(blockAdTrackerValue.getText().equals("OFF"));
        TestHelper.navigateUp.click();
        TestHelper.browserURLbar.waitForExists(waitingTime);

        //Back to the webpage
        TestHelper.browserURLbar.click();
        TestHelper.inlineAutocompleteEditText.waitForExists(waitingTime);
        TestHelper.inlineAutocompleteEditText.clearTextField();
        TestHelper.inlineAutocompleteEditText.setText("ads-blocker.com/testing/");
        TestHelper.hint.waitForExists(waitingTime);
        TestHelper.pressEnterKey();
        assertTrue(TestHelper.webView.waitForExists(waitingTime));
        TestHelper.mDevice.swipe(
                xScrollPosition,
                yScrollStop,
                xScrollPosition,
                0,
                20);
        unBlocked.waitForExists(waitingTime);
        assertTrue(unBlocked.exists());
        TestHelper.mDevice.swipe(
                xScrollPosition,
                dHeight / 2,
                xScrollPosition,
                dHeight / 2 + 200,
                20);


        //Let's set it back for future tests
        TestHelper.menuButton.perform(click());
        TestHelper.browserViewSettingsMenuItem.click();
        TestHelper.settingsHeading.waitForExists(waitingTime);
        blockAdTrackerEntry.click();
        assertTrue(blockAdTrackerValue.getText().equals("ON"));
        TestHelper.navigateUp.click();
        assertTrue(TestHelper.webView.waitForExists(waitingTime));
        TestHelper.floatingEraseButton.perform(click());
   }
}
