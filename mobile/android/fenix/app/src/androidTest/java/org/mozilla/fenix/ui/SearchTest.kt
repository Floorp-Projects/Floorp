/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ui

import androidx.compose.ui.test.junit4.AndroidComposeTestRule
import org.junit.Ignore
import org.junit.Rule
import org.junit.Test
import org.mozilla.fenix.helpers.HomeActivityTestRule
import org.mozilla.fenix.ui.robots.homeScreen

/**
 *  Tests for verifying the search fragment
 *
 *  Including:
 * - Verify the toolbar, awesomebar, and shortcut bar are displayed
 * - Select shortcut button
 * - Select scan button
 *
 */

class SearchTest {
    /* ktlint-disable no-blank-line-before-rbrace */ // This imposes unreadable grouping.
    @get:Rule
    val activityTestRule = AndroidComposeTestRule(
        HomeActivityTestRule(),
        { it.activity }
    )

    @Test
    fun searchScreenItemsTest() {
        homeScreen {
        }.openSearch {
            verifySearchView()
            verifyBrowserToolbar()
            verifyScanButton()
            verifySearchEngineButton()
        }
    }

    @Ignore("This test cannot run on virtual devices due to camera permissions being required")
    @Test
    fun scanButtonTest() {
        homeScreen {
        }.openSearch {
            clickScanButton()
            clickDenyPermission()
            clickScanButton()
            clickAllowPermission()
        }
    }

    @Test
    fun shortcutButtonTest() {
        homeScreen {
        }.openThreeDotMenu {
        }.openSettings {
        }.openSearchSubMenu {
            enableShowSearchShortcuts()
        }.goBack {
        }.goBack {
        }.openSearch {
//            verifySearchWithText()
            clickSearchEngineButton(activityTestRule, "DuckDuckGo")
            typeSearch("mozilla")
            verifySearchEngineResults(activityTestRule, "DuckDuckGo", 4)
            clickSearchEngineResult(activityTestRule, "DuckDuckGo")
            verifySearchEngineURL("DuckDuckGo")
        }
    }

    @Test
    fun shortcutSearchEngineSettingsTest() {
        homeScreen {
        }.openThreeDotMenu {
        }.openSettings {
        }.openSearchSubMenu {
            enableShowSearchShortcuts()
        }.goBack {
        }.goBack {
        }.openSearch {
            scrollToSearchEngineSettings(activityTestRule)
            clickSearchEngineSettings(activityTestRule)
            verifySearchSettings()
        }
    }

    @Test
    fun clearSearchTest() {
        homeScreen {
        }.openSearch {
            typeSearch("test")
            clickClearButton()
            verifySearchBarEmpty()
        }
    }
}
