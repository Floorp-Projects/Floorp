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

import static android.view.KeyEvent.KEYCODE_A;
import static android.view.KeyEvent.KEYCODE_I;
import static android.view.KeyEvent.KEYCODE_L;
import static android.view.KeyEvent.KEYCODE_M;
import static android.view.KeyEvent.KEYCODE_O;
import static android.view.KeyEvent.KEYCODE_Z;
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
                    .putBoolean(FIRSTRUN_PREF, true)
                    .apply();
        }
    };

    @Test
    public void MismatchTest() throws InterruptedException, UiObjectNotFoundException {
        final long waitingTime = TestHelper.waitingTime * 3;

        UiObject mozillaLogo = TestHelper.mDevice.findObject(new UiSelector()
                .className("android.webkit.WebView")
                .description("Internet for people, not profit — Mozilla"));

        // Search for mozilla
        TestHelper.urlBar.waitForExists(waitingTime);
        TestHelper.urlBar.click();

        TestHelper.inlineAutocompleteEditText.waitForExists(waitingTime);
        TestHelper.inlineAutocompleteEditText.clearTextField();
        TestHelper.mDevice.pressKeyCode(KEYCODE_M);
        TestHelper.mDevice.pressKeyCode(KEYCODE_O);
        TestHelper.mDevice.pressKeyCode(KEYCODE_Z);
        TestHelper.mDevice.pressKeyCode(KEYCODE_I);
        TestHelper.mDevice.pressKeyCode(KEYCODE_L);
        TestHelper.mDevice.pressKeyCode(KEYCODE_L);
        TestHelper.mDevice.pressKeyCode(KEYCODE_A);
        assertTrue(TestHelper.hint.getText().equals("Search for mozilla"));
        TestHelper.hint.click();
        TestHelper.webView.waitForExists(waitingTime);
        assertTrue (TestHelper.browserURLbar.getText().contains("mozilla"));

        // Now, go to mozilla.org directly
        TestHelper.browserURLbar.click();
        TestHelper.inlineAutocompleteEditText.clearTextField();;
        TestHelper.mDevice.pressKeyCode(KEYCODE_M);
        TestHelper.mDevice.pressKeyCode(KEYCODE_O);
        TestHelper.mDevice.pressKeyCode(KEYCODE_Z);
        TestHelper.hint.waitForExists(waitingTime);
        assertTrue (TestHelper.inlineAutocompleteEditText.getText().equals("mozilla.org"));
        TestHelper.pressEnterKey();

        TestHelper.webView.waitForExists(waitingTime);
        mozillaLogo.waitForExists(waitingTime);
        assertTrue(TestHelper.browserURLbar.getText().contains("www.mozilla.org"));
        assertTrue(mozillaLogo.exists());
    }
}
