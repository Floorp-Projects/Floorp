/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ui

import org.junit.Rule
import org.junit.Test
import org.mozilla.fenix.helpers.HomeActivityTestRule
import org.mozilla.fenix.ui.robots.homeScreen

/**
 *  Tests for verifying the main three dot menu options
 *
 *  Including:
 * - Verify all menu items present
 * - Open library button opens library menu
 * - Open settings button opens settings menu
 * - Open Help button opens support page in browser
 *
 */

class ThreeDotMenuTest {
    /* ktlint-disable no-blank-line-before-rbrace */ // This imposes unreadable grouping.
    @get:Rule
    val activityTestRule = HomeActivityTestRule()
    @Test
    fun threeDotMenuItemsTest() {
        homeScreen {
        }.openThreeDotMenu {
            verifySettingsButton()
            verifyLibraryButton()
            verifyHelpButton()
        }.openSettings {
            verifySettingsView()
        }.goBack {
        }.openThreeDotMenu {
        }.openLibrary {
            verifyLibraryView()
        }.goBack {
        }.openThreeDotMenu {
        }.openHelp {
            verifyHelpUrl()
        }
    }
}
