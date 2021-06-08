/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

@file:Suppress("TooManyFunctions")

package org.mozilla.fenix.ui.robots

import android.content.pm.PackageManager
import androidx.recyclerview.widget.RecyclerView
import androidx.test.espresso.Espresso.onView
import androidx.test.espresso.ViewInteraction
import androidx.test.espresso.action.ViewActions
import androidx.test.espresso.assertion.ViewAssertions.matches
import androidx.test.espresso.contrib.RecyclerViewActions
import androidx.test.espresso.intent.Intents.intended
import androidx.test.espresso.intent.matcher.IntentMatchers
import androidx.test.espresso.intent.matcher.IntentMatchers.toPackage
import androidx.test.espresso.matcher.ViewMatchers
import androidx.test.espresso.matcher.ViewMatchers.Visibility
import androidx.test.espresso.matcher.ViewMatchers.hasDescendant
import androidx.test.espresso.matcher.ViewMatchers.isCompletelyDisplayed
import androidx.test.espresso.matcher.ViewMatchers.isDisplayed
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
import org.mozilla.fenix.helpers.TestHelper.appName
import org.mozilla.fenix.helpers.TestHelper.scrollToElementByText
import org.mozilla.fenix.helpers.assertIsEnabled
import org.mozilla.fenix.helpers.click
import org.mozilla.fenix.ui.robots.SettingsRobot.Companion.DEFAULT_APPS_SETTINGS_ACTION

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
    fun verifyDefaultBrowserItem() = assertDefaultBrowserItem()
    fun verifyTabsItem() = assertTabsItem()
    fun verifyDefaultBrowserIsDisaled() = assertDefaultBrowserIsDisabled()
    fun clickDefaultBrowserSwitch() = toggleDefaultBrowserSwitch()
    fun verifyAndroidDefaultAppsMenuAppears() = assertAndroidDefaultAppsMenuAppears()

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
    fun verifyDeleteBrowsingDataOnQuitValue(state: String) =
        assertDeleteBrowsingDataValue(state)
    fun verifyNotificationsButton() = assertNotificationsButton()
    fun verifyDataCollectionButton() = assertDataCollectionButton()
    fun verifyOpenLinksInAppsButton() = assertOpenLinksInAppsButton()
    fun verifyOpenLinksInAppsSwitchDefault() = assertOpenLinksInAppsValue()
    fun verifySettingsView() = assertSettingsView()

    // ADVANCED SECTION
    fun verifyAdvancedHeading() = assertAdvancedHeading()
    fun verifyAddons() = assertAddonsButton()

    // DEVELOPER TOOLS SECTION
    fun verifyRemoteDebug() = assertRemoteDebug()
    fun verifyLeakCanaryButton() = assertLeakCanaryButton()

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

        fun goBackToBrowser(interact: BrowserRobot.() -> Unit): BrowserRobot.Transition {
            goBackButton().click()

            BrowserRobot().interact()
            return BrowserRobot.Transition()
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

        fun openTabsSubMenu(interact: SettingsSubMenuTabsRobot.() -> Unit): SettingsSubMenuTabsRobot.Transition {

            fun tabsButton() = onView(withText("Tabs"))
            tabsButton().click()

            SettingsSubMenuTabsRobot().interact()
            return SettingsSubMenuTabsRobot.Transition()
        }

        fun openAccessibilitySubMenu(interact: SettingsSubMenuAccessibilityRobot.() -> Unit): SettingsSubMenuAccessibilityRobot.Transition {
            scrollToElementByText("Accessibility")

            fun accessibilityButton() = onView(withText("Accessibility"))
            accessibilityButton()
                .check(matches(isDisplayed()))
                .click()

            SettingsSubMenuAccessibilityRobot().interact()
            return SettingsSubMenuAccessibilityRobot.Transition()
        }

        fun openLanguageSubMenu(interact: SettingsSubMenuLanguageRobot.() -> Unit): SettingsSubMenuLanguageRobot.Transition {
            scrollToElementByText("Language")

            fun languageButton() = onView(withText("Language"))
            languageButton().click()

            SettingsSubMenuLanguageRobot().interact()
            return SettingsSubMenuLanguageRobot.Transition()
        }

        fun openSetDefaultBrowserSubMenu(interact: SettingsSubMenuSetDefaultBrowserRobot.() -> Unit): SettingsSubMenuSetDefaultBrowserRobot.Transition {
            scrollToElementByText("Set as default browser")
            fun setDefaultBrowserButton() = onView(withText("Set as default browser"))
            setDefaultBrowserButton().click()

            SettingsSubMenuSetDefaultBrowserRobot().interact()
            return SettingsSubMenuSetDefaultBrowserRobot.Transition()
        }

        fun openEnhancedTrackingProtectionSubMenu(interact: SettingsSubMenuEnhancedTrackingProtectionRobot.() -> Unit): SettingsSubMenuEnhancedTrackingProtectionRobot.Transition {
            scrollToElementByText("Enhanced Tracking Protection")
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

        fun openSettingsSubMenuSitePermissions(interact: SettingsSubMenuSitePermissionsRobot.() -> Unit): SettingsSubMenuSitePermissionsRobot.Transition {
            scrollToElementByText("Site permissions")
            fun sitePermissionButton() = mDevice.findObject(textContains("Site permissions"))
            sitePermissionButton().click()

            SettingsSubMenuSitePermissionsRobot().interact()
            return SettingsSubMenuSitePermissionsRobot.Transition()
        }

        fun openSettingsSubMenuDeleteBrowsingData(interact: SettingsSubMenuDeleteBrowsingDataRobot.() -> Unit): SettingsSubMenuDeleteBrowsingDataRobot.Transition {
            scrollToElementByText("Delete browsing data")
            fun deleteBrowsingDataButton() = mDevice.findObject(textContains("Delete browsing data"))
            deleteBrowsingDataButton().click()

            SettingsSubMenuDeleteBrowsingDataRobot().interact()
            return SettingsSubMenuDeleteBrowsingDataRobot.Transition()
        }

        fun openSettingsSubMenuDeleteBrowsingDataOnQuit(interact: SettingsSubMenuDeleteBrowsingDataOnQuitRobot.() -> Unit): SettingsSubMenuDeleteBrowsingDataOnQuitRobot.Transition {
            scrollToElementByText("Delete browsing data on quit")
            fun deleteBrowsingDataOnQuitButton() = mDevice.findObject(textContains("Delete browsing data on quit"))
            deleteBrowsingDataOnQuitButton().click()

            SettingsSubMenuDeleteBrowsingDataOnQuitRobot().interact()
            return SettingsSubMenuDeleteBrowsingDataOnQuitRobot.Transition()
        }

        fun openSettingsSubMenuNotifications(interact: SystemSettingsRobot.() -> Unit): SystemSettingsRobot.Transition {
            scrollToElementByText("Notifications")
            fun notificationsButton() = mDevice.findObject(textContains("Notifications"))
            notificationsButton().click()

            SystemSettingsRobot().interact()
            return SystemSettingsRobot.Transition()
        }

        fun openSettingsSubMenuDataCollection(interact: SettingsSubMenuDataCollectionRobot.() -> Unit): SettingsSubMenuDataCollectionRobot.Transition {
            scrollToElementByText("Data collection")
            fun dataCollectionButton() = mDevice.findObject(textContains("Data collection"))
            dataCollectionButton().click()

            SettingsSubMenuDataCollectionRobot().interact()
            return SettingsSubMenuDataCollectionRobot.Transition()
        }

        fun openAddonsManagerMenu(interact: SettingsSubMenuAddonsManagerRobot.() -> Unit): SettingsSubMenuAddonsManagerRobot.Transition {
            addonsManagerButton().click()

            SettingsSubMenuAddonsManagerRobot().interact()
            return SettingsSubMenuAddonsManagerRobot.Transition()
        }
    }

    companion object {
        const val DEFAULT_APPS_SETTINGS_ACTION = "android.settings.MANAGE_DEFAULT_APPS_SETTINGS"
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
private fun assertGeneralHeading() {
    scrollToElementByText("General")
    onView(withText("General"))
        .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))
}

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

private fun assertSetAsDefaultBrowserButton() {
    scrollToElementByText("Set as default browser")
    onView(withText("Set as default browser"))
        .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))
}

private fun assertDefaultBrowserIsDisabled() {
    scrollToElementByText("Set as default browser")
    onView(withId(R.id.switch_widget))
        .check(matches(ViewMatchers.isNotChecked()))
}

private fun toggleDefaultBrowserSwitch() {
    scrollToElementByText("Privacy and security")
    onView(withText("Set as default browser")).perform(ViewActions.click())
}

private fun assertAndroidDefaultAppsMenuAppears() {
    intended(IntentMatchers.hasAction(DEFAULT_APPS_SETTINGS_ACTION))
}

private fun assertDefaultBrowserItem() {
    mDevice.wait(Until.findObject(By.text("Set as default browser")), waitingTime)
    onView(withText("Set as default browser"))
        .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))
}

private fun assertTabsItem() {
    mDevice.wait(Until.findObject(By.text("Tabs")), waitingTime)
    onView(withText("Tabs"))
        .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))
}

// PRIVACY SECTION
private fun assertPrivacyHeading() {
    scrollToElementByText("Privacy and security")
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

private fun assertDeleteBrowsingDataValue(state: String) {
    mDevice.wait(Until.findObject(By.text("Delete browsing data on quit")), waitingTime)
    onView(withText(state)).check(matches(withEffectiveVisibility(Visibility.VISIBLE)))
}

private fun assertNotificationsButton() {
    scrollToElementByText("Notifications")
    onView(withText("Notifications"))
        .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))
}

private fun assertDataCollectionButton() {
    scrollToElementByText("Data collection")
    onView(withText("Data collection"))
    .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))
}

private fun openLinksInAppsButton() = onView(withText("Open links in apps"))

private fun assertOpenLinksInAppsButton() {
    scrollToElementByText("Open links in apps")
    openLinksInAppsButton()
        .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))
}

private fun assertOpenLinksInAppsValue() {
    scrollToElementByText("Open links in apps")
    openLinksInAppsButton().assertIsEnabled(isEnabled = true)
}

// DEVELOPER TOOLS SECTION
private fun assertDeveloperToolsHeading() {
    scrollToElementByText("Developer tools")
    onView(withText("Developer tools"))
}

// ADVANCED SECTION
private fun assertAdvancedHeading() {
    onView(withId(R.id.recycler_view)).perform(
        RecyclerViewActions.scrollTo<RecyclerView.ViewHolder>(
            hasDescendant(withText("Add-ons"))
        )
    )

    onView(withText("Add-ons"))
        .check(matches(isCompletelyDisplayed()))
}

private fun assertAddonsButton() {
    onView(withId(R.id.recycler_view)).perform(
        RecyclerViewActions.scrollTo<RecyclerView.ViewHolder>(
            hasDescendant(withText("Add-ons"))
        )
    )

    addonsManagerButton()
        .check(matches(isCompletelyDisplayed()))
}

private fun assertRemoteDebug() {
    scrollToElementByText("Remote debugging via USB")
    onView(withText("Remote debugging via USB"))
        .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))
}

private fun assertLeakCanaryButton() {
    scrollToElementByText("LeakCanary")
    onView(withText("LeakCanary"))
        .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))
}

// ABOUT SECTION
private fun assertAboutHeading(): ViewInteraction {
    scrollToElementByText("About")
    return onView(withText("About"))
        .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))
}

private fun assertRateOnGooglePlay(): ViewInteraction {
    onView(withId(R.id.recycler_view))
        .perform(RecyclerViewActions.scrollTo<RecyclerView.ViewHolder>(hasDescendant(withText("Rate on Google Play"))))
    return onView(withText("Rate on Google Play"))
        .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))
}

private fun assertAboutFirefoxPreview(): ViewInteraction {
    onView(withId(R.id.recycler_view))
        .perform(RecyclerViewActions.scrollTo<RecyclerView.ViewHolder>(hasDescendant(withText("About $appName"))))
    return onView(withText("About $appName"))
        .check(matches(isDisplayed()))
}

fun swipeToBottom() = onView(withId(R.id.recycler_view)).perform(ViewActions.swipeUp())

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

private fun addonsManagerButton() = onView(withText(R.string.preferences_addons))

private fun goBackButton() =
    onView(CoreMatchers.allOf(ViewMatchers.withContentDescription("Navigate up")))
