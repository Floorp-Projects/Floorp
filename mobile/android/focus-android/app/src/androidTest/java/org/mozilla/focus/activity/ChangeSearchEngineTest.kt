/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.focus.activity

import android.view.KeyEvent
import android.widget.RadioButton
import androidx.test.espresso.Espresso
import androidx.test.espresso.assertion.ViewAssertions
import androidx.test.espresso.matcher.ViewMatchers
import androidx.test.uiautomator.UiObjectNotFoundException
import androidx.test.uiautomator.UiScrollable
import androidx.test.uiautomator.UiSelector
import org.hamcrest.Matchers
import org.junit.After
import org.junit.Assert
import org.junit.Ignore
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.junit.runners.Parameterized
import org.mozilla.focus.R
import org.mozilla.focus.helpers.EspressoHelper.childAtPosition
import org.mozilla.focus.helpers.EspressoHelper.openSettings
import org.mozilla.focus.helpers.MainActivityFirstrunTestRule
import org.mozilla.focus.helpers.TestHelper
import org.mozilla.focus.helpers.TestHelper.appName
import org.mozilla.focus.helpers.TestHelper.pressBackKey
import org.mozilla.focus.helpers.TestHelper.pressEnterKey
import org.mozilla.focus.helpers.TestHelper.waitingTime
import org.mozilla.focus.helpers.TestHelper.webPageLoadwaitingTime
import java.util.Arrays

// This test checks the search engine can be changed
// https://testrail.stage.mozaws.net/index.php?/cases/view/47588
@RunWith(Parameterized::class)
@Ignore("This test was written specifically for WebView and needs to be adapted for GeckoView")
class ChangeSearchEngineTest {
    @Parameterized.Parameter
    var mSearchEngine: String? = null

    @get: Rule
    var mActivityTestRule = MainActivityFirstrunTestRule(showFirstRun = false)

    @After
    fun tearDown() {
        mActivityTestRule.activity.finishAndRemoveTask()
    }

    @Suppress("LongMethod")
    @Test
    @Throws(UiObjectNotFoundException::class)
    fun SearchTest() {
        val searchMenu = TestHelper.settingsMenu.getChild(
            UiSelector()
                .className("android.widget.LinearLayout")
                .instance(2)
        )
        val searchEngineSelectorLabel = searchMenu.getChild(
            UiSelector()
                .resourceId("android:id/title")
                .text("Search")
                .enabled(true)
        )
        val searchString = String.format("mozilla focus - %s Search", mSearchEngine)
        val googleWebView = TestHelper.mDevice.findObject(
            UiSelector()
                .description(searchString)
                .className("android.webkit.WebView")
        )

        // Open [settings menu] and select Search
        Assert.assertTrue(TestHelper.inlineAutocompleteEditText.waitForExists(waitingTime))
        openSettings()
        TestHelper.settingsHeading.waitForExists(waitingTime)
        searchEngineSelectorLabel.waitForExists(waitingTime)
        searchEngineSelectorLabel.click()

        // Open [settings menu] > [search engine menu] and click "Search engine" label
        val defaultSearchEngineLabel = TestHelper.settingsMenu.getChild(
            UiSelector()
                .className("android.widget.LinearLayout")
                .instance(0)
        )
        defaultSearchEngineLabel.waitForExists(waitingTime)
        defaultSearchEngineLabel.click()

        // Open [settings menu] > [search engine menu] > [search engine list menu]
        // then select desired search engine
        val searchEngineList = UiScrollable(
            UiSelector()
                .resourceId(appName + ":id/search_engine_group").enabled(true)
        )
        val defaultEngineSelection = searchEngineList.getChildByText(
            UiSelector()
                .className(RadioButton::class.java), mSearchEngine
        )
        defaultEngineSelection.waitForExists(waitingTime)
        Assert.assertTrue(defaultEngineSelection.text == mSearchEngine)
        defaultEngineSelection.click()
        pressBackKey()
        TestHelper.settingsHeading.waitForExists(waitingTime)
        val defaultSearchEngine = TestHelper.mDevice.findObject(
            UiSelector()
                .text(mSearchEngine)
                .resourceId("android:id/summary")
        )
        Assert.assertTrue(defaultSearchEngine.text == mSearchEngine)
        pressBackKey()
        pressBackKey()

        // Do search on blank spaces and press enter for search (should not do anything)
        TestHelper.inlineAutocompleteEditText.waitForExists(waitingTime)
        TestHelper.inlineAutocompleteEditText.clearTextField()
        TestHelper.inlineAutocompleteEditText.text = "   "
        pressEnterKey()
        Assert.assertTrue(TestHelper.inlineAutocompleteEditText.exists())

        // Do search on text string
        TestHelper.inlineAutocompleteEditText.clearTextField()
        TestHelper.inlineAutocompleteEditText.text = "mozilla "
        // Would you like to turn on search suggestions? Yes No
        // fresh install only)
        if (TestHelper.searchSuggestionsTitle.exists()) {
            TestHelper.searchSuggestionsButtonYes.waitForExists(waitingTime)
            TestHelper.searchSuggestionsButtonYes.click()
        }

        // verify search hints... "mozilla firefox", "mozilla careers", etc.TestHelper.suggestionList.waitForExists(waitingTime);
        TestHelper.mDevice.pressKeyCode(KeyEvent.KEYCODE_SPACE)
        TestHelper.suggestionList.waitForExists(waitingTime)
        Assert.assertTrue(TestHelper.suggestionList.childCount >= 1)
        Espresso.onView(
            Matchers.allOf(
                ViewMatchers.withText(Matchers.containsString("mozilla")),
                ViewMatchers.withId(R.id.searchView)
            )
        )
            .check(ViewAssertions.matches(ViewMatchers.isDisplayed()))
        // we expect min=1, max=5
        var count = 0
        val maxCount = 3
        // while (count <= maxCount) {
        while (count < maxCount) {
            Espresso.onView(
                Matchers.allOf(
                    ViewMatchers.withText(Matchers.containsString("mozilla")),
                    ViewMatchers.withId(R.id.suggestion),
                    ViewMatchers.isDescendantOfA(
                        childAtPosition(
                            ViewMatchers.withId(R.id.suggestionList),
                            count
                        )
                    )
                )
            )
                .check(ViewAssertions.matches(ViewMatchers.isDisplayed()))
            count++
        }

        // Tap URL bar, check it displays search term (instead of URL)
        TestHelper.inlineAutocompleteEditText.waitForExists(waitingTime)
        TestHelper.inlineAutocompleteEditText.click()
        TestHelper.inlineAutocompleteEditText.clearTextField()
        TestHelper.inlineAutocompleteEditText.text = "mozilla focus"
        pressEnterKey()
        googleWebView.waitForExists(waitingTime)
        TestHelper.progressBar.waitUntilGone(webPageLoadwaitingTime)

        // Search for words: <Google|DuckDuckGo|etc.>, mozilla, focus
        Assert.assertTrue(TestHelper.browserURLbar.text.contains(mSearchEngine!!.toLowerCase()))
        Assert.assertTrue(TestHelper.browserURLbar.text.contains("mozilla"))
        Assert.assertTrue(TestHelper.browserURLbar.text.contains("focus"))
    }

    companion object {
        @Parameterized.Parameters
        fun data(): Iterable<*> {
            return Arrays.asList("Google", "DuckDuckGo")
        }
    }
}
