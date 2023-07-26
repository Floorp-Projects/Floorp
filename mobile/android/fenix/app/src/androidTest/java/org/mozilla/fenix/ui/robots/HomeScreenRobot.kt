/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

@file:Suppress("TooManyFunctions")

package org.mozilla.fenix.ui.robots

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
import org.junit.Assert
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.mozilla.fenix.R
import org.mozilla.fenix.helpers.Constants.LISTS_MAXSWIPES
import org.mozilla.fenix.helpers.Constants.LONG_CLICK_DURATION
import org.mozilla.fenix.helpers.HomeActivityComposeTestRule
import org.mozilla.fenix.helpers.MatcherHelper.assertItemContainingTextExists
import org.mozilla.fenix.helpers.MatcherHelper.assertItemWithDescriptionExists
import org.mozilla.fenix.helpers.MatcherHelper.assertItemWithResIdAndTextExists
import org.mozilla.fenix.helpers.MatcherHelper.assertItemWithResIdExists
import org.mozilla.fenix.helpers.MatcherHelper.itemContainingText
import org.mozilla.fenix.helpers.MatcherHelper.itemWithDescription
import org.mozilla.fenix.helpers.MatcherHelper.itemWithResId
import org.mozilla.fenix.helpers.MatcherHelper.itemWithResIdAndDescription
import org.mozilla.fenix.helpers.MatcherHelper.itemWithResIdAndText
import org.mozilla.fenix.helpers.TestAssetHelper.waitingTime
import org.mozilla.fenix.helpers.TestAssetHelper.waitingTimeShort
import org.mozilla.fenix.helpers.TestHelper.appName
import org.mozilla.fenix.helpers.TestHelper.getStringResource
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
    val privateSessionMessage =
        "$appName clears your search and browsing history from private tabs when you close them" +
            " or quit the app. While this doesn’t make you anonymous to websites or your internet" +
            " service provider, it makes it easier to keep what you do online private from anyone" +
            " else who uses this device."

    fun verifyNavigationToolbar() = assertItemWithResIdExists(navigationToolbar)

    fun verifyHomeScreen() = assertItemWithResIdExists(homeScreen)

    fun verifyPrivateBrowsingHomeScreen() {
        verifyHomeScreenAppBarItems()
        assertItemContainingTextExists(itemContainingText(privateSessionMessage))
        verifyCommonMythsLink()
        verifyNavigationToolbarItems()
    }

    fun verifyHomeScreenAppBarItems() =
        assertItemWithResIdExists(homeScreen, privateBrowsingButton, homepageWordmark)

    fun verifyNavigationToolbarItems(numberOfOpenTabs: String = "0") {
        assertItemWithResIdExists(navigationToolbar, menuButton)
        assertItemWithResIdAndTextExists(tabCounter(numberOfOpenTabs))
    }

    fun verifyHomePrivateBrowsingButton() = assertItemWithResIdExists(privateBrowsingButton)
    fun verifyHomeMenuButton() = assertItemWithResIdExists(menuButton)
    fun verifyTabButton() = assertTabButton()
    fun verifyCollectionsHeader() = assertCollectionsHeader()
    fun verifyNoCollectionsText() = assertNoCollectionsText()
    fun verifyHomeWordmark() {
        homeScreenList().scrollToBeginning(3)
        assertItemWithResIdExists(homepageWordmark)
    }
    fun verifyHomeComponent() = assertHomeComponent()

    fun verifyTabCounter(numberOfOpenTabs: String) =
        assertItemWithResIdAndTextExists(tabCounter(numberOfOpenTabs))

    fun verifyWallpaperImageApplied(isEnabled: Boolean) {
        if (isEnabled) {
            assertTrue(
                mDevice.findObject(
                    UiSelector().resourceId("$packageName:id/wallpaperImageView"),
                ).waitForExists(waitingTimeShort),
            )
        } else {
            assertFalse(
                mDevice.findObject(
                    UiSelector().resourceId("$packageName:id/wallpaperImageView"),
                ).waitForExists(waitingTimeShort),
            )
        }

        mDevice.findObject(UiSelector())
    }

    // Upgrading users onboarding dialog
    fun verifyUpgradingUserOnboardingFirstScreen(testRule: ComposeTestRule) {
        testRule.also {
            it.onNodeWithText(getStringResource(R.string.onboarding_home_welcome_title_2))
                .assertIsDisplayed()

            it.onNodeWithText(getStringResource(R.string.onboarding_home_welcome_description))
                .assertIsDisplayed()

            it.onNodeWithText(getStringResource(R.string.onboarding_home_get_started_button))
                .assertIsDisplayed()
        }
    }

    fun clickGetStartedButton(testRule: ComposeTestRule) =
        testRule.onNodeWithText(getStringResource(R.string.onboarding_home_get_started_button)).performClick()

    fun clickCloseButton(testRule: ComposeTestRule) =
        testRule.onNode(hasContentDescription("Close")).performClick()

    fun verifyUpgradingUserOnboardingSecondScreen(testRule: ComposeTestRule) {
        testRule.also {
            it.onNodeWithText(getStringResource(R.string.onboarding_home_sync_title_3))
                .assertIsDisplayed()

            it.onNodeWithText(getStringResource(R.string.onboarding_home_sync_description))
                .assertIsDisplayed()

            it.onNodeWithText(getStringResource(R.string.onboarding_home_sign_in_button))
                .assertIsDisplayed()

            it.onNodeWithText(getStringResource(R.string.onboarding_home_skip_button))
                .assertIsDisplayed()
        }
    }

    fun clickSkipButton(testRule: ComposeTestRule) =
        testRule
            .onNodeWithText(getStringResource(R.string.onboarding_home_skip_button))
            .performClick()

    fun verifyCommonMythsLink() =
        assertItemContainingTextExists(itemContainingText(getStringResource(R.string.private_browsing_common_myths)))

    fun verifyExistingTopSitesList() = assertExistingTopSitesList()
    fun verifyNotExistingTopSitesList(title: String) = assertNotExistingTopSitesList(title)
    fun verifySponsoredShortcutDoesNotExist(sponsoredShortcutTitle: String, position: Int) =
        assertFalse(
            mDevice.findObject(
                UiSelector()
                    .resourceId("$packageName:id/top_site_item")
                    .index(position - 1),
            ).getChild(
                UiSelector()
                    .textContains(sponsoredShortcutTitle),
            ).waitForExists(waitingTimeShort),
        )
    fun verifyNotExistingSponsoredTopSitesList() = assertSponsoredTopSitesNotDisplayed()
    fun verifyExistingTopSitesTabs(title: String) {
        homeScreenList().scrollIntoView(itemWithResId("$packageName:id/top_sites_list"))
        assertExistingTopSitesTabs(title)
    }
    fun verifySponsoredShortcutDetails(sponsoredShortcutTitle: String, position: Int) {
        assertSponsoredShortcutLogoIsDisplayed(position)
        assertSponsoredShortcutTitle(sponsoredShortcutTitle, position)
        assertSponsoredSubtitleIsDisplayed(position)
    }
    fun verifyTopSiteContextMenuItems() = assertTopSiteContextMenuItems()

    fun verifyJumpBackInSectionIsDisplayed() {
        scrollToElementByText(getStringResource(R.string.recent_tabs_header))
        assertTrue(jumpBackInSection().waitForExists(waitingTime))
    }
    fun verifyJumpBackInSectionIsNotDisplayed() = assertJumpBackInSectionIsNotDisplayed()
    fun verifyJumpBackInItemTitle(testRule: ComposeTestRule, itemTitle: String) =
        assertJumpBackInItemTitle(testRule, itemTitle)
    fun verifyJumpBackInItemWithUrl(testRule: ComposeTestRule, itemUrl: String) =
        assertJumpBackInItemWithUrl(testRule, itemUrl)
    fun verifyJumpBackInShowAllButton() = assertJumpBackInShowAllButton()
    fun verifyRecentlyVisitedSectionIsDisplayed() = assertRecentlyVisitedSectionIsDisplayed()
    fun verifyRecentlyVisitedSectionIsNotDisplayed() = assertRecentlyVisitedSectionIsNotDisplayed()
    fun verifyRecentBookmarksSectionIsDisplayed() = assertRecentBookmarksSectionIsDisplayed()
    fun verifyRecentBookmarksSectionIsNotDisplayed() = assertRecentBookmarksSectionIsNotDisplayed()
    fun verifyPocketSectionIsDisplayed() = assertPocketSectionIsDisplayed()
    fun verifyPocketSectionIsNotDisplayed() = assertPocketSectionIsNotDisplayed()

    fun verifyRecentlyVisitedSearchGroupDisplayed(shouldBeDisplayed: Boolean, searchTerm: String, groupSize: Int) {
        // checks if the search group exists in the Recently visited section
        if (shouldBeDisplayed) {
            scrollToElementByText("Recently visited")
            assertTrue(
                mDevice.findObject(UiSelector().text(searchTerm))
                    .getFromParent(UiSelector().text("$groupSize sites"))
                    .waitForExists(waitingTimeShort),
            )
        } else {
            assertTrue(
                mDevice.findObject(UiSelector().text(searchTerm))
                    .getFromParent(UiSelector().text("$groupSize sites"))
                    .waitUntilGone(waitingTimeShort),
            )
        }
    }

    // Collections elements
    fun verifyCollectionIsDisplayed(title: String, collectionExists: Boolean = true) {
        if (collectionExists) {
            assertTrue(mDevice.findObject(UiSelector().text(title)).waitForExists(waitingTime))
        } else {
            assertTrue(mDevice.findObject(UiSelector().text(title)).waitUntilGone(waitingTime))
        }
    }

    fun togglePrivateBrowsingModeOnOff() {
        onView(ViewMatchers.withResourceName("privateBrowsingButton"))
            .perform(click())
    }

    fun swipeToBottom() = onView(withId(R.id.homeLayout)).perform(ViewActions.swipeUp())

    fun swipeToTop() =
        onView(withId(R.id.sessionControlRecyclerView)).perform(ViewActions.swipeDown())

    fun verifySnackBarText(expectedText: String) {
        mDevice.waitNotNull(findObject(By.text(expectedText)), waitingTime)
    }

    fun clickFirefoxLogo() = homepageWordmark.click()

    fun verifyThoughtProvokingStories(enabled: Boolean) {
        if (enabled) {
            scrollToElementByText(getStringResource(R.string.pocket_stories_header_1))
            assertTrue(
                mDevice.findObject(
                    UiSelector()
                        .textContains(
                            getStringResource(R.string.pocket_stories_header_1),
                        ),
                ).waitForExists(waitingTime),
            )
        } else {
            homeScreenList().scrollToEnd(LISTS_MAXSWIPES)
            assertFalse(
                mDevice.findObject(
                    UiSelector()
                        .textContains(
                            getStringResource(R.string.pocket_stories_header_1),
                        ),
                ).waitForExists(waitingTimeShort),
            )
        }
    }

    fun scrollToPocketProvokingStories() {
        homeScreenList().scrollIntoView(
            mDevice.findObject(UiSelector().resourceId("pocket.recommended.story").index(2)),
        )
    }

    fun verifyPocketRecommendedStoriesItems() {
        for (position in 0..8) {
            pocketStoriesList
                .scrollIntoView(UiSelector().index(position))

            assertTrue(
                "Pocket story item at position $position not found.",
                mDevice.findObject(UiSelector().index(position))
                    .waitForExists(waitingTimeShort),
            )
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
        pocketStoriesList
            .scrollIntoView(UiSelector().text("Discover more"))
        assertTrue(
            mDevice.findObject(UiSelector().text("Discover more"))
                .waitForExists(waitingTimeShort),
        )
    }

    fun verifyStoriesByTopic(enabled: Boolean) {
        if (enabled) {
            scrollToElementByText(getStringResource(R.string.pocket_stories_categories_header))
            assertTrue(
                mDevice.findObject(
                    UiSelector()
                        .textContains(
                            getStringResource(R.string.pocket_stories_categories_header),
                        ),
                ).waitForExists(waitingTime),
            )
        } else {
            homeScreenList().scrollToEnd(LISTS_MAXSWIPES)
            assertFalse(
                mDevice.findObject(
                    UiSelector()
                        .textContains(
                            getStringResource(R.string.pocket_stories_categories_header),
                        ),
                ).waitForExists(waitingTimeShort),
            )
        }
    }

    fun verifyStoriesByTopicItems() {
        homeScreenList().scrollIntoView(UiSelector().resourceId("pocket.categories"))
        assertTrue(mDevice.findObject(UiSelector().resourceId("pocket.categories")).childCount > 1)
    }

    fun verifyStoriesByTopicItemState(composeTestRule: ComposeTestRule, isSelected: Boolean, position: Int) {
        homeScreenList().scrollIntoView(mDevice.findObject(UiSelector().resourceId("pocket.header")))

        if (isSelected) {
            composeTestRule.onNodeWithTag("pocket.categories").assertIsDisplayed()
            storyByTopicItem(composeTestRule, position).assertIsSelected()
        } else {
            composeTestRule.onNodeWithTag("pocket.categories").assertIsDisplayed()
            storyByTopicItem(composeTestRule, position).assertIsNotSelected()
        }
    }

    fun clickStoriesByTopicItem(composeTestRule: ComposeTestRule, position: Int) =
        storyByTopicItem(composeTestRule, position).performClick()

    fun verifyPoweredByPocket() {
        homeScreenList().scrollIntoView(mDevice.findObject(UiSelector().resourceId("pocket.header")))
        assertTrue(mDevice.findObject(UiSelector().resourceId("pocket.header.title")).exists())
    }

    fun verifyCustomizeHomepageButton(enabled: Boolean) {
        if (enabled) {
            scrollToElementByText(getStringResource(R.string.browser_menu_customize_home_1))
            assertTrue(
                mDevice.findObject(
                    UiSelector()
                        .textContains("Customize homepage"),
                ).waitForExists(waitingTime),
            )
        } else {
            homeScreenList().scrollToEnd(LISTS_MAXSWIPES)
            assertFalse(
                mDevice.findObject(
                    UiSelector()
                        .textContains("Customize homepage"),
                ).waitForExists(waitingTimeShort),
            )
        }
    }

    fun verifyJumpBackInMessage(composeTestRule: ComposeTestRule) =
        composeTestRule
            .onNodeWithText(
                getStringResource(R.string.onboarding_home_screen_jump_back_contextual_hint_2),
            ).assertExists()

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
        onView(withId(R.id.toolbarLayout))
            .check(
                if (defaultPosition) {
                    isPartiallyBelow(withId(R.id.sessionControlRecyclerView))
                } else {
                    isCompletelyAbove(withId(R.id.homeAppBar))
                },
            )
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

    class Transition {

        fun openTabDrawer(interact: TabDrawerRobot.() -> Unit): TabDrawerRobot.Transition {
            mDevice.findObject(
                UiSelector().descriptionContains("open tab. Tap to switch tabs."),
            ).waitForExists(waitingTime)

            tabsCounter().click()
            mDevice.waitNotNull(Until.findObject(By.res("$packageName:id/tab_layout")))

            TabDrawerRobot().interact()
            return TabDrawerRobot.Transition()
        }

        fun openComposeTabDrawer(composeTestRule: HomeActivityComposeTestRule, interact: ComposeTabDrawerRobot.() -> Unit): ComposeTabDrawerRobot.Transition {
            mDevice.waitForIdle(waitingTime)
            onView(withId(R.id.tab_button)).click()
            composeTestRule.onNodeWithTag(TabsTrayTestTag.tabsTray).assertExists()

            ComposeTabDrawerRobot(composeTestRule).interact()
            return ComposeTabDrawerRobot.Transition(composeTestRule)
        }

        fun openThreeDotMenu(interact: ThreeDotMenuMainRobot.() -> Unit): ThreeDotMenuMainRobot.Transition {
            // Issue: https://github.com/mozilla-mobile/fenix/issues/21578
            try {
                mDevice.waitNotNull(
                    Until.findObject(By.res("$packageName:id/menuButton")),
                    waitingTime,
                )
            } catch (e: AssertionError) {
                mDevice.pressBack()
            } finally {
                threeDotButton().perform(click())
            }

            ThreeDotMenuMainRobot().interact()
            return ThreeDotMenuMainRobot.Transition()
        }

        fun openSearch(interact: SearchRobot.() -> Unit): SearchRobot.Transition {
            navigationToolbar.waitForExists(waitingTime)
            navigationToolbar.click()
            mDevice.waitForIdle()

            SearchRobot().interact()
            return SearchRobot.Transition()
        }

        fun dismissOnboarding() {
            openThreeDotMenu { }.openSettings { }.goBack { }
        }

        fun clickUpgradingUserOnboardingSignInButton(
            testRule: ComposeTestRule,
            interact: SyncSignInRobot.() -> Unit,
        ): SyncSignInRobot.Transition {
            testRule.onNodeWithText("Sign in").performClick()

            SyncSignInRobot().interact()
            return SyncSignInRobot.Transition()
        }

        fun togglePrivateBrowsingMode() {
            if (
                !itemWithResIdAndDescription(
                    "$packageName:id/privateBrowsingButton",
                    "Disable private browsing",
                ).exists()
            ) {
                mDevice.findObject(UiSelector().resourceId("$packageName:id/privateBrowsingButton"))
                    .waitForExists(
                        waitingTime,
                    )
                privateBrowsingButton.click()
            }
        }

        fun triggerPrivateBrowsingShortcutPrompt(interact: AddToHomeScreenRobot.() -> Unit): AddToHomeScreenRobot.Transition {
            // Loop to press the PB icon for 5 times to display the Add the Private Browsing Shortcut CFR
            for (i in 1..5) {
                mDevice.findObject(UiSelector().resourceId("$packageName:id/privateBrowsingButton"))
                    .waitForExists(
                        waitingTime,
                    )

                privateBrowsingButton.click()
            }

            AddToHomeScreenRobot().interact()
            return AddToHomeScreenRobot.Transition()
        }

        fun pressBack() {
            onView(ViewMatchers.isRoot()).perform(ViewActions.pressBack())
        }

        fun openNavigationToolbar(interact: NavigationToolbarRobot.() -> Unit): NavigationToolbarRobot.Transition {
            mDevice.findObject(UiSelector().resourceId("$packageName:id/toolbar"))
                .waitForExists(waitingTime)
            navigationToolbar.click()

            NavigationToolbarRobot().interact()
            return NavigationToolbarRobot.Transition()
        }

        fun openContextMenuOnTopSitesWithTitle(
            title: String,
            interact: HomeScreenRobot.() -> Unit,
        ): Transition {
            onView(withId(R.id.top_sites_list)).perform(
                actionOnItem<RecyclerView.ViewHolder>(
                    hasDescendant(withText(title)),
                    ViewActions.longClick(),
                ),
            )

            HomeScreenRobot().interact()
            return Transition()
        }

        fun openContextMenuOnSponsoredShortcut(sponsoredShortcutTitle: String, interact: HomeScreenRobot.() -> Unit): Transition {
            sponsoredShortcut(sponsoredShortcutTitle).click(LONG_CLICK_DURATION)

            HomeScreenRobot().interact()
            return Transition()
        }

        fun openTopSiteTabWithTitle(
            title: String,
            interact: BrowserRobot.() -> Unit,
        ): BrowserRobot.Transition {
            onView(withId(R.id.top_sites_list)).perform(
                actionOnItem<RecyclerView.ViewHolder>(hasDescendant(withText(title)), click()),
            )

            BrowserRobot().interact()
            return BrowserRobot.Transition()
        }

        fun openSponsoredShortcut(sponsoredShortcutTitle: String, interact: BrowserRobot.() -> Unit): BrowserRobot.Transition {
            sponsoredShortcut(sponsoredShortcutTitle).click()

            BrowserRobot().interact()
            return BrowserRobot.Transition()
        }

        fun renameTopSite(title: String, interact: HomeScreenRobot.() -> Unit): Transition {
            onView(withText("Rename"))
                .check((matches(withEffectiveVisibility(Visibility.VISIBLE))))
                .perform(click())
            onView(Matchers.allOf(withId(R.id.top_site_title), instanceOf(EditText::class.java)))
                .perform(ViewActions.replaceText(title))
            onView(withId(android.R.id.button1)).perform((click()))

            HomeScreenRobot().interact()
            return Transition()
        }

        fun removeTopSite(interact: HomeScreenRobot.() -> Unit): Transition {
            onView(withText("Remove"))
                .check((matches(withEffectiveVisibility(Visibility.VISIBLE))))
                .perform(click())

            HomeScreenRobot().interact()
            return Transition()
        }

        fun deleteTopSiteFromHistory(interact: HomeScreenRobot.() -> Unit): Transition {
            mDevice.findObject(
                UiSelector().resourceId("$packageName:id/simple_text"),
            ).waitForExists(waitingTime)
            deleteFromHistory.click()

            HomeScreenRobot().interact()
            return Transition()
        }

        fun openTopSiteInPrivateTab(interact: BrowserRobot.() -> Unit): BrowserRobot.Transition {
            onView(withText("Open in private tab"))
                .check((matches(withEffectiveVisibility(Visibility.VISIBLE))))
                .perform(click())

            BrowserRobot().interact()
            return BrowserRobot.Transition()
        }

        fun clickSponsorsAndPrivacyButton(interact: BrowserRobot.() -> Unit): BrowserRobot.Transition {
            sponsorsAndPrivacyButton.waitForExists(waitingTime)
            sponsorsAndPrivacyButton.clickAndWaitForNewWindow(waitingTime)

            BrowserRobot().interact()
            return BrowserRobot.Transition()
        }

        fun clickSponsoredShortcutsSettingsButton(interact: SettingsSubMenuHomepageRobot.() -> Unit): SettingsSubMenuHomepageRobot.Transition {
            sponsoredShortcutsSettingsButton.waitForExists(waitingTime)
            sponsoredShortcutsSettingsButton.clickAndWaitForNewWindow(waitingTime)

            SettingsSubMenuHomepageRobot().interact()
            return SettingsSubMenuHomepageRobot.Transition()
        }

        fun openCommonMythsLink(interact: BrowserRobot.() -> Unit): BrowserRobot.Transition {
            mDevice.findObject(
                UiSelector()
                    .textContains(
                        getStringResource(R.string.private_browsing_common_myths),
                    ),
            ).also { it.click() }

            BrowserRobot().interact()
            return BrowserRobot.Transition()
        }

        fun clickSaveTabsToCollectionButton(interact: TabDrawerRobot.() -> Unit): TabDrawerRobot.Transition {
            scrollToElementByText(getStringResource(R.string.no_collections_description2))
            saveTabsToCollectionButton().click()

            TabDrawerRobot().interact()
            return TabDrawerRobot.Transition()
        }

        fun clickSaveTabsToCollectionButton(composeTestRule: HomeActivityComposeTestRule, interact: ComposeTabDrawerRobot.() -> Unit): ComposeTabDrawerRobot.Transition {
            scrollToElementByText(getStringResource(R.string.no_collections_description2))
            saveTabsToCollectionButton().click()

            ComposeTabDrawerRobot(composeTestRule).interact()
            return ComposeTabDrawerRobot.Transition(composeTestRule)
        }

        fun expandCollection(title: String, interact: CollectionRobot.() -> Unit): CollectionRobot.Transition {
            assertItemContainingTextExists(itemContainingText(title))
            itemContainingText(title).clickAndWaitForNewWindow(waitingTimeShort)
            assertItemWithDescriptionExists(itemWithDescription(getStringResource(R.string.remove_tab_from_collection)))

            CollectionRobot().interact()
            return CollectionRobot.Transition()
        }

        fun openRecentlyVisitedSearchGroupHistoryList(title: String, interact: HistoryRobot.() -> Unit): HistoryRobot.Transition {
            scrollToElementByText("Recently visited")
            val searchGroup = mDevice.findObject(UiSelector().text(title))
            searchGroup.waitForExists(waitingTimeShort)
            searchGroup.click()

            HistoryRobot().interact()
            return HistoryRobot.Transition()
        }

        fun openCustomizeHomepage(interact: SettingsSubMenuHomepageRobot.() -> Unit): SettingsSubMenuHomepageRobot.Transition {
            homeScreenList().scrollToEnd(LISTS_MAXSWIPES)
            mDevice.findObject(
                UiSelector()
                    .textContains(
                        "Customize homepage",
                    ),
            ).clickAndWaitForNewWindow(waitingTime)

            SettingsSubMenuHomepageRobot().interact()
            return SettingsSubMenuHomepageRobot.Transition()
        }

        fun clickJumpBackInShowAllButton(interact: TabDrawerRobot.() -> Unit): TabDrawerRobot.Transition {
            mDevice
                .findObject(
                    UiSelector()
                        .textContains(getStringResource(R.string.recent_tabs_show_all)),
                ).clickAndWaitForNewWindow(waitingTime)

            TabDrawerRobot().interact()
            return TabDrawerRobot.Transition()
        }

        fun clickJumpBackInShowAllButton(composeTestRule: HomeActivityComposeTestRule, interact: ComposeTabDrawerRobot.() -> Unit): ComposeTabDrawerRobot.Transition {
            mDevice
                .findObject(
                    UiSelector()
                        .textContains(getStringResource(R.string.recent_tabs_show_all)),
                ).clickAndWaitForNewWindow(waitingTime)

            ComposeTabDrawerRobot(composeTestRule).interact()
            return ComposeTabDrawerRobot.Transition(composeTestRule)
        }

        fun clickJumpBackInItemWithTitle(itemTitle: String, interact: BrowserRobot.() -> Unit): BrowserRobot.Transition {
            mDevice
                .findObject(
                    UiSelector()
                        .resourceId("recent.tab.title")
                        .textContains(itemTitle),
                ).clickAndWaitForNewWindow(waitingTime)

            BrowserRobot().interact()
            return BrowserRobot.Transition()
        }

        fun clickPocketStoryItem(publisher: String, position: Int, interact: BrowserRobot.() -> Unit): BrowserRobot.Transition {
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

            BrowserRobot().interact()
            return BrowserRobot.Transition()
        }

        fun clickPocketDiscoverMoreButton(interact: BrowserRobot.() -> Unit): BrowserRobot.Transition {
            pocketStoriesList
                .scrollIntoView(UiSelector().text("Discover more"))

            mDevice.findObject(UiSelector().text("Discover more")).also {
                it.waitForExists(waitingTimeShort)
                it.click()
            }

            BrowserRobot().interact()
            return BrowserRobot.Transition()
        }

        fun clickPocketLearnMoreLink(composeTestRule: ComposeTestRule, interact: BrowserRobot.() -> Unit): BrowserRobot.Transition {
            composeTestRule.onNodeWithTag("pocket.header.subtitle", true).performClick()

            BrowserRobot().interact()
            return BrowserRobot.Transition()
        }
    }
}

fun homeScreen(interact: HomeScreenRobot.() -> Unit): HomeScreenRobot.Transition {
    HomeScreenRobot().interact()
    return HomeScreenRobot.Transition()
}

private fun homeScreenList() =
    UiScrollable(
        UiSelector()
            .resourceId("$packageName:id/sessionControlRecyclerView")
            .scrollable(true),
    ).setAsVerticalList()

private fun assertKeyboardVisibility(isExpectedToBeVisible: Boolean) =
    Assert.assertEquals(
        isExpectedToBeVisible,
        mDevice
            .executeShellCommand("dumpsys input_method | grep mInputShown")
            .contains("mInputShown=true"),
    )

private fun assertTabButton() =
    onView(allOf(withId(R.id.tab_button), isDisplayed()))
        .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))

private fun assertCollectionsHeader() =
    onView(allOf(withText("Collections")))
        .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))

private fun assertNoCollectionsText() =
    onView(
        withText(
            containsString(
                "Collect the things that matter to you.\n" +
                    "Group together similar searches, sites, and tabs for quick access later.",
            ),
        ),
    ).check(matches(isDisplayed()))

private fun assertHomeComponent() =
    onView(ViewMatchers.withResourceName("sessionControlRecyclerView"))
        .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))

private fun threeDotButton() = onView(allOf(withId(R.id.menuButton)))

private fun assertExistingTopSitesList() =
    assertItemWithResIdExists(itemWithResId("$packageName:id/top_sites_list"))

private fun assertExistingTopSitesTabs(title: String) {
    mDevice.findObject(
        UiSelector()
            .resourceId("$packageName:id/top_site_title")
            .textContains(title),
    ).waitForExists(waitingTime)

    onView(allOf(withId(R.id.top_sites_list)))
        .check(matches(hasDescendant(withText(title))))
        .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))
}

private fun assertSponsoredShortcutLogoIsDisplayed(position: Int) =
    assertTrue(
        mDevice.findObject(
            UiSelector()
                .resourceId("$packageName:id/top_site_item")
                .index(position - 1),
        ).getChild(
            UiSelector()
                .resourceId("$packageName:id/favicon_card"),
        ).waitForExists(waitingTime),
    )

private fun assertSponsoredSubtitleIsDisplayed(position: Int) =
    assertTrue(
        mDevice.findObject(
            UiSelector()
                .resourceId("$packageName:id/top_site_item")
                .index(position - 1),
        ).getChild(
            UiSelector()
                .resourceId("$packageName:id/top_site_subtitle"),
        ).waitForExists(waitingTime),
    )

private fun assertSponsoredShortcutTitle(sponsoredShortcutTitle: String, position: Int) =
    assertTrue(
        mDevice.findObject(
            UiSelector()
                .resourceId("$packageName:id/top_site_item")
                .index(position - 1),
        ).getChild(
            UiSelector()
                .textContains(sponsoredShortcutTitle),
        ).waitForExists(waitingTime),
    )

private fun assertNotExistingTopSitesList(title: String) {
    mDevice.findObject(UiSelector().text(title)).waitUntilGone(waitingTime)

    assertFalse(
        mDevice.findObject(
            UiSelector()
                .resourceId("$packageName:id/top_site_title")
                .textContains(title),
        ).waitForExists(waitingTimeShort),
    )
}

private fun assertSponsoredTopSitesNotDisplayed() {
    assertFalse(
        mDevice.findObject(
            UiSelector()
                .resourceId("$packageName:id/top_site_subtitle")
                .textContains(getStringResource(R.string.top_sites_sponsored_label)),
        ).waitForExists(waitingTimeShort),
    )
}

private fun assertTopSiteContextMenuItems() {
    mDevice.waitNotNull(
        findObject(By.text("Open in private tab")),
        waitingTime,
    )
    mDevice.waitNotNull(
        findObject(By.text("Remove")),
        waitingTime,
    )
}

private fun assertJumpBackInSectionIsNotDisplayed() = assertFalse(jumpBackInSection().waitForExists(waitingTimeShort))

private fun assertJumpBackInItemTitle(testRule: ComposeTestRule, itemTitle: String) =
    testRule.onNodeWithTag("recent.tab.title", useUnmergedTree = true).assert(hasText(itemTitle))

private fun assertJumpBackInItemWithUrl(testRule: ComposeTestRule, itemUrl: String) =
    testRule.onNodeWithTag("recent.tab.url", useUnmergedTree = true).assert(hasText(itemUrl))

private fun assertJumpBackInShowAllButton() =
    assertTrue(
        mDevice
            .findObject(
                UiSelector()
                    .textContains(getStringResource(R.string.recent_tabs_show_all)),
            ).waitForExists(waitingTime),
    )

private fun assertRecentlyVisitedSectionIsDisplayed() = assertTrue(recentlyVisitedSection().waitForExists(waitingTime))

private fun assertRecentlyVisitedSectionIsNotDisplayed() = assertFalse(recentlyVisitedSection().waitForExists(waitingTimeShort))

private fun assertRecentBookmarksSectionIsDisplayed() =
    assertTrue(recentBookmarksSection().waitForExists(waitingTime))

private fun assertRecentBookmarksSectionIsNotDisplayed() =
    assertFalse(recentBookmarksSection().waitForExists(waitingTimeShort))

private fun assertPocketSectionIsDisplayed() = assertTrue(pocketSection().waitForExists(waitingTime))

private fun assertPocketSectionIsNotDisplayed() = assertFalse(pocketSection().waitForExists(waitingTimeShort))

private fun saveTabsToCollectionButton() = onView(withId(R.id.add_tabs_to_collections_button))

private fun tabsCounter() = onView(withId(R.id.tab_button))

private fun jumpBackInSection() =
    mDevice.findObject(UiSelector().textContains(getStringResource(R.string.recent_tabs_header)))

private fun recentlyVisitedSection() =
    mDevice.findObject(UiSelector().textContains(getStringResource(R.string.history_metadata_header_2)))

private fun recentBookmarksSection() =
    mDevice.findObject(UiSelector().textContains(getStringResource(R.string.recently_saved_title)))

private fun pocketSection() =
    mDevice.findObject(UiSelector().textContains(getStringResource(R.string.pocket_stories_header_1)))

private fun sponsoredShortcut(sponsoredShortcutTitle: String) =
    mDevice.findObject(
        By
            .res("$packageName:id/top_site_title")
            .textContains(sponsoredShortcutTitle),
    )

private fun storyByTopicItem(composeTestRule: ComposeTestRule, position: Int) =
    composeTestRule.onNodeWithTag("pocket.categories").onChildAt(position - 1)

private val homeScreen =
    itemWithResId("$packageName:id/homeLayout")
private val privateBrowsingButton =
    itemWithResId("$packageName:id/privateBrowsingButton")
private val homepageWordmark =
    itemWithResId("$packageName:id/wordmark")

private val navigationToolbar =
    itemWithResId("$packageName:id/toolbar")
private val menuButton =
    itemWithResId("$packageName:id/menuButton")
private fun tabCounter(numberOfOpenTabs: String) =
    itemWithResIdAndText("$packageName:id/counter_text", numberOfOpenTabs)

val deleteFromHistory =
    onView(
        allOf(
            withId(R.id.simple_text),
            withText(R.string.delete_from_history),
        ),
    ).inRoot(RootMatchers.isPlatformPopup())

private val recentlyVisitedList =
    UiScrollable(
        UiSelector()
            .className("android.widget.HorizontalScrollView"),
    ).setAsHorizontalList()

private val sponsoredShortcutsSettingsButton =
    mDevice
        .findObject(
            UiSelector()
                .textContains(getStringResource(R.string.top_sites_menu_settings))
                .resourceId("$packageName:id/simple_text"),
        )

private val sponsorsAndPrivacyButton =
    mDevice
        .findObject(
            UiSelector()
                .textContains(getStringResource(R.string.top_sites_menu_sponsor_privacy))
                .resourceId("$packageName:id/simple_text"),
        )

private val pocketStoriesList =
    UiScrollable(UiSelector().resourceId("pocket.stories")).setAsHorizontalList()
