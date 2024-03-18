/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ui

import androidx.compose.ui.test.junit4.AndroidComposeTestRule
import org.junit.Rule
import org.junit.Test
import org.mozilla.fenix.customannotations.SmokeTest
import org.mozilla.fenix.helpers.AppAndSystemHelper.runWithSystemLocaleChanged
import org.mozilla.fenix.helpers.HomeActivityTestRule
import org.mozilla.fenix.helpers.TestAssetHelper
import org.mozilla.fenix.helpers.TestSetup
import org.mozilla.fenix.ui.robots.navigationToolbar
import java.util.Locale

/**
 *  Tests for verifying basic functionality of browser navigation and page related interactions
 *
 *  Including:
 *  - Visiting a URL
 *  - Back and Forward navigation
 *  - Refresh
 *  - Find in page
 */

class ComposeNavigationToolbarTest : TestSetup() {
    @get:Rule
    val composeTestRule =
        AndroidComposeTestRule(
            HomeActivityTestRule.withDefaultSettingsOverrides(
                tabsTrayRewriteEnabled = true,
            ),
        ) { it.activity }

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/987326
    // Swipes the nav bar left/right to switch between tabs
    @SmokeTest
    @Test
    fun swipeToSwitchTabTest() {
        val firstWebPage = TestAssetHelper.getGenericAsset(mockWebServer, 1)
        val secondWebPage = TestAssetHelper.getGenericAsset(mockWebServer, 2)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(firstWebPage.url) {
        }.openComposeTabDrawer(composeTestRule) {
        }.openNewTab {
        }.submitQuery(secondWebPage.url.toString()) {
            swipeNavBarRight(secondWebPage.url.toString())
            verifyUrl(firstWebPage.url.toString())
            swipeNavBarLeft(firstWebPage.url.toString())
            verifyUrl(secondWebPage.url.toString())
        }
    }

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/987327
    // Because it requires changing system prefs, this test will run only on Debug builds
    @Test
    fun swipeToSwitchTabInRTLTest() {
        val firstWebPage = TestAssetHelper.getGenericAsset(mockWebServer, 1)
        val secondWebPage = TestAssetHelper.getGenericAsset(mockWebServer, 2)
        val arabicLocale = Locale("ar", "AR")

        runWithSystemLocaleChanged(arabicLocale, composeTestRule.activityRule) {
            navigationToolbar {
            }.enterURLAndEnterToBrowser(firstWebPage.url) {
            }.openComposeTabDrawer(composeTestRule) {
            }.openNewTab {
            }.submitQuery(secondWebPage.url.toString()) {
                swipeNavBarLeft(secondWebPage.url.toString())
                verifyUrl(firstWebPage.url.toString())
                swipeNavBarRight(firstWebPage.url.toString())
                verifyUrl(secondWebPage.url.toString())
            }
        }
    }
}
