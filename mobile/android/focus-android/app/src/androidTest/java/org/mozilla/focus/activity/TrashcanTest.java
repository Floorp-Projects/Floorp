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

import static android.support.test.espresso.action.ViewActions.click;
import static junit.framework.Assert.assertTrue;
import static org.mozilla.focus.fragment.FirstrunFragment.FIRSTRUN_PREF;

// This test erases URL and checks for message
@RunWith(AndroidJUnit4.class)
public class TrashcanTest {

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
    public void TrashTest() throws InterruptedException, UiObjectNotFoundException {

        // Initialize UiDevice instance
        final long waitingTime = TestHelper.waitingTime;

        /* Wait for app to load, and take the First View screenshot */
        TestHelper.firstViewBtn.waitForExists(waitingTime);
        TestHelper.firstViewBtn.click();

        // Open a webpage
        TestHelper.urlBar.waitForExists(waitingTime);
        TestHelper.urlBar.click();
        TestHelper.inlineAutocompleteEditText.waitForExists(waitingTime);
        TestHelper.inlineAutocompleteEditText.clearTextField();
        TestHelper.inlineAutocompleteEditText.setText("mozilla");
        TestHelper.hint.waitForExists(waitingTime);
        TestHelper.pressEnterKey();
        TestHelper.webView.waitForExists(waitingTime);

        // Press erase button, and check for message and return to the main page
        TestHelper.floatingEraseButton.perform(click());
        TestHelper.erasedMsg.waitForExists(waitingTime);
        assertTrue(TestHelper.erasedMsg.exists());
        assertTrue(TestHelper.urlBar.exists());
    }
}
