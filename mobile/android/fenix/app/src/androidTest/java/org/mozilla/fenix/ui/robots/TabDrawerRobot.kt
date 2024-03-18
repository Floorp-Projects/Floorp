/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

@file:Suppress("TooManyFunctions")

package org.mozilla.fenix.ui.robots

import android.util.Log
import android.view.View
import androidx.test.espresso.Espresso.onView
import androidx.test.espresso.UiController
import androidx.test.espresso.ViewAction
import androidx.test.espresso.action.GeneralLocation
import androidx.test.espresso.action.ViewActions.click
import androidx.test.espresso.action.ViewActions.swipeLeft
import androidx.test.espresso.action.ViewActions.swipeRight
import androidx.test.espresso.assertion.ViewAssertions.doesNotExist
import androidx.test.espresso.assertion.ViewAssertions.matches
import androidx.test.espresso.matcher.RootMatchers
import androidx.test.espresso.matcher.ViewMatchers
import androidx.test.espresso.matcher.ViewMatchers.hasDescendant
import androidx.test.espresso.matcher.ViewMatchers.withContentDescription
import androidx.test.espresso.matcher.ViewMatchers.withEffectiveVisibility
import androidx.test.espresso.matcher.ViewMatchers.withId
import androidx.test.espresso.matcher.ViewMatchers.withText
import androidx.test.uiautomator.By
import androidx.test.uiautomator.By.text
import androidx.test.uiautomator.UiScrollable
import androidx.test.uiautomator.UiSelector
import androidx.test.uiautomator.Until.findObject
import com.google.android.material.bottomsheet.BottomSheetBehavior
import org.hamcrest.CoreMatchers.allOf
import org.hamcrest.CoreMatchers.anyOf
import org.hamcrest.CoreMatchers.containsString
import org.hamcrest.Matcher
import org.mozilla.fenix.R
import org.mozilla.fenix.helpers.Constants.LONG_CLICK_DURATION
import org.mozilla.fenix.helpers.Constants.RETRY_COUNT
import org.mozilla.fenix.helpers.Constants.TAG
import org.mozilla.fenix.helpers.DataGenerationHelper.getStringResource
import org.mozilla.fenix.helpers.MatcherHelper.assertUIObjectExists
import org.mozilla.fenix.helpers.MatcherHelper.assertUIObjectIsGone
import org.mozilla.fenix.helpers.MatcherHelper.itemContainingText
import org.mozilla.fenix.helpers.MatcherHelper.itemWithDescription
import org.mozilla.fenix.helpers.MatcherHelper.itemWithResId
import org.mozilla.fenix.helpers.MatcherHelper.itemWithResIdAndDescription
import org.mozilla.fenix.helpers.MatcherHelper.itemWithResIdContainingText
import org.mozilla.fenix.helpers.TestAssetHelper.waitingTime
import org.mozilla.fenix.helpers.TestAssetHelper.waitingTimeLong
import org.mozilla.fenix.helpers.TestAssetHelper.waitingTimeShort
import org.mozilla.fenix.helpers.TestHelper.mDevice
import org.mozilla.fenix.helpers.TestHelper.packageName
import org.mozilla.fenix.helpers.TestHelper.scrollToElementByText
import org.mozilla.fenix.helpers.click
import org.mozilla.fenix.helpers.clickAtLocationInView
import org.mozilla.fenix.helpers.ext.waitNotNull
import org.mozilla.fenix.helpers.idlingresource.BottomSheetBehaviorStateIdlingResource
import org.mozilla.fenix.helpers.isSelected
import org.mozilla.fenix.helpers.matchers.BottomSheetBehaviorHalfExpandedMaxRatioMatcher
import org.mozilla.fenix.helpers.matchers.BottomSheetBehaviorStateMatcher

/**
 * Implementation of Robot Pattern for the home screen menu.
 */
class TabDrawerRobot {

    fun verifyNormalBrowsingButtonIsSelected(isSelected: Boolean) {
        Log.i(TAG, "verifyNormalBrowsingButtonIsSelected: Trying to verify that the normal browsing button is selected: $isSelected")
        normalBrowsingButton().check(matches(isSelected(isSelected)))
        Log.i(TAG, "verifyNormalBrowsingButtonIsSelected: Verified that the normal browsing button is selected: $isSelected")
    }

    fun verifyPrivateBrowsingButtonIsSelected(isSelected: Boolean) {
        Log.i(TAG, "verifyPrivateBrowsingButtonIsSelected: Trying to verify that the private browsing button is selected: $isSelected")
        privateBrowsingButton().check(matches(isSelected(isSelected)))
        Log.i(TAG, "verifyPrivateBrowsingButtonIsSelected: Verified that the private browsing button is selected: $isSelected")
    }

    fun verifySyncedTabsButtonIsSelected(isSelected: Boolean) {
        Log.i(TAG, "verifySyncedTabsButtonIsSelected: Trying to verify that the synced tabs button is selected: $isSelected")
        syncedTabsButton().check(matches(isSelected(isSelected)))
        Log.i(TAG, "verifySyncedTabsButtonIsSelected: Verified that the synced tabs button is selected: $isSelected")
    }

    fun clickSyncedTabsButton() {
        Log.i(TAG, "clickSyncedTabsButton: Trying to click the synced tabs button")
        syncedTabsButton().click()
        Log.i(TAG, "clickSyncedTabsButton: Clicked the synced tabs button")
    }

    fun verifyExistingOpenTabs(vararg tabTitles: String) {
        var retries = 0

        for (title in tabTitles) {
            while (!tabItem(title).waitForExists(waitingTime) && retries++ < 3) {
                tabsList()
                    .getChildByText(UiSelector().text(title), title, true)
                assertUIObjectExists(tabItem(title), waitingTime = waitingTimeLong)
            }
        }
    }

    fun verifyOpenTabsOrder(position: Int, title: String) {
        Log.i(TAG, "verifyOpenTabsOrder: Trying to verify that the open tab at position: $position has title: $title")
        mDevice.findObject(
            UiSelector()
                .resourceId("$packageName:id/tab_item")
                .childSelector(
                    UiSelector().textContains(title),
                ),
        ).getFromParent(
            UiSelector()
                .resourceId("$packageName:id/tab_tray_grid_item")
                .index(position - 1),
        )
        Log.i(TAG, "verifyOpenTabsOrder: Verified that the open tab at position: $position has title: $title")
    }
    fun verifyNoExistingOpenTabs(vararg tabTitles: String) {
        for (title in tabTitles) {
            assertUIObjectExists(tabItem(title), exists = false)
        }
    }
    fun verifyCloseTabsButton(title: String) =
        assertUIObjectExists(itemWithDescription("Close tab").getFromParent(UiSelector().textContains(title)))

    fun verifyExistingTabList() {
        Log.i(TAG, "verifyExistingTabList: Waiting for $waitingTime ms for tab tray to exist")
        mDevice.findObject(
            UiSelector().resourceId("$packageName:id/tabsTray"),
        ).waitForExists(waitingTime)
        Log.i(TAG, "verifyExistingTabList: Waited for $waitingTime ms for tab tray to exist")
        assertUIObjectExists(itemWithResId("$packageName:id/tray_list_item"))
    }

    fun verifyNoOpenTabsInNormalBrowsing() {
        Log.i(TAG, "verifyNoOpenTabsInNormalBrowsing: Trying to verify that the empty normal tabs list is visible")
        onView(
            allOf(
                withId(R.id.tab_tray_empty_view),
                withText(R.string.no_open_tabs_description),
            ),
        ).check(matches(withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)))
        Log.i(TAG, "verifyNoOpenTabsInNormalBrowsing: Verified that the empty normal tabs list is visible")
    }

    fun verifyNoOpenTabsInPrivateBrowsing() {
        Log.i(TAG, "verifyNoOpenTabsInPrivateBrowsing: Trying to verify that the empty private tabs list is visible")
        onView(
            allOf(
                withId(R.id.tab_tray_empty_view),
                withText(R.string.no_private_tabs_description),
            ),
        ).check(matches(withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)))
        Log.i(TAG, "verifyNoOpenTabsInPrivateBrowsing: Verified that the empty private tabs list is visible")
    }

    fun verifyPrivateModeSelected() {
        Log.i(TAG, "verifyPrivateModeSelected: Trying to verify that the private browsing button is selected")
        privateBrowsingButton().check(matches(ViewMatchers.isSelected()))
        Log.i(TAG, "verifyPrivateModeSelected: Verified that the private browsing button is selected")
    }

    fun verifyNormalModeSelected() {
        Log.i(TAG, "verifyNormalModeSelected: Trying to verify that the normal browsing button is selected")
        normalBrowsingButton().check(matches(ViewMatchers.isSelected()))
        Log.i(TAG, "verifyNormalModeSelected: Verified that the normal browsing button is selected")
    }

    fun verifyNormalBrowsingNewTabButton() {
        Log.i(TAG, "verifyNormalBrowsingNewTabButton: Trying to verify that the new tab FAB button is visible")
        onView(
            allOf(
                withId(R.id.new_tab_button),
                withContentDescription(R.string.add_tab),
            ),
        ).check(matches(withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)))
        Log.i(TAG, "verifyNormalBrowsingNewTabButton: Verified that the new tab FAB button is visible")
    }

    fun verifyPrivateBrowsingNewTabButton() {
        Log.i(TAG, "verifyPrivateBrowsingNewTabButton: Trying to verify that the new private tab FAB button is visible")
        onView(
            allOf(
                withId(R.id.new_tab_button),
                withContentDescription(R.string.add_private_tab),
            ),
        ).check(matches(withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)))
        Log.i(TAG, "verifyPrivateBrowsingNewTabButton: Verified that the new private tab FAB button is visible")
    }

    fun verifyEmptyTabsTrayMenuButtons() {
        Log.i(TAG, "verifyEmptyTabsTrayMenuButtons: Trying to click the three dot button")
        threeDotMenu().click()
        Log.i(TAG, "verifyEmptyTabsTrayMenuButtons: Clicked the three dot button")
        Log.i(TAG, "verifyEmptyTabsTrayMenuButtons: Trying to verify that the \"Tab settings\" menu button is visible")
        tabsSettingsButton()
            .inRoot(RootMatchers.isPlatformPopup())
            .check(matches(withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)))
        Log.i(TAG, "verifyEmptyTabsTrayMenuButtons: Verified that the \"Tab settings\" menu button is visible")
        Log.i(TAG, "verifyEmptyTabsTrayMenuButtons: Trying to verify that the \"Recently closed tabs\" menu button is visible")
        recentlyClosedTabsButton()
            .inRoot(RootMatchers.isPlatformPopup())
            .check(matches(withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)))
        Log.i(TAG, "verifyEmptyTabsTrayMenuButtons: Verified that the \"Recently closed tabs\" menu button exists")
    }

    fun verifyTabTrayOverflowMenu(visibility: Boolean) {
        Log.i(TAG, "verifyTabTrayOverflowMenu: Trying to verify that the three dot menu is visible: $visibility")
        onView(withId(R.id.tab_tray_overflow)).check(
            matches(
                withEffectiveVisibility(
                    visibleOrGone(
                        visibility,
                    ),
                ),
            ),
        )
        Log.i(TAG, "verifyTabTrayOverflowMenu: Verified that the three dot menu is visible: $visibility")
    }
    fun verifyTabsTrayCounter() {
        Log.i(TAG, "verifyTabsTrayCounter: Trying to verify that the tabs list counter is visible")
        tabsTrayCounterBox().check(matches(withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)))
        Log.i(TAG, "verifyTabsTrayCounter: Verified that the tabs list counter is visible")
    }

    fun verifyTabTrayIsOpened() {
        Log.i(TAG, "verifyTabTrayIsOpened: Trying to verify that the tabs tray is visible")
        onView(withId(R.id.tab_wrapper)).check(matches(withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)))
        Log.i(TAG, "verifyTabTrayIsOpened: Verified that the tabs tray is visible")
    }
    fun verifyTabTrayIsClosed() {
        Log.i(TAG, "verifyTabTrayIsClosed: Trying to verify that the tabs tray does not exist")
        onView(withId(R.id.tab_wrapper)).check(doesNotExist())
        Log.i(TAG, "verifyTabTrayIsClosed: Verified that the tabs tray does not exist")
    }
    fun verifyHalfExpandedRatio() {
        Log.i(TAG, "verifyHalfExpandedRatio: Trying to verify the tabs tray half expanded ratio")
        onView(withId(R.id.tab_wrapper)).check(
            matches(
                BottomSheetBehaviorHalfExpandedMaxRatioMatcher(0.001f),
            ),
        )
        Log.i(TAG, "verifyHalfExpandedRatio: Verified the tabs tray half expanded ratio")
    }
    fun verifyBehaviorState(expectedState: Int) {
        Log.i(TAG, "verifyBehaviorState: Trying to verify that the tabs tray state matches: $expectedState")
        onView(withId(R.id.tab_wrapper)).check(matches(BottomSheetBehaviorStateMatcher(expectedState)))
        Log.i(TAG, "verifyBehaviorState: Verified that the tabs tray state matches: $expectedState")
    }
    fun verifyOpenedTabThumbnail() =
        assertUIObjectExists(itemWithResId("$packageName:id/mozac_browser_tabstray_thumbnail"))

    fun closeTab() {
        Log.i(TAG, "closeTab: Waiting for $waitingTime ms for the close tab button to exist")
        closeTabButton().waitForExists(waitingTime)
        Log.i(TAG, "closeTab: Waited for $waitingTime ms for the close tab button to exist")

        var retries = 0 // number of retries before failing, will stop at 2
        do {
            Log.i(TAG, "closeTab: Trying to click the close tab button")
            closeTabButton().click()
            Log.i(TAG, "closeTab: Clicked the close tab button")
            retries++
        } while (closeTabButton().exists() && retries < 3)
    }

    fun closeTabWithTitle(title: String) {
        itemWithResIdAndDescription(
            "$packageName:id/mozac_browser_tabstray_close",
            "Close tab $title",
        ).also {
            Log.i(TAG, "closeTabWithTitle: Waiting for $waitingTime ms for the close button for tab with title: $title to exist")
            it.waitForExists(waitingTime)
            Log.i(TAG, "closeTabWithTitle: Waited for $waitingTime ms for the close button for tab with title: $title to exist")
            Log.i(TAG, "closeTabWithTitle: Trying to click the close button for tab with title: $title")
            it.click()
            Log.i(TAG, "closeTabWithTitle: Clicked the close button for tab with title: $title")
        }
    }

    fun swipeTabRight(title: String) {
        for (i in 1..RETRY_COUNT) {
            Log.i(TAG, "swipeTabRight: Started try #$i")
            try {
                Log.i(TAG, "swipeTabRight: Trying to perform swipe right action on tab: $title")
                onView(
                    allOf(
                        withId(R.id.tab_item),
                        hasDescendant(
                            allOf(
                                withId(R.id.mozac_browser_tabstray_title),
                                withText(title),
                            ),
                        ),
                    ),
                ).perform(swipeRight())
                Log.i(TAG, "swipeTabRight: Performed swipe right action on tab: $title")
                assertUIObjectIsGone(
                    itemWithResIdContainingText(
                        "$packageName:id/mozac_browser_tabstray_title",
                        title,
                    ),
                )

                break
            } catch (e: AssertionError) {
                Log.i(TAG, "swipeTabRight: AssertionError caught, executing fallback methods")
                if (i == RETRY_COUNT) {
                    throw e
                }
            }
        }
    }

    fun swipeTabLeft(title: String) {
        for (i in 1..RETRY_COUNT) {
            try {
                Log.i(TAG, "swipeTabLeft: Trying to perform swipe left action on tab: $title")
                onView(
                    allOf(
                        withId(R.id.tab_item),
                        hasDescendant(
                            allOf(
                                withId(R.id.mozac_browser_tabstray_title),
                                withText(title),
                            ),
                        ),
                    ),
                ).perform(swipeLeft())
                Log.i(TAG, "swipeTabLeft: Performed swipe left action on tab: $title")
                assertUIObjectIsGone(
                    itemWithResIdContainingText(
                        "$packageName:id/mozac_browser_tabstray_title",
                        title,
                    ),
                )

                break
            } catch (e: AssertionError) {
                Log.i(TAG, "swipeTabLeft: AssertionError caught, executing fallback methods")
                if (i == RETRY_COUNT) {
                    throw e
                }
            }
        }
    }

    fun snackBarButtonClick(expectedText: String) {
        mDevice.findObject(
            UiSelector()
                .resourceId("$packageName:id/snackbar_btn")
                .text(expectedText),
        ).also {
            Log.i(TAG, "snackBarButtonClick: Waiting for $waitingTime ms for the snack bar button: $expectedText to exist")
            it.waitForExists(waitingTime)
            Log.i(TAG, "snackBarButtonClick: Waited for $waitingTime ms for the snack bar button: $expectedText to exist")
            Log.i(TAG, "snackBarButtonClick: Trying to click the $expectedText snack bar button")
            it.click()
            Log.i(TAG, "snackBarButtonClick: Clicked the $expectedText snack bar button")
        }
    }

    fun verifyTabMediaControlButtonState(action: String) = assertUIObjectExists(tabMediaControlButton(action))

    fun clickTabMediaControlButton(action: String) {
        tabMediaControlButton(action).also {
            Log.i(TAG, "clickTabMediaControlButton: Waiting for $waitingTime ms for the media tab control button: $action to exist")
            it.waitForExists(waitingTime)
            Log.i(TAG, "clickTabMediaControlButton: Waited for $waitingTime ms for the media tab control button: $action to exists")
            Log.i(TAG, "clickTabMediaControlButton: Trying to click the tab media control button: $action")
            it.click()
            Log.i(TAG, "clickTabMediaControlButton: Clicked the tab media control button: $action")
        }
    }

    fun clickSelectTabsOption() {
        Log.i(TAG, "clickSelectTabsOption: Trying to click the three dot button")
        threeDotMenu().click()
        Log.i(TAG, "clickSelectTabsOption: Clicked the three dot button")
        mDevice.findObject(UiSelector().text("Select tabs")).also {
            Log.i(TAG, "clickSelectTabsOption: Waiting for $waitingTime ms for the the \"Select tabs\" menu button to exist")
            it.waitForExists(waitingTime)
            Log.i(TAG, "clickSelectTabsOption: Waited for $waitingTime ms for the the \"Select tabs\" menu button to exists")
            Log.i(TAG, "clickSelectTabsOption: Trying to click the \"Select tabs\" menu button")
            it.click()
            Log.i(TAG, "clickSelectTabsOption: Clicked the \"Select tabs\" menu button")
        }
    }

    fun selectTab(title: String, numOfTabs: Int) {
        val tabsSelected =
            mDevice.findObject(UiSelector().text("$numOfTabs selected"))
        var retries = 0 // number of retries before failing

        while (!tabsSelected.exists() && retries++ < 3) {
            Log.i(TAG, "selectTab: Waiting for $waitingTime ms for tab with title: $title to exist")
            tabItem(title).waitForExists(waitingTime)
            Log.i(TAG, "selectTab: Waited for $waitingTime ms for tab with title: $title to exist")
            Log.i(TAG, "selectTab: Trying to click tab with title: $title")
            tabItem(title).click()
            Log.i(TAG, "selectTab: Clicked tab with title: $title")
        }
    }

    fun longClickTab(title: String) {
        mDevice.waitNotNull(
            findObject(text(title)),
            waitingTime,
        )
        Log.i(TAG, "longClickTab: Trying to long click tab with title: $title")
        mDevice.findObject(
            By
                .textContains(title)
                .res("$packageName:id/mozac_browser_tabstray_title"),
        ).click(LONG_CLICK_DURATION)
        Log.i(TAG, "longClickTab: Long clicked tab with title: $title")
    }

    fun createCollection(
        vararg tabTitles: String,
        collectionName: String,
        firstCollection: Boolean = true,
    ) {
        tabDrawer {
            clickSelectTabsOption()
            for (tab in tabTitles) {
                selectTab(tab, tabTitles.indexOf(tab) + 1)
            }
        }.clickSaveCollection {
            if (!firstCollection) {
                clickAddNewCollection()
            }
            typeCollectionNameAndSave(collectionName)
        }
    }

    fun verifyTabsMultiSelectionCounter(numOfTabs: Int) =
        assertUIObjectExists(
            itemWithResId("$packageName:id/multiselect_title"),
            itemContainingText("$numOfTabs selected"),
        )

    fun verifySyncedTabsListWhenUserIsNotSignedIn() =
        assertUIObjectExists(
            itemWithResId("$packageName:id/tabsTray"),
            itemContainingText(getStringResource(R.string.synced_tabs_sign_in_message)),
            itemContainingText(getStringResource(R.string.sync_sign_in)),
        )

    class Transition {
        fun openTabDrawer(interact: TabDrawerRobot.() -> Unit): Transition {
            Log.i(TAG, "openTabDrawer: Waiting for device to be idle for $waitingTime ms")
            mDevice.waitForIdle(waitingTime)
            Log.i(TAG, "openTabDrawer: Waited for device to be idle for $waitingTime ms")
            Log.i(TAG, "openTabDrawer: Trying to click the tab counter button")
            tabsCounter().click()
            Log.i(TAG, "openTabDrawer: Clicked the tab counter button")
            mDevice.waitNotNull(
                findObject(By.res("$packageName:id/tab_layout")),
                waitingTime,
            )

            TabDrawerRobot().interact()
            return Transition()
        }

        fun closeTabDrawer(interact: BrowserRobot.() -> Unit): BrowserRobot.Transition {
            Log.i(TAG, "closeTabDrawer: Waiting for device to be idle for $waitingTime ms")
            mDevice.waitForIdle(waitingTime)
            Log.i(TAG, "closeTabDrawer: Waited for device to be idle for $waitingTime ms")
            Log.i(TAG, "closeTabDrawer: Trying to click the tabs tray handler")
            onView(withId(R.id.handle)).perform(
                click(),
            )
            Log.i(TAG, "closeTabDrawer: Clicked the tabs tray handler")
            BrowserRobot().interact()
            return BrowserRobot.Transition()
        }

        fun openNewTab(interact: SearchRobot.() -> Unit): SearchRobot.Transition {
            Log.i(TAG, "openNewTab: Waiting for device to be idle")
            mDevice.waitForIdle()
            Log.i(TAG, "openNewTab: Waited for device to be idle")
            Log.i(TAG, "openNewTab: Trying to click the new tab FAB button")
            newTabButton().click()
            Log.i(TAG, "openNewTab: Clicked the new tab FAB button")
            SearchRobot().interact()
            return SearchRobot.Transition()
        }

        fun toggleToNormalTabs(interact: TabDrawerRobot.() -> Unit): Transition {
            Log.i(TAG, "toggleToNormalTabs: Trying to click the normal browsing button")
            normalBrowsingButton().perform(click())
            Log.i(TAG, "toggleToNormalTabs: Clicked the normal browsing button")
            TabDrawerRobot().interact()
            return Transition()
        }

        fun toggleToPrivateTabs(interact: TabDrawerRobot.() -> Unit): Transition {
            Log.i(TAG, "toggleToPrivateTabs: Trying to click the private browsing button")
            privateBrowsingButton().perform(click())
            Log.i(TAG, "toggleToPrivateTabs: Clicked the private browsing button")
            TabDrawerRobot().interact()
            return Transition()
        }

        fun toggleToSyncedTabs(interact: TabDrawerRobot.() -> Unit): Transition {
            Log.i(TAG, "toggleToSyncedTabs: Trying to click the synced tabs button")
            syncedTabsButton().perform(click())
            Log.i(TAG, "toggleToSyncedTabs: Clicked the synced tabs button")
            TabDrawerRobot().interact()
            return Transition()
        }

        fun clickSignInToSyncButton(interact: SyncSignInRobot.() -> Unit): Transition {
            Log.i(TAG, "clickSignInToSyncButton: Trying to click the sign in to sync button and wait for $waitingTimeShort ms for a new window")
            itemContainingText(getStringResource(R.string.sync_sign_in))
                .clickAndWaitForNewWindow(waitingTimeShort)
            Log.i(TAG, "clickSignInToSyncButton: Clicked the sign in to sync button and waited for $waitingTimeShort ms for a new window")
            SyncSignInRobot().interact()
            return Transition()
        }

        fun openTabsListThreeDotMenu(interact: ThreeDotMenuMainRobot.() -> Unit): ThreeDotMenuMainRobot.Transition {
            Log.i(TAG, "openTabsListThreeDotMenu: Trying to click the three dot button")
            threeDotMenu().perform(click())
            Log.i(TAG, "openTabsListThreeDotMenu: Clicked three dot button")

            ThreeDotMenuMainRobot().interact()
            return ThreeDotMenuMainRobot.Transition()
        }

        fun openTab(title: String, interact: BrowserRobot.() -> Unit): BrowserRobot.Transition {
            scrollToElementByText(title)
            Log.i(TAG, "openTab: Waiting for $waitingTime ms for tab with title: $title to exist")
            tabItem(title).waitForExists(waitingTime)
            Log.i(TAG, "openTab: Waited for $waitingTime ms for tab with title: $title to exist")
            Log.i(TAG, "openTab: Trying to click tab with title: $title")
            tabItem(title).click()
            Log.i(TAG, "openTab: Clicked tab with title: $title")

            BrowserRobot().interact()
            return BrowserRobot.Transition()
        }

        // Temporary method to use indexes instead of tab titles, until the compose migration is complete
        fun openTabWithIndex(
            tabPosition: Int,
            interact: BrowserRobot.() -> Unit,
        ): BrowserRobot.Transition {
            mDevice.findObject(
                UiSelector()
                    .resourceId("$packageName:id/tab_tray_grid_item")
                    .index(tabPosition),
            ).also {
                Log.i(TAG, "openTabWithIndex: Waiting for $waitingTime ms for tab at position: ${tabPosition + 1} to exist")
                it.waitForExists(waitingTime)
                Log.i(TAG, "openTabWithIndex: Waited for $waitingTime ms for tab at position: ${tabPosition + 1} to exist")
                Log.i(TAG, "openTabWithIndex: Trying to click tab at position: ${tabPosition + 1}")
                it.click()
                Log.i(TAG, "openTabWithIndex: Clicked tab at position: ${tabPosition + 1}")
            }

            BrowserRobot().interact()
            return BrowserRobot.Transition()
        }

        fun clickTopBar(interact: TabDrawerRobot.() -> Unit): Transition {
            // The topBar contains other views.
            // Don't do the default click in the middle, rather click in some free space - top right.
            Log.i(TAG, "clickTopBar: Trying to click the tabs tray top bar")
            onView(withId(R.id.topBar)).clickAtLocationInView(GeneralLocation.TOP_RIGHT)
            Log.i(TAG, "clickTopBar: Clicked the tabs tray top bar")
            TabDrawerRobot().interact()
            return Transition()
        }

        fun advanceToHalfExpandedState(interact: TabDrawerRobot.() -> Unit): Transition {
            onView(withId(R.id.tab_wrapper)).perform(
                object : ViewAction {
                    override fun getDescription(): String {
                        return "Advance a BottomSheetBehavior to STATE_HALF_EXPANDED"
                    }

                    override fun getConstraints(): Matcher<View> {
                        return ViewMatchers.isAssignableFrom(View::class.java)
                    }

                    override fun perform(uiController: UiController?, view: View?) {
                        val behavior = BottomSheetBehavior.from(view!!)
                        behavior.state = BottomSheetBehavior.STATE_HALF_EXPANDED
                    }
                },
            )
            TabDrawerRobot().interact()
            return Transition()
        }

        fun waitForTabTrayBehaviorToIdle(interact: TabDrawerRobot.() -> Unit): Transition {
            // Need to get the behavior of tab_wrapper and wait for that to idle.
            var behavior: BottomSheetBehavior<*>? = null

            // Null check here since it's possible that the view is already animated away from the screen.
            onView(withId(R.id.tab_wrapper))?.perform(
                object : ViewAction {
                    override fun getDescription(): String {
                        return "Postpone actions to after the BottomSheetBehavior has settled"
                    }

                    override fun getConstraints(): Matcher<View> {
                        return ViewMatchers.isAssignableFrom(View::class.java)
                    }

                    override fun perform(uiController: UiController?, view: View?) {
                        behavior = BottomSheetBehavior.from(view!!)
                    }
                },
            )

            behavior?.let {
                runWithIdleRes(BottomSheetBehaviorStateIdlingResource(it)) {
                    TabDrawerRobot().interact()
                }
            }

            return Transition()
        }

        fun clickSaveCollection(interact: CollectionRobot.() -> Unit): CollectionRobot.Transition {
            Log.i(TAG, "clickSaveCollection: Trying to click the collections button")
            saveTabsToCollectionButton().click()
            Log.i(TAG, "clickSaveCollection: Clicked the collections button")

            CollectionRobot().interact()
            return CollectionRobot.Transition()
        }
    }
}

fun tabDrawer(interact: TabDrawerRobot.() -> Unit): TabDrawerRobot.Transition {
    TabDrawerRobot().interact()
    return TabDrawerRobot.Transition()
}

private fun tabMediaControlButton(action: String) =
    mDevice.findObject(UiSelector().descriptionContains(action))

private fun closeTabButton() =
    mDevice.findObject(UiSelector().descriptionContains("Close tab"))

private fun normalBrowsingButton() = onView(
    anyOf(
        withContentDescription(containsString("open tabs. Tap to switch tabs.")),
        withContentDescription(containsString("open tab. Tap to switch tabs.")),
    ),
)

private fun privateBrowsingButton() = onView(withContentDescription("Private tabs"))
private fun syncedTabsButton() = onView(withContentDescription("Synced tabs"))
private fun newTabButton() =
    mDevice.findObject(UiSelector().resourceId("$packageName:id/new_tab_button"))

private fun threeDotMenu() = onView(withId(R.id.tab_tray_overflow))

private fun tabsList() =
    UiScrollable(UiSelector().className("androidx.recyclerview.widget.RecyclerView"))

// This tab selector is used for actions that involve waiting and asserting the existence of the view
private fun tabItem(title: String) =
    mDevice.findObject(
        UiSelector()
            .textContains(title),
    )

private fun tabsCounter() = onView(withId(R.id.tab_button))

private fun tabsTrayCounterBox() = onView(withId(R.id.counter_box))

private fun tabsSettingsButton() =
    onView(
        allOf(
            withId(R.id.simple_text),
            withText(R.string.tab_tray_menu_tab_settings),
        ),
    )

private fun recentlyClosedTabsButton() =
    onView(
        allOf(
            withId(R.id.simple_text),
            withText(R.string.tab_tray_menu_recently_closed),
        ),
    )

private fun visibleOrGone(visibility: Boolean) =
    if (visibility) ViewMatchers.Visibility.VISIBLE else ViewMatchers.Visibility.GONE

private fun saveTabsToCollectionButton() = onView(withId(R.id.collect_multi_select))
