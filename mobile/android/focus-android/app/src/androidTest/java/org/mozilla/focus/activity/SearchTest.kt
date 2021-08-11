/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.focus.activity

import androidx.test.espresso.IdlingRegistry
import org.junit.After
import org.junit.Ignore
import org.junit.Rule
import org.junit.Test
import org.mozilla.focus.activity.robots.browserScreen
import org.mozilla.focus.activity.robots.homeScreen
import org.mozilla.focus.activity.robots.searchScreen
import org.mozilla.focus.helpers.MainActivityFirstrunTestRule
import org.mozilla.focus.helpers.TestHelper.exitToTop
import org.mozilla.focus.helpers.TestHelper.pressEnterKey
import org.mozilla.focus.helpers.TestHelper.webPageLoadwaitingTime
import org.mozilla.focus.idlingResources.RecyclerViewIdlingResource
import org.mozilla.focus.testAnnotations.SmokeTest

// This test checks the search engine can be changed and that search suggestions appear
class SearchTest {
    private val enginesList = listOf("DuckDuckGo", "Google", "Amazon.com", "Wikipedia")
    private var searchSuggestionsIdlingResources: RecyclerViewIdlingResource? = null

    @get: Rule
    var mActivityTestRule = MainActivityFirstrunTestRule(showFirstRun = false)

    @After
    fun tearDown() {
        if (searchSuggestionsIdlingResources != null) {
            IdlingRegistry.getInstance().unregister(searchSuggestionsIdlingResources!!)
        }
    }

    @Test
    fun changeSearchEngineTest() {

        for (searchEngine in enginesList) {
            // Open [settings menu] and select Search engine
            homeScreen {
            }.openMainMenu {
            }.openSettings {
            }.openSearchSettingsMenu {
                openSearchEngineSubMenu()
                selectSearchEngine(searchEngine)
                exitToTop()
            }

            searchScreen {
                typeInSearchBar("mozilla ")
                pressEnterKey()
            }

            browserScreen {
                verifyPageURL(searchEngine)
            }.clearBrowsingData { }
        }
    }

    @SmokeTest
    @Test
    @Ignore("Needs to be rewritten to work with Compose AwesomeBar")
    fun enableSearchSuggestionOnFirstRunTest() {
        val searchString = "mozilla "

        searchScreen {
            // type and check search suggestions are displayed
            typeInSearchBar(searchString)
            allowEnableSearchSuggestions()

            IdlingRegistry.getInstance().register(searchSuggestionsIdlingResources!!)

            verifyHintForSearch(searchString)
            verifySearchSuggestionsAreShown()
        }
    }

    @Test
    fun testBlankSearchDoesNothing() {
        searchScreen {
            // Search on blank spaces should not do anything
            typeInSearchBar(" ")
            pressEnterKey()
            searchScreen {
                verifySearchEditBarContainsText(" ")
            }
        }
    }

    @Test
    fun testSearchBarShowsSearchTermOnEdit() {
        val searchString = "mozilla focus"

        searchScreen {
            typeInSearchBar(searchString)
            pressEnterKey()
        }
        browserScreen {
            verifyPageContent(searchString)
            progressBar.waitUntilGone(webPageLoadwaitingTime)
        }.openSearchBar {
            // Tap URL bar, check it displays search term (instead of URL)
            verifySearchEditBarContainsText(searchString)
        }
    }

    @Test
    fun disableSearchSuggestionsTest() {
        val searchString = "mozilla "

        searchScreen {
            // Search on blank spaces should not do anything
            verifySearchBarIsDisplayed()
            typeInSearchBar(searchString)
            allowEnableSearchSuggestions()
            clearSearchBar()
        }
        homeScreen {
        }.openMainMenu {
        }.openSettings {
        }.openSearchSettingsMenu {
            clickSearchSuggestionsSwitch()
            exitToTop()
        }

        searchScreen {
            typeInSearchBar(searchString)
            verifySearchSuggestionsAreNotShown()
        }
    }
}
