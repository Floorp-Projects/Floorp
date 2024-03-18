/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.activity.robots

import androidx.test.uiautomator.By
import androidx.test.uiautomator.UiSelector
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.mozilla.focus.R
import org.mozilla.focus.helpers.Constants.LONG_CLICK_DURATION
import org.mozilla.focus.helpers.TestHelper.getStringResource
import org.mozilla.focus.helpers.TestHelper.mDevice
import org.mozilla.focus.helpers.TestHelper.packageName
import org.mozilla.focus.helpers.TestHelper.pressEnterKey
import org.mozilla.focus.helpers.TestHelper.waitingTime
import org.mozilla.focus.idlingResources.SessionLoadedIdlingResource

class SearchRobot {

    fun verifySearchBarIsDisplayed() = assertTrue(searchBar.exists())

    fun typeInSearchBar(searchString: String) {
        assertTrue(searchBar.waitForExists(waitingTime))
        searchBar.clearTextField()
        searchBar.setText(searchString)
    }

    // Would you like to turn on search suggestions? Yes No
    // fresh install only
    fun allowEnableSearchSuggestions() {
        if (searchSuggestionsTitle.waitForExists(waitingTime)) {
            searchSuggestionsButtonYes.waitForExists(waitingTime)
            searchSuggestionsButtonYes.click()
        }
    }

    // Would you like to turn on search suggestions? Yes No
    // fresh install only
    fun denyEnableSearchSuggestions() {
        if (searchSuggestionsTitle.waitForExists(waitingTime)) {
            searchSuggestionsButtonNo.waitForExists(waitingTime)
            searchSuggestionsButtonNo.click()
        }
    }

    fun verifySearchSuggestionsAreShown() {
        suggestionsList.waitForExists(waitingTime)
        assertTrue(suggestionsList.childCount >= 1)
    }

    fun verifySearchSuggestionsAreNotShown() {
        assertFalse(suggestionsList.exists())
    }

    fun verifySearchEditBarContainsText(text: String) {
        mDevice.findObject(UiSelector().textContains(text)).waitForExists(waitingTime)
        assertTrue(searchBar.text.equals(text))
    }

    fun verifySearchEditBarIsEmpty() {
        searchBar.waitForExists(waitingTime)
        assertTrue(searchBar.text.equals(getStringResource(R.string.urlbar_hint)))
    }

    fun clickToolbar() {
        toolbar.waitForExists(waitingTime)
        toolbar.click()
    }

    fun longPressSearchBar() {
        searchBar.waitForExists(waitingTime)
        mDevice.findObject(By.res("$packageName:id/mozac_browser_toolbar_edit_url_view")).click(LONG_CLICK_DURATION)
    }

    fun clearSearchBar() = clearSearchButton.click()

    fun verifySearchSuggestionsContain(title: String) {
        assertTrue(
            suggestionsList.getChild(UiSelector().textContains(title)).waitForExists(waitingTime),
        )
    }

    class Transition {

        private lateinit var sessionLoadedIdlingResource: SessionLoadedIdlingResource

        fun loadPage(url: String, interact: BrowserRobot.() -> Unit): BrowserRobot.Transition {
            val geckoEngineView = mDevice.findObject(UiSelector().resourceId("$packageName:id/engineView"))
            val trackingProtectionDialog = mDevice.findObject(UiSelector().resourceId("$packageName:id/message"))

            sessionLoadedIdlingResource = SessionLoadedIdlingResource()

            searchScreen { typeInSearchBar(url) }
            pressEnterKey()

            runWithIdleRes(sessionLoadedIdlingResource) {
                assertTrue(
                    BrowserRobot().progressBar.waitUntilGone(waitingTime),
                )
                assertTrue(
                    geckoEngineView.waitForExists(waitingTime) ||
                        trackingProtectionDialog.waitForExists(waitingTime),
                )
            }

            BrowserRobot().interact()
            return BrowserRobot.Transition()
        }

        fun pasteAndLoadLink(interact: BrowserRobot.() -> Unit): BrowserRobot.Transition {
            var currentTries = 0
            while (currentTries++ < 3) {
                try {
                    mDevice.findObject(UiSelector().textContains("Paste")).waitForExists(waitingTime)
                    val pasteText = mDevice.findObject(By.textContains("Paste"))
                    pasteText.click()
                    mDevice.pressEnter()
                    break
                } catch (e: NullPointerException) {
                    SearchRobot().longPressSearchBar()
                }
            }

            BrowserRobot().interact()
            return BrowserRobot.Transition()
        }
    }
}

fun searchScreen(interact: SearchRobot.() -> Unit): SearchRobot.Transition {
    SearchRobot().interact()
    return SearchRobot.Transition()
}

private val searchBar =
    mDevice.findObject(UiSelector().resourceId("$packageName:id/mozac_browser_toolbar_edit_url_view"))

private val toolbar =
    mDevice.findObject(UiSelector().resourceId("$packageName:id/mozac_browser_toolbar_url_view"))

private val searchSuggestionsTitle = mDevice.findObject(
    UiSelector()
        .resourceId("$packageName:id/enable_search_suggestions_title")
        .enabled(true),
)

private val searchSuggestionsButtonYes = mDevice.findObject(
    UiSelector()
        .resourceId("$packageName:id/enable_search_suggestions_button")
        .enabled(true),
)

private val searchSuggestionsButtonNo = mDevice.findObject(
    UiSelector()
        .resourceId("$packageName:id/disable_search_suggestions_button")
        .enabled(true),
)

private val suggestionsList = mDevice.findObject(
    UiSelector()
        .resourceId("$packageName:id/search_suggestions_view"),
)

private val clearSearchButton = mDevice.findObject(UiSelector().resourceId("$packageName:id/mozac_browser_toolbar_clear_view"))
