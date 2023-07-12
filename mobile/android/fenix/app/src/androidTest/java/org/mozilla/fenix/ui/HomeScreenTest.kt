/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ui

import androidx.compose.ui.test.junit4.AndroidComposeTestRule
import androidx.test.platform.app.InstrumentationRegistry
import androidx.test.uiautomator.UiDevice
import okhttp3.mockwebserver.MockWebServer
import org.junit.After
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.mozilla.fenix.helpers.AndroidAssetDispatcher
import org.mozilla.fenix.helpers.Constants.POCKET_RECOMMENDED_STORIES_UTM_PARAM
import org.mozilla.fenix.helpers.HomeActivityTestRule
import org.mozilla.fenix.helpers.RetryTestRule
import org.mozilla.fenix.helpers.TestAssetHelper
import org.mozilla.fenix.ui.robots.homeScreen
import org.mozilla.fenix.ui.robots.navigationToolbar

/**
 *  Tests for verifying the presence of home screen and first-run homescreen elements
 *
 *  Note: For private browsing, navigation bar and tabs see separate test class
 *
 */

class HomeScreenTest {
    private lateinit var mDevice: UiDevice
    private lateinit var mockWebServer: MockWebServer
    private lateinit var firstPocketStoryPublisher: String

    @get:Rule(order = 0)
    val activityTestRule =
        AndroidComposeTestRule(HomeActivityTestRule.withDefaultSettingsOverrides()) { it.activity }

    @Rule(order = 1)
    @JvmField
    val retryTestRule = RetryTestRule(3)

    @Before
    fun setUp() {
        mDevice = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation())

        mockWebServer = MockWebServer().apply {
            dispatcher = AndroidAssetDispatcher()
            start()
        }
    }

    @After
    fun tearDown() {
        mockWebServer.shutdown()
    }

    @Test
    fun homeScreenItemsTest() {
        homeScreen {}.dismissOnboarding()
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

    @Test
    fun privateModeScreenItemsTest() {
        homeScreen { }.dismissOnboarding()
        homeScreen { }.togglePrivateBrowsingMode()

        homeScreen {
            verifyPrivateBrowsingHomeScreen()
        }.openCommonMythsLink {
            verifyUrl("common-myths-about-private-browsing")
        }
    }

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
        }.clickJumpBackInShowAllButton {
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
        }.openTabDrawer {
            closeTabWithTitle(secondWebPage.title)
        }.closeTabDrawer {
        }

        homeScreen {
            verifyJumpBackInSectionIsDisplayed()
            verifyJumpBackInItemTitle(activityTestRule, firstWebPage.title)
            verifyJumpBackInItemWithUrl(activityTestRule, firstWebPage.url.toString())
        }.openTabDrawer {
            closeTab()
        }

        homeScreen {
            verifyJumpBackInSectionIsNotDisplayed()
        }
    }

    @Test
    fun verifyPocketHomepageStoriesTest() {
        activityTestRule.activityRule.applySettingsExceptions {
            it.isRecentTabsFeatureEnabled = false
            it.isRecentlyVisitedFeatureEnabled = false
        }

        homeScreen {
        }.dismissOnboarding()

        homeScreen {
            verifyThoughtProvokingStories(true)
            scrollToPocketProvokingStories()
            verifyPocketRecommendedStoriesItems()
            // Sponsored Pocket stories are only advertised for a limited time.
            // See also known issue https://bugzilla.mozilla.org/show_bug.cgi?id=1828629
            // verifyPocketSponsoredStoriesItems(2, 8)
            verifyDiscoverMoreStoriesButton()
            verifyStoriesByTopic(true)
            verifyPoweredByPocket()
        }.openThreeDotMenu {
        }.openCustomizeHome {
            clickPocketButton()
        }.goBackToHomeScreen {
            verifyThoughtProvokingStories(false)
            verifyStoriesByTopic(false)
        }
    }

    @Test
    fun openPocketStoryItemTest() {
        activityTestRule.activityRule.applySettingsExceptions {
            it.isRecentTabsFeatureEnabled = false
            it.isRecentlyVisitedFeatureEnabled = false
        }

        homeScreen {
        }.dismissOnboarding()

        homeScreen {
            verifyThoughtProvokingStories(true)
            scrollToPocketProvokingStories()
            firstPocketStoryPublisher = getProvokingStoryPublisher(1)
        }.clickPocketStoryItem(firstPocketStoryPublisher, 1) {
            verifyUrl(POCKET_RECOMMENDED_STORIES_UTM_PARAM)
        }
    }

    @Test
    fun openPocketDiscoverMoreTest() {
        activityTestRule.activityRule.applySettingsExceptions {
            it.isRecentTabsFeatureEnabled = false
            it.isRecentlyVisitedFeatureEnabled = false
        }

        homeScreen {
        }.dismissOnboarding()

        homeScreen {
            scrollToPocketProvokingStories()
            verifyDiscoverMoreStoriesButton()
        }.clickPocketDiscoverMoreButton {
            verifyUrl("getpocket.com/explore")
        }
    }

    @Test
    fun selectStoriesByTopicItemTest() {
        activityTestRule.activityRule.applySettingsExceptions {
            it.isRecentTabsFeatureEnabled = false
            it.isRecentlyVisitedFeatureEnabled = false
        }

        homeScreen {
        }.dismissOnboarding()

        homeScreen {
            verifyStoriesByTopicItemState(activityTestRule, false, 1)
            clickStoriesByTopicItem(activityTestRule, 1)
            verifyStoriesByTopicItemState(activityTestRule, true, 1)
        }
    }

    @Test
    fun verifyPocketLearnMoreLinkTest() {
        activityTestRule.activityRule.applySettingsExceptions {
            it.isRecentTabsFeatureEnabled = false
            it.isRecentlyVisitedFeatureEnabled = false
        }

        homeScreen {
        }.dismissOnboarding()

        homeScreen {
            verifyPoweredByPocket()
        }.clickPocketLearnMoreLink(activityTestRule) {
            verifyUrl("mozilla.org/en-US/firefox/pocket")
        }
    }

    @Test
    fun verifyCustomizeHomepageTest() {
        val defaultWebPage = TestAssetHelper.getGenericAsset(mockWebServer, 1)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(defaultWebPage.url) {
        }.goToHomescreen {
        }.openCustomizeHomepage {
            clickJumpBackInButton()
            clickRecentBookmarksButton()
            clickRecentSearchesButton()
            clickPocketButton()
        }.goBackToHomeScreen {
            verifyCustomizeHomepageButton(false)
        }.openThreeDotMenu {
        }.openCustomizeHome {
            clickJumpBackInButton()
        }.goBackToHomeScreen {
            verifyCustomizeHomepageButton(true)
        }
    }
}
