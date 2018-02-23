/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


package org.mozilla.focus.activity;

import android.content.Context;
import android.preference.PreferenceManager;
import android.support.test.InstrumentationRegistry;
import android.support.test.rule.ActivityTestRule;
import android.support.test.uiautomator.UiObject;
import android.support.test.uiautomator.UiObjectNotFoundException;
import android.support.test.uiautomator.UiScrollable;
import android.support.test.uiautomator.UiSelector;
import android.widget.RadioButton;

import org.junit.After;
import org.junit.Assert;
import org.junit.Ignore;
import org.junit.Rule;
import org.junit.Test;
import org.mozilla.focus.helpers.TestHelper;

import static junit.framework.Assert.assertEquals;
import static junit.framework.Assert.assertTrue;
import static org.mozilla.focus.fragment.FirstrunFragment.FIRSTRUN_PREF;
import static org.mozilla.focus.helpers.EspressoHelper.openSettings;
import static org.mozilla.focus.helpers.TestHelper.waitingTime;
import static org.mozilla.focus.helpers.TestHelper.webPageLoadwaitingTime;

// This test checks the search engine can be changed
@Ignore("Need to pick another search engine")
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

    @After
    public void tearDown() throws Exception {
        mActivityTestRule.getActivity().finishAndRemoveTask();
    }

    @Test
    public void SearchTest() throws InterruptedException, UiObjectNotFoundException {

        UiObject SearchEngineSelection = TestHelper.settingsList.getChild(new UiSelector()
                .className("android.widget.LinearLayout")
                .instance(1));
        UiObject searchName = SearchEngineSelection.getChild(new UiSelector()
                .resourceId("android:id/title")
                .enabled(true));

        UiObject googleWebView = TestHelper.mDevice.findObject(new UiSelector()
                .description("mozilla focus - Google Search")
                .className("android.webkit.WebView"));

        /* Go to Settings and select the Search Engine */
        assertTrue(TestHelper.inlineAutocompleteEditText.waitForExists(waitingTime));

        openSettings();

        /* Set the search engine to Google */
        SearchEngineSelection.click();

        /* Get the dynamically generated search engine list from this settings page */
        UiScrollable SearchEngineList = new UiScrollable(new UiSelector()
                .resourceId(TestHelper.getAppName() + ":id/search_engine_group").enabled(true));

        UiObject GoogleSelection = SearchEngineList.getChildByText(new UiSelector()
                .className(RadioButton.class), "Google");

        GoogleSelection.waitForExists(waitingTime);
        GoogleSelection.click();
        TestHelper.pressBackKey();
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
        TestHelper.progressBar.waitForExists(webPageLoadwaitingTime);
        Assert.assertTrue(TestHelper.progressBar.waitUntilGone(webPageLoadwaitingTime));
        assertTrue (TestHelper.browserURLbar.getText().contains("google"));
        assertTrue (TestHelper.browserURLbar.getText().contains("mozilla"));
        assertTrue (TestHelper.browserURLbar.getText().contains("focus"));

        /* tap url bar, and check it displays search term instead of URL */
        TestHelper.browserURLbar.click();
        TestHelper.inlineAutocompleteEditText.waitForExists(waitingTime);
        assertEquals(TestHelper.inlineAutocompleteEditText.getText(), "mozilla focus");
        TestHelper.pressEnterKey();
        googleWebView.waitForExists(waitingTime);
        TestHelper.progressBar.waitForExists(webPageLoadwaitingTime);
        Assert.assertTrue(TestHelper.progressBar.waitUntilGone(webPageLoadwaitingTime));
        assertTrue (TestHelper.browserURLbar.getText().contains("google"));
        assertTrue (TestHelper.browserURLbar.getText().contains("mozilla"));
        assertTrue (TestHelper.browserURLbar.getText().contains("focus"));

         /* Now do another search */
        TestHelper.browserURLbar.click();
        TestHelper.inlineAutocompleteEditText.waitForExists(waitingTime);
        TestHelper.inlineAutocompleteEditText.clearTextField();
        TestHelper.inlineAutocompleteEditText.setText("mozilla focus");
        TestHelper.hint.waitForExists(waitingTime);

        // Check the search hint bar is correctly displayed
        assertTrue(TestHelper.hint.getText().equals("Search for mozilla focus"));
        TestHelper.hint.click();
    }
}
