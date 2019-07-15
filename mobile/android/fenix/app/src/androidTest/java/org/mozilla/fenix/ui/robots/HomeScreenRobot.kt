/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

@file:Suppress("TooManyFunctions")

package org.mozilla.fenix.ui.robots

import androidx.test.espresso.Espresso.onView
import androidx.test.espresso.action.ViewActions.click
import androidx.test.espresso.assertion.ViewAssertions.matches
import androidx.test.espresso.matcher.ViewMatchers
import androidx.test.espresso.matcher.ViewMatchers.Visibility
import androidx.test.espresso.matcher.ViewMatchers.withEffectiveVisibility
import androidx.test.platform.app.InstrumentationRegistry
import androidx.test.uiautomator.UiDevice
import androidx.test.uiautomator.UiScrollable
import androidx.test.uiautomator.UiSelector
import org.hamcrest.CoreMatchers
import org.hamcrest.Matchers.allOf
import org.hamcrest.Matchers.containsString
import org.mozilla.fenix.R

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

    // First Run elements
    fun verifyWelcomeHeader() = assertWelcomeHeader()
    fun verifyGetTheMostHeader() = assertGetTheMostHeader()
    fun verifyAccountsSignInButton() = assertAccountsSignInButton()
    fun verifyGetToKnowHeader() = assertGetToKnowHeader()
    fun verifyChooseThemeHeader() = assertChooseThemeHeader()
    fun verifyChooseThemeText() = assertChooseThemeText()
    fun verifyLightThemeToggle() = assertLightThemeToggle()
    fun verifyLightThemeDescription() = assertLightThemeDescription()
    fun verifyDarkThemeToggle() = assertDarkThemeToggle()
    fun verifyDarkThemeDescription() = assertDarkThemeDescription()
    fun verifyAutomaticThemeToggle() = assertAutomaticThemeToggle()
    fun verifyAutomaticThemeDescription() = assertAutomaticThemeDescription()
    fun verifyProtectYourselfHeader() = assertProtectYourselfHeader()
    fun verifyTrackingProtectionToggle() = assertTrackingProtectionToggle()
    fun verifyProtectYourselfText() = assertProtectYourselfText()

    fun verifyBrowsePrivatelyHeader() = assertBrowsePrivatelyHeader()
    fun verifyBrowsePrivatelyText() = assertBrowsePrivatelyText()
    fun verifyYourPrivacyHeader() = assertYourPrivacyHeader()
    fun verifyYourPrivacyText() = assertYourPrivacyText()
    fun verifyPrivacyNoticeButton() = assertPrivacyNoticeButton()
    fun verifyStartBrowsingButton() = assertStartBrowsingButton()

    private fun scrollToElementByText(text: String): UiScrollable {
        val appView = UiScrollable(UiSelector().scrollable(true))
        appView.scrollTextIntoView(text)
        return appView
    }

    fun swipeUpToDismissFirstRun() {
        scrollToElementByText("Start browsing")
    }

    class Transition {
        val mDevice = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation())

        fun openThreeDotMenu(interact: ThreeDotMenuRobot.() -> Unit): ThreeDotMenuRobot.Transition {
            mDevice.waitForIdle()
            threeDotButton().perform(click())

            ThreeDotMenuRobot().interact()
            return ThreeDotMenuRobot.Transition()
        }

        fun openSearch(interact: SearchRobot.() -> Unit): SearchRobot.Transition {
            mDevice.waitForIdle()
            navigationToolbar().perform(click())

            SearchRobot().interact()
            return SearchRobot.Transition()
        }

        fun dismissOnboarding() {
            openThreeDotMenu { }.openSettings { }.goBack { }
        }
    }
}

fun homeScreen(interact: HomeScreenRobot.() -> Unit): HomeScreenRobot.Transition {
    HomeScreenRobot().interact()
    return HomeScreenRobot.Transition()
}

val mDevice = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation())

private fun navigationToolbar() = onView(CoreMatchers.allOf(ViewMatchers.withText("Search or enter address")))

private fun assertNavigationToolbar() = onView(CoreMatchers.allOf(ViewMatchers.withText("Search or enter address")))
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
private fun assertOpenTabsHeader() =
    onView(CoreMatchers.allOf(ViewMatchers.withText("Open tabs")))
        .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))

private fun assertAddTabButton() =
    onView(CoreMatchers.allOf(ViewMatchers.withResourceName("add_tab_button")))
        .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))

private fun assertNoTabsOpenedHeader() =
    onView(CoreMatchers.allOf(ViewMatchers.withText("No tabs opened")))
        .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))

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

// First Run elements
private fun assertWelcomeHeader() = onView(CoreMatchers.allOf(ViewMatchers.withText("Welcome to Firefox Preview!")))
    .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))
private fun assertGetTheMostHeader() =
    onView(CoreMatchers.allOf(ViewMatchers.withText("Get the most out of Firefox Preview.")))
        .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))
private fun assertAccountsSignInButton() = onView(ViewMatchers.withResourceName("turn_on_sync_button"))
    .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))
private fun assertGetToKnowHeader() = onView(CoreMatchers.allOf(ViewMatchers.withText("Get to know Firefox Preview")))
    .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))

private fun assertChooseThemeHeader() =
    onView(CoreMatchers.allOf(ViewMatchers.withText("Choose your theme")))
        .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))

private fun assertChooseThemeText() =
    onView(CoreMatchers.allOf(ViewMatchers.withText("Try dark theme: easier on your battery and your eyes.")))
        .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))

private fun assertLightThemeToggle() = onView(ViewMatchers.withResourceName("theme_light_radio_button"))
    .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))
private fun assertLightThemeDescription() = onView(CoreMatchers.allOf(ViewMatchers.withText("Light theme")))
    .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))
private fun assertDarkThemeToggle() = onView(ViewMatchers.withResourceName("theme_dark_radio_button"))
    .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))
private fun assertDarkThemeDescription() = onView(CoreMatchers.allOf(ViewMatchers.withText("Dark theme")))
    .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))
private fun assertAutomaticThemeToggle() = onView(ViewMatchers.withResourceName("theme_automatic_radio_button"))
    .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))
private fun assertAutomaticThemeDescription() = onView(CoreMatchers.allOf(ViewMatchers.withText("Automatic")))
    .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))

private fun assertProtectYourselfHeader() =
    onView(CoreMatchers.allOf(ViewMatchers.withText("Protect yourself")))
        .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))
private fun assertTrackingProtectionToggle() = onView(
    CoreMatchers.allOf(ViewMatchers.withResourceName("tracking_protection_toggle")))
    .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))
private fun assertProtectYourselfText() {
    onView(CoreMatchers.allOf(ViewMatchers.withText(
        "Firefox Preview blocks ad trackers that follow you around the web.")))
        .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))
}
private fun assertBrowsePrivatelyHeader() =
    onView(CoreMatchers.allOf(ViewMatchers.withText("Browse privately")))
        .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))
private fun assertBrowsePrivatelyText() =
    onView(CoreMatchers.allOf(ViewMatchers.withText(containsString("private browsing is just a tap away."))))
        .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))

private fun assertYourPrivacyHeader() =
    onView(CoreMatchers.allOf(ViewMatchers.withText("Your privacy")))
        .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))
private fun assertYourPrivacyText() =
    onView(CoreMatchers.allOf(ViewMatchers.withText(
        "We’ve designed Firefox Preview to give you control over what you share online and what you share with us.")))
        .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))
private fun assertPrivacyNoticeButton() =
    onView(CoreMatchers.allOf(ViewMatchers.withText("Read our privacy notice")))
        .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))

private fun assertStartBrowsingButton() =
    onView(CoreMatchers.allOf(ViewMatchers.withText("Start browsing")))
        .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))
