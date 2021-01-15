/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

@file:Suppress("TooManyFunctions")

package org.mozilla.fenix.ui.robots

import android.content.Context
import android.view.View
import androidx.recyclerview.widget.RecyclerView
import androidx.test.core.app.ApplicationProvider
import androidx.test.espresso.Espresso
import androidx.test.espresso.Espresso.onView
import androidx.test.espresso.NoMatchingViewException
import androidx.test.espresso.UiController
import androidx.test.espresso.ViewAction
import androidx.test.espresso.action.ViewActions
import androidx.test.espresso.action.ViewActions.click
import androidx.test.espresso.action.ViewActions.replaceText
import androidx.test.espresso.assertion.ViewAssertions.doesNotExist
import androidx.test.espresso.assertion.ViewAssertions.matches
import androidx.test.espresso.contrib.RecyclerViewActions
import androidx.test.espresso.matcher.ViewMatchers
import androidx.test.espresso.matcher.ViewMatchers.isDisplayed
import androidx.test.espresso.matcher.ViewMatchers.withContentDescription
import androidx.test.espresso.matcher.ViewMatchers.withEffectiveVisibility
import androidx.test.espresso.matcher.ViewMatchers.withId
import androidx.test.espresso.matcher.ViewMatchers.withText
import androidx.test.platform.app.InstrumentationRegistry
import androidx.test.uiautomator.By
import androidx.test.uiautomator.By.text
import androidx.test.uiautomator.UiDevice
import androidx.test.uiautomator.UiSelector
import androidx.test.uiautomator.Until
import androidx.test.uiautomator.Until.findObject
import com.google.android.material.bottomsheet.BottomSheetBehavior
import org.hamcrest.CoreMatchers.allOf
import org.hamcrest.CoreMatchers.anyOf
import org.hamcrest.CoreMatchers.containsString
import org.hamcrest.Matcher
import org.mozilla.fenix.R
import org.mozilla.fenix.helpers.TestAssetHelper
import org.mozilla.fenix.helpers.TestAssetHelper.waitingTime
import org.mozilla.fenix.helpers.click
import org.mozilla.fenix.helpers.ext.waitNotNull
import org.mozilla.fenix.helpers.idlingresource.BottomSheetBehaviorStateIdlingResource
import org.mozilla.fenix.helpers.matchers.BottomSheetBehaviorHalfExpandedMaxRatioMatcher
import org.mozilla.fenix.helpers.matchers.BottomSheetBehaviorStateMatcher

/**
 * Implementation of Robot Pattern for the home screen menu.
 */
class TabDrawerRobot {
    fun verifyExistingOpenTabs(title: String) = assertExistingOpenTabs(title)
    fun verifyCloseTabsButton(title: String) = assertCloseTabsButton(title)

    fun verifyExistingTabList() = assertExistingTabList()

    fun verifyNoTabsOpened() = assertNoTabsOpenedText()
    fun verifyPrivateModeSelected() = assertPrivateModeSelected()
    fun verifyNormalModeSelected() = assertNormalModeSelected()
    fun verifyNewTabButton() = assertNewTabButton()
    fun verifyTabTrayOverflowMenu(visibility: Boolean) = assertTabTrayOverflowButton(visibility)

    fun verifyTabTrayIsClosed() = assertTabTrayDoesNotExist()
    fun verifyHalfExpandedRatio() = assertMinisculeHalfExpandedRatio()
    fun verifyBehaviorState(expectedState: Int) = assertBehaviorState(expectedState)

    fun closeTab() {
        mDevice.findObject(
            UiSelector().resourceId("org.mozilla.fenix.debug:id/mozac_browser_tabstray_close")
        ).waitForExists(waitingTime)
        closeTabButton().click()
    }

    fun swipeTabRight(title: String) =
        tab(title).perform(ViewActions.swipeRight())

    fun swipeTabLeft(title: String) =
        tab(title).perform(ViewActions.swipeLeft())

    fun closeTabViaXButton(title: String) {
        val closeButton = onView(
            allOf(
                withId(R.id.mozac_browser_tabstray_close),
                withContentDescription("Close tab $title")
            )
        )
        closeButton.perform(click())
    }

    fun verifySnackBarText(expectedText: String) {
        val mDevice = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation())
        mDevice.waitNotNull(findObject(By.text(expectedText)), TestAssetHelper.waitingTime)
    }

    fun snackBarButtonClick(expectedText: String) {
        mDevice.findObject(
            UiSelector().resourceId("org.mozilla.fenix.debug:id/snackbar_btn")
        ).waitForExists(waitingTime)
        onView(allOf(withId(R.id.snackbar_btn), withText(expectedText))).check(
            matches(withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE))
        ).perform(click())
    }

    fun verifyTabMediaControlButtonState(action: String) {
        mDevice.waitNotNull(
            findObject(
                By
                    .res("org.mozilla.fenix.debug:id/play_pause_button")
                    .desc(action)
            ),
            waitingTime
        )

        tabMediaControlButton().check(matches(withContentDescription(action)))
    }

    fun clickTabMediaControlButton() = tabMediaControlButton().click()

    fun clickSelectTabs() = onView(withText("Select tabs")).click()

    fun clickAddNewCollection() = addNewCollectionButton().click()

    fun selectTab(title: String) = tab(title).click()

    fun clickSaveCollection() = saveTabsToCollectionButton().click()

    fun typeCollectionName(collectionName: String) {
        collectionNameTextField().perform(replaceText(collectionName))
        mDevice.findObject(UiSelector().textContains("OK")).click()
    }

    fun createCollection(
        tabTitle: String,
        collectionName: String,
        firstCollection: Boolean = true
    ) {
        clickSelectTabs()
        selectTab(tabTitle)
        clickSaveCollection()
        if (!firstCollection)
            clickAddNewCollection()
        typeCollectionName(collectionName)
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
            mDevice.findObject(UiSelector().resourceId("org.mozilla.fenix.debug:id/tab_button"))
                .waitForExists(waitingTime)

            tabsCounter().click()

            org.mozilla.fenix.ui.robots.mDevice.waitNotNull(
                Until.findObject(By.res("org.mozilla.fenix.debug:id/tab_layout")),
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

            newTabButton().perform(click())
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
            mDevice.waitNotNull(findObject(text(title)))
            tab(title).click()

            BrowserRobot().interact()
            return BrowserRobot.Transition()
        }

        fun clickTopBar(interact: TabDrawerRobot.() -> Unit): Transition {
            onView(withId(R.id.topBar)).click()
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
            var behavior: BottomSheetBehavior<*>? = null
            onView(withId(R.id.tab_wrapper)).perform(object : ViewAction {
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
            runWithIdleRes(BottomSheetBehaviorStateIdlingResource(behavior!!)) {
                TabDrawerRobot().interact()
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
    }
}

fun tabDrawer(interact: TabDrawerRobot.() -> Unit): TabDrawerRobot.Transition {
    TabDrawerRobot().interact()
    return TabDrawerRobot.Transition()
}

private fun tabMediaControlButton() = onView(withId(R.id.play_pause_button))

private fun closeTabButton() = onView(withId(R.id.mozac_browser_tabstray_close))
private fun assertCloseTabsButton(title: String) =
    onView(
        allOf(
            withId(R.id.mozac_browser_tabstray_close),
            withContentDescription("Close tab $title")
        )
    )
        .check(matches(withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)))

private fun normalBrowsingButton() = onView(
    anyOf(
        withContentDescription(containsString("open tabs. Tap to switch tabs.")),
        withContentDescription(containsString("open tab. Tap to switch tabs."))
    )
)

private fun privateBrowsingButton() = onView(withContentDescription("Private tabs"))
private fun newTabButton() = onView(withId(R.id.new_tab_button))
private fun threeDotMenu() = onView(withId(R.id.tab_tray_overflow))

private fun assertExistingOpenTabs(title: String) {
    try {
        tab(title).check(matches(isDisplayed()))
    } catch (e: NoMatchingViewException) {
        onView(withId(R.id.tabsTray)).perform(
            RecyclerViewActions.scrollTo<RecyclerView.ViewHolder>(
                allOf(
                    withId(R.id.mozac_browser_tabstray_title),
                    withText(title)
                )
            )
        ).check(matches(withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)))
    }
}

private fun assertExistingTabList() =
    onView(allOf(withId(R.id.tab_item)))
        .check(matches(withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)))

private fun assertNoTabsOpenedText() =
    onView(withId(R.id.tab_tray_empty_view))
        .check(matches(withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)))

private fun assertNewTabButton() =
    onView(withId(R.id.new_tab_button))
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

private fun tab(title: String) =
    onView(
        allOf(
            withId(R.id.mozac_browser_tabstray_title),
            withText(title)
        )
    )

private fun tabsCounter() = onView(withId(R.id.tab_button))

private fun visibleOrGone(visibility: Boolean) =
    if (visibility) ViewMatchers.Visibility.VISIBLE else ViewMatchers.Visibility.GONE

private fun addNewCollectionButton() = onView(withId(R.id.add_new_collection))

private fun saveTabsToCollectionButton() = onView(withId(R.id.collect_multi_select))

private fun collectionNameTextField() = onView(withId(R.id.collection_name))
