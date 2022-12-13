/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

@file:Suppress("TooManyFunctions")

package org.mozilla.fenix.ui.robots

import android.content.Context
import android.view.View
import androidx.test.core.app.ApplicationProvider
import androidx.test.espresso.Espresso
import androidx.test.espresso.Espresso.onView
import androidx.test.espresso.UiController
import androidx.test.espresso.ViewAction
import androidx.test.espresso.action.GeneralLocation
import androidx.test.espresso.action.ViewActions
import androidx.test.espresso.action.ViewActions.click
import androidx.test.espresso.action.ViewActions.longClick
import androidx.test.espresso.assertion.ViewAssertions.doesNotExist
import androidx.test.espresso.assertion.ViewAssertions.matches
import androidx.test.espresso.matcher.RootMatchers
import androidx.test.espresso.matcher.ViewMatchers
import androidx.test.espresso.matcher.ViewMatchers.withContentDescription
import androidx.test.espresso.matcher.ViewMatchers.withEffectiveVisibility
import androidx.test.espresso.matcher.ViewMatchers.withId
import androidx.test.espresso.matcher.ViewMatchers.withText
import androidx.test.uiautomator.By
import androidx.test.uiautomator.By.text
import androidx.test.uiautomator.UiScrollable
import androidx.test.uiautomator.UiSelector
import androidx.test.uiautomator.Until
import androidx.test.uiautomator.Until.findObject
import com.google.android.material.bottomsheet.BottomSheetBehavior
import junit.framework.TestCase.assertFalse
import junit.framework.TestCase.assertTrue
import org.hamcrest.CoreMatchers.allOf
import org.hamcrest.CoreMatchers.anyOf
import org.hamcrest.CoreMatchers.containsString
import org.hamcrest.Matcher
import org.mozilla.fenix.R
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

    fun verifyBrowserTabsTrayURL(url: String) {
        mDevice.waitNotNull(
            Until.findObject(By.res("$packageName:id/mozac_browser_tabstray_url")),
            waitingTime,
        )
        onView(withId(R.id.mozac_browser_tabstray_url))
            .check(matches(withText(containsString(url))))
    }

    fun verifyNormalBrowsingButtonIsDisplayed() = assertNormalBrowsingButton()
    fun verifyNormalBrowsingButtonIsSelected(isSelected: Boolean) =
        assertNormalBrowsingButtonIsSelected(isSelected)

    fun verifyPrivateBrowsingButtonIsSelected(isSelected: Boolean) =
        assertPrivateBrowsingButtonIsSelected(isSelected)

    fun verifySyncedTabsButtonIsSelected(isSelected: Boolean) =
        assertSyncedTabsButtonIsSelected(isSelected)

    fun verifyExistingOpenTabs(vararg titles: String) = assertExistingOpenTabs(*titles)
    fun verifyNoExistingOpenTabs(vararg titles: String) = assertNoExistingOpenTabs(*titles)
    fun verifyCloseTabsButton(title: String) = assertCloseTabsButton(title)

    fun verifyExistingTabList() = assertExistingTabList()

    fun verifyNoOpenTabsInNormalBrowsing() = assertNoOpenTabsInNormalBrowsing()
    fun verifyNoOpenTabsInPrivateBrowsing() = assertNoOpenTabsInPrivateBrowsing()
    fun verifyPrivateModeSelected() = assertPrivateModeSelected()
    fun verifyNormalModeSelected() = assertNormalModeSelected()
    fun verifyNormalBrowsingNewTabButton() = assertNormalBrowsingNewTabButton()
    fun verifyPrivateBrowsingNewTabButton() = assertPrivateBrowsingNewTabButton()
    fun verifyEmptyTabsTrayMenuButtons() = assertEmptyTabsTrayMenuButtons()
    fun verifySelectTabsButton() = assertSelectTabsButton()
    fun verifyTabTrayOverflowMenu(visibility: Boolean) = assertTabTrayOverflowButton(visibility)
    fun verifyTabsTrayCounter() = assertTabsTrayCounter()

    fun verifyTabTrayIsOpened() = assertTabTrayDoesExist()
    fun verifyTabTrayIsClosed() = assertTabTrayDoesNotExist()
    fun verifyHalfExpandedRatio() = assertMinisculeHalfExpandedRatio()
    fun verifyBehaviorState(expectedState: Int) = assertBehaviorState(expectedState)
    fun verifyOpenedTabThumbnail() = assertTabThumbnail()

    fun closeTab() {
        closeTabButton().waitForExists(waitingTime)

        var retries = 0 // number of retries before failing, will stop at 2
        do {
            closeTabButton().click()
            retries++
        } while (closeTabButton().exists() && retries < 3)
    }

    fun swipeTabRight(title: String) {
        var retries = 0 // number of retries before failing, will stop at 2
        while (!tabItem(title).waitUntilGone(waitingTimeShort) && retries < 3
        ) {
            tab(title).perform(ViewActions.swipeRight())
            retries++
        }
    }

    fun swipeTabLeft(title: String) {
        var retries = 0 // number of retries before failing, will stop at 2
        while (!tabItem(title).waitUntilGone(waitingTimeShort) && retries < 3
        ) {
            tab(title).perform(ViewActions.swipeLeft())
            retries++
        }
    }

    fun verifySnackBarText(expectedText: String) {
        assertTrue(
            mDevice.findObject(
                UiSelector().text(expectedText),
            ).waitForExists(waitingTime),
        )
    }

    fun snackBarButtonClick(expectedText: String) {
        val snackBarButton =
            mDevice.findObject(
                UiSelector()
                    .resourceId("$packageName:id/snackbar_btn")
                    .text(expectedText),
            )

        snackBarButton.waitForExists(waitingTime)
        snackBarButton.click()
    }

    fun verifyTabMediaControlButtonState(action: String) =
        assertTrue(tabMediaControlButton(action).waitForExists(waitingTime))

    fun clickTabMediaControlButton(action: String) {
        tabMediaControlButton(action).also {
            it.waitForExists(waitingTime)
            it.click()
        }
    }

    fun clickSelectTabsOption() {
        threeDotMenu().click()

        val selectTabsButton = mDevice.findObject(UiSelector().text("Select tabs"))
        selectTabsButton.waitForExists(waitingTime)
        selectTabsButton.click()
    }

    fun selectTab(title: String, numOfTabs: Int) {
        val tabsSelected =
            mDevice.findObject(UiSelector().text("$numOfTabs selected"))
        var retries = 0 // number of retries before failing

        while (!tabsSelected.exists() && retries++ < 3) {
            tabItem(title).waitForExists(waitingTime)
            tabItem(title).click()
        }
    }

    fun longClickTab(title: String) {
        mDevice.waitNotNull(
            findObject(text(title)),
            waitingTime,
        )

        tab(title).perform(longClick())
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

    fun verifyTabsMultiSelectionCounter(numOfTabs: Int) {
        assertTrue(
            mDevice.findObject(UiSelector().text("$numOfTabs selected"))
                .waitForExists(waitingTime),
        )
    }

    class Transition {
        fun openThreeDotMenu(interact: ThreeDotMenuMainRobot.() -> Unit): ThreeDotMenuMainRobot.Transition {
            mDevice.waitForIdle()

            Espresso.openActionBarOverflowOrOptionsMenu(ApplicationProvider.getApplicationContext<Context>())
            ThreeDotMenuMainRobot().interact()
            return ThreeDotMenuMainRobot.Transition()
        }

        fun openTabDrawer(interact: TabDrawerRobot.() -> Unit): TabDrawerRobot.Transition {
            mDevice.waitForIdle(waitingTime)
            tabsCounter().click()
            mDevice.waitNotNull(
                Until.findObject(By.res("$packageName:id/tab_layout")),
                waitingTime,
            )

            TabDrawerRobot().interact()
            return TabDrawerRobot.Transition()
        }

        fun closeTabDrawer(interact: BrowserRobot.() -> Unit): BrowserRobot.Transition {
            mDevice.waitForIdle(waitingTime)

            onView(withId(R.id.handle)).perform(
                click(),
            )
            BrowserRobot().interact()
            return BrowserRobot.Transition()
        }

        fun openNewTab(interact: SearchRobot.() -> Unit): SearchRobot.Transition {
            mDevice.waitForIdle()

            newTabButton().click()
            SearchRobot().interact()
            return SearchRobot.Transition()
        }

        fun toggleToNormalTabs(interact: TabDrawerRobot.() -> Unit): Transition {
            normalBrowsingButton().perform(click())
            TabDrawerRobot().interact()
            return Transition()
        }

        fun toggleToPrivateTabs(interact: TabDrawerRobot.() -> Unit): Transition {
            privateBrowsingButton().perform(click())
            TabDrawerRobot().interact()
            return Transition()
        }

        fun openTabsListThreeDotMenu(interact: ThreeDotMenuMainRobot.() -> Unit): ThreeDotMenuMainRobot.Transition {
            threeDotMenu().perform(click())

            ThreeDotMenuMainRobot().interact()
            return ThreeDotMenuMainRobot.Transition()
        }

        fun openTab(title: String, interact: BrowserRobot.() -> Unit): BrowserRobot.Transition {
            scrollToElementByText(title)
            tabItem(title).waitForExists(waitingTime)
            tabItem(title).click()

            BrowserRobot().interact()
            return BrowserRobot.Transition()
        }

        // Temporary method to use indexes instead of tab titles, until the compose migration is complete
        fun openTabWithIndex(
            tabPosition: Int,
            interact: BrowserRobot.() -> Unit,
        ): BrowserRobot.Transition {
            val tab = mDevice.findObject(
                UiSelector()
                    .className("androidx.compose.ui.platform.ComposeView")
                    .index(tabPosition),
            )

            UiScrollable(UiSelector().resourceId("$packageName:id/tray_list_item")).scrollIntoView(tab)
            tab.waitForExists(waitingTime)
            tab.click()

            BrowserRobot().interact()
            return BrowserRobot.Transition()
        }

        fun clickTopBar(interact: TabDrawerRobot.() -> Unit): Transition {
            // The topBar contains other views.
            // Don't do the default click in the middle, rather click in some free space - top right.
            onView(withId(R.id.topBar)).clickAtLocationInView(GeneralLocation.TOP_RIGHT)
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

        fun openRecentlyClosedTabs(interact: RecentlyClosedTabsRobot.() -> Unit):
            RecentlyClosedTabsRobot.Transition {
            threeDotMenu().click()

            mDevice.waitNotNull(
                Until.findObject(text("Recently closed tabs")),
                waitingTime,
            )

            val menuRecentlyClosedTabs = mDevice.findObject(text("Recently closed tabs"))
            menuRecentlyClosedTabs.click()

            RecentlyClosedTabsRobot().interact()
            return RecentlyClosedTabsRobot.Transition()
        }

        fun clickSaveCollection(interact: CollectionRobot.() -> Unit):
            CollectionRobot.Transition {
            saveTabsToCollectionButton().click()

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

private fun assertCloseTabsButton(title: String) =
    assertTrue(
        mDevice.findObject(
            UiSelector()
                .resourceId("$packageName:id/mozac_browser_tabstray_close")
                .descriptionContains("Close tab $title"),
        ).waitForExists(waitingTime),
    )

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

private fun assertExistingOpenTabs(vararg tabTitles: String) {
    var retries = 0

    for (title in tabTitles) {
        while (!tabItem(title).waitForExists(waitingTime) && retries++ < 3) {
            tabsList
                .getChildByText(UiSelector().text(title), title, true)
            assertTrue(
                tabItem(title).waitForExists(waitingTimeLong),
            )
        }
    }
}

private fun assertNoExistingOpenTabs(vararg tabTitles: String) {
    for (title in tabTitles) {
        assertFalse(
            tabItem(title).waitForExists(waitingTimeLong),
        )
    }
}

private fun assertExistingTabList() {
    mDevice.findObject(
        UiSelector().resourceId("$packageName:id/tabsTray"),
    ).waitForExists(waitingTime)

    assertTrue(
        mDevice.findObject(
            UiSelector().resourceId("$packageName:id/tray_list_item"),
        ).waitForExists(waitingTime),
    )
}

private fun assertNoOpenTabsInNormalBrowsing() =
    onView(
        allOf(
            withId(R.id.tab_tray_empty_view),
            withText(R.string.no_open_tabs_description),
        ),
    ).check(matches(withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)))

private fun assertNoOpenTabsInPrivateBrowsing() =
    onView(
        allOf(
            withId(R.id.tab_tray_empty_view),
            withText(R.string.no_private_tabs_description),
        ),
    ).check(matches(withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)))

private fun assertNormalBrowsingNewTabButton() =
    onView(
        allOf(
            withId(R.id.new_tab_button),
            withContentDescription(R.string.add_tab),
        ),
    ).check(matches(withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)))

private fun assertPrivateBrowsingNewTabButton() =
    onView(
        allOf(
            withId(R.id.new_tab_button),
            withContentDescription(R.string.add_private_tab),
        ),
    ).check(matches(withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)))

private fun assertSelectTabsButton() =
    onView(withText("Select tabs"))
        .check(matches(withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)))

private fun assertNormalModeSelected() =
    normalBrowsingButton()
        .check(matches(ViewMatchers.isSelected()))

private fun assertPrivateModeSelected() =
    privateBrowsingButton()
        .check(matches(ViewMatchers.isSelected()))

private fun assertTabTrayOverflowButton(visible: Boolean) =
    onView(withId(R.id.tab_tray_overflow))
        .check(matches(withEffectiveVisibility(visibleOrGone(visible))))

private fun assertTabsTrayCounter() =
    tabsTrayCounterBox().check(matches(withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)))

private fun assertEmptyTabsTrayMenuButtons() {
    threeDotMenu().click()
    tabsSettingsButton()
        .inRoot(RootMatchers.isPlatformPopup())
        .check(matches(withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)))
    recentlyClosedTabsButton()
        .inRoot(RootMatchers.isPlatformPopup())
        .check(matches(withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)))
}

private fun assertTabTrayDoesExist() {
    onView(withId(R.id.tab_wrapper))
        .check(matches(withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)))
}

private fun assertTabTrayDoesNotExist() {
    onView(withId(R.id.tab_wrapper))
        .check(doesNotExist())
}

private fun assertMinisculeHalfExpandedRatio() {
    onView(withId(R.id.tab_wrapper))
        .check(matches(BottomSheetBehaviorHalfExpandedMaxRatioMatcher(0.001f)))
}

private fun assertBehaviorState(expectedState: Int) {
    onView(withId(R.id.tab_wrapper))
        .check(matches(BottomSheetBehaviorStateMatcher(expectedState)))
}

private fun assertNormalBrowsingButton() {
    normalBrowsingButton().check(matches(withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)))
}

private fun assertNormalBrowsingButtonIsSelected(isSelected: Boolean) {
    normalBrowsingButton().check(matches(isSelected(isSelected)))
}

private fun assertPrivateBrowsingButtonIsSelected(isSelected: Boolean) {
    privateBrowsingButton().check(matches(isSelected(isSelected)))
}

private fun assertSyncedTabsButtonIsSelected(isSelected: Boolean) {
    syncedTabsButton().check(matches(isSelected(isSelected)))
}

private fun assertTabThumbnail() {
    assertTrue(
        mDevice.findObject(
            UiSelector().resourceId("$packageName:id/mozac_browser_tabstray_thumbnail"),
        ).waitForExists(waitingTime),
    )
}

private val tabsList =
    UiScrollable(UiSelector().className("androidx.recyclerview.widget.RecyclerView"))

// This Espresso tab selector is used for actions that UIAutomator doesn't handle very well: swipe and long-tap
private fun tab(title: String) =
    onView(
        allOf(
            withId(R.id.mozac_browser_tabstray_title),
            withText(title),
        ),
    )

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
