/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.activity;

import android.content.Context;
import android.content.res.Resources;
import android.preference.PreferenceManager;
import android.support.test.InstrumentationRegistry;
import android.support.test.espresso.ViewInteraction;
import android.support.test.rule.ActivityTestRule;
import android.support.test.runner.AndroidJUnit4;
import android.support.test.uiautomator.By;
import android.support.test.uiautomator.BySelector;
import android.support.test.uiautomator.UiDevice;
import android.support.test.uiautomator.UiObject;
import android.support.test.uiautomator.UiObjectNotFoundException;
import android.support.test.uiautomator.UiScrollable;
import android.support.test.uiautomator.UiSelector;
import android.support.test.uiautomator.Until;
import android.text.format.DateUtils;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mozilla.focus.R;

import static android.support.test.espresso.Espresso.onView;
import static android.support.test.espresso.action.ViewActions.click;
import static android.support.test.espresso.matcher.ViewMatchers.isDisplayed;
import static android.support.test.espresso.matcher.ViewMatchers.withId;
import static junit.framework.Assert.assertTrue;
import static org.hamcrest.Matchers.allOf;
import static org.mozilla.focus.fragment.FirstrunFragment.FIRSTRUN_PREF;

// This test checks all the headings in the Settings menu are there
@RunWith(AndroidJUnit4.class)
public class SettingsAppearanceTest {

    @Rule
    public ActivityTestRule<MainActivity> mActivityTestRule
            = new ActivityTestRule<MainActivity>(MainActivity.class) {

        @Override
        protected void beforeActivityLaunched() {
            super.beforeActivityLaunched();

            Context appContext = InstrumentationRegistry.getInstrumentation()
                    .getTargetContext()
                    .getApplicationContext();
            Resources resources = appContext.getResources();

            PreferenceManager.getDefaultSharedPreferences(appContext)
                    .edit()
                    .putBoolean(FIRSTRUN_PREF, false)
                    .apply();
        }
    };

    public static void swipeDownNotificationBar (UiDevice deviceInstance) {
        int dHeight = deviceInstance.getDisplayHeight();
        int dWidth = deviceInstance.getDisplayWidth();
        int xScrollPosition = dWidth/2;
        int yScrollStop = dHeight/4 * 3;
        deviceInstance.swipe(
                xScrollPosition,
                yScrollStop,
                xScrollPosition,
                0,
                20
        );
    }

    @Test
    public void settingsScreenTest() throws InterruptedException, UiObjectNotFoundException {
        // Initialize UiDevice instance
        UiDevice mDevice = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation());
        final long waitingTime = DateUtils.SECOND_IN_MILLIS * 2;

        /* Wait for app to load, and take the First View screenshot */
        UiObject firstViewBtn = mDevice.findObject(new UiSelector()
                .resourceId("org.mozilla.focus.debug:id/firstrun_exitbutton")
                .enabled(true));
        UiObject urlBar = mDevice.findObject(new UiSelector()
                .resourceId("org.mozilla.focus.debug:id/url")
                .clickable(true));
        ViewInteraction menuButton = onView(
                allOf(withId(R.id.menu),
                        isDisplayed()));
        UiObject settingsMenuItem = mDevice.findObject(new UiSelector()
                .className("android.widget.LinearLayout")
                .instance(3));
        BySelector settingsHeading = By.clazz("android.view.View")
                .res("org.mozilla.focus.debug","toolbar")
                .enabled(true);
        UiScrollable settingsList = new UiScrollable(new UiSelector()
                .resourceId("android:id/list").scrollable(true));
        UiObject SearchEngineSelection = settingsList.getChild(new UiSelector()
                .className("android.widget.LinearLayout")
                .instance(0));
        UiObject searchHeading = mDevice.findObject(new UiSelector()
                .text("Search")
                .resourceId("android:id/title"));
        UiObject privacyHeading = mDevice.findObject(new UiSelector()
                .text("Privacy")
                .resourceId("android:id/title"));
        UiObject perfHeading = mDevice.findObject(new UiSelector()
                .text("Performance")
                .resourceId("android:id/title"));
        UiObject mozHeading = mDevice.findObject(new UiSelector()
                .text("Mozilla")
                .resourceId("android:id/title"));

        firstViewBtn.waitForExists(waitingTime);

        /* Go to Settings */
        firstViewBtn.click();
        urlBar.waitForExists(waitingTime);
        menuButton.perform(click());
        settingsMenuItem.click();
        mDevice.wait(Until.hasObject(settingsHeading),waitingTime);

        /* Check the first element and other headings are present */
        assertTrue(SearchEngineSelection.isEnabled());
        assertTrue(searchHeading.exists());
        assertTrue(privacyHeading.exists());
        assertTrue(perfHeading.exists());
        swipeDownNotificationBar(mDevice);
        mozHeading.waitForExists(waitingTime);
        assertTrue(mozHeading.exists());
    }
}
