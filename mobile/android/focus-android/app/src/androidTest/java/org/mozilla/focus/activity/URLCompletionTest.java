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
import android.support.test.uiautomator.By;
import android.support.test.uiautomator.BySelector;
import android.support.test.uiautomator.UiDevice;
import android.support.test.uiautomator.UiObject;
import android.support.test.uiautomator.UiObjectNotFoundException;
import android.support.test.uiautomator.UiSelector;
import android.support.test.uiautomator.Until;
import android.text.format.DateUtils;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import static junit.framework.Assert.assertTrue;
import static org.mozilla.focus.fragment.FirstrunFragment.FIRSTRUN_PREF;

@RunWith(AndroidJUnit4.class)
public class URLCompletionTest {

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
    public void CompletionTest() throws InterruptedException, UiObjectNotFoundException {
        // Initialize UiDevice instance
        UiDevice mDevice = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation());
        final long waitingTime = DateUtils.SECOND_IN_MILLIS * 2;

        UiObject firstViewBtn = mDevice.findObject(new UiSelector()
                .resourceId("org.mozilla.focus.debug:id/firstrun_exitbutton")
                .enabled(true));
        UiObject urlBar = mDevice.findObject(new UiSelector()
                .resourceId("org.mozilla.focus.debug:id/url")
                .clickable(true));
        UiObject inlineAutocompleteEditText = mDevice.findObject(new UiSelector()
                .resourceId("org.mozilla.focus.debug:id/url_edit")
                .focused(true)
                .enabled(true));
        BySelector hint = By.clazz("android.widget.TextView")
                .res("org.mozilla.focus.debug","search_hint")
                .clickable(true);

        /* Wait for first time UI to load*/
        firstViewBtn.waitForExists(waitingTime);
        firstViewBtn.click();

        /* Open website */
        urlBar.waitForExists(waitingTime);
        urlBar.click();

        /* type a partial url, and check it autocompletes*/
        inlineAutocompleteEditText.waitForExists(waitingTime);
        inlineAutocompleteEditText.clearTextField();
        inlineAutocompleteEditText.setText("mozilla");
        mDevice.wait(Until.hasObject(hint),waitingTime);
        assertTrue (inlineAutocompleteEditText.getText().equals("mozilla.org"));

        /* type a full url, and check it does not autocomplete */
        inlineAutocompleteEditText.clearTextField();;
        inlineAutocompleteEditText.setText("http://www.mozilla.org");
        mDevice.wait(Until.hasObject(hint),waitingTime);
        assertTrue (inlineAutocompleteEditText.getText().equals("http://www.mozilla.org"));
    }
}
