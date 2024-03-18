/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ui

import androidx.compose.ui.test.junit4.AndroidComposeTestRule
import org.junit.Ignore
import org.junit.Rule
import org.junit.Test
import org.mozilla.fenix.customannotations.SmokeTest
import org.mozilla.fenix.helpers.HomeActivityTestRule
import org.mozilla.fenix.helpers.RetryTestRule
import org.mozilla.fenix.helpers.TestAssetHelper
import org.mozilla.fenix.helpers.TestSetup
import org.mozilla.fenix.ui.robots.homeScreen
import org.mozilla.fenix.ui.robots.navigationToolbar

/**
 *  Tests for verifying the presence of home screen and first-run homescreen elements
 *
 *  Note: For private browsing, navigation bar and tabs see separate test class
 *
 */

class ComposeHomeScreenTest : TestSetup() {
    @get:Rule(order = 0)
    val activityTestRule =
        AndroidComposeTestRule(
            HomeActivityTestRule.withDefaultSettingsOverrides(
                tabsTrayRewriteEnabled = true,
            ),
        ) { it.activity }

    @Rule(order = 1)
    @JvmField
    val retryTestRule = RetryTestRule(3)

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/235396
    @Ignore("Failing, see: https://bugzilla.mozilla.org/show_bug.cgi?id=1844580")
    @Test
    fun homeScreenItemsTest() {
        homeScreen {
            verifyHomeWordmark()
            verifyHomePrivateBrowsingButton()
            verifyExistingTopSitesTabs("Wikipedia")
            verifyExistingTopSitesTabs("Top Articles")
            verifyExistingTopSitesTabs("Google")
            verifyCollectionsHeader()
            verifyNoCollectionsText()
            scrollToPocketProvokingStories()
            verifyThoughtProvokingStories(true)
            verifyStoriesByTopicItems()
            verifyCustomizeHomepageButton(true)
            verifyNavigationToolbar()
            verifyHomeMenuButton()
            verifyTabButton()
            verifyTabCounter("0")
        }
    }

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/244199
    @Test
    fun privateBrowsingHomeScreenItemsTest() {
        homeScreen { }.togglePrivateBrowsingMode()

        homeScreen {
            verifyPrivateBrowsingHomeScreenItems()
        }.openCommonMythsLink {
            verifyUrl("common-myths-about-private-browsing")
        }
    }

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/1364362
    @SmokeTest
    @Test
    fun verifyJumpBackInSectionTest() {
        activityTestRule.activityRule.applySettingsExceptions {
            it.isRecentlyVisitedFeatureEnabled = false
            it.isPocketEnabled = false
        }

        val firstWebPage = TestAssetHelper.getGenericAsset(mockWebServer, 4)
        val secondWebPage = TestAssetHelper.getGenericAsset(mockWebServer, 1)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(firstWebPage.url) {
            verifyPageContent(firstWebPage.content)
            verifyUrl(firstWebPage.url.toString())
        }.goToHomescreen {
            verifyJumpBackInSectionIsDisplayed()
            verifyJumpBackInItemTitle(activityTestRule, firstWebPage.title)
            verifyJumpBackInItemWithUrl(activityTestRule, firstWebPage.url.toString())
            verifyJumpBackInShowAllButton()
        }.clickJumpBackInShowAllButton(activityTestRule) {
            verifyExistingOpenTabs(firstWebPage.title)
        }.closeTabDrawer {
        }

        navigationToolbar {
        }.enterURLAndEnterToBrowser(secondWebPage.url) {
            verifyPageContent(secondWebPage.content)
            verifyUrl(secondWebPage.url.toString())
        }.goToHomescreen {
            verifyJumpBackInSectionIsDisplayed()
            verifyJumpBackInItemTitle(activityTestRule, secondWebPage.title)
            verifyJumpBackInItemWithUrl(activityTestRule, secondWebPage.url.toString())
        }.openComposeTabDrawer(activityTestRule) {
            closeTabWithTitle(secondWebPage.title)
        }.closeTabDrawer {
        }

        homeScreen {
            verifyJumpBackInSectionIsDisplayed()
            verifyJumpBackInItemTitle(activityTestRule, firstWebPage.title)
            verifyJumpBackInItemWithUrl(activityTestRule, firstWebPage.url.toString())
        }.openComposeTabDrawer(activityTestRule) {
            closeTab()
        }

        homeScreen {
            verifyJumpBackInSectionIsNotDisplayed()
        }
    }

    // TestRail link: https://testrail.stage.mozaws.net/index.php?/cases/view/1569839
    @Test
    fun verifyCustomizeHomepageButtonTest() {
        val defaultWebPage = TestAssetHelper.getGenericAsset(mockWebServer, 1)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(defaultWebPage.url) {
        }.goToHomescreen {
        }.openCustomizeHomepage {
            clickShortcutsButton()
            clickJumpBackInButton()
            clickRecentBookmarksButton()
            clickRecentSearchesButton()
            clickPocketButton()
        }.goBackToHomeScreen {
            verifyCustomizeHomepageButton(false)
        }.openThreeDotMenu {
        }.openCustomizeHome {
            clickShortcutsButton()
        }.goBackToHomeScreen {
            verifyCustomizeHomepageButton(true)
        }
    }
}
