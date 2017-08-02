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
import android.support.test.uiautomator.UiObjectNotFoundException;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.util.Objects;

import static android.support.test.espresso.action.ViewActions.click;
import static junit.framework.Assert.assertTrue;
import static org.mozilla.focus.activity.TestHelper.waitingTime;
import static org.mozilla.focus.fragment.FirstrunFragment.FIRSTRUN_PREF;

// This test toggles blocking within the browser view
// mozilla.org site has one google analytics tracker - tests will see whether this gets blocked properly
@RunWith(AndroidJUnit4.class)
public class ToggleBlockTest {

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

    @Test
    public void SimpleToggleTest() throws UiObjectNotFoundException {
        // Open mozilla webpage
        TestHelper.inlineAutocompleteEditText.waitForExists(waitingTime);
        TestHelper.inlineAutocompleteEditText.clearTextField();
        TestHelper.inlineAutocompleteEditText.setText("mozilla");
        TestHelper.hint.waitForExists(waitingTime);
        TestHelper.pressEnterKey();
        TestHelper.webView.waitForExists(waitingTime);

        // Check that it blocked 1 tracker
        TestHelper.menuButton.perform(click());
        TestHelper.blockCounterItem.waitForExists(waitingTime);
        assertTrue(Objects.equals(TestHelper.blockCounterItem.getText(), "1"));

        // disable blocking, check the site is reloaded
        assertTrue(TestHelper.blockToggleSwitch.isChecked());
        TestHelper.blockToggleSwitch.click();
        TestHelper.webView.waitForExists(waitingTime);

        // check that it blocked 0 tracker
        TestHelper.menuButton.perform(click());
        TestHelper.blockCounterItem.waitForExists(waitingTime);
        assertTrue(Objects.equals(TestHelper.blockCounterItem.getText(), "-"));
        assertTrue(!TestHelper.blockToggleSwitch.isChecked());
        TestHelper.blockToggleSwitch.click();
        TestHelper.webView.waitForExists(waitingTime);
    }

    @Test
    public void PreDisableTrackerTest() throws UiObjectNotFoundException {
        // Go to settings, disable 'Block Analytic Trackers'
        TestHelper.menuButton.perform(click());
        TestHelper.settingsMenuItem.click();
        TestHelper.settingsList.waitForExists(waitingTime);
        assertTrue(TestHelper.toggleAnalyticBlock.isChecked());
        TestHelper.toggleAnalyticBlock.click();
        assertTrue(!TestHelper.toggleAnalyticBlock.isChecked());
        TestHelper.navigateUp.click();

        // Go to mozilla.org
        TestHelper.inlineAutocompleteEditText.waitForExists(waitingTime);
        TestHelper.inlineAutocompleteEditText.clearTextField();
        TestHelper.inlineAutocompleteEditText.setText("mozilla");
        TestHelper.hint.waitForExists(waitingTime);
        TestHelper.pressEnterKey();
        TestHelper.webView.waitForExists(waitingTime);

        // Check it does not block any trackers even with switch on
        TestHelper.menuButton.perform(click());
        TestHelper.blockCounterItem.waitForExists(waitingTime);
        assertTrue(TestHelper.blockToggleSwitch.isChecked());
        assertTrue(Objects.equals(TestHelper.blockCounterItem.getText(), "0"));

        // Go to settings, enable blocking of analytic trackers
        TestHelper.browserViewSettingsMenuItem.click();
        TestHelper.settingsList.waitForExists(waitingTime);
        assertTrue(!TestHelper.toggleAnalyticBlock.isChecked());
        TestHelper.toggleAnalyticBlock.click();
        assertTrue(TestHelper.toggleAnalyticBlock.isChecked());

        // Exit setting, refresh page- verify 1 tracker is blocked
        TestHelper.navigateUp.click();
        TestHelper.webView.waitForExists(waitingTime);
        TestHelper.menuButton.perform(click());
        TestHelper.refreshBtn.click();
        TestHelper.webView.waitForExists(waitingTime);
        TestHelper.menuButton.perform(click());
        TestHelper.blockCounterItem.waitForExists(waitingTime);
        assertTrue(TestHelper.blockToggleSwitch.isChecked());
        assertTrue(Objects.equals(TestHelper.blockCounterItem.getText(), "1"));
    }
}
