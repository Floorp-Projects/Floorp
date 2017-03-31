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

import static android.view.KeyEvent.KEYCODE_E;
import static android.view.KeyEvent.KEYCODE_F;
import static android.view.KeyEvent.KEYCODE_I;
import static android.view.KeyEvent.KEYCODE_L;
import static android.view.KeyEvent.KEYCODE_N;
import static android.view.KeyEvent.KEYCODE_T;
import static android.view.KeyEvent.KEYCODE_X;
import static junit.framework.Assert.assertTrue;
import static org.mozilla.focus.fragment.FirstrunFragment.FIRSTRUN_PREF;

// This test checks whether URL and displayed site are in sync
@RunWith(AndroidJUnit4.class)
public class URLMismatchTest {

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
    public void MismatchTest() throws InterruptedException, UiObjectNotFoundException {
        final long waitingTime = TestHelper.waitingTime * 3;

        UiObject netflixLogo = TestHelper.mDevice.findObject(new UiSelector()
                .className("android.view.View")
                .description("Netflix")
                .clickable(true));

        // Search for netflix
        TestHelper.firstViewBtn.waitForExists(waitingTime);
        TestHelper.firstViewBtn.click();
        TestHelper.urlBar.waitForExists(waitingTime);
        TestHelper.urlBar.click();

        TestHelper.inlineAutocompleteEditText.waitForExists(waitingTime);
        TestHelper.inlineAutocompleteEditText.clearTextField();
        TestHelper.mDevice.pressKeyCode(KEYCODE_N);
        TestHelper.mDevice.pressKeyCode(KEYCODE_E);
        TestHelper.mDevice.pressKeyCode(KEYCODE_T);
        TestHelper.mDevice.pressKeyCode(KEYCODE_F);
        TestHelper.mDevice.pressKeyCode(KEYCODE_L);
        TestHelper.mDevice.pressKeyCode(KEYCODE_I);
        TestHelper.mDevice.pressKeyCode(KEYCODE_X);
        assertTrue(TestHelper.hint.getText().equals("Search for netflix"));
        TestHelper.hint.click();
        TestHelper.webView.waitForExists(waitingTime);
        assertTrue (TestHelper.browserURLbar.getText().contains("netflix"));

        // Now, go to netflix.com directly
        TestHelper.browserURLbar.click();
        TestHelper.inlineAutocompleteEditText.clearTextField();;
        TestHelper.mDevice.pressKeyCode(KEYCODE_N);
        TestHelper.mDevice.pressKeyCode(KEYCODE_E);
        TestHelper.mDevice.pressKeyCode(KEYCODE_T);
        TestHelper.hint.waitForExists(waitingTime);
        assertTrue (TestHelper.inlineAutocompleteEditText.getText().equals("netflix.com"));
        TestHelper.pressEnterKey();

        // Verify it's in netflix site and both URL and the displayed site match
        // For API 25 (N) devices in BB, it does not allow https connection,
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.N) {
            TestHelper.webView.waitForExists(waitingTime);
            netflixLogo.waitForExists(waitingTime);
            assertTrue(TestHelper.browserURLbar.getText().contains("www.netflix.com"));
            assertTrue(netflixLogo.exists());
        }
    }
}
