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
import android.support.test.uiautomator.UiScrollable;
import android.support.test.uiautomator.UiSelector;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import static android.support.test.espresso.action.ViewActions.click;
import static junit.framework.Assert.assertTrue;
import static org.mozilla.focus.activity.TestHelper.waitingTime;
import static org.mozilla.focus.fragment.FirstrunFragment.FIRSTRUN_PREF;

// This test checks the search engine can be changed
@RunWith(AndroidJUnit4.class)
public class SearchEngineSelectionTest {

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
    public void SearchTest() throws InterruptedException, UiObjectNotFoundException {

        UiObject SearchEngineSelection = TestHelper.settingsList.getChild(new UiSelector()
                .className("android.widget.LinearLayout")
                .instance(1));
        UiObject searchName = SearchEngineSelection.getChild(new UiSelector()
                .resourceId("android:id/title")
                .enabled(true));
        UiObject SearchEngineList = new UiScrollable(new UiSelector()
                .resourceId("android:id/select_dialog_listview").enabled(true));
        UiObject GoogleSelection = SearchEngineList.getChild(new UiSelector()
                .resourceId("org.mozilla.focus.debug:id/title")
                .text("Google"));
        UiObject YahooSelection = SearchEngineList.getChild(new UiSelector()
                .resourceId("org.mozilla.focus.debug:id/title")
                .text("Yahoo"));
        UiObject googleWebView = TestHelper.mDevice.findObject(new UiSelector()
                .description("mozilla focus - Google Search")
                .className("android.webkit.WebView"));
        UiObject yahooWebView = TestHelper.mDevice.findObject(new UiSelector()
                .description("mozilla focus - - Yahoo Search Results")
                .className("android.webkit.WebView"));

        /* Go to Settings and select the Search Engine */
        TestHelper.menuButton.perform(click());
        TestHelper.settingsMenuItem.click();
        TestHelper.settingsHeading.waitForExists(waitingTime);

        /* Set the search engine to Google */
        SearchEngineSelection.click();
        GoogleSelection.waitForExists(waitingTime);
        GoogleSelection.click();
        // Now it's changed to Google
        assertTrue(searchName.getText().equals("Google"));
        TestHelper.settingsHeading.waitForExists(waitingTime);
        TestHelper.pressBackKey();

        /* load blank spaces and press enter key for search, it should not do anything */
        TestHelper.inlineAutocompleteEditText.waitForExists(waitingTime);
        TestHelper.inlineAutocompleteEditText.clearTextField();
        TestHelper.inlineAutocompleteEditText.setText("   ");
        TestHelper.pressEnterKey();
        assertTrue(TestHelper.inlineAutocompleteEditText.exists());

        /* Now do some search */
        TestHelper.inlineAutocompleteEditText.clearTextField();
        TestHelper.inlineAutocompleteEditText.setText("mozilla focus");
        TestHelper.hint.waitForExists(waitingTime);

        // Check the search hint bar is correctly displayed
        assertTrue(TestHelper.hint.getText().equals("Search for mozilla focus"));
        TestHelper.hint.click();

        /* Browser shows google search webview*/
        googleWebView.waitForExists(waitingTime);
        assertTrue (TestHelper.browserURLbar.getText().contains("google"));
        assertTrue (TestHelper.browserURLbar.getText().contains("mozilla"));
        assertTrue (TestHelper.browserURLbar.getText().contains("focus"));

        // Now let's change the search engine back to Yahoo
        TestHelper.menuButton.perform(click());
        TestHelper.browserViewSettingsMenuItem.click();
        TestHelper.settingsHeading.waitForExists(waitingTime);
        assertTrue(searchName.getText().equals("Google"));
        SearchEngineSelection.click();
        YahooSelection.waitForExists(waitingTime);
        YahooSelection.click();
        assertTrue(searchName.getText().equals("Yahoo"));
        TestHelper.pressBackKey();

         /* Now do another search */
        TestHelper.browserURLbar.click();
        TestHelper.inlineAutocompleteEditText.waitForExists(waitingTime);
        TestHelper.inlineAutocompleteEditText.clearTextField();
        TestHelper.inlineAutocompleteEditText.setText("mozilla focus");
        TestHelper.hint.waitForExists(waitingTime);

        // Check the search hint bar is correctly displayed
        assertTrue(TestHelper.hint.getText().equals("Search for mozilla focus"));
        TestHelper.hint.click();

        /* Browser shows google search webview*/
        yahooWebView.waitForExists(waitingTime);
        assertTrue (TestHelper.browserURLbar.getText().contains("yahoo"));
        assertTrue (TestHelper.browserURLbar.getText().contains("mozilla"));
        assertTrue (TestHelper.browserURLbar.getText().contains("focus"));
    }
}
