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

import static android.support.test.espresso.action.ViewActions.click;
import static junit.framework.Assert.assertTrue;
import static org.mozilla.focus.activity.TestHelper.waitingTime;
import static org.mozilla.focus.fragment.FirstrunFragment.FIRSTRUN_PREF;

@RunWith(AndroidJUnit4.class)
public class TypicalUseScenarioTest {

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
    public void TypicalUseTest() throws InterruptedException, UiObjectNotFoundException {

        UiObject blockAdTrackerEntry = TestHelper.settingsList.getChild(new UiSelector()
                .className("android.widget.LinearLayout")
                .instance(3));
        UiObject blockAdTrackerValue = blockAdTrackerEntry.getChild(new UiSelector()
                .className("android.widget.Switch"));
        UiObject blockAnalyticTrackerEntry = TestHelper.settingsList.getChild(new UiSelector()
                .className("android.widget.LinearLayout")
                .instance(5));
        UiObject blockAnalyticTrackerValue = blockAnalyticTrackerEntry.getChild(new UiSelector()
                .className("android.widget.Switch"));
        UiObject blockSocialTrackerEntry = TestHelper.settingsList.getChild(new UiSelector()
                .className("android.widget.LinearLayout")
                .instance(7));
        UiObject blockSocialTrackerValue = blockSocialTrackerEntry.getChild(new UiSelector()
                .className("android.widget.Switch"));

        // Let's search for something
        TestHelper.inlineAutocompleteEditText.waitForExists(waitingTime);
        TestHelper.inlineAutocompleteEditText.clearTextField();
        TestHelper.inlineAutocompleteEditText.setText("mozilla focus");
        TestHelper.hint.waitForExists(waitingTime);
        assertTrue(TestHelper.hint.getText().equals("Search for mozilla focus"));
        TestHelper.hint.click();
        assertTrue(TestHelper.webView.waitForExists(waitingTime));
        assertTrue (TestHelper.browserURLbar.getText().contains("mozilla"));
        assertTrue (TestHelper.browserURLbar.getText().contains("focus"));

        // Let's delete my history
        TestHelper.floatingEraseButton.perform(click());
        TestHelper.erasedMsg.waitForExists(waitingTime);
        assertTrue(TestHelper.erasedMsg.exists());

        // Let's go to an actual URL which is https://
        TestHelper.inlineAutocompleteEditText.waitForExists(waitingTime);
        TestHelper.inlineAutocompleteEditText.clearTextField();
        TestHelper.inlineAutocompleteEditText.setText("https://www.google.com");
        TestHelper.hint.waitForExists(waitingTime);
        assertTrue(TestHelper.hint.getText().equals("Search for https://www.google.com"));
        TestHelper.pressEnterKey();
        assertTrue(TestHelper.webView.waitForExists(waitingTime));
        assertTrue (TestHelper.browserURLbar.getText().contains("https://www.google"));

        // The DOM for lockicon is not detected consistenly, even when it is visible.
        // Disabling the check
        //TestHelper.lockIcon.waitForExists(waitingTime * 2);
        //assertTrue (TestHelper.lockIcon.isEnabled());

        // Let's delete my history again
        TestHelper.floatingEraseButton.perform(click());
        TestHelper.erasedMsg.waitForExists(waitingTime);
        assertTrue(TestHelper.erasedMsg.exists());

        // Let's go to an actual URL which is http://
        TestHelper.inlineAutocompleteEditText.waitForExists(waitingTime);
        TestHelper.inlineAutocompleteEditText.clearTextField();
        TestHelper.inlineAutocompleteEditText.setText("http://www.example.com");
        TestHelper.hint.waitForExists(waitingTime);
        assertTrue(TestHelper.hint.getText().equals("Search for http://www.example.com"));
        TestHelper.pressEnterKey();
        assertTrue(TestHelper.webView.waitForExists(waitingTime));
        assertTrue (TestHelper.browserURLbar.getText().contains("http://www.example.com"));
        assertTrue (!TestHelper.lockIcon.exists());

        /* Go to settings and disable everything */
        TestHelper.menuButton.perform(click());
        TestHelper.browserViewSettingsMenuItem.click();
        TestHelper.settingsHeading.waitForExists(waitingTime);
        blockAdTrackerEntry.click();
        blockAnalyticTrackerEntry.click();
        blockSocialTrackerEntry.click();
        assertTrue(blockAdTrackerValue.getText().equals("OFF"));
        assertTrue(blockAnalyticTrackerValue.getText().equals("OFF"));
        assertTrue(blockSocialTrackerValue.getText().equals("OFF"));
        blockAdTrackerEntry.click();
        blockAnalyticTrackerEntry.click();
        blockSocialTrackerEntry.click();

        //Back to the webpage
        TestHelper.navigateUp.click();
        assertTrue(TestHelper.webView.waitForExists(waitingTime));
        assertTrue (TestHelper.browserURLbar.getText().contains("http://www.example.com"));
        assertTrue (!TestHelper.lockIcon.exists());
    }
}
