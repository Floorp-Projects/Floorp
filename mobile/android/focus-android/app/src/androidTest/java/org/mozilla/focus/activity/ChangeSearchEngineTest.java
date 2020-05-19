/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.activity;

import android.content.Context;

import androidx.preference.PreferenceManager;
import androidx.test.InstrumentationRegistry;
import androidx.test.rule.ActivityTestRule;
import androidx.test.uiautomator.UiObject;
import androidx.test.uiautomator.UiObjectNotFoundException;
import androidx.test.uiautomator.UiScrollable;
import androidx.test.uiautomator.UiSelector;
import android.widget.RadioButton;

import org.junit.After;
import org.junit.Ignore;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.Parameterized;
import org.mozilla.focus.R;
import org.mozilla.focus.helpers.TestHelper;
import org.mozilla.focus.utils.AppConstants;

import java.util.Arrays;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isDescendantOfA;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;
import static android.view.KeyEvent.KEYCODE_SPACE;
import static junit.framework.Assert.assertTrue;
import static org.hamcrest.Matchers.allOf;
import static org.hamcrest.Matchers.containsString;
import static org.mozilla.focus.fragment.FirstrunFragment.FIRSTRUN_PREF;
import static org.mozilla.focus.helpers.EspressoHelper.childAtPosition;
import static org.mozilla.focus.helpers.EspressoHelper.openSettings;
import static org.mozilla.focus.helpers.TestHelper.waitingTime;
import static org.mozilla.focus.helpers.TestHelper.webPageLoadwaitingTime;

// This test checks the search engine can be changed
// https://testrail.stage.mozaws.net/index.php?/cases/view/47588
@RunWith(Parameterized.class)
@Ignore("This test was written specifically for WebView and needs to be adapted for GeckoView")
public class ChangeSearchEngineTest {
    @Parameterized.Parameter
    public String mSearchEngine;

    @Parameterized.Parameters
    public static Iterable<?> data() {
        return Arrays.asList("Google", "DuckDuckGo");
    }

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
    public void tearDown() {
        mActivityTestRule.getActivity().finishAndRemoveTask();
    }

    @Test
    public void SearchTest() throws UiObjectNotFoundException {
        UiObject searchMenu = TestHelper.settingsMenu.getChild(new UiSelector()
                .className("android.widget.LinearLayout")
                .instance(2));

        UiObject searchEngineSelectorLabel = searchMenu.getChild(new UiSelector()
                .resourceId("android:id/title")
                .text("Search")
                .enabled(true));

        String searchString = String.format("mozilla focus - %s Search", mSearchEngine);
        UiObject googleWebView = TestHelper.mDevice.findObject(new UiSelector()
                .description(searchString)
                .className("android.webkit.WebView"));

        // Open [settings menu] and select Search
        assertTrue(TestHelper.inlineAutocompleteEditText.waitForExists(waitingTime));
        openSettings();
        TestHelper.settingsHeading.waitForExists(waitingTime);
        searchEngineSelectorLabel.waitForExists(waitingTime);
        searchEngineSelectorLabel.click();

        // Open [settings menu] > [search engine menu] and click "Search engine" label
        UiObject defaultSearchEngineLabel = TestHelper.settingsMenu.getChild(new UiSelector()
                .className("android.widget.LinearLayout")
                .instance(0));
        defaultSearchEngineLabel.waitForExists(waitingTime);
        defaultSearchEngineLabel.click();

        // Open [settings menu] > [search engine menu] > [search engine list menu]
        // then select desired search engine
        UiScrollable searchEngineList = new UiScrollable(new UiSelector()
                .resourceId(TestHelper.getAppName() + ":id/search_engine_group").enabled(true));
        UiObject defaultEngineSelection = searchEngineList.getChildByText(new UiSelector()
                .className(RadioButton.class), mSearchEngine);
        defaultEngineSelection.waitForExists(waitingTime);
        assertTrue(defaultEngineSelection.getText().equals(mSearchEngine));
        defaultEngineSelection.click();
        TestHelper.pressBackKey();
        TestHelper.settingsHeading.waitForExists(waitingTime);
        UiObject defaultSearchEngine = TestHelper.mDevice.findObject(new UiSelector()
                .text(mSearchEngine)
                .resourceId("android:id/summary"));
        assertTrue(defaultSearchEngine.getText().equals(mSearchEngine));
        TestHelper.pressBackKey();
        TestHelper.pressBackKey();

        // Do search on blank spaces and press enter for search (should not do anything)
        TestHelper.inlineAutocompleteEditText.waitForExists(waitingTime);
        TestHelper.inlineAutocompleteEditText.clearTextField();
        TestHelper.inlineAutocompleteEditText.setText("   ");
        TestHelper.pressEnterKey();
        assertTrue(TestHelper.inlineAutocompleteEditText.exists());

        // Do search on text string
        TestHelper.inlineAutocompleteEditText.clearTextField();
        TestHelper.inlineAutocompleteEditText.setText("mozilla ");
        // Would you like to turn on search suggestions? Yes No
        // fresh install only)
        if (TestHelper.searchSuggestionsTitle.exists()) {
            TestHelper.searchSuggestionsButtonYes.waitForExists(waitingTime);
            TestHelper.searchSuggestionsButtonYes.click();
        }

        // verify search hints... "mozilla firefox", "mozilla careers", etc.TestHelper.suggestionList.waitForExists(waitingTime);
        TestHelper.mDevice.pressKeyCode(KEYCODE_SPACE);
        TestHelper.suggestionList.waitForExists(waitingTime);
        assertTrue(TestHelper.suggestionList.getChildCount() >= 1);

        onView(allOf(withText(containsString("mozilla")),
                withId(R.id.searchView)))
                .check(matches(isDisplayed()));
        // we expect min=1, max=5
        int count = 0;
        int maxCount = 3;
        //while (count <= maxCount) {
        while (count < maxCount) {
            onView(allOf(withText(containsString("mozilla")),
                    withId(R.id.suggestion),
                    isDescendantOfA(childAtPosition(withId(R.id.suggestionList), count))))
                    .check(matches(isDisplayed()));
            count++;
        }

        // Tap URL bar, check it displays search term (instead of URL)
       TestHelper.inlineAutocompleteEditText.waitForExists(waitingTime);
       TestHelper.inlineAutocompleteEditText.click();
       TestHelper.inlineAutocompleteEditText.clearTextField();
       TestHelper.inlineAutocompleteEditText.setText("mozilla focus");
       TestHelper.pressEnterKey();
       googleWebView.waitForExists(waitingTime);
       TestHelper.progressBar.waitUntilGone(webPageLoadwaitingTime);

       // Search for words: <Google|DuckDuckGo|etc.>, mozilla, focus
       assertTrue(TestHelper.browserURLbar.getText().contains(mSearchEngine.toLowerCase()));
       assertTrue(TestHelper.browserURLbar.getText().contains("mozilla"));
       assertTrue(TestHelper.browserURLbar.getText().contains("focus"));
    }
}
