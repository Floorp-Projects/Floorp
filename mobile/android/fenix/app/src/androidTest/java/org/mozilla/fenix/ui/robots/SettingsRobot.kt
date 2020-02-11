/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

@file:Suppress("TooManyFunctions")

package org.mozilla.fenix.ui.robots

import android.content.pm.PackageManager
import androidx.recyclerview.widget.RecyclerView
import androidx.test.espresso.Espresso.onView
import androidx.test.espresso.action.ViewActions
import androidx.test.espresso.contrib.RecyclerViewActions
import androidx.test.espresso.ViewInteraction
import androidx.test.espresso.assertion.ViewAssertions.matches
import androidx.test.espresso.intent.Intents.intended
import androidx.test.espresso.intent.matcher.IntentMatchers.toPackage
import androidx.test.espresso.matcher.ViewMatchers
import androidx.test.espresso.matcher.ViewMatchers.Visibility
import androidx.test.espresso.matcher.ViewMatchers.isCompletelyDisplayed
import androidx.test.espresso.matcher.ViewMatchers.withEffectiveVisibility
import androidx.test.platform.app.InstrumentationRegistry
import androidx.test.uiautomator.By
import androidx.test.uiautomator.UiDevice
import androidx.test.uiautomator.Until
import org.hamcrest.CoreMatchers
import org.mozilla.fenix.R
import org.mozilla.fenix.helpers.Constants.PackageName.GOOGLE_PLAY_SERVICES
import org.mozilla.fenix.helpers.TestAssetHelper.waitingTime
import org.mozilla.fenix.helpers.TestHelper
import org.mozilla.fenix.helpers.click

/**
 * Implementation of Robot Pattern for the settings menu.
 */
class SettingsRobot {

    // BASICS SECTION
    fun verifyBasicsHeading() = assertBasicsHeading()

    fun verifySearchEngineButton() = assertSearchEngineButton()
    fun verifyThemeButton() = assertThemeButton()
    fun verifyThemeSelected() = assertThemeSelected()
    fun verifyAccessibilityButton() = assertAccessibilityButton()
    fun verifySetAsDefaultBrowserButton() = assertSetAsDefaultBrowserButton()

    // PRIVACY SECTION
    fun verifyPrivacyHeading() = assertPrivacyHeading()

    fun verifyEnhancedTrackingProtectionButton() = assertEnhancedTrackingProtectionButton()
    fun verifyLoginsButton() = assertLoginsButton()
    fun verifyEnhancedTrackingProtectionValue(state: String) =
        assertEnhancedTrackingProtectionValue(state)

    fun verifyAddPrivateBrowsingShortcutButton() = assertAddPrivateBrowsingShortcutButton()
    fun verifySitePermissionsButton() = assertSitePermissionsButton()
    fun verifyDeleteBrowsingDataButton() = assertDeleteBrowsingDataButton()
    fun verifyDeleteBrowsingDataOnQuitButton() = assertDeleteBrowsingDataOnQuitButton()
    fun verifyDataCollectionButton() = assertDataCollectionButton()
    fun verifyLeakCanaryButton() = assertLeakCanaryButton()
    fun verifySettingsView() = assertSettingsView()

    // DEVELOPER TOOLS SECTION
    fun verifyDeveloperToolsHeading() = assertDeveloperToolsHeading()

    fun verifyRemoteDebug() = assertRemoteDebug()

    // ABOUT SECTION
    fun verifyAboutHeading() = assertAboutHeading()

    fun verifyRateOnGooglePlay() = assertRateOnGooglePlay()
    fun verifyAboutFirefoxPreview() = assertAboutFirefoxPreview()
    fun verifyGooglePlayRedirect() = assertGooglePlayRedirect()

    class Transition {

        val mDevice = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation())

        fun goBack(interact: HomeScreenRobot.() -> Unit): HomeScreenRobot.Transition {
            goBackButton().click()

            HomeScreenRobot().interact()
            return HomeScreenRobot.Transition()
        }

        fun openAboutFirefoxPreview(interact: SettingsSubMenuAboutRobot.() -> Unit):
                SettingsSubMenuAboutRobot.Transition {

            assertAboutFirefoxPreview().click()

            SettingsSubMenuAboutRobot().interact()
            return SettingsSubMenuAboutRobot.Transition()
        }

        fun openSearchSubMenu(interact: SettingsSubMenuSearchRobot.() -> Unit):
                SettingsSubMenuSearchRobot.Transition {

            fun searchEngineButton() = onView(ViewMatchers.withText("Search"))
            searchEngineButton().click()

            SettingsSubMenuSearchRobot().interact()
            return SettingsSubMenuSearchRobot.Transition()
        }

        fun openThemeSubMenu(interact: SettingsSubMenuThemeRobot.() -> Unit): SettingsSubMenuThemeRobot.Transition {

            fun themeButton() = onView(ViewMatchers.withText("Theme"))
            themeButton().click()

            SettingsSubMenuThemeRobot().interact()
            return SettingsSubMenuThemeRobot.Transition()
        }

        fun openAccessibilitySubMenu(interact: SettingsSubMenuAccessibilityRobot.() -> Unit): SettingsSubMenuAccessibilityRobot.Transition {

            fun accessibilityButton() = onView(ViewMatchers.withText("Accessibility"))
            accessibilityButton().click()

            SettingsSubMenuAccessibilityRobot().interact()
            return SettingsSubMenuAccessibilityRobot.Transition()
        }

        fun openDefaultBrowserSubMenu(interact: SettingsSubMenuDefaultBrowserRobot.() -> Unit): SettingsSubMenuDefaultBrowserRobot.Transition {

            fun defaultBrowserButton() = onView(ViewMatchers.withText("Set as default browser"))
            defaultBrowserButton().click()

            SettingsSubMenuDefaultBrowserRobot().interact()
            return SettingsSubMenuDefaultBrowserRobot.Transition()
        }

        fun openEnhancedTrackingProtectionSubMenu(interact: SettingsSubMenuEnhancedTrackingProtectionRobot.() -> Unit): SettingsSubMenuEnhancedTrackingProtectionRobot.Transition {
            fun enhancedTrackingProtectionButton() =
                onView(ViewMatchers.withText("Enhanced Tracking Protection"))
            enhancedTrackingProtectionButton().click()

            SettingsSubMenuEnhancedTrackingProtectionRobot().interact()
            return SettingsSubMenuEnhancedTrackingProtectionRobot.Transition()
        }

        fun openLoginsAndPasswordSubMenu(interact: SettingsSubMenuLoginsAndPasswordRobot.() -> Unit): SettingsSubMenuLoginsAndPasswordRobot.Transition {
            TestHelper.scrollToElementByText("Logins and passwords")
            fun loginsAndPasswordsButton() = onView(ViewMatchers.withText("Logins and passwords"))
            loginsAndPasswordsButton().click()

            SettingsSubMenuLoginsAndPasswordRobot().interact()
            return SettingsSubMenuLoginsAndPasswordRobot.Transition()
        }

        fun openTurnOnSyncMenu(interact: SettingsTurnOnSyncRobot.() -> Unit): SettingsTurnOnSyncRobot.Transition {
            fun turnOnSyncButton() = onView(ViewMatchers.withText("Turn on Sync"))
            turnOnSyncButton().click()

            SettingsTurnOnSyncRobot().interact()
            return SettingsTurnOnSyncRobot.Transition()
        }
    }
}

private fun assertSettingsView() {
    // verify that we are in the correct library view
    assertBasicsHeading()
    assertPrivacyHeading()
    assertDeveloperToolsHeading()
    assertAboutHeading()
}

// BASICS SECTION
private fun assertBasicsHeading() = onView(ViewMatchers.withText("Basics"))
    .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))

private fun assertSearchEngineButton() {
    mDevice.wait(Until.findObject(By.text("Search")), waitingTime)
    onView(ViewMatchers.withText("Search"))
        .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))
}

private fun assertThemeButton() = onView(ViewMatchers.withText("Theme"))
    .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))

private fun assertThemeSelected() = onView(ViewMatchers.withText("Light"))
    .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))

private fun assertAccessibilityButton() = onView(ViewMatchers.withText("Accessibility"))
    .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))

private fun assertSetAsDefaultBrowserButton() =
    onView(ViewMatchers.withText("Set as default browser"))
        .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))

// PRIVACY SECTION
private fun assertPrivacyHeading() {
    onView(ViewMatchers.withText("Privacy"))
        .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))
}

private fun assertEnhancedTrackingProtectionButton() {
    onView(ViewMatchers.withId(R.id.recycler_view)).perform(
        RecyclerViewActions.scrollTo<RecyclerView.ViewHolder>(
            ViewMatchers.hasDescendant(ViewMatchers.withText("Enhanced Tracking Protection"))
        )
    ).check(matches(withEffectiveVisibility(Visibility.VISIBLE)))
}

private fun assertEnhancedTrackingProtectionValue(state: String) =
    onView(ViewMatchers.withText(state)).check(matches(withEffectiveVisibility(Visibility.VISIBLE)))

private fun assertLoginsButton() {
    TestHelper.scrollToElementByText("Logins and passwords")
    onView(ViewMatchers.withText("Logins and passwords"))
        .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))
}

private fun assertAddPrivateBrowsingShortcutButton() {
    TestHelper.scrollToElementByText("Add private browsing shortcut")
    mDevice.wait(Until.findObject(By.text("Add private browsing shortcut")), waitingTime)
    onView(ViewMatchers.withText("Add private browsing shortcut"))
        .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))
}

private fun assertSitePermissionsButton() {
    TestHelper.scrollToElementByText("Site permissions")
    onView(ViewMatchers.withText("Site permissions"))
        .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))
}

private fun assertDeleteBrowsingDataButton() {
    TestHelper.scrollToElementByText("Delete browsing data")
    onView(ViewMatchers.withText("Delete browsing data"))
        .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))
}

private fun assertDeleteBrowsingDataOnQuitButton() {
    TestHelper.scrollToElementByText("Delete browsing data on quit")
    onView(ViewMatchers.withText("Delete browsing data on quit"))
        .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))
}

private fun assertDataCollectionButton() = onView(ViewMatchers.withText("Data collection"))
    .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))

private fun assertLeakCanaryButton() = onView(ViewMatchers.withText("LeakCanary"))
    .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))

// DEVELOPER TOOLS SECTION
private fun assertDeveloperToolsHeading() {
    TestHelper.scrollToElementByText("Developer tools")
    onView(ViewMatchers.withText("Developer tools"))
        .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))
}

private fun assertRemoteDebug() {
    TestHelper.scrollToElementByText("Remote debugging via USB")
    onView(ViewMatchers.withText("Remote debugging via USB"))
        .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))
}

// ABOUT SECTION
private fun assertAboutHeading(): ViewInteraction {
    TestHelper.scrollToElementByText("About")
    return onView(ViewMatchers.withText("About"))
        .check(matches(isCompletelyDisplayed()))
}

private fun assertRateOnGooglePlay(): ViewInteraction {
    TestHelper.scrollToElementByText("About Firefox Preview")
    return onView(ViewMatchers.withText("Rate on Google Play"))
        .check(matches(isCompletelyDisplayed()))
}

private fun assertAboutFirefoxPreview(): ViewInteraction {
    TestHelper.scrollToElementByText("About Firefox Preview")
    return onView(ViewMatchers.withText("About Firefox Preview"))
        .check(matches(isCompletelyDisplayed()))
}

fun swipeToBottom() = onView(ViewMatchers.withId(R.id.recycler_view)).perform(ViewActions.swipeUp())

fun clickRateButtonGooglePlay() {
    assertRateOnGooglePlay().click()
}

private fun assertGooglePlayRedirect() {
    if (isPackageInstalled(GOOGLE_PLAY_SERVICES)) {
        intended(toPackage(GOOGLE_PLAY_SERVICES))
    } else {
        BrowserRobot().verifyRateOnGooglePlayURL()
    }
}

fun isPackageInstalled(packageName: String): Boolean {
    return try {
        val packageManager = InstrumentationRegistry.getInstrumentation().context.packageManager
        packageManager.getApplicationInfo(packageName, 0).enabled
    } catch (exception: PackageManager.NameNotFoundException) {
        false
    }
}

private fun goBackButton() =
    onView(CoreMatchers.allOf(ViewMatchers.withContentDescription("Navigate up")))
