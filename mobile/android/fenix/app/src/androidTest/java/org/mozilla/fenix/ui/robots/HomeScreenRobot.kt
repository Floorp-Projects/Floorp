/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

@file:Suppress("TooManyFunctions")

package org.mozilla.fenix.ui.robots

import android.net.Uri
import androidx.test.espresso.Espresso.onView
import androidx.test.espresso.action.ViewActions
import androidx.test.espresso.action.ViewActions.click
import androidx.test.espresso.action.ViewActions.typeText
import androidx.test.espresso.assertion.ViewAssertions.matches
import androidx.test.espresso.matcher.ViewMatchers
import androidx.test.espresso.matcher.ViewMatchers.Visibility
import androidx.test.espresso.matcher.ViewMatchers.withEffectiveVisibility
import androidx.test.platform.app.InstrumentationRegistry
import androidx.test.uiautomator.By
import androidx.test.uiautomator.UiDevice
import androidx.test.uiautomator.Until
import org.hamcrest.CoreMatchers
import org.hamcrest.Matchers.allOf
import org.mozilla.fenix.R
import org.mozilla.fenix.helpers.TestAssetHelper.waitingTime
import org.mozilla.fenix.helpers.click

/**
 * Implementation of Robot Pattern for the home screen menu.
 */
class HomeScreenRobot {
    fun verifyNavigationToolbar() = assertNavigationToolbar()
    fun verifyHomeScreen() = assertHomeScreen()
    fun verifyHomePrivateBrowsingButton() = assertHomePrivateBrowsingButton()
    fun verifyHomeMenu() = assertHomeMenu()
    fun verifyOpenTabsHeader() = assertOpenTabsHeader()
    fun verifyAddTabButton() = assertAddTabButton()
    fun verifyNoTabsOpenedText() = assertNoTabsOpenedText()
    fun verifyCollectionsHeader() = assertCollectionsHeader()
    fun verifyNoCollectionsHeader() = assertNoCollectionsHeader()
    fun verifyNoCollectionsText() = assertNoCollectionsText()
    fun verifyNoTabsOpenedHeader() = assertNoTabsOpenedHeader()
    fun verifyHomeWordmark() = assertHomeWordmark()
    fun verifyHomeToolbar() = assertHomeToolbar()
    fun verifyHomeComponent() = assertHomeComponent()

    class Transition {

        val mDevice = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation())

        fun openThreeDotMenu(interact: ThreeDotMenuRobot.() -> Unit): ThreeDotMenuRobot.Transition {

            mDevice.waitForIdle()
            threeDotButton().perform(click())

            ThreeDotMenuRobot().interact()
            return ThreeDotMenuRobot.Transition()
        }

        fun completeFirstRun(url: Uri, interact: BrowserRobot.() -> Unit): BrowserRobot.Transition {

            mDevice.wait(Until.findObject(By.text("Search or enter address")), waitingTime)
            navigationToolbar().perform(click())

            browserToolbarEditView().perform(
                typeText(url.toString()),
                ViewActions.pressImeActionButton())

            tabCounterText().click()
            mDevice.wait(Until.findObject(By.res("close_tab_button")), waitingTime)
            closeTabButton().click()

            BrowserRobot().interact()
            return BrowserRobot.Transition()
        }
    }
}

fun homeScreen(interact: HomeScreenRobot.() -> Unit): HomeScreenRobot.Transition {
    HomeScreenRobot().interact()
    return HomeScreenRobot.Transition()
}

val mDevice = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation())

private fun navigationToolbar() = onView(CoreMatchers.allOf(ViewMatchers.withText("Search or enter address")))

private fun browserToolbarEditView() = onView(allOf(ViewMatchers.withId(R.id.mozac_browser_toolbar_edit_url_view)))
private fun closeTabButton() = onView(allOf(ViewMatchers.withId(R.id.close_tab_button)))

private fun assertNavigationToolbar() = onView(CoreMatchers.allOf(ViewMatchers.withText("Search or enter address")))
    .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))
private fun tabCounterText() = onView(allOf(ViewMatchers.withId(R.id.counter_text)))
    .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))
private fun assertHomeScreen() = onView(ViewMatchers.withResourceName("homeLayout"))
    .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))
private fun assertHomeMenu() = onView(ViewMatchers.withResourceName("menuButton"))
    .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))
private fun assertHomePrivateBrowsingButton() = onView(ViewMatchers.withResourceName("privateBrowsingButton"))
    .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))
private fun assertHomeWordmark() = onView(ViewMatchers.withResourceName("wordmark"))
    .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))
private fun assertHomeToolbar() = onView(ViewMatchers.withResourceName("toolbar"))
    .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))
private fun assertOpenTabsHeader() {
    mDevice.wait(Until.findObject(By.text("Open tabs")), waitingTime)
}
private fun assertAddTabButton() {
    mDevice.wait(Until.findObject(By.res("add_tab_button")), waitingTime)
}
private fun assertNoTabsOpenedHeader() {
    mDevice.wait(Until.findObject(By.text("No tabs opened")), waitingTime)
}
private fun assertNoTabsOpenedText() {
    onView(CoreMatchers.allOf(ViewMatchers.withText("Your open tabs will be shown here.")))
        .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))
}
private fun assertCollectionsHeader() = onView(CoreMatchers.allOf(ViewMatchers.withText("Collections")))
        .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))
private fun assertNoCollectionsHeader() = onView(CoreMatchers.allOf(ViewMatchers.withText("No collections")))
        .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))
private fun assertNoCollectionsText() =
    onView(CoreMatchers.allOf(ViewMatchers.withText("Your collections will be shown here.")))
        .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))
private fun assertHomeComponent() = onView(ViewMatchers.withResourceName("home_component"))
    .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))

private fun threeDotButton() = onView(allOf(ViewMatchers.withId(R.id.menuButton)))
