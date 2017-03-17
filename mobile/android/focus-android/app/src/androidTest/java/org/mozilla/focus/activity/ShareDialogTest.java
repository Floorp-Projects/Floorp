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
import static android.view.KeyEvent.KEYCODE_ENTER;
import static junit.framework.Assert.assertTrue;
import static org.hamcrest.Matchers.allOf;
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
            Resources resources = appContext.getResources();

            PreferenceManager.getDefaultSharedPreferences(appContext)
                    .edit()
                    .putBoolean(FIRSTRUN_PREF, false)
                    .apply();
        }
    };

    @Test
    public void shareTest() throws InterruptedException, UiObjectNotFoundException {
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
        UiObject webView = mDevice.findObject(new UiSelector()
                .className("android.webkit.Webview")
                .focused(true)
                .enabled(true));
        ViewInteraction menuButton = onView(
                allOf(withId(R.id.menu),
                        isDisplayed()));
        UiObject shareBtn = mDevice.findObject(new UiSelector()
                .resourceId("org.mozilla.focus.debug:id/share")
                .enabled(true));
        UiObject menuHeader = mDevice.findObject(new UiSelector()
                .resourceId("android:id/title")
                .text("Share with")
                .enabled(true));
        UiObject appList = mDevice.findObject(new UiSelector()
                .resourceId("android:id/resolver_list")
                .enabled(true));

        /* Wait for app to load, and take the First View screenshot */
        firstViewBtn.waitForExists(waitingTime);
        firstViewBtn.click();

        /* Go to a webpage */
        urlBar.waitForExists(waitingTime);
        urlBar.click();
        inlineAutocompleteEditText.waitForExists(waitingTime);
        inlineAutocompleteEditText.clearTextField();
        inlineAutocompleteEditText.setText("mozilla");
        mDevice.wait(Until.hasObject(hint),waitingTime);
        mDevice.pressKeyCode(KEYCODE_ENTER);
        webView.waitForExists(waitingTime);

        /* Select share */
        menuButton.perform(click());
        shareBtn.waitForExists(waitingTime);
        shareBtn.click();

        // For simulators, where apps are not installed, it'll take to message app
        menuHeader.waitForExists(waitingTime);
        assertTrue(menuHeader.exists());
        assertTrue(appList.exists());
    }
}
