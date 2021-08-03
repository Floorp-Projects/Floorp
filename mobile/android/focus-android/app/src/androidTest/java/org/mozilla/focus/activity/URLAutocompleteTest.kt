/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.focus.activity

import androidx.test.internal.runner.junit4.AndroidJUnit4ClassRunner
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.focus.activity.robots.homeScreen
import org.mozilla.focus.activity.robots.searchScreen
import org.mozilla.focus.helpers.MainActivityFirstrunTestRule
import org.mozilla.focus.helpers.TestHelper.exitToTop
import org.mozilla.focus.helpers.TestHelper.mDevice
import org.mozilla.focus.testAnnotations.SmokeTest

// Tests url autocompletion and adding custom autocomplete urls
@RunWith(AndroidJUnit4ClassRunner::class)
class URLAutocompleteTest {
    private val searchTerm = "mozilla"
    private val autocompleteSuggestion = "mozilla.org"
    private val pageUrl = "https://www.mozilla.org/"
    private val customURL = "680news.com"

    @get: Rule
    var mActivityTestRule = MainActivityFirstrunTestRule(showFirstRun = false)

    @SmokeTest
    @Test
    // Test the url autocomplete feature with default settings
    fun defaultAutoCompleteURLTest() {
        searchScreen {
            // type a partial url, and check it autocompletes
            typeInSearchBar(searchTerm)
            verifySearchEditBarContainsText(autocompleteSuggestion)
            clearSearchBar()
            // type a full url, and check it does not autocomplete
            typeInSearchBar(pageUrl)
            verifySearchEditBarContainsText(pageUrl)
        }
    }

    @SmokeTest
    @Test
    fun disableTopSitesAutocompleteTest() {
        homeScreen {
        }.openMainMenu {
        }.openSettings {
        }.openSearchSettingsMenu {
            openUrlAutocompleteSubMenu()
            toggleTopSitesAutocomplete()
            exitToTop()
        }

        searchScreen {
            typeInSearchBar(searchTerm)
            verifySearchEditBarContainsText(searchTerm)
        }
    }

    @Test
    // Add custom Url, verify it works, then remove it and check it is no longer autocompleted
    fun customUrlAutoCompletionTest() {
        homeScreen {
        }.openMainMenu {
        }.openSettings {
        }.openSearchSettingsMenu {
            // Add custom autocomplete url
            openUrlAutocompleteSubMenu()
            openManageSitesSubMenu()
            openAddCustomUrlDialog()
            enterCustomUrl(customURL)
            saveCustomUrl()
            verifySavedCustomURL(customURL)
            exitToTop()
        }
        // verify the custom url auto-completes
        searchScreen {
            typeInSearchBar(customURL.substring(0, 1))
            verifySearchEditBarContainsText(customURL)
            clearSearchBar()
        }

        homeScreen {
        }.openMainMenu {
        }.openSettings {
        }.openSearchSettingsMenu {
            // remove custom Url
            openUrlAutocompleteSubMenu()
            openManageSitesSubMenu()
            removeCustomUrl()
            exitToTop()
        }
        // verify it is no longer auto-completed
        searchScreen {
            typeInSearchBar(customURL.substring(0, 3))
            verifySearchEditBarContainsText(customURL.substring(0, 3))
            clearSearchBar()
        }
    }

    @Test
    // Add custom autocompletion site, then disable autocomplete
    fun disableAutocompleteForCustomSiteTest() {
        homeScreen {
        }.openMainMenu {
        }.openSettings {
        }.openSearchSettingsMenu {
            openUrlAutocompleteSubMenu()
            openManageSitesSubMenu()
            openAddCustomUrlDialog()
            enterCustomUrl(customURL)
            saveCustomUrl()
            verifySavedCustomURL(customURL)
            mDevice.pressBack()
            toggleCustomAutocomplete()
            exitToTop()
        }

        searchScreen {
            typeInSearchBar(customURL.substring(0, 3))
            verifySearchEditBarContainsText(customURL.substring(0, 3))
            clearSearchBar()
        }
    }

    @Test
    // Verifies the custom Url can't be added twice
    fun duplicateCustomUrlNotAllowedTest() {
        homeScreen {
        }.openMainMenu {
        }.openSettings {
        }.openSearchSettingsMenu {
            openUrlAutocompleteSubMenu()
            openManageSitesSubMenu()
            openAddCustomUrlDialog()
            enterCustomUrl(customURL)
            saveCustomUrl()
            verifySavedCustomURL(customURL)
            openAddCustomUrlDialog()
            enterCustomUrl(customURL)
            saveCustomUrl()
            verifyCustomUrlDialogNotClosed()
            exitToTop()
        }
    }
}
