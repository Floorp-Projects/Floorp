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

import junit.framework.Assert;

import org.junit.After;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mozilla.focus.helpers.TestHelper;

import static org.mozilla.focus.fragment.FirstrunFragment.FIRSTRUN_PREF;
import static org.mozilla.focus.helpers.TestHelper.waitingTime;

@RunWith(AndroidJUnit4.class)
public class OnBoardingTest {

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

    @After
    public void tearDown() throws Exception {
        mActivityTestRule.getActivity().finishAndRemoveTask();
    }

    @Test
    public void OnBoardingTest() throws InterruptedException, UiObjectNotFoundException {

        // Let's search for something
        TestHelper.firstSlide.waitForExists(waitingTime);
        Assert.assertTrue(TestHelper.firstSlide.exists());
        TestHelper.nextBtn.click();
        TestHelper.secondSlide.waitForExists(waitingTime);
        Assert.assertTrue(TestHelper.secondSlide.exists());
        TestHelper.nextBtn.click();
        TestHelper.thirdSlide.waitForExists(waitingTime);
        Assert.assertTrue(TestHelper.thirdSlide.exists());
        TestHelper.nextBtn.click();
        TestHelper.lastSlide.waitForExists(waitingTime);
        Assert.assertTrue(TestHelper.lastSlide.exists());
        TestHelper.finishBtn.click();

        TestHelper.inlineAutocompleteEditText.waitForExists(waitingTime);
        Assert.assertTrue(TestHelper.inlineAutocompleteEditText.exists());
    }
}
