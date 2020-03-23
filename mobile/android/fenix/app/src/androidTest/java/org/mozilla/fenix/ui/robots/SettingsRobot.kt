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
import androidx.test.espresso.matcher.ViewMatchers.hasDescendant
import androidx.test.espresso.matcher.ViewMatchers.isCompletelyDisplayed
import androidx.test.espresso.matcher.ViewMatchers.withEffectiveVisibility
import androidx.test.espresso.matcher.ViewMatchers.withId
import androidx.test.espresso.matcher.ViewMatchers.withText
import androidx.test.platform.app.InstrumentationRegistry
import androidx.test.uiautomator.By
import androidx.test.uiautomator.By.textContains
import androidx.test.uiautomator.UiDevice
import androidx.test.uiautomator.Until
import org.hamcrest.CoreMatchers
import org.mozilla.fenix.R
import org.mozilla.fenix.helpers.Constants.PackageName.GOOGLE_PLAY_SERVICES
import org.mozilla.fenix.helpers.TestAssetHelper.waitingTime
import org.mozilla.fenix.helpers.TestHelper.scrollToElementByText
import org.mozilla.fenix.helpers.click

/**
 * Implementation of Robot Pattern for the settings menu.
 */
class SettingsRobot {

    // BASICS SECTION
    fun verifyBasicsHeading() = assertGeneralHeading()

    fun verifySearchEngineButton() = assertSearchEngineButton()
    fun verifyThemeButton() = assertCustomizeButton()
    fun verifyThemeSelected() = assertThemeSelected()
    fun verifyAccessibilityButton() = assertAccessibilityButton()
    fun verifySetAsDefaultBrowserButton() = assertSetAsDefaultBrowserButton()

    // PRIVACY SECTION
    fun verifyPrivacyHeading() = assertPrivacyHeading()

    fun verifyEnhancedTrackingProtectionButton() = assertEnhancedTrackingProtectionButton()
    fun verifyLoginsButton() = assertLoginsButton()
    fun verifyEnhancedTrackingProtectionValue(state: String) =
        assertEnhancedTrackingProtectionValue(state)

    fun verifyPrivateBrowsingButton() = assertPrivateBrowsingButton()
    fun verifySitePermissionsButton() = assertSitePermissionsButton()
    fun verifyDeleteBrowsingDataButton() = assertDeleteBrowsingDataButton()
    fun verifyDeleteBrowsingDataOnQuitButton() = assertDeleteBrowsingDataOnQuitButton()
    fun verifyDataCollectionButton() = assertDataCollectionButton()
    fun verifyLeakCanaryButton() = assertLeakCanaryButton()
    fun verifySettingsView() = assertSettingsView()

    // ADVANCED SECTION
    fun verifyAdvancedHeading() = assertAdvancedHeading()
    fun verifyAddons() = assertAddons()
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

            fun searchEngineButton() = onView(withText("Search"))
            searchEngineButton().click()

            SettingsSubMenuSearchRobot().interact()
            return SettingsSubMenuSearchRobot.Transition()
        }

        fun openCustomizeSubMenu(interact: SettingsSubMenuThemeRobot.() -> Unit): SettingsSubMenuThemeRobot.Transition {

            fun customizeButton() = onView(withText("Customize"))
            customizeButton().click()

            SettingsSubMenuThemeRobot().interact()
            return SettingsSubMenuThemeRobot.Transition()
        }

        fun openAccessibilitySubMenu(interact: SettingsSubMenuAccessibilityRobot.() -> Unit): SettingsSubMenuAccessibilityRobot.Transition {

            fun accessibilityButton() = onView(withText("Accessibility"))
            accessibilityButton().click()

            SettingsSubMenuAccessibilityRobot().interact()
            return SettingsSubMenuAccessibilityRobot.Transition()
        }

        fun openDefaultBrowserSubMenu(interact: SettingsSubMenuDefaultBrowserRobot.() -> Unit): SettingsSubMenuDefaultBrowserRobot.Transition {

            fun defaultBrowserButton() = onView(withText("Set as default browser"))
            defaultBrowserButton().click()

            SettingsSubMenuDefaultBrowserRobot().interact()
            return SettingsSubMenuDefaultBrowserRobot.Transition()
        }

        fun openEnhancedTrackingProtectionSubMenu(interact: SettingsSubMenuEnhancedTrackingProtectionRobot.() -> Unit): SettingsSubMenuEnhancedTrackingProtectionRobot.Transition {
            fun enhancedTrackingProtectionButton() =
                onView(withText("Enhanced Tracking Protection"))
            enhancedTrackingProtectionButton().click()

            SettingsSubMenuEnhancedTrackingProtectionRobot().interact()
            return SettingsSubMenuEnhancedTrackingProtectionRobot.Transition()
        }

        fun openLoginsAndPasswordSubMenu(interact: SettingsSubMenuLoginsAndPasswordRobot.() -> Unit): SettingsSubMenuLoginsAndPasswordRobot.Transition {
            scrollToElementByText("Logins and passwords")
            fun loginsAndPasswordsButton() = onView(withText("Logins and passwords"))
            loginsAndPasswordsButton().click()

            SettingsSubMenuLoginsAndPasswordRobot().interact()
            return SettingsSubMenuLoginsAndPasswordRobot.Transition()
        }

        fun openTurnOnSyncMenu(interact: SettingsTurnOnSyncRobot.() -> Unit): SettingsTurnOnSyncRobot.Transition {
            fun turnOnSyncButton() = onView(withText("Turn on Sync"))
            turnOnSyncButton().click()

            SettingsTurnOnSyncRobot().interact()
            return SettingsTurnOnSyncRobot.Transition()
        }

        fun openPrivateBrowsingSubMenu(interact: SettingsSubMenuPrivateBrowsingRobot.() -> Unit): SettingsSubMenuPrivateBrowsingRobot.Transition {
            scrollToElementByText("Private browsing")
            fun privateBrowsingButton() = mDevice.findObject(textContains("Private browsing"))
            privateBrowsingButton().click()

            SettingsSubMenuPrivateBrowsingRobot().interact()
            return SettingsSubMenuPrivateBrowsingRobot.Transition()
        }
    }
}

private fun assertSettingsView() {
    // verify that we are in the correct library view
    assertGeneralHeading()
    assertPrivacyHeading()
    assertAdvancedHeading()
    assertAboutHeading()
}

// GENERAL SECTION
private fun assertGeneralHeading() = onView(withText("General"))
    .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))

private fun assertSearchEngineButton() {
    mDevice.wait(Until.findObject(By.text("Search")), waitingTime)
    onView(withText("Search"))
        .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))
}

private fun assertCustomizeButton() = onView(withText("Customize"))
    .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))

private fun assertThemeSelected() = onView(withText("Light"))
    .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))

private fun assertAccessibilityButton() = onView(withText("Accessibility"))
    .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))

private fun assertSetAsDefaultBrowserButton() =
    onView(withText("Set as default browser"))
        .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))

// PRIVACY SECTION
private fun assertPrivacyHeading() {
    onView(withText("Privacy and security"))
        .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))
}

private fun assertEnhancedTrackingProtectionButton() {
    mDevice.wait(Until.findObject(By.text("Privacy and Security")), waitingTime)
    onView(withId(R.id.recycler_view)).perform(
        RecyclerViewActions.scrollTo<RecyclerView.ViewHolder>(
            hasDescendant(withText("Enhanced Tracking Protection"))
        )
    ).check(matches(withEffectiveVisibility(Visibility.VISIBLE)))
}

private fun assertEnhancedTrackingProtectionValue(state: String) {
    mDevice.wait(Until.findObject(By.text("Enhanced Tracking Protection")), waitingTime)
    onView(withText(state)).check(matches(withEffectiveVisibility(Visibility.VISIBLE)))
}

private fun assertLoginsButton() {
    scrollToElementByText("Logins and passwords")
    onView(withText("Logins and passwords"))
        .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))
}

private fun assertPrivateBrowsingButton() {
    scrollToElementByText("Private browsing")
    mDevice.wait(Until.findObject(By.text("Private browsing")), waitingTime)
    onView(withText("Private browsing"))
        .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))
}

private fun assertSitePermissionsButton() {
    scrollToElementByText("Site permissions")
    onView(withText("Site permissions"))
        .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))
}

private fun assertDeleteBrowsingDataButton() {
    scrollToElementByText("Delete browsing data")
    onView(withText("Delete browsing data"))
        .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))
}

private fun assertDeleteBrowsingDataOnQuitButton() {
    scrollToElementByText("Delete browsing data on quit")
    onView(withText("Delete browsing data on quit"))
        .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))
}

private fun assertDataCollectionButton() = onView(withText("Data collection"))
    .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))

private fun assertLeakCanaryButton() {
    scrollToElementByText("LeakCanary")
    onView(withText("LeakCanary"))
        .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))
}

// ADVANCED SECTION
private fun assertAdvancedHeading() {
    scrollToElementByText("Advanced")
    onView(withText("Advanced"))
        .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))
}

private fun assertAddons() {
    scrollToElementByText("Add-ons")
    onView(withText("Add-ons"))
        .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))
}

private fun assertRemoteDebug() {
    scrollToElementByText("Remote debugging via USB")
    onView(withText("Remote debugging via USB"))
        .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))
}

// ABOUT SECTION
private fun assertAboutHeading(): ViewInteraction {
    scrollToElementByText("About")
    return onView(withText("About"))
        .check(matches(isCompletelyDisplayed()))
}

private fun assertRateOnGooglePlay(): ViewInteraction {
    scrollToElementByText("About Firefox Preview")
    return onView(withText("Rate on Google Play"))
        .check(matches(isCompletelyDisplayed()))
}

private fun assertAboutFirefoxPreview(): ViewInteraction {
    scrollToElementByText("About Firefox Preview")
    return onView(withText("About Firefox Preview"))
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
