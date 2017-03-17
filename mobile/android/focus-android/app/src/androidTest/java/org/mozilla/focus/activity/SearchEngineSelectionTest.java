/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.activity;

import android.content.Context;
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
                    .putBoolean(FIRSTRUN_PREF, false)
                    .apply();
        }
    };

    @Test
    public void SearchTest() throws InterruptedException, UiObjectNotFoundException {

        // Initialize UiDevice instance
        UiDevice mDevice = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation());
        final long waitingTime = DateUtils.SECOND_IN_MILLIS * 2;

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
        UiObject browserViewSettingsMenuItem = mDevice.findObject(new UiSelector()
                .resourceId("org.mozilla.focus.debug:id/settings")
                .clickable(true));
        UiScrollable settingsList = new UiScrollable(new UiSelector()
                .resourceId("android:id/list").scrollable(true));
        UiObject SearchEngineSelection = settingsList.getChild(new UiSelector()
                .className("android.widget.LinearLayout")
                .instance(0));
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
        UiObject inlineAutocompleteEditText = mDevice.findObject(new UiSelector()
                .resourceId("org.mozilla.focus.debug:id/url_edit")
                .focused(true)
                .enabled(true));
        UiObject searchHint = mDevice.findObject(new UiSelector()
                .resourceId("org.mozilla.focus.debug:id/search_hint")
                .clickable(true));
        UiObject googleWebView = mDevice.findObject(new UiSelector()
                .description("mozilla focus - Google Search")
                .className("android.webkit.WebView"));
        UiObject yahooWebView = mDevice.findObject(new UiSelector()
                .description("mozilla focus - - Yahoo Search Results")
                .className("android.webkit.WebView"));
        BySelector settingsHeading = By.clazz("android.view.View")
                .res("org.mozilla.focus.debug","toolbar")
                .enabled(true);

        /* Wait for app to load, and take the First View screenshot */
        firstViewBtn.click();
        urlBar.waitForExists(waitingTime);

        /* Go to Settings and select the Search Engine */
        menuButton.perform(click());
        settingsMenuItem.click();
        mDevice.wait(Until.hasObject(settingsHeading),waitingTime);

        /* Set the search engine to Google */
        SearchEngineSelection.click();
        mDevice.wait(Until.gone(settingsHeading),waitingTime);
        GoogleSelection.click();
        // Now it's changed to Google
        assertTrue(searchName.getText().equals("Google"));
        mDevice.pressBack();

        /* Now do some search */
        urlBar.click();
        inlineAutocompleteEditText.waitForExists(waitingTime);
        inlineAutocompleteEditText.clearTextField();
        inlineAutocompleteEditText.setText("mozilla focus");
        searchHint.waitForExists(waitingTime);

        // Check the search hint bar is correctly displayed
        assertTrue(searchHint.getText().equals("Search for mozilla focus"));
        searchHint.click();

        /* Browser shows google search webview*/
        googleWebView.waitForExists(waitingTime);
        assertTrue (urlBar.getText().contains("google"));
        assertTrue (urlBar.getText().contains("mozilla"));
        assertTrue (urlBar.getText().contains("focus"));

        // Now let's change the search engine back to Yahoo
        menuButton.perform(click());
        browserViewSettingsMenuItem.click();
        mDevice.wait(Until.hasObject(settingsHeading),waitingTime);
        assertTrue(searchName.getText().equals("Google"));
        SearchEngineSelection.click();
        mDevice.wait(Until.gone(settingsHeading),waitingTime);
        YahooSelection.click();
        assertTrue(searchName.getText().equals("Yahoo"));
        mDevice.pressBack();

         /* Now do another search */
        urlBar.click();
        inlineAutocompleteEditText.waitForExists(waitingTime);
        inlineAutocompleteEditText.clearTextField();
        inlineAutocompleteEditText.setText("mozilla focus");
        searchHint.waitForExists(waitingTime);

        // Check the search hint bar is correctly displayed
        assertTrue(searchHint.getText().equals("Search for mozilla focus"));
        searchHint.click();

        /* Browser shows google search webview*/
        yahooWebView.waitForExists(waitingTime);
        assertTrue (urlBar.getText().contains("yahoo"));
        assertTrue (urlBar.getText().contains("mozilla"));
        assertTrue (urlBar.getText().contains("focus"));

    }
}
