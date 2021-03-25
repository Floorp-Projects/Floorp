/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.activity.robots

import androidx.test.espresso.Espresso.onView
import androidx.test.espresso.action.ViewActions.click
import androidx.test.espresso.matcher.ViewMatchers.withId
import androidx.test.uiautomator.UiSelector
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.mozilla.focus.R
import org.mozilla.focus.helpers.TestHelper.appName
import org.mozilla.focus.helpers.TestHelper.mDevice
import org.mozilla.focus.helpers.TestHelper.waitingTime

class SearchRobot {

    fun verifySearchBarIsDisplayed() = assertTrue(searchBar.exists())

    fun typeInSearchBar(searchString: String) {
        assertTrue(searchBar.waitForExists(waitingTime))
        searchBar.clearTextField()
        searchBar.text = searchString
    }

    // Would you like to turn on search suggestions? Yes No
    // fresh install only
    fun allowEnableSearchSuggestions() {
        if (searchSuggestionsTitle.exists()) {
            searchSuggestionsButtonYes.waitForExists(waitingTime)
            searchSuggestionsButtonYes.click()
        }
    }

    fun verifySearchSuggestionsAreShown() {
        suggestionsList.waitForExists(waitingTime)
        assertTrue(suggestionsList.childCount >= 1)
    }

    fun verifySearchSuggestionsAreNotShown() {
        searchHint.waitForExists(waitingTime)
        assertFalse(suggestionsList.exists())
    }

    fun verifyHintForSearch(searchTerm: String) {
        searchHint.waitForExists(waitingTime)
        assertTrue(searchHint.text.equals(searchTerm))
    }

    fun verifySearchEditBarContainsTest(text: String) {
        assertTrue(searchBar.text.equals(text))
    }

    fun clearSearchBar() = clearSearchButton.perform(click())

    class Transition
}

fun searchScreen(interact: SearchRobot.() -> Unit): SearchRobot.Transition {
    SearchRobot().interact()
    return SearchRobot.Transition()
}

private val searchBar =
    mDevice.findObject(UiSelector().resourceId("$appName:id/urlView"))

private val searchHint = mDevice.findObject(
    UiSelector()
        .resourceId("$appName:id/searchView")
        .clickable(true)
)

private val searchSuggestionsTitle = mDevice.findObject(
    UiSelector()
        .resourceId("$appName:id/enable_search_suggestions_title")
        .enabled(true)
)

private val searchSuggestionsButtonYes = mDevice.findObject(
    UiSelector()
        .resourceId("$appName:id/enable_search_suggestions_button")
        .enabled(true)
)

private val suggestionsList = mDevice.findObject(
    UiSelector()
        .resourceId("$appName:id/suggestionList")
)

private val clearSearchButton = onView(withId(R.id.clearView))
