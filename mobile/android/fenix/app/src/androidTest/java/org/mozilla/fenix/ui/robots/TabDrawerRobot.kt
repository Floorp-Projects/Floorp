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
import androidx.test.platform.app.InstrumentationRegistry
import androidx.test.uiautomator.By
import androidx.test.uiautomator.By.text
import androidx.test.uiautomator.UiDevice
import androidx.test.uiautomator.UiScrollable
import androidx.test.uiautomator.UiSelector
import androidx.test.uiautomator.Until
import androidx.test.uiautomator.Until.findObject
import com.google.android.material.bottomsheet.BottomSheetBehavior
import junit.framework.AssertionFailedError
import junit.framework.TestCase.assertTrue
import org.hamcrest.CoreMatchers.allOf
import org.hamcrest.CoreMatchers.anyOf
import org.hamcrest.CoreMatchers.containsString
import org.hamcrest.Matcher
import org.mozilla.fenix.R
import org.mozilla.fenix.helpers.TestAssetHelper.waitingTime
import org.mozilla.fenix.helpers.TestAssetHelper.waitingTimeShort
import org.mozilla.fenix.helpers.TestHelper.getStringResource
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
        val mDevice = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation())

        mDevice.waitNotNull(
            Until.findObject(By.res("$packageName:id/mozac_browser_tabstray_url")),
            waitingTime
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
    fun verifyExistingOpenTabs(title: String) = assertExistingOpenTabs(title)
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
        mDevice.findObject(
            UiSelector().resourceId("$packageName:id/mozac_browser_tabstray_close")
        ).waitForExists(waitingTime)

        var retries = 0 // number of retries before failing, will stop at 2
        do {
            closeTabButton().click()
            retries++
        } while (mDevice.findObject(
                UiSelector().resourceId("$packageName:id/mozac_browser_tabstray_close")
            ).exists() && retries < 3
        )
    }

    fun swipeTabRight(title: String) {
        var retries = 0 // number of retries before failing, will stop at 2
        while (!mDevice.findObject(
                UiSelector()
                    .resourceId("$packageName:id/mozac_browser_tabstray_title")
                    .text(title)
            ).waitUntilGone(waitingTimeShort) && retries < 2
        ) {
            tab(title).perform(ViewActions.swipeRight())
            retries++
        }
    }

    fun swipeTabLeft(title: String) {
        var retries = 0 // number of retries before failing, will stop at 2
        while (!mDevice.findObject(
                UiSelector()
                    .resourceId("$packageName:id/mozac_browser_tabstray_title")
                    .text(title)
            ).waitUntilGone(waitingTimeShort) && retries < 2
        ) {
            tab(title).perform(ViewActions.swipeLeft())
            retries++
        }
    }

    fun verifySnackBarText(expectedText: String) {
        assertTrue(
            mDevice.findObject(
                UiSelector().text(expectedText)
            ).waitForExists(waitingTime)
        )
    }

    fun snackBarButtonClick(expectedText: String) {
        val snackBarButton =
            mDevice.findObject(
                UiSelector()
                    .resourceId("$packageName:id/snackbar_btn")
                    .text(expectedText)
            )

        snackBarButton.waitForExists(waitingTime)
        snackBarButton.click()
    }

    fun verifyTabMediaControlButtonState(action: String) {
        try {
            mDevice.findObject(
                UiSelector().resourceId("$packageName:id/tab_tray_empty_view")
            ).waitUntilGone(waitingTime)

            mDevice.findObject(
                UiSelector().resourceId("$packageName:id/tab_tray_grid_item")
            ).waitForExists(waitingTime)

            mDevice.findObject(
                UiSelector()
                    .resourceId("$packageName:id/play_pause_button")
                    .descriptionContains(action)
            ).waitForExists(waitingTime)

            assertTrue(
                mDevice.findObject(UiSelector().descriptionContains(action)).waitForExists(waitingTime)
            )
        } catch (e: AssertionFailedError) {
            // In some cases the tab media button isn't updated after performing an action on it
            println("Failed to update the state of the tab media button")

            // Let's dismiss the tabs tray and try again
            mDevice.pressBack()
            mDevice.findObject(
                UiSelector()
                    .resourceId("$packageName:id/toolbar")
            ).waitForExists(waitingTime)

            browserScreen {
            }.openTabDrawer {
                // Click again the tab media button
                tabMediaControlButton().click()

                mDevice.findObject(
                    UiSelector().resourceId("$packageName:id/tab_tray_empty_view")
                ).waitUntilGone(waitingTime)

                mDevice.findObject(
                    UiSelector().resourceId("$packageName:id/tab_tray_grid_item")
                ).waitForExists(waitingTime)

                mDevice.findObject(
                    UiSelector()
                        .resourceId("$packageName:id/play_pause_button")
                        .descriptionContains(action)
                ).waitForExists(waitingTime)

                assertTrue(
                    mDevice.findObject(UiSelector().descriptionContains(action)).waitForExists(waitingTime)
                )
            }
        }
    }

    fun clickTabMediaControlButton(action: String) {
        mDevice.waitNotNull(
            Until.findObjects(
                By
                    .res("$packageName:id/play_pause_button")
                    .descContains(action)
            ),
            waitingTime
        )

        tabMediaControlButton().click()
    }

    private fun clickSelectTabs() {
        threeDotMenu().click()

        mDevice.waitNotNull(
            findObject(
                text(getStringResource(R.string.tabs_tray_select_tabs))
            ),
            waitingTime
        )

        val selectTabsButton = mDevice.findObject(text(getStringResource(R.string.tabs_tray_select_tabs)))
        selectTabsButton.click()
    }

    fun selectTab(title: String) {
        mDevice.waitNotNull(
            findObject(text(title)),
            waitingTime
        )

        val tab = mDevice.findObject(text(title))
        tab.click()
    }

    fun longClickTab(title: String) {
        mDevice.waitNotNull(
            findObject(text(title)),
            waitingTime
        )

        val tab = onView(withText(title))
        tab.perform(longClick())
    }

    fun createCollection(
        tabTitle: String,
        collectionName: String,
        firstCollection: Boolean = true
    ) {
        clickSelectTabs()
        selectTab(tabTitle)
        tabDrawer {
        }.clickSaveCollection {
            if (!firstCollection)
                clickAddNewCollection()
            typeCollectionNameAndSave(collectionName)
        }
    }

    fun verifyTabsMultiSelectionCounter(numOfTabs: Int) {
        assertTrue(
            mDevice.findObject(UiSelector().text("$numOfTabs selected"))
                .waitForExists(waitingTime)
        )
    }

    class Transition {
        val mDevice = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation())

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
                waitingTime
            )

            TabDrawerRobot().interact()
            return TabDrawerRobot.Transition()
        }

        fun closeTabDrawer(interact: BrowserRobot.() -> Unit): BrowserRobot.Transition {
            mDevice.waitForIdle(waitingTime)

            onView(withId(R.id.handle)).perform(
                click()
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
            val tab = mDevice.findObject(
                UiSelector()
                    .resourceId("$packageName:id/mozac_browser_tabstray_title")
                    .textContains(title)
            )
            scrollToElementByText(title)
            tab.waitForExists(waitingTime)
            tab.click()

            BrowserRobot().interact()
            return BrowserRobot.Transition()
        }

        fun openTabFromGroup(title: String, interact: BrowserRobot.() -> Unit): BrowserRobot.Transition {
            val tab = UiScrollable(UiSelector().resourceId("$packageName:id/tab_group_list"))
                .setAsHorizontalList()
                .getChildByText(
                    UiSelector()
                        .resourceId("$packageName:id/mozac_browser_tabstray_title")
                        .textContains(title),
                    title,
                    true
                )
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
            onView(withId(R.id.tab_wrapper)).perform(object : ViewAction {
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
            })
            TabDrawerRobot().interact()
            return Transition()
        }

        fun waitForTabTrayBehaviorToIdle(interact: TabDrawerRobot.() -> Unit): Transition {
            // Need to get the behavior of tab_wrapper and wait for that to idle.
            var behavior: BottomSheetBehavior<*>? = null

            // Null check here since it's possible that the view is already animated away from the screen.
            onView(withId(R.id.tab_wrapper))?.perform(object : ViewAction {
                override fun getDescription(): String {
                    return "Postpone actions to after the BottomSheetBehavior has settled"
                }

                override fun getConstraints(): Matcher<View> {
                    return ViewMatchers.isAssignableFrom(View::class.java)
                }

                override fun perform(uiController: UiController?, view: View?) {
                    behavior = BottomSheetBehavior.from(view!!)
                }
            })

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

            val mDevice = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation())
            mDevice.waitNotNull(
                Until.findObject(text("Recently closed tabs")),
                waitingTime
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

private fun tabMediaControlButton() =
    mDevice.findObject(UiSelector().resourceId("$packageName:id/play_pause_button"))

private fun closeTabButton() =
    mDevice.findObject(UiSelector().resourceId("$packageName:id/mozac_browser_tabstray_close"))
private fun assertCloseTabsButton(title: String) =
    assertTrue(
        mDevice.findObject(
            UiSelector()
                .resourceId("$packageName:id/mozac_browser_tabstray_close")
                .descriptionContains("Close tab $title")
        ).waitForExists(waitingTime)
    )

private fun normalBrowsingButton() = onView(
    anyOf(
        withContentDescription(containsString("open tabs. Tap to switch tabs.")),
        withContentDescription(containsString("open tab. Tap to switch tabs."))
    )
)

private fun privateBrowsingButton() = onView(withContentDescription("Private tabs"))
private fun syncedTabsButton() = onView(withContentDescription("Synced tabs"))
private fun newTabButton() = mDevice.findObject(UiSelector().resourceId("$packageName:id/new_tab_button"))
private fun threeDotMenu() = onView(withId(R.id.tab_tray_overflow))

private fun assertExistingOpenTabs(title: String) {
    try {
        mDevice.findObject(
            UiSelector()
                .resourceId("$packageName:id/mozac_browser_tabstray_title")
                .textContains(title)
        )
            .waitForExists(waitingTime)

        assertTrue(
            mDevice.findObject(
                UiSelector()
                    .resourceId("$packageName:id/mozac_browser_tabstray_title")
                    .textContains(title)
            ).waitForExists(waitingTime)
        )
    } catch (e: AssertionError) {
        println("The tab wasn't found")
        mDevice.findObject(UiSelector().resourceId("$packageName:id/tabsTray")).swipeUp(2)
        assertTrue(
            mDevice.findObject(
                UiSelector()
                    .resourceId("$packageName:id/mozac_browser_tabstray_title")
                    .textContains(title)
            ).waitForExists(waitingTime)
        )
    }
}

private fun assertExistingTabList() {
    mDevice.findObject(
        UiSelector().resourceId("$packageName:id/tabsTray")
    ).waitForExists(waitingTime)

    assertTrue(
        mDevice.findObject(
            UiSelector().resourceId("$packageName:id/tab_item")
        ).waitForExists(waitingTime)
    )
}

private fun assertNoOpenTabsInNormalBrowsing() =
    onView(
        allOf(
            withId(R.id.tab_tray_empty_view),
            withText(R.string.no_open_tabs_description)
        )
    ).check(matches(withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)))

private fun assertNoOpenTabsInPrivateBrowsing() =
    onView(
        allOf(
            withId(R.id.tab_tray_empty_view),
            withText(R.string.no_private_tabs_description)
        )
    ).check(matches(withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)))

private fun assertNormalBrowsingNewTabButton() =
    onView(
        allOf(
            withId(R.id.new_tab_button),
            withContentDescription(R.string.add_tab)
        )
    ).check(matches(withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)))

private fun assertPrivateBrowsingNewTabButton() =
    onView(
        allOf(
            withId(R.id.new_tab_button),
            withContentDescription(R.string.add_private_tab)
        )
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
            UiSelector().resourceId("$packageName:id/mozac_browser_tabstray_thumbnail")
        ).waitForExists(waitingTime)
    )
}

private fun tab(title: String) =
    onView(
        allOf(
            withId(R.id.mozac_browser_tabstray_title),
            withText(title)
        )
    )

private fun tabsCounter() = onView(withId(R.id.tab_button))

private fun tabsTrayCounterBox() = onView(withId(R.id.counter_box))

private fun tabsSettingsButton() =
    onView(
        allOf(
            withId(R.id.simple_text),
            withText(R.string.tab_tray_menu_tab_settings)
        )
    )

private fun recentlyClosedTabsButton() =
    onView(
        allOf(
            withId(R.id.simple_text),
            withText(R.string.tab_tray_menu_recently_closed)
        )
    )

private fun visibleOrGone(visibility: Boolean) =
    if (visibility) ViewMatchers.Visibility.VISIBLE else ViewMatchers.Visibility.GONE

private fun saveTabsToCollectionButton() = onView(withId(R.id.collect_multi_select))
