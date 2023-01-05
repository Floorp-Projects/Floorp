/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ui

import androidx.compose.ui.test.junit4.AndroidComposeTestRule
import androidx.test.platform.app.InstrumentationRegistry
import androidx.test.uiautomator.By
import androidx.test.uiautomator.UiDevice
import androidx.test.uiautomator.Until
import okhttp3.mockwebserver.MockWebServer
import org.junit.After
import org.junit.Before
import org.junit.Ignore
import org.junit.Rule
import org.junit.Test
import org.mozilla.fenix.helpers.AndroidAssetDispatcher
import org.mozilla.fenix.helpers.Constants.POCKET_RECOMMENDED_STORIES_UTM_PARAM
import org.mozilla.fenix.helpers.HomeActivityTestRule
import org.mozilla.fenix.helpers.RetryTestRule
import org.mozilla.fenix.helpers.TestAssetHelper
import org.mozilla.fenix.helpers.TestAssetHelper.waitingTime
import org.mozilla.fenix.helpers.ext.waitNotNull
import org.mozilla.fenix.ui.robots.homeScreen
import org.mozilla.fenix.ui.robots.navigationToolbar

/**
 *  Tests for verifying the presence of home screen and first-run homescreen elements
 *
 *  Note: For private browsing, navigation bar and tabs see separate test class
 *
 */

class HomeScreenTest {
    /* ktlint-disable no-blank-line-before-rbrace */ // This imposes unreadable grouping.

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
        homeScreen { }.dismissOnboarding()

        homeScreen {
            verifyHomeWordmark()
            verifyHomePrivateBrowsingButton()
            verifyExistingTopSitesTabs("Wikipedia")
            verifyExistingTopSitesTabs("Top Articles")
            verifyExistingTopSitesTabs("Google")
            verifyCollectionsHeader()
            verifyNoCollectionsText()
            scrollToPocketProvokingStories()
            swipePocketProvokingStories()
            verifyPocketRecommendedStoriesItems(activityTestRule, 1, 3, 4, 5, 6, 7)
            verifyPocketSponsoredStoriesItems(activityTestRule, 2, 8)
            verifyDiscoverMoreStoriesButton(activityTestRule, 9)
            verifyStoriesByTopicItems()
            verifyPoweredByPocket(activityTestRule)
            verifyCustomizeHomepageButton(true)
            verifyNavigationToolbar()
            verifyDefaultSearchEngine("Google")
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
            verifyHomeScreen()
            verifyNavigationToolbar()
            verifyHomePrivateBrowsingButton()
            verifyHomeMenuButton()
            verifyHomeWordmark()
            verifyTabButton()
            verifyPrivateSessionMessage()
            verifyNavigationToolbar()
            verifyHomeComponent()
        }.openCommonMythsLink {
            verifyUrl("common-myths-about-private-browsing")
            mDevice.pressBack()
        }

        homeScreen {
            // To deal with the race condition where multiple "add tab" buttons are present,
            // we need to wait until previous HomeFragment View objects are gone.
            mDevice.waitNotNull(Until.gone(By.text(privateSessionMessage)), waitingTime)
            verifyHomeScreen()
            verifyNavigationToolbar()
            verifyHomePrivateBrowsingButton()
            verifyHomeMenuButton()
            verifyHomeWordmark()
            verifyTabButton()
            verifyPrivateSessionMessage()
            verifyNavigationToolbar()
            verifyHomeComponent()
        }
    }

    @Test
    fun verifyJumpBackInSectionTest() {
        val firstWebPage = TestAssetHelper.getGenericAsset(mockWebServer, 4)
        val secondWebPage = TestAssetHelper.getGenericAsset(mockWebServer, 1)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(firstWebPage.url) {
        }.goToHomescreen {
            verifyJumpBackInSectionIsDisplayed()
            verifyJumpBackInItemTitle(firstWebPage.title)
            verifyJumpBackInItemWithUrl(firstWebPage.url.toString())
            verifyJumpBackInShowAllButton()
        }.clickJumpBackInShowAllButton {
            verifyExistingOpenTabs(firstWebPage.title)
        }.closeTabDrawer() {
        }
        homeScreen {
        }.clickJumpBackInItemWithTitle(firstWebPage.title) {
            verifyUrl(firstWebPage.url.toString())
            clickLinkMatchingText("Link 1")
        }.goToHomescreen {
            verifyJumpBackInSectionIsDisplayed()
            verifyJumpBackInItemTitle(secondWebPage.title)
            verifyJumpBackInItemWithUrl(secondWebPage.url.toString())
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
            swipePocketProvokingStories()
            verifyPocketRecommendedStoriesItems(activityTestRule, 1, 3, 4, 5, 6, 7)
            verifyPocketSponsoredStoriesItems(activityTestRule, 2, 8)
            verifyDiscoverMoreStoriesButton(activityTestRule, 9)
            verifyStoriesByTopic(true)
        }.openThreeDotMenu {
        }.openCustomizeHome {
            clickPocketButton()
        }.goBack {
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

    @Ignore("Failed, see: https://github.com/mozilla-mobile/fenix/issues/28098")
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
            swipePocketProvokingStories()
            verifyDiscoverMoreStoriesButton(activityTestRule, 9)
        }.clickPocketDiscoverMoreButton(activityTestRule, 9) {
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
            verifyPoweredByPocket(activityTestRule)
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
        }.goBack {
            verifyCustomizeHomepageButton(false)
        }.openThreeDotMenu {
        }.openCustomizeHome {
            clickJumpBackInButton()
        }.goBack {
            verifyCustomizeHomepageButton(true)
        }
    }
}
