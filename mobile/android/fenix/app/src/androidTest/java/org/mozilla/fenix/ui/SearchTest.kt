/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ui

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
    val activityTestRule = HomeActivityTestRule()

    @Test
    fun searchScreenItemsTest() {
        homeScreen { }.dismissOnboarding()

        homeScreen {
        }.openSearch {
            verifySearchView()
            verifyBrowserToolbar()
            verifyScanButton()
            verifyShortcutsButton()
        }
    }

    @Ignore("This test cannot run on virtual devices due to camera permissions being required")
    @Test
    fun scanButtonTest() {
        homeScreen { }.dismissOnboarding()

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
        homeScreen { }.dismissOnboarding()

        homeScreen {
        }.openSearch {
            verifySearchWithText()
            clickDuckDuckGoEngineButton()
            typeSearch()
            verifyDuckDuckGoResults()
            clickDuckDuckGoResult()
            verifyDuckDuckGoURL()
        }
    }

    @Test
    fun shortcutSearchEngineSettingsTest() {
        homeScreen { }.dismissOnboarding()

        homeScreen {
        }.openSearch {
            scrollToSearchEngineSettings()
            clickSearchEngineSettings()
            verifySearchEngineSettings()
        }
    }
}
