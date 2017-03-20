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

// This test visits each page and checks whether some essential elements are being displayed
@RunWith(AndroidJUnit4.class)
public class PageVisitTest {

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
    public void visitPagesTest() throws InterruptedException, UiObjectNotFoundException {

        long waitTime = TestHelper.waitingTime;

        /********* Page View Locators ***********/
        UiObject rightsPartialText = TestHelper.mDevice.findObject(new UiSelector()
                .className("android.view.View")
                .description("Firefox Focus is free and open source software " +
                        "made by Mozilla and other contributors."));
        UiObject rightsHeading = TestHelper.mDevice.findObject(new UiSelector()
                .className("android.widget.TextView")
                .text("Your Rights"));
        UiObject aboutPartialText = TestHelper.mDevice.findObject(new UiSelector()
                .className("android.view.View")
                .description("Firefox Focus (Dev) puts you in control."));
        UiObject aboutHeading = TestHelper.mDevice.findObject(new UiSelector()
                .className("android.widget.TextView")
                .text("About"));
        UiObject helpHeading = TestHelper.mDevice.findObject(new UiSelector()
                .className("android.widget.TextView")
                .text("Help"));
        UiObject helpView = TestHelper.mDevice.findObject(new UiSelector()
                .className("android.webkit.WebView")
                .description("Mozilla Support - English"));

        /* Wait for app to load, and take the First View screenshot */
        TestHelper.firstViewBtn.waitForExists(waitTime);
        TestHelper.firstViewBtn.click();

        /* Main View Menu */
        TestHelper.menuButton.perform(click());

        /* Go to Your Rights Page */
        TestHelper.RightsItem.click();
        TestHelper.webView.waitForExists(waitTime);
        assertTrue(rightsHeading.exists());
        assertTrue(rightsPartialText.exists());

        /* Go to About Page */
        TestHelper.mDevice.pressBack();
        TestHelper.waitForIdle();
        TestHelper.menuButton.perform(click());
        TestHelper.AboutItem.click();
        TestHelper.webView.waitForExists(waitTime);
        assertTrue(aboutHeading.exists());
        assertTrue(aboutPartialText.exists());

        /* Help Page */
        TestHelper.mDevice.pressBack();
        TestHelper.waitForIdle();
        TestHelper.menuButton.perform(click());
        TestHelper.HelpItem.click();
        helpView.waitForExists(waitTime * 3);
        assertTrue(helpHeading.exists());
        assertTrue(helpView.exists());
    }
}
