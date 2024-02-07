/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

@file:Suppress("TooManyFunctions")

package org.mozilla.fenix.ui.robots

import android.util.Log
import android.view.View
import android.widget.EditText
import android.widget.TextView
import androidx.compose.ui.platform.ComposeView
import androidx.compose.ui.test.assert
import androidx.compose.ui.test.assertIsDisplayed
import androidx.compose.ui.test.assertIsNotSelected
import androidx.compose.ui.test.assertIsSelected
import androidx.compose.ui.test.hasContentDescription
import androidx.compose.ui.test.hasText
import androidx.compose.ui.test.junit4.ComposeTestRule
import androidx.compose.ui.test.onChildAt
import androidx.compose.ui.test.onNodeWithTag
import androidx.compose.ui.test.onNodeWithText
import androidx.compose.ui.test.performClick
import androidx.recyclerview.widget.RecyclerView
import androidx.test.espresso.Espresso.onView
import androidx.test.espresso.action.ViewActions
import androidx.test.espresso.action.ViewActions.click
import androidx.test.espresso.action.ViewActions.longClick
import androidx.test.espresso.assertion.PositionAssertions.isCompletelyAbove
import androidx.test.espresso.assertion.PositionAssertions.isPartiallyBelow
import androidx.test.espresso.assertion.ViewAssertions.matches
import androidx.test.espresso.contrib.RecyclerViewActions.actionOnItem
import androidx.test.espresso.matcher.RootMatchers
import androidx.test.espresso.matcher.ViewMatchers
import androidx.test.espresso.matcher.ViewMatchers.Visibility
import androidx.test.espresso.matcher.ViewMatchers.hasDescendant
import androidx.test.espresso.matcher.ViewMatchers.isDisplayed
import androidx.test.espresso.matcher.ViewMatchers.withEffectiveVisibility
import androidx.test.espresso.matcher.ViewMatchers.withId
import androidx.test.espresso.matcher.ViewMatchers.withText
import androidx.test.uiautomator.By
import androidx.test.uiautomator.UiScrollable
import androidx.test.uiautomator.UiSelector
import androidx.test.uiautomator.Until
import androidx.test.uiautomator.Until.findObject
import org.hamcrest.CoreMatchers.allOf
import org.hamcrest.CoreMatchers.containsString
import org.hamcrest.CoreMatchers.instanceOf
import org.hamcrest.Matchers
import org.junit.Assert.assertTrue
import org.mozilla.fenix.R
import org.mozilla.fenix.helpers.Constants.LISTS_MAXSWIPES
import org.mozilla.fenix.helpers.Constants.TAG
import org.mozilla.fenix.helpers.DataGenerationHelper.getStringResource
import org.mozilla.fenix.helpers.HomeActivityComposeTestRule
import org.mozilla.fenix.helpers.MatcherHelper.assertUIObjectExists
import org.mozilla.fenix.helpers.MatcherHelper.assertUIObjectIsGone
import org.mozilla.fenix.helpers.MatcherHelper.itemContainingText
import org.mozilla.fenix.helpers.MatcherHelper.itemWithDescription
import org.mozilla.fenix.helpers.MatcherHelper.itemWithIndex
import org.mozilla.fenix.helpers.MatcherHelper.itemWithResId
import org.mozilla.fenix.helpers.MatcherHelper.itemWithResIdAndDescription
import org.mozilla.fenix.helpers.MatcherHelper.itemWithResIdAndIndex
import org.mozilla.fenix.helpers.MatcherHelper.itemWithResIdAndText
import org.mozilla.fenix.helpers.MatcherHelper.itemWithResIdContainingText
import org.mozilla.fenix.helpers.MatcherHelper.itemWithText
import org.mozilla.fenix.helpers.TestAssetHelper.waitingTime
import org.mozilla.fenix.helpers.TestAssetHelper.waitingTimeShort
import org.mozilla.fenix.helpers.TestHelper.appName
import org.mozilla.fenix.helpers.TestHelper.mDevice
import org.mozilla.fenix.helpers.TestHelper.packageName
import org.mozilla.fenix.helpers.TestHelper.scrollToElementByText
import org.mozilla.fenix.helpers.click
import org.mozilla.fenix.helpers.ext.waitNotNull
import org.mozilla.fenix.tabstray.TabsTrayTestTag

/**
 * Implementation of Robot Pattern for the home screen menu.
 */
class HomeScreenRobot {
    fun verifyNavigationToolbar() = assertUIObjectExists(navigationToolbar())

    fun verifyHomeScreen() = assertUIObjectExists(homeScreen())

    fun verifyPrivateBrowsingHomeScreenItems() {
        verifyHomeScreenAppBarItems()
        assertUIObjectExists(
            itemContainingText(
                "$appName clears your search and browsing history from private tabs when you close them" +
                    " or quit the app. While this doesn’t make you anonymous to websites or your internet" +
                    " service provider, it makes it easier to keep what you do online private from anyone" +
                    " else who uses this device.",
            ),
        )
        verifyCommonMythsLink()
    }

    fun verifyHomeScreenAppBarItems() =
        assertUIObjectExists(homeScreen(), privateBrowsingButton(), homepageWordmark())

    fun verifyHomePrivateBrowsingButton() = assertUIObjectExists(privateBrowsingButton())
    fun verifyHomeMenuButton() = assertUIObjectExists(menuButton())
    fun verifyTabButton() {
        Log.i(TAG, "verifyTabButton: Trying to verify tab counter button is visible")
        onView(allOf(withId(R.id.tab_button), isDisplayed())).check(
            matches(
                withEffectiveVisibility(
                    Visibility.VISIBLE,
                ),
            ),
        )
        Log.i(TAG, "verifyTabButton: Verified tab counter button is visible")
    }
    fun verifyCollectionsHeader() {
        Log.i(TAG, "verifyCollectionsHeader: Trying to verify collections header is visible")
        onView(allOf(withText("Collections"))).check(matches(withEffectiveVisibility(Visibility.VISIBLE)))
        Log.i(TAG, "verifyCollectionsHeader: Verified collections header is visible")
    }
    fun verifyNoCollectionsText() {
        Log.i(TAG, "verifyNoCollectionsText: Trying to verify empty collections placeholder text is displayed")
        onView(
            withText(
                containsString(
                    "Collect the things that matter to you.\n" +
                        "Group together similar searches, sites, and tabs for quick access later.",
                ),
            ),
        ).check(matches(isDisplayed()))
        Log.i(TAG, "verifyNoCollectionsText: Verified empty collections placeholder text is displayed")
    }

    fun verifyHomeWordmark() {
        Log.i(TAG, "verifyHomeWordmark: Trying to scroll 3x to the beginning of the home screen")
        homeScreenList().scrollToBeginning(3)
        Log.i(TAG, "verifyHomeWordmark: Scrolled 3x to the beginning of the home screen")
        assertUIObjectExists(homepageWordmark())
    }
    fun verifyHomeComponent() {
        Log.i(TAG, "verifyHomeComponent: Trying to verify home screen view is visible")
        onView(ViewMatchers.withResourceName("sessionControlRecyclerView"))
            .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))
        Log.i(TAG, "verifyHomeComponent: Verified home screen view is visible")
    }

    fun verifyTabCounter(numberOfOpenTabs: String) =
        onView(
            allOf(
                withId(R.id.counter_text),
                withText(numberOfOpenTabs),
            ),
        ).check(matches(isDisplayed()))

    fun verifyWallpaperImageApplied(isEnabled: Boolean) =
        assertUIObjectExists(itemWithResId("$packageName:id/wallpaperImageView"), exists = isEnabled)

    // Upgrading users onboarding dialog
    fun verifyUpgradingUserOnboardingFirstScreen(testRule: ComposeTestRule) {
        testRule.also {
            Log.i(TAG, "verifyUpgradingUserOnboardingFirstScreen: Trying to verify that the upgrading user first onboarding screen title is displayed")
            it.onNodeWithText(getStringResource(R.string.onboarding_home_welcome_title_2))
                .assertIsDisplayed()
            Log.i(TAG, "verifyUpgradingUserOnboardingFirstScreen: Verified that the upgrading user first onboarding screen title is displayed")
            Log.i(TAG, "verifyUpgradingUserOnboardingFirstScreen: Trying to verify that the upgrading user first onboarding screen description is displayed")
            it.onNodeWithText(getStringResource(R.string.onboarding_home_welcome_description))
                .assertIsDisplayed()
            Log.i(TAG, "verifyUpgradingUserOnboardingFirstScreen: Verified that the upgrading user first onboarding screen description is displayed")
            Log.i(TAG, "verifyUpgradingUserOnboardingFirstScreen: Trying to verify that the upgrading user first onboarding \"Get started\" button is displayed")
            it.onNodeWithText(getStringResource(R.string.onboarding_home_get_started_button))
                .assertIsDisplayed()
            Log.i(TAG, "verifyUpgradingUserOnboardingFirstScreen: Verified that the upgrading user first onboarding  \"Get started\" button is displayed")
        }
    }

    fun verifyFirstOnboardingCard(composeTestRule: ComposeTestRule) {
        composeTestRule.also {
            Log.i(TAG, "verifyFirstOnboardingCard: Trying to verify that the first onboarding screen title exists")
            it.onNodeWithText(
                getStringResource(R.string.juno_onboarding_default_browser_title_nimbus_2),
            ).assertExists()
            Log.i(TAG, "verifyFirstOnboardingCard: Verified that the first onboarding screen title exists")
            Log.i(TAG, "verifyFirstOnboardingCard: Trying to verify that the first onboarding screen description exists")
            it.onNodeWithText(
                getStringResource(R.string.juno_onboarding_default_browser_description_nimbus_3),
            ).assertExists()
            Log.i(TAG, "verifyFirstOnboardingCard: Verified that the first onboarding screen description exists")
            Log.i(TAG, "verifyFirstOnboardingCard: Trying to verify that the first onboarding \"Set as default browser\" button exists")
            it.onNodeWithText(
                getStringResource(R.string.juno_onboarding_default_browser_positive_button),
            ).assertExists()
            Log.i(TAG, "verifyFirstOnboardingCard: Verified that the first onboarding \"Set as default browser\" button exists")
            Log.i(TAG, "verifyFirstOnboardingCard: Trying to verify that the first onboarding \"Not now\" button exists")
            it.onNodeWithText(
                getStringResource(R.string.juno_onboarding_default_browser_negative_button),
            ).assertExists()
            Log.i(TAG, "verifyFirstOnboardingCard: Verified that the first onboarding \"Not now\" button exists")
        }
    }

    fun verifySecondOnboardingCard(composeTestRule: ComposeTestRule) {
        composeTestRule.also {
            Log.i(TAG, "verifySecondOnboardingCard: Trying to verify that the second onboarding screen title exists")
            it.onNodeWithText(
                getStringResource(R.string.juno_onboarding_sign_in_title_2),
            ).assertExists()
            Log.i(TAG, "verifySecondOnboardingCard: Verified that the second onboarding screen title exists")
            Log.i(TAG, "verifySecondOnboardingCard: Trying to verify that the  second onboarding screen description exists")
            it.onNodeWithText(
                getStringResource(R.string.juno_onboarding_sign_in_description_2),
            ).assertExists()
            Log.i(TAG, "verifySecondOnboardingCard: Verified that the second onboarding screen description exists")
            Log.i(TAG, "verifySecondOnboardingCard: Trying to verify that the first onboarding \"Sign in\" button exists")
            it.onNodeWithText(
                getStringResource(R.string.juno_onboarding_sign_in_positive_button),
            ).assertExists()
            Log.i(TAG, "verifySecondOnboardingCard: Verified that the first onboarding \"Sign in\" button exists")
            Log.i(TAG, "verifySecondOnboardingCard: Trying to verify that the first onboarding \"Not now\" button exists")
            it.onNodeWithText(
                getStringResource(R.string.juno_onboarding_sign_in_negative_button),
            ).assertExists()
            Log.i(TAG, "verifySecondOnboardingCard: Verified that the first onboarding \"Not now\" button exists")
        }
    }

    fun clickNotNowOnboardingButton(composeTestRule: ComposeTestRule) {
        Log.i(TAG, "clickNotNowOnboardingButton: Trying to click \"Not now\" onboarding button")
        composeTestRule.onNodeWithText(
            getStringResource(R.string.juno_onboarding_default_browser_negative_button),
        ).performClick()
        Log.i(TAG, "clickNotNowOnboardingButton: Clicked \"Not now\" onboarding button")
    }

    fun swipeSecondOnboardingCardToRight() {
        Log.i(TAG, "swipeSecondOnboardingCardToRight: Trying to perform swipe right action on second onboarding card")
        mDevice.findObject(
            UiSelector().textContains(
                getStringResource(R.string.juno_onboarding_sign_in_title_2),
            ),
        ).swipeRight(3)
        Log.i(TAG, "swipeSecondOnboardingCardToRight: Performed swipe right action on second onboarding card")
    }

    fun clickGetStartedButton(testRule: ComposeTestRule) {
        Log.i(TAG, "clickGetStartedButton: Trying to click \"Get started\" onboarding button")
        testRule.onNodeWithText(getStringResource(R.string.onboarding_home_get_started_button))
            .performClick()
        Log.i(TAG, "clickGetStartedButton: Clicked \"Get started\" onboarding button")
    }

    fun clickCloseButton(testRule: ComposeTestRule) {
        Log.i(TAG, "clickCloseButton: Trying to click close onboarding button")
        testRule.onNode(hasContentDescription("Close")).performClick()
        Log.i(TAG, "clickCloseButton: Clicked close onboarding button")
    }

    fun verifyUpgradingUserOnboardingSecondScreen(testRule: ComposeTestRule) {
        testRule.also {
            Log.i(TAG, "verifyUpgradingUserOnboardingSecondScreen: Trying to verify that the upgrading user second onboarding screen title is displayed")
            it.onNodeWithText(getStringResource(R.string.onboarding_home_sync_title_3))
                .assertIsDisplayed()
            Log.i(TAG, "verifyUpgradingUserOnboardingSecondScreen: Verified that the upgrading user second onboarding screen title is displayed")
            Log.i(TAG, "verifyUpgradingUserOnboardingSecondScreen: Trying to verify that the upgrading user second onboarding screen description is displayed")
            it.onNodeWithText(getStringResource(R.string.onboarding_home_sync_description))
                .assertIsDisplayed()
            Log.i(TAG, "verifyUpgradingUserOnboardingSecondScreen: Verified that the upgrading user second onboarding screen description is displayed")
            Log.i(TAG, "verifyUpgradingUserOnboardingSecondScreen: Trying to verify that the upgrading user second onboarding \"Sign in\" button is displayed")
            it.onNodeWithText(getStringResource(R.string.onboarding_home_sign_in_button))
                .assertIsDisplayed()
            Log.i(TAG, "verifyUpgradingUserOnboardingSecondScreen: Verified that the upgrading user second onboarding \"Sign in\" button is displayed")
            Log.i(TAG, "verifyUpgradingUserOnboardingSecondScreen: Trying to that the verify upgrading user second onboarding \"Skip\" button is displayed")
            it.onNodeWithText(getStringResource(R.string.onboarding_home_skip_button))
                .assertIsDisplayed()
            Log.i(TAG, "verifyUpgradingUserOnboardingSecondScreen: Verified that the upgrading user second onboarding \"Skip\" button is displayed")
        }
    }

    fun clickSkipButton(testRule: ComposeTestRule) {
        Log.i(TAG, "clickSkipButton: Trying to click \"Skip\" onboarding button")
        testRule
            .onNodeWithText(getStringResource(R.string.onboarding_home_skip_button))
            .performClick()
        Log.i(TAG, "clickSkipButton: Clicked \"Skip\" onboarding button")
    }

    fun verifyCommonMythsLink() =
        assertUIObjectExists(itemContainingText(getStringResource(R.string.private_browsing_common_myths)))

    fun verifyExistingTopSitesList() =
        assertUIObjectExists(itemWithResId("$packageName:id/top_sites_list"))

    fun verifyNotExistingTopSitesList(title: String) {
        Log.i(TAG, "verifyNotExistingTopSitesList: Waiting for $waitingTime ms for top site: $title to be gone")
        mDevice.findObject(UiSelector().text(title)).waitUntilGone(waitingTime)
        Log.i(TAG, "verifyNotExistingTopSitesList: Waited for $waitingTime ms for top site: $title to be gone")
        assertUIObjectExists(
            itemWithResIdContainingText(
                "$packageName:id/top_site_title",
                title,
            ),
            exists = false,
        )
    }
    fun verifySponsoredShortcutDoesNotExist(sponsoredShortcutTitle: String, position: Int) =
        assertUIObjectExists(
            itemWithResIdAndIndex("$packageName:id/top_site_item", index = position - 1)
                .getChild(
                    UiSelector()
                        .textContains(sponsoredShortcutTitle),
                ),
            exists = false,
        )
    fun verifyNotExistingSponsoredTopSitesList() =
        assertUIObjectExists(
            itemWithResIdContainingText(
                "$packageName:id/top_site_subtitle",
                getStringResource(R.string.top_sites_sponsored_label),
            ),
            exists = false,
        )

    fun verifyExistingTopSitesTabs(title: String) {
        Log.i(TAG, "verifyExistingTopSitesTabs: Trying to scroll into view the top sites list")
        homeScreenList().scrollIntoView(itemWithResId("$packageName:id/top_sites_list"))
        Log.i(TAG, "verifyExistingTopSitesTabs: Scrolled into view the top sites list")
        Log.i(TAG, "verifyExistingTopSitesTabs: Waiting for $waitingTime ms for top site: $title to exist")
        mDevice.findObject(
            UiSelector()
                .resourceId("$packageName:id/top_site_title")
                .textContains(title),
        ).waitForExists(waitingTime)
        Log.i(TAG, "verifyExistingTopSitesTabs: Waited for $waitingTime ms for top site: $title to exist")
        Log.i(TAG, "verifyExistingTopSitesTabs: Trying to verify top site: $title is visible")
        onView(allOf(withId(R.id.top_sites_list)))
            .check(matches(hasDescendant(withText(title))))
            .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))
        Log.i(TAG, "verifyExistingTopSitesTabs: Verified top site: $title is visible")
    }
    fun verifySponsoredShortcutDetails(sponsoredShortcutTitle: String, position: Int) {
        assertUIObjectExists(
            itemWithResIdAndIndex(resourceId = "$packageName:id/top_site_item", index = position - 1)
                .getChild(
                    UiSelector()
                        .resourceId("$packageName:id/favicon_card"),
                ),
        )
        assertUIObjectExists(
            itemWithResIdAndIndex(resourceId = "$packageName:id/top_site_item", index = position - 1)
                .getChild(
                    UiSelector()
                        .textContains(sponsoredShortcutTitle),
                ),
        )
        assertUIObjectExists(
            itemWithResIdAndIndex(resourceId = "$packageName:id/top_site_item", index = position - 1)
                .getChild(
                    UiSelector()
                        .resourceId("$packageName:id/top_site_subtitle"),
                ),
        )
    }
    fun verifyTopSiteContextMenuItems() {
        mDevice.waitNotNull(
            findObject(By.text("Open in private tab")),
            waitingTime,
        )
        mDevice.waitNotNull(
            findObject(By.text("Remove")),
            waitingTime,
        )
    }

    fun verifyJumpBackInSectionIsDisplayed() {
        scrollToElementByText(getStringResource(R.string.recent_tabs_header))
        assertUIObjectExists(itemContainingText(getStringResource(R.string.recent_tabs_header)))
    }
    fun verifyJumpBackInSectionIsNotDisplayed() =
        assertUIObjectExists(itemContainingText(getStringResource(R.string.recent_tabs_header)), exists = false)
    fun verifyJumpBackInItemTitle(testRule: ComposeTestRule, itemTitle: String) {
        Log.i(TAG, "verifyJumpBackInItemTitle: Trying to verify jump back in item with title: $itemTitle")
        testRule.onNodeWithTag("recent.tab.title", useUnmergedTree = true)
            .assert(hasText(itemTitle))
        Log.i(TAG, "verifyJumpBackInItemTitle: Verified jump back in item with title: $itemTitle")
    }
    fun verifyJumpBackInItemWithUrl(testRule: ComposeTestRule, itemUrl: String) {
        Log.i(TAG, "verifyJumpBackInItemWithUrl: Trying to verify jump back in item with URL: $itemUrl")
        testRule.onNodeWithTag("recent.tab.url", useUnmergedTree = true).assert(hasText(itemUrl))
        Log.i(TAG, "verifyJumpBackInItemWithUrl: Verified jump back in item with URL: $itemUrl")
    }
    fun verifyJumpBackInShowAllButton() = assertUIObjectExists(itemContainingText(getStringResource(R.string.recent_tabs_show_all)))
    fun verifyRecentlyVisitedSectionIsDisplayed(exists: Boolean) =
        assertUIObjectExists(itemContainingText(getStringResource(R.string.history_metadata_header_2)), exists = exists)
    fun verifyRecentBookmarksSectionIsDisplayed(exists: Boolean) =
        assertUIObjectExists(itemContainingText(getStringResource(R.string.recently_saved_title)), exists = exists)

    fun verifyRecentlyVisitedSearchGroupDisplayed(shouldBeDisplayed: Boolean, searchTerm: String, groupSize: Int) {
        // checks if the search group exists in the Recently visited section
        if (shouldBeDisplayed) {
            scrollToElementByText("Recently visited")
            assertUIObjectExists(
                itemContainingText(searchTerm)
                    .getFromParent(UiSelector().text("$groupSize sites")),
            )
        } else {
            assertUIObjectIsGone(
                itemContainingText(searchTerm)
                    .getFromParent(UiSelector().text("$groupSize sites")),
            )
        }
    }

    // Collections elements
    fun verifyCollectionIsDisplayed(title: String, collectionExists: Boolean = true) {
        if (collectionExists) {
            assertUIObjectExists(itemContainingText(title))
        } else {
            assertUIObjectIsGone(itemWithText(title))
        }
    }

    fun togglePrivateBrowsingModeOnOff() {
        Log.i(TAG, "togglePrivateBrowsingModeOnOff: Trying to click private browsing home screen button")
        onView(ViewMatchers.withResourceName("privateBrowsingButton"))
            .perform(click())
        Log.i(TAG, "togglePrivateBrowsingModeOnOff: Clicked private browsing home screen button")
    }

    fun verifyThoughtProvokingStories(enabled: Boolean) {
        if (enabled) {
            scrollToElementByText(getStringResource(R.string.pocket_stories_header_1))
            assertUIObjectExists(itemContainingText(getStringResource(R.string.pocket_stories_header_1)))
        } else {
            Log.i(TAG, "verifyThoughtProvokingStories: Trying to perform ${LISTS_MAXSWIPES}x a scroll action to the end of the home screen")
            homeScreenList().scrollToEnd(LISTS_MAXSWIPES)
            Log.i(TAG, "verifyThoughtProvokingStories: Performed ${LISTS_MAXSWIPES}x a scroll action to the end of the home screen")
            assertUIObjectExists(itemContainingText(getStringResource(R.string.pocket_stories_header_1)), exists = false)
        }
    }

    fun scrollToPocketProvokingStories() {
        Log.i(TAG, "scrollToPocketProvokingStories: Trying to scroll into view the featured pocket stories")
        homeScreenList().scrollIntoView(
            mDevice.findObject(UiSelector().resourceId("pocket.recommended.story").index(2)),
        )
        Log.i(TAG, "scrollToPocketProvokingStories: Scrolled into view the featured pocket stories")
    }

    fun verifyPocketRecommendedStoriesItems() {
        for (position in 0..8) {
            Log.i(TAG, "verifyPocketRecommendedStoriesItems: Trying to scroll into view the featured pocket story from position: $position")
            pocketStoriesList().scrollIntoView(UiSelector().index(position))
            Log.i(TAG, "verifyPocketRecommendedStoriesItems: Scrolled into view the featured pocket story from position: $position")
            assertUIObjectExists(itemWithIndex(position))
        }
    }

    // Temporarily not in use because Sponsored Pocket stories are only advertised for a limited time.
    // See also known issue https://bugzilla.mozilla.org/show_bug.cgi?id=1828629
//    fun verifyPocketSponsoredStoriesItems(vararg positions: Int) {
//        positions.forEach {
//            pocketStoriesList
//                .scrollIntoView(UiSelector().resourceId("pocket.sponsored.story").index(it - 1))
//
//            assertTrue(
//                "Pocket story item at position $it not found.",
//                mDevice.findObject(UiSelector().index(it - 1).resourceId("pocket.sponsored.story"))
//                    .waitForExists(waitingTimeShort),
//            )
//        }
//    }

    fun verifyDiscoverMoreStoriesButton() {
        Log.i(TAG, "verifyDiscoverMoreStoriesButton: Trying to scroll into view the Pocket \"Discover more\" button")
        pocketStoriesList().scrollIntoView(UiSelector().text("Discover more"))
        Log.i(TAG, "verifyDiscoverMoreStoriesButton: Scrolled into view the Pocket \"Discover more\" button")
        assertUIObjectExists(itemWithText("Discover more"))
    }

    fun verifyStoriesByTopic(enabled: Boolean) {
        if (enabled) {
            scrollToElementByText(getStringResource(R.string.pocket_stories_categories_header))
            assertUIObjectExists(itemContainingText(getStringResource(R.string.pocket_stories_categories_header)))
        } else {
            Log.i(TAG, "verifyStoriesByTopic: Trying to perform ${LISTS_MAXSWIPES}x a scroll action to the end of the home screen")
            homeScreenList().scrollToEnd(LISTS_MAXSWIPES)
            Log.i(TAG, "verifyStoriesByTopic: Performed ${LISTS_MAXSWIPES}x a scroll action to the end of the home screen")
            assertUIObjectExists(itemContainingText(getStringResource(R.string.pocket_stories_categories_header)), exists = false)
        }
    }

    fun verifyStoriesByTopicItems() {
        Log.i(TAG, "verifyStoriesByTopicItems: Trying to scroll into view the stories by topic home screen section")
        homeScreenList().scrollIntoView(UiSelector().resourceId("pocket.categories"))
        Log.i(TAG, "verifyStoriesByTopicItems: Scrolled into view the stories by topic home screen section")
        Log.i(TAG, "verifyStoriesByTopicItems: Trying to verify that there are more than 1 \"Stories by topic\" categories")
        assertTrue(mDevice.findObject(UiSelector().resourceId("pocket.categories")).childCount > 1)
        Log.i(TAG, "verifyStoriesByTopicItems: Verified that there are more than 1 \"Stories by topic\" categories")
    }

    fun verifyStoriesByTopicItemState(composeTestRule: ComposeTestRule, isSelected: Boolean, position: Int) {
        Log.i(TAG, "verifyStoriesByTopicItemState: Trying to scroll into view \"Powered By Pocket\" home screen section")
        homeScreenList().scrollIntoView(mDevice.findObject(UiSelector().resourceId("pocket.header")))
        Log.i(TAG, "verifyStoriesByTopicItemState: Scrolled into view \"Powered By Pocket\" home screen section")

        if (isSelected) {
            Log.i(TAG, "verifyStoriesByTopicItemState: Trying verify that the stories by topic home screen section is displayed")
            composeTestRule.onNodeWithTag("pocket.categories").assertIsDisplayed()
            Log.i(TAG, "verifyStoriesByTopicItemState: Verified that the stories by topic home screen section is displayed")
            Log.i(TAG, "verifyStoriesByTopicItemState: Trying verify that the stories by topic item at position: $position is selected")
            storyByTopicItem(composeTestRule, position).assertIsSelected()
            Log.i(TAG, "verifyStoriesByTopicItemState: Verified that the stories by topic item at position: $position is selected")
        } else {
            Log.i(TAG, "verifyStoriesByTopicItemState: Trying verify that the stories by topic home screen section is displayed")
            composeTestRule.onNodeWithTag("pocket.categories").assertIsDisplayed()
            Log.i(TAG, "verifyStoriesByTopicItemState: Verified that the stories by topic home screen section is displayed")
            Log.i(TAG, "verifyStoriesByTopicItemState: Trying to verify that the stories by topic item at position: $position is not selected")
            storyByTopicItem(composeTestRule, position).assertIsNotSelected()
            Log.i(TAG, "verifyStoriesByTopicItemState: Verified that the stories by topic item at position: $position is not selected")
        }
    }

    fun clickStoriesByTopicItem(composeTestRule: ComposeTestRule, position: Int) {
        Log.i(TAG, "clickStoriesByTopicItem: Trying to click stories by topic item from position: $position")
        storyByTopicItem(composeTestRule, position).performClick()
        Log.i(TAG, "clickStoriesByTopicItem: Clicked stories by topic item from position: $position")
    }

    fun verifyPoweredByPocket() {
        Log.i(TAG, "verifyPoweredByPocket: Trying to scroll into view \"Powered By Pocket\" home screen section")
        homeScreenList().scrollIntoView(mDevice.findObject(UiSelector().resourceId("pocket.header")))
        Log.i(TAG, "verifyPoweredByPocket: Scrolled into view \"Powered By Pocket\" home screen section")
        assertUIObjectExists(itemWithResId("pocket.header.title"))
    }

    fun verifyCustomizeHomepageButton(enabled: Boolean) {
        if (enabled) {
            scrollToElementByText(getStringResource(R.string.browser_menu_customize_home_1))
            assertUIObjectExists(itemContainingText("Customize homepage"))
        } else {
            Log.i(TAG, "verifyCustomizeHomepageButton: Trying to perform ${LISTS_MAXSWIPES}x a scroll action to the end of the home screen")
            homeScreenList().scrollToEnd(LISTS_MAXSWIPES)
            Log.i(TAG, "verifyCustomizeHomepageButton: Performed ${LISTS_MAXSWIPES}x a scroll action to the end of the home screen")
            assertUIObjectExists(itemContainingText("Customize homepage"), exists = false)
        }
    }

    fun verifyJumpBackInMessage(composeTestRule: ComposeTestRule) {
        Log.i(TAG, "verifyJumpBackInMessage: Trying to verify jump back in contextual message")
        composeTestRule
            .onNodeWithText(
                getStringResource(R.string.onboarding_home_screen_jump_back_contextual_hint_2),
            ).assertExists()
        Log.i(TAG, "verifyJumpBackInMessage: Verified jump back in contextual message")
    }

    fun getProvokingStoryPublisher(position: Int): String {
        val publisher = mDevice.findObject(
            UiSelector()
                .className("android.view.View")
                .index(position - 1),
        ).getChild(
            UiSelector()
                .className("android.widget.TextView")
                .index(1),
        ).text

        return publisher
    }

    fun verifyToolbarPosition(defaultPosition: Boolean) {
        Log.i(TAG, "verifyToolbarPosition: Trying to verify toolbar is set to top: $defaultPosition")
        onView(withId(R.id.toolbarLayout))
            .check(
                if (defaultPosition) {
                    isPartiallyBelow(withId(R.id.sessionControlRecyclerView))
                } else {
                    isCompletelyAbove(withId(R.id.homeAppBar))
                },
            )
        Log.i(TAG, "verifyToolbarPosition: Verified toolbar position is set to top: $defaultPosition")
    }
    fun verifyNimbusMessageCard(title: String, text: String, action: String) {
        val textView = UiSelector()
            .className(ComposeView::class.java)
            .className(View::class.java)
            .className(TextView::class.java)
        assertTrue(
            mDevice.findObject(textView.textContains(title)).waitForExists(waitingTime),
        )
        assertTrue(
            mDevice.findObject(textView.textContains(text)).waitForExists(waitingTime),
        )
        assertTrue(
            mDevice.findObject(textView.textContains(action)).waitForExists(waitingTime),
        )
    }

    fun verifyIfInPrivateOrNormalMode(privateBrowsingEnabled: Boolean) {
        Log.i(TAG, "verifyIfInPrivateOrNormalMode: Trying to verify private browsing mode is enabled")
        assert(isPrivateModeEnabled() == privateBrowsingEnabled)
        Log.i(TAG, "verifyIfInPrivateOrNormalMode: Verified private browsing mode is enabled: $privateBrowsingEnabled")
    }

    class Transition {

        fun openTabDrawer(interact: TabDrawerRobot.() -> Unit): TabDrawerRobot.Transition {
            Log.i(TAG, "openTabDrawer: Waiting for $waitingTime ms for tab counter button to exist")
            mDevice.findObject(
                UiSelector().descriptionContains("open tab. Tap to switch tabs."),
            ).waitForExists(waitingTime)
            Log.i(TAG, "openTabDrawer: Waited for $waitingTime ms for tab counter button to exist")
            Log.i(TAG, "openTabDrawer: Trying to click tab counter button")
            tabsCounter().click()
            Log.i(TAG, "openTabDrawer: Clicked tab counter button")
            mDevice.waitNotNull(Until.findObject(By.res("$packageName:id/tab_layout")))

            TabDrawerRobot().interact()
            return TabDrawerRobot.Transition()
        }

        fun openComposeTabDrawer(composeTestRule: HomeActivityComposeTestRule, interact: ComposeTabDrawerRobot.() -> Unit): ComposeTabDrawerRobot.Transition {
            Log.i(TAG, "openComposeTabDrawer: Waiting for device to be idle for $waitingTime ms")
            mDevice.waitForIdle(waitingTime)
            Log.i(TAG, "openComposeTabDrawer: Device was idle for $waitingTime ms")
            Log.i(TAG, "openComposeTabDrawer: Trying to click tab counter button")
            onView(withId(R.id.tab_button)).click()
            Log.i(TAG, "openComposeTabDrawer: Clicked tab counter button")
            Log.i(TAG, "openComposeTabDrawer: Trying to verify the tabs tray exists")
            composeTestRule.onNodeWithTag(TabsTrayTestTag.tabsTray).assertExists()
            Log.i(TAG, "openComposeTabDrawer: Verified the tabs tray exists")

            ComposeTabDrawerRobot(composeTestRule).interact()
            return ComposeTabDrawerRobot.Transition(composeTestRule)
        }

        fun openThreeDotMenu(interact: ThreeDotMenuMainRobot.() -> Unit): ThreeDotMenuMainRobot.Transition {
            // Issue: https://github.com/mozilla-mobile/fenix/issues/21578
            try {
                Log.i(TAG, "openThreeDotMenu: Try block")
                mDevice.waitNotNull(
                    Until.findObject(By.res("$packageName:id/menuButton")),
                    waitingTime,
                )
            } catch (e: AssertionError) {
                Log.i(TAG, "openThreeDotMenu: Catch block")
                Log.i(TAG, "openThreeDotMenu: Trying to click device back button")
                mDevice.pressBack()
                Log.i(TAG, "openThreeDotMenu: Clicked device back button")
            } finally {
                Log.i(TAG, "openThreeDotMenu: Finally block")
                Log.i(TAG, "openThreeDotMenu: Trying to click main menu button")
                threeDotButton().perform(click())
                Log.i(TAG, "openThreeDotMenu: Clicked main menu button")
            }

            ThreeDotMenuMainRobot().interact()
            return ThreeDotMenuMainRobot.Transition()
        }

        fun openSearch(interact: SearchRobot.() -> Unit): SearchRobot.Transition {
            Log.i(TAG, "openSearch: Waiting for $waitingTime ms for the navigation toolbar to exist")
            navigationToolbar().waitForExists(waitingTime)
            Log.i(TAG, "openSearch: Waited for $waitingTime ms for the navigation toolbar to exist")
            Log.i(TAG, "openSearch: Trying to click navigation toolbar")
            navigationToolbar().click()
            Log.i(TAG, "openSearch: Clicked navigation toolbar")
            Log.i(TAG, "openSearch: Waiting for device to be idle")
            mDevice.waitForIdle()
            Log.i(TAG, "openSearch: Device was idle")

            SearchRobot().interact()
            return SearchRobot.Transition()
        }

        fun clickUpgradingUserOnboardingSignInButton(
            testRule: ComposeTestRule,
            interact: SyncSignInRobot.() -> Unit,
        ): SyncSignInRobot.Transition {
            Log.i(TAG, "clickUpgradingUserOnboardingSignInButton: Trying to click the upgrading user onboarding \"Sign in\" button")
            testRule.onNodeWithText("Sign in").performClick()
            Log.i(TAG, "clickUpgradingUserOnboardingSignInButton: Clicked the upgrading user onboarding \"Sign in\" button")

            SyncSignInRobot().interact()
            return SyncSignInRobot.Transition()
        }

        fun togglePrivateBrowsingMode(switchPBModeOn: Boolean = true) {
            // Switch to private browsing homescreen
            if (switchPBModeOn && !isPrivateModeEnabled()) {
                Log.i(TAG, "togglePrivateBrowsingMode: Waiting for $waitingTime ms for private browsing button to exist")
                privateBrowsingButton().waitForExists(waitingTime)
                Log.i(TAG, "togglePrivateBrowsingMode: Waited for $waitingTime ms for private browsing button to exist")
                Log.i(TAG, "togglePrivateBrowsingMode: Trying to click private browsing button")
                privateBrowsingButton().click()
                Log.i(TAG, "togglePrivateBrowsingMode: Clicked private browsing button")
            }

            // Switch to normal browsing homescreen
            if (!switchPBModeOn && isPrivateModeEnabled()) {
                Log.i(TAG, "togglePrivateBrowsingMode: Waiting for $waitingTime ms for private browsing button to exist")
                privateBrowsingButton().waitForExists(waitingTime)
                Log.i(TAG, "togglePrivateBrowsingMode: Waited for $waitingTime ms for private browsing button to exist")
                Log.i(TAG, "togglePrivateBrowsingMode: Trying to click private browsing button")
                privateBrowsingButton().click()
                privateBrowsingButton().click()
                Log.i(TAG, "togglePrivateBrowsingMode: Clicked private browsing button")
            }
        }

        fun triggerPrivateBrowsingShortcutPrompt(interact: AddToHomeScreenRobot.() -> Unit): AddToHomeScreenRobot.Transition {
            // Loop to press the PB icon for 5 times to display the Add the Private Browsing Shortcut CFR
            for (i in 1..5) {
                Log.i(TAG, "triggerPrivateBrowsingShortcutPrompt: Waiting for $waitingTime ms for private browsing button to exist")
                mDevice.findObject(UiSelector().resourceId("$packageName:id/privateBrowsingButton"))
                    .waitForExists(
                        waitingTime,
                    )
                Log.i(TAG, "triggerPrivateBrowsingShortcutPrompt: Waited for $waitingTime ms for private browsing button to exist")
                Log.i(TAG, "triggerPrivateBrowsingShortcutPrompt: Trying to click private browsing button")
                privateBrowsingButton().click()
                Log.i(TAG, "triggerPrivateBrowsingShortcutPrompt: Clicked private browsing button")
            }

            AddToHomeScreenRobot().interact()
            return AddToHomeScreenRobot.Transition()
        }

        fun pressBack() {
            Log.i(TAG, "pressBack: Trying to click device back button")
            onView(ViewMatchers.isRoot()).perform(ViewActions.pressBack())
            Log.i(TAG, "pressBack: Clicked device back button")
        }

        fun openNavigationToolbar(interact: NavigationToolbarRobot.() -> Unit): NavigationToolbarRobot.Transition {
            Log.i(TAG, "openNavigationToolbar: Waiting for $waitingTime ms for navigation the toolbar to exist")
            mDevice.findObject(UiSelector().resourceId("$packageName:id/toolbar"))
                .waitForExists(waitingTime)
            Log.i(TAG, "openNavigationToolbar: Waited for $waitingTime ms for the navigation toolbar to exist")
            Log.i(TAG, "openNavigationToolbar: Trying to click the navigation toolbar")
            navigationToolbar().click()
            Log.i(TAG, "openNavigationToolbar: Clicked the navigation toolbar")

            NavigationToolbarRobot().interact()
            return NavigationToolbarRobot.Transition()
        }

        fun openContextMenuOnTopSitesWithTitle(
            title: String,
            interact: HomeScreenRobot.() -> Unit,
        ): Transition {
            Log.i(TAG, "openContextMenuOnTopSitesWithTitle: Trying to long click top site with title: $title")
            onView(withId(R.id.top_sites_list)).perform(
                actionOnItem<RecyclerView.ViewHolder>(
                    hasDescendant(withText(title)),
                    ViewActions.longClick(),
                ),
            )
            Log.i(TAG, "openContextMenuOnTopSitesWithTitle: Long clicked top site with title: $title")

            HomeScreenRobot().interact()
            return Transition()
        }

        fun openContextMenuOnSponsoredShortcut(sponsoredShortcutTitle: String, interact: HomeScreenRobot.() -> Unit): Transition {
            Log.i(TAG, "openContextMenuOnSponsoredShortcut: Trying to long click: $sponsoredShortcutTitle sponsored shortcut")
            sponsoredShortcut(sponsoredShortcutTitle).perform(longClick())
            Log.i(TAG, "openContextMenuOnSponsoredShortcut: Long clicked: $sponsoredShortcutTitle sponsored shortcut")

            HomeScreenRobot().interact()
            return Transition()
        }

        fun openTopSiteTabWithTitle(
            title: String,
            interact: BrowserRobot.() -> Unit,
        ): BrowserRobot.Transition {
            Log.i(TAG, "openTopSiteTabWithTitle: Trying to click top site with title $title")
            onView(withId(R.id.top_sites_list)).perform(
                actionOnItem<RecyclerView.ViewHolder>(hasDescendant(withText(title)), click()),
            )
            Log.i(TAG, "openTopSiteTabWithTitle:Clicked top site with title $title")

            BrowserRobot().interact()
            return BrowserRobot.Transition()
        }

        fun openSponsoredShortcut(sponsoredShortcutTitle: String, interact: BrowserRobot.() -> Unit): BrowserRobot.Transition {
            Log.i(TAG, "openSponsoredShortcut: Trying to click sponsored top site with title: $sponsoredShortcutTitle")
            sponsoredShortcut(sponsoredShortcutTitle).click()
            Log.i(TAG, "openSponsoredShortcut: Clicked sponsored top site with title: $sponsoredShortcutTitle")

            BrowserRobot().interact()
            return BrowserRobot.Transition()
        }

        fun renameTopSite(title: String, interact: HomeScreenRobot.() -> Unit): Transition {
            Log.i(TAG, "renameTopSite: Trying to click context menu \"Rename\" button")
            onView(withText("Rename"))
                .check((matches(withEffectiveVisibility(Visibility.VISIBLE))))
                .perform(click())
            Log.i(TAG, "renameTopSite: Clicked context menu \"Rename\" button")
            Log.i(TAG, "renameTopSite: Trying to set top site title to: $title")
            onView(Matchers.allOf(withId(R.id.top_site_title), instanceOf(EditText::class.java)))
                .perform(ViewActions.replaceText(title))
            Log.i(TAG, "renameTopSite: Set top site title to: $title")
            Log.i(TAG, "renameTopSite: Trying to click \"Ok\" rename top site dialog button")
            onView(withId(android.R.id.button1)).perform((click()))
            Log.i(TAG, "renameTopSite: Clicked \"Ok\" rename top site dialog button")

            HomeScreenRobot().interact()
            return Transition()
        }

        fun removeTopSite(interact: HomeScreenRobot.() -> Unit): Transition {
            Log.i(TAG, "removeTopSite: Trying to click context menu \"Remove\" button")
            onView(withText("Remove"))
                .check((matches(withEffectiveVisibility(Visibility.VISIBLE))))
                .perform(click())
            Log.i(TAG, "removeTopSite: Clicked context menu \"Remove\" button")

            HomeScreenRobot().interact()
            return Transition()
        }

        fun deleteTopSiteFromHistory(interact: HomeScreenRobot.() -> Unit): Transition {
            Log.i(TAG, "deleteTopSiteFromHistory: Waiting for $waitingTime ms for context menu \"Remove from history\" button to exist")
            mDevice.findObject(
                UiSelector().resourceId("$packageName:id/simple_text"),
            ).waitForExists(waitingTime)
            Log.i(TAG, "deleteTopSiteFromHistory: Waited for $waitingTime ms for context menu \"Remove from history\" button to exist")
            Log.i(TAG, "deleteTopSiteFromHistory: Trying to click context menu \"Remove from history\" button")
            deleteFromHistory().click()
            Log.i(TAG, "deleteTopSiteFromHistory: Clicked context menu \"Remove from history\" button")

            HomeScreenRobot().interact()
            return Transition()
        }

        fun openTopSiteInPrivateTab(interact: BrowserRobot.() -> Unit): BrowserRobot.Transition {
            Log.i(TAG, "openTopSiteInPrivateTab: Trying to click context menu \"Open in private tab\" button")
            onView(withText("Open in private tab"))
                .check((matches(withEffectiveVisibility(Visibility.VISIBLE))))
                .perform(click())
            Log.i(TAG, "openTopSiteInPrivateTab: Clicked context menu \"Open in private tab\" button")

            BrowserRobot().interact()
            return BrowserRobot.Transition()
        }

        fun clickSponsorsAndPrivacyButton(interact: BrowserRobot.() -> Unit): BrowserRobot.Transition {
            Log.i(TAG, "clickSponsorsAndPrivacyButton: Waiting for $waitingTime ms for context menu \"Our sponsors & your privacy\" button to exist")
            sponsorsAndPrivacyButton().waitForExists(waitingTime)
            Log.i(TAG, "clickSponsorsAndPrivacyButton: Waited for $waitingTime ms for context menu \"Our sponsors & your privacy\" button to exist")
            Log.i(TAG, "clickSponsorsAndPrivacyButton: Trying to click \"Our sponsors & your privacy\" context menu button and wait for $waitingTime ms for a new window")
            sponsorsAndPrivacyButton().clickAndWaitForNewWindow(waitingTime)
            Log.i(TAG, "clickSponsorsAndPrivacyButton: Clicked \"Our sponsors & your privacy\" context menu button and waited for $waitingTime ms for a new window")

            BrowserRobot().interact()
            return BrowserRobot.Transition()
        }

        fun clickSponsoredShortcutsSettingsButton(interact: SettingsSubMenuHomepageRobot.() -> Unit): SettingsSubMenuHomepageRobot.Transition {
            Log.i(TAG, "clickSponsoredShortcutsSettingsButton: Waiting for $waitingTime ms for context menu \"Settings\" button to exist")
            sponsoredShortcutsSettingsButton().waitForExists(waitingTime)
            Log.i(TAG, "clickSponsoredShortcutsSettingsButton: Waited for $waitingTime ms for context menu \"Settings\" button to exist")
            Log.i(TAG, "clickSponsoredShortcutsSettingsButton: Trying to click \"Settings\" context menu button and wait for $waitingTime for a new window")
            sponsoredShortcutsSettingsButton().clickAndWaitForNewWindow(waitingTime)
            Log.i(TAG, "clickSponsoredShortcutsSettingsButton: Clicked \"Settings\" context menu button and waited for $waitingTime for a new window")

            SettingsSubMenuHomepageRobot().interact()
            return SettingsSubMenuHomepageRobot.Transition()
        }

        fun openCommonMythsLink(interact: BrowserRobot.() -> Unit): BrowserRobot.Transition {
            Log.i(TAG, "openCommonMythsLink: Trying to click private browsing home screen common myths link")
            mDevice.findObject(
                UiSelector()
                    .textContains(
                        getStringResource(R.string.private_browsing_common_myths),
                    ),
            ).also { it.click() }
            Log.i(TAG, "openCommonMythsLink: Clicked private browsing home screen common myths link")

            BrowserRobot().interact()
            return BrowserRobot.Transition()
        }

        fun clickSaveTabsToCollectionButton(interact: TabDrawerRobot.() -> Unit): TabDrawerRobot.Transition {
            scrollToElementByText(getStringResource(R.string.no_collections_description2))
            Log.i(TAG, "clickSaveTabsToCollectionButton: Trying to click save tabs to collection button")
            saveTabsToCollectionButton().click()
            Log.i(TAG, "clickSaveTabsToCollectionButton: Clicked save tabs to collection button")

            TabDrawerRobot().interact()
            return TabDrawerRobot.Transition()
        }

        fun clickSaveTabsToCollectionButton(composeTestRule: HomeActivityComposeTestRule, interact: ComposeTabDrawerRobot.() -> Unit): ComposeTabDrawerRobot.Transition {
            scrollToElementByText(getStringResource(R.string.no_collections_description2))
            Log.i(TAG, "clickSaveTabsToCollectionButton: Trying to click save tabs to collection button")
            saveTabsToCollectionButton().click()
            Log.i(TAG, "clickSaveTabsToCollectionButton: Clicked save tabs to collection button")
            ComposeTabDrawerRobot(composeTestRule).interact()
            return ComposeTabDrawerRobot.Transition(composeTestRule)
        }

        fun expandCollection(title: String, interact: CollectionRobot.() -> Unit): CollectionRobot.Transition {
            assertUIObjectExists(itemContainingText(title))
            Log.i(TAG, "expandCollection: Trying to click collection with title: $title and wait for $waitingTimeShort ms for a new window")
            itemContainingText(title).clickAndWaitForNewWindow(waitingTimeShort)
            Log.i(TAG, "expandCollection: Clicked collection with title: $title and waited for $waitingTimeShort ms for a new window")
            assertUIObjectExists(itemWithDescription(getStringResource(R.string.remove_tab_from_collection)))

            CollectionRobot().interact()
            return CollectionRobot.Transition()
        }

        fun openRecentlyVisitedSearchGroupHistoryList(title: String, interact: HistoryRobot.() -> Unit): HistoryRobot.Transition {
            scrollToElementByText("Recently visited")
            val searchGroup = mDevice.findObject(UiSelector().text(title))
            Log.i(TAG, "openRecentlyVisitedSearchGroupHistoryList: Waiting for $waitingTimeShort ms for recently visited search group with title: $title to exist")
            searchGroup.waitForExists(waitingTimeShort)
            Log.i(TAG, "openRecentlyVisitedSearchGroupHistoryList: Waited for $waitingTimeShort ms for recently visited search group with title: $title to exist")
            Log.i(TAG, "openRecentlyVisitedSearchGroupHistoryList: Trying to click recently visited search group with title: $title")
            searchGroup.click()
            Log.i(TAG, "openRecentlyVisitedSearchGroupHistoryList: Clicked recently visited search group with title: $title")

            HistoryRobot().interact()
            return HistoryRobot.Transition()
        }

        fun openCustomizeHomepage(interact: SettingsSubMenuHomepageRobot.() -> Unit): SettingsSubMenuHomepageRobot.Transition {
            Log.i(TAG, "openCustomizeHomepage: Trying to perform ${LISTS_MAXSWIPES}x a scroll action to the end of the home screen")
            homeScreenList().scrollToEnd(LISTS_MAXSWIPES)
            Log.i(TAG, "openCustomizeHomepage: Performed ${LISTS_MAXSWIPES}x a scroll action to the end of the home screen")
            Log.i(TAG, "openCustomizeHomepage: Trying to click \"Customize homepage\" button and wait for $waitingTime ms for a new window")
            mDevice.findObject(
                UiSelector()
                    .textContains(
                        "Customize homepage",
                    ),
            ).clickAndWaitForNewWindow(waitingTime)
            Log.i(TAG, "openCustomizeHomepage: Clicked \"Customize homepage\" button and wait for $waitingTime ms for a new window")

            SettingsSubMenuHomepageRobot().interact()
            return SettingsSubMenuHomepageRobot.Transition()
        }

        fun clickJumpBackInShowAllButton(interact: TabDrawerRobot.() -> Unit): TabDrawerRobot.Transition {
            Log.i(TAG, "clickJumpBackInShowAllButton: Trying to click \"Show all\" button and wait for $waitingTime ms for a new window")
            mDevice
                .findObject(
                    UiSelector()
                        .textContains(getStringResource(R.string.recent_tabs_show_all)),
                ).clickAndWaitForNewWindow(waitingTime)
            Log.i(TAG, "clickJumpBackInShowAllButton: Clicked \"Show all\" button and wait for $waitingTime ms for a new window")

            TabDrawerRobot().interact()
            return TabDrawerRobot.Transition()
        }

        fun clickJumpBackInShowAllButton(composeTestRule: HomeActivityComposeTestRule, interact: ComposeTabDrawerRobot.() -> Unit): ComposeTabDrawerRobot.Transition {
            Log.i(TAG, "clickJumpBackInShowAllButton: Trying to click \"Show all\" button and wait for $waitingTime ms for a new window")
            mDevice
                .findObject(
                    UiSelector()
                        .textContains(getStringResource(R.string.recent_tabs_show_all)),
                ).clickAndWaitForNewWindow(waitingTime)
            Log.i(TAG, "clickJumpBackInShowAllButton: Clicked \"Show all\" button and wait for $waitingTime ms for a new window")

            ComposeTabDrawerRobot(composeTestRule).interact()
            return ComposeTabDrawerRobot.Transition(composeTestRule)
        }

        fun clickPocketStoryItem(publisher: String, position: Int, interact: BrowserRobot.() -> Unit): BrowserRobot.Transition {
            Log.i(TAG, "clickPocketStoryItem: Trying to click pocket story item published by: $publisher at position: $position and wait for $waitingTime ms for a new window")
            mDevice.findObject(
                UiSelector()
                    .className("android.view.View")
                    .index(position - 1),
            ).getChild(
                UiSelector()
                    .className("android.widget.TextView")
                    .index(1)
                    .textContains(publisher),
            ).clickAndWaitForNewWindow(waitingTime)
            Log.i(TAG, "clickPocketStoryItem: Clicked pocket story item published by: $publisher at position: $position and wait for $waitingTime ms for a new window")

            BrowserRobot().interact()
            return BrowserRobot.Transition()
        }

        fun clickPocketDiscoverMoreButton(interact: BrowserRobot.() -> Unit): BrowserRobot.Transition {
            Log.i(TAG, "clickPocketDiscoverMoreButton: Trying to scroll into view the \"Discover more\" button")
            pocketStoriesList()
                .scrollIntoView(UiSelector().text("Discover more"))
            Log.i(TAG, "clickPocketDiscoverMoreButton: Scrolled into view the \"Discover more\" button")

            mDevice.findObject(UiSelector().text("Discover more")).also {
                Log.i(TAG, "clickPocketDiscoverMoreButton: Waiting for $waitingTime ms for \"Discover more\" button to exist")
                it.waitForExists(waitingTimeShort)
                Log.i(TAG, "clickPocketDiscoverMoreButton: Waited for $waitingTime ms for \"Discover more\" button to exist")
                Log.i(TAG, "clickPocketDiscoverMoreButton: Trying to click \"Discover more\" button")
                it.click()
                Log.i(TAG, "clickPocketDiscoverMoreButton: Clicked \"Discover more\" button")
            }

            BrowserRobot().interact()
            return BrowserRobot.Transition()
        }

        fun clickPocketLearnMoreLink(composeTestRule: ComposeTestRule, interact: BrowserRobot.() -> Unit): BrowserRobot.Transition {
            Log.i(TAG, "clickPocketLearnMoreLink: Trying to click pocket \"Learn more\" link")
            composeTestRule.onNodeWithTag("pocket.header.subtitle", true).performClick()
            Log.i(TAG, "clickPocketLearnMoreLink: Clicked pocket \"Learn more\" link")

            BrowserRobot().interact()
            return BrowserRobot.Transition()
        }

        fun clickSetAsDefaultBrowserOnboardingButton(
            composeTestRule: ComposeTestRule,
            interact: SettingsRobot.() -> Unit,
        ): SettingsRobot.Transition {
            Log.i(TAG, "clickSetAsDefaultBrowserOnboardingButton: Trying to click \"Set as default browser\" onboarding button")
            composeTestRule.onNodeWithText(
                getStringResource(R.string.juno_onboarding_default_browser_positive_button),
            ).performClick()
            Log.i(TAG, "clickSetAsDefaultBrowserOnboardingButton: Clicked \"Set as default browser\" onboarding button")

            SettingsRobot().interact()
            return SettingsRobot.Transition()
        }

        fun clickSignInOnboardingButton(
            composeTestRule: ComposeTestRule,
            interact: SyncSignInRobot.() -> Unit,
        ): SyncSignInRobot.Transition {
            Log.i(TAG, "clickSignInOnboardingButton: Trying to click \"Sign in\" onboarding button")
            composeTestRule.onNodeWithText(
                getStringResource(R.string.juno_onboarding_sign_in_positive_button),
            ).performClick()
            Log.i(TAG, "clickSignInOnboardingButton: Clicked \"Sign in\" onboarding button")

            SyncSignInRobot().interact()
            return SyncSignInRobot.Transition()
        }
    }
}

fun homeScreen(interact: HomeScreenRobot.() -> Unit): HomeScreenRobot.Transition {
    HomeScreenRobot().interact()
    return HomeScreenRobot.Transition()
}

fun homeScreenWithComposeTopSites(composeTestRule: HomeActivityComposeTestRule, interact: ComposeTopSitesRobot.() -> Unit): ComposeTopSitesRobot.Transition {
    ComposeTopSitesRobot(composeTestRule).interact()
    return ComposeTopSitesRobot.Transition(composeTestRule)
}

private fun homeScreenList() =
    UiScrollable(
        UiSelector()
            .resourceId("$packageName:id/sessionControlRecyclerView")
            .scrollable(true),
    ).setAsVerticalList()

private fun threeDotButton() = onView(allOf(withId(R.id.menuButton)))

private fun saveTabsToCollectionButton() = onView(withId(R.id.add_tabs_to_collections_button))

private fun tabsCounter() = onView(withId(R.id.tab_button))

private fun sponsoredShortcut(sponsoredShortcutTitle: String) =
    onView(
        allOf(
            withId(R.id.top_site_title),
            withText(sponsoredShortcutTitle),
        ),
    )

private fun storyByTopicItem(composeTestRule: ComposeTestRule, position: Int) =
    composeTestRule.onNodeWithTag("pocket.categories").onChildAt(position - 1)

private fun homeScreen() =
    itemWithResId("$packageName:id/homeLayout")
private fun privateBrowsingButton() =
    itemWithResId("$packageName:id/privateBrowsingButton")

private fun isPrivateModeEnabled(): Boolean =
    itemWithResIdAndDescription(
        "$packageName:id/privateBrowsingButton",
        "Disable private browsing",
    ).exists()

private fun homepageWordmark() =
    itemWithResId("$packageName:id/wordmark")

private fun navigationToolbar() =
    itemWithResId("$packageName:id/toolbar")
private fun menuButton() =
    itemWithResId("$packageName:id/menuButton")
private fun tabCounter(numberOfOpenTabs: String) =
    itemWithResIdAndText("$packageName:id/counter_text", numberOfOpenTabs)

fun deleteFromHistory() =
    onView(
        allOf(
            withId(R.id.simple_text),
            withText(R.string.delete_from_history),
        ),
    ).inRoot(RootMatchers.isPlatformPopup())

private fun sponsoredShortcutsSettingsButton() =
    mDevice
        .findObject(
            UiSelector()
                .textContains(getStringResource(R.string.top_sites_menu_settings))
                .resourceId("$packageName:id/simple_text"),
        )

private fun sponsorsAndPrivacyButton() =
    mDevice
        .findObject(
            UiSelector()
                .textContains(getStringResource(R.string.top_sites_menu_sponsor_privacy))
                .resourceId("$packageName:id/simple_text"),
        )

private fun pocketStoriesList() =
    UiScrollable(UiSelector().resourceId("pocket.stories")).setAsHorizontalList()
