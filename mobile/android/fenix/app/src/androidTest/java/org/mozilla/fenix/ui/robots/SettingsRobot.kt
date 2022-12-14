/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

@file:Suppress("TooManyFunctions")

package org.mozilla.fenix.ui.robots

import android.content.Intent
import android.net.Uri
import androidx.recyclerview.widget.RecyclerView
import androidx.test.espresso.Espresso.onView
import androidx.test.espresso.ViewInteraction
import androidx.test.espresso.action.ViewActions
import androidx.test.espresso.assertion.ViewAssertions.matches
import androidx.test.espresso.contrib.RecyclerViewActions
import androidx.test.espresso.intent.Intents.intended
import androidx.test.espresso.intent.matcher.IntentMatchers
import androidx.test.espresso.intent.matcher.IntentMatchers.hasAction
import androidx.test.espresso.intent.matcher.IntentMatchers.hasData
import androidx.test.espresso.matcher.ViewMatchers
import androidx.test.espresso.matcher.ViewMatchers.Visibility
import androidx.test.espresso.matcher.ViewMatchers.hasDescendant
import androidx.test.espresso.matcher.ViewMatchers.hasSibling
import androidx.test.espresso.matcher.ViewMatchers.isChecked
import androidx.test.espresso.matcher.ViewMatchers.isCompletelyDisplayed
import androidx.test.espresso.matcher.ViewMatchers.isDisplayed
import androidx.test.espresso.matcher.ViewMatchers.isNotChecked
import androidx.test.espresso.matcher.ViewMatchers.withClassName
import androidx.test.espresso.matcher.ViewMatchers.withEffectiveVisibility
import androidx.test.espresso.matcher.ViewMatchers.withId
import androidx.test.espresso.matcher.ViewMatchers.withText
import androidx.test.uiautomator.By
import androidx.test.uiautomator.By.textContains
import androidx.test.uiautomator.UiObject
import androidx.test.uiautomator.UiScrollable
import androidx.test.uiautomator.UiSelector
import androidx.test.uiautomator.Until
import junit.framework.AssertionFailedError
import org.hamcrest.CoreMatchers
import org.hamcrest.Matchers.allOf
import org.hamcrest.Matchers.endsWith
import org.junit.Assert.assertTrue
import org.mozilla.fenix.R
import org.mozilla.fenix.helpers.Constants.LISTS_MAXSWIPES
import org.mozilla.fenix.helpers.Constants.PackageName.GOOGLE_PLAY_SERVICES
import org.mozilla.fenix.helpers.Constants.RETRY_COUNT
import org.mozilla.fenix.helpers.MatcherHelper.itemContainingText
import org.mozilla.fenix.helpers.TestAssetHelper.waitingTime
import org.mozilla.fenix.helpers.TestHelper.appName
import org.mozilla.fenix.helpers.TestHelper.getStringResource
import org.mozilla.fenix.helpers.TestHelper.hasCousin
import org.mozilla.fenix.helpers.TestHelper.isPackageInstalled
import org.mozilla.fenix.helpers.TestHelper.mDevice
import org.mozilla.fenix.helpers.TestHelper.packageName
import org.mozilla.fenix.helpers.TestHelper.scrollToElementByText
import org.mozilla.fenix.helpers.click
import org.mozilla.fenix.settings.SupportUtils
import org.mozilla.fenix.ui.robots.SettingsRobot.Companion.DEFAULT_APPS_SETTINGS_ACTION

/**
 * Implementation of Robot Pattern for the settings menu.
 */
class SettingsRobot {

    // BASICS SECTION
    fun verifyGeneralHeading() = assertGeneralHeading()

    fun verifySearchButton() = assertSearchButton()
    fun verifyCustomizeButton() = assertCustomizeButton()
    fun verifyThemeSelected() = assertThemeSelected()
    fun verifyAccessibilityButton() = assertAccessibilityButton()
    fun verifySetAsDefaultBrowserButton() = assertSetAsDefaultBrowserButton()
    fun verifyTabsButton() = assertTabsButton()
    fun verifyHomepageButton() = assertHomepageButton()
    fun verifyAutofillButton() = assertAutofillButton()
    fun verifyLanguageButton() = assertLanguageButton()
    fun verifyDefaultBrowserIsDisabled() = assertDefaultBrowserIsDisabled()
    fun clickDefaultBrowserSwitch() = toggleDefaultBrowserSwitch()
    fun verifyAndroidDefaultAppsMenuAppears() = assertAndroidDefaultAppsMenuAppears()

    // PRIVACY SECTION
    fun verifyPrivacyHeading() = assertPrivacyHeading()

    fun verifyHTTPSOnlyModeButton() = assertHTTPSOnlyModeButton()
    fun verifyHTTPSOnlyModeState(state: String) = assertHTTPSOnlyModeState(state)
    fun verifyEnhancedTrackingProtectionButton() = assertEnhancedTrackingProtectionButton()
    fun verifyLoginsAndPasswordsButton() = assertLoginsAndPasswordsButton()
    fun verifyEnhancedTrackingProtectionState(state: String) =
        assertEnhancedTrackingProtectionState(state)
    fun verifyPrivateBrowsingButton() = assertPrivateBrowsingButton()
    fun verifySitePermissionsButton() = assertSitePermissionsButton()
    fun verifyDeleteBrowsingDataButton() = assertDeleteBrowsingDataButton()
    fun verifyDeleteBrowsingDataOnQuitButton() = assertDeleteBrowsingDataOnQuitButton()
    fun verifyDeleteBrowsingDataOnQuitState(state: String) =
        assertDeleteBrowsingDataState(state)
    fun verifyNotificationsButton() = assertNotificationsButton()
    fun verifyDataCollectionButton() = assertDataCollectionButton()
    fun verifyOpenLinksInAppsButton() = assertOpenLinksInAppsButton()
    fun verifyOpenLinksInAppsSwitchState(enabled: Boolean) = assertOpenLinksInAppsSwitchState(enabled)
    fun clickOpenLinksInAppsSwitch() = openLinksInAppsButton().click()
    fun verifySettingsView() = assertSettingsView()
    fun verifySettingsToolbar() = assertSettingsToolbar()

    // ADVANCED SECTION
    fun verifyAdvancedHeading() = assertAdvancedHeading()
    fun verifyAddons() = assertAddonsButton()

    // DEVELOPER TOOLS SECTION
    fun verifyRemoteDebug() = assertRemoteDebug()
    fun verifyLeakCanaryButton() = assertLeakCanaryButton()

    // ABOUT SECTION
    fun verifyAboutHeading() = assertAboutHeading()

    fun verifyRateOnGooglePlay() = assertTrue(rateOnGooglePlayHeading().waitForExists(waitingTime))
    fun verifyAboutFirefoxPreview() = assertTrue(aboutFirefoxHeading().waitForExists(waitingTime))
    fun verifyGooglePlayRedirect() = assertGooglePlayRedirect()

    class Transition {
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
            aboutFirefoxHeading().click()
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

        fun openCustomizeSubMenu(interact: SettingsSubMenuCustomizeRobot.() -> Unit): SettingsSubMenuCustomizeRobot.Transition {
            fun customizeButton() = onView(withText("Customize"))
            customizeButton().click()

            SettingsSubMenuCustomizeRobot().interact()
            return SettingsSubMenuCustomizeRobot.Transition()
        }

        fun openTabsSubMenu(interact: SettingsSubMenuTabsRobot.() -> Unit): SettingsSubMenuTabsRobot.Transition {
            fun tabsButton() = onView(withText("Tabs"))
            tabsButton().click()

            SettingsSubMenuTabsRobot().interact()
            return SettingsSubMenuTabsRobot.Transition()
        }

        fun openHomepageSubMenu(interact: SettingsSubMenuHomepageRobot.() -> Unit): SettingsSubMenuHomepageRobot.Transition {
            mDevice.findObject(UiSelector().textContains("Homepage")).waitForExists(waitingTime)
            onView(withText(R.string.preferences_home_2)).click()

            SettingsSubMenuHomepageRobot().interact()
            return SettingsSubMenuHomepageRobot.Transition()
        }

        fun openAutofillSubMenu(interact: SettingsSubMenuAutofillRobot.() -> Unit): SettingsSubMenuAutofillRobot.Transition {
            mDevice.findObject(UiSelector().textContains(getStringResource(R.string.preferences_autofill))).waitForExists(waitingTime)
            onView(withText(R.string.preferences_autofill)).click()

            SettingsSubMenuAutofillRobot().interact()
            return SettingsSubMenuAutofillRobot.Transition()
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

        fun openLanguageSubMenu(
            localizedText: String = getStringResource(R.string.preferences_language),
            interact: SettingsSubMenuLanguageRobot.() -> Unit,
        ): SettingsSubMenuLanguageRobot.Transition {
            onView(withId(R.id.recycler_view))
                .perform(
                    RecyclerViewActions.actionOnItem<RecyclerView.ViewHolder>(
                        hasDescendant(
                            withText(localizedText),
                        ),
                        ViewActions.click(),
                    ),
                )

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

        fun openCookieBannerReductionSubMenu(interact: SettingsSubMenuCookieBannerReductionRobot.() -> Unit): SettingsSubMenuCookieBannerReductionRobot.Transition {
            scrollToElementByText(getStringResource(R.string.preferences_cookie_banner_reduction))
            itemContainingText(getStringResource(R.string.preferences_cookie_banner_reduction)).click()

            SettingsSubMenuCookieBannerReductionRobot().interact()
            return SettingsSubMenuCookieBannerReductionRobot.Transition()
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
            fun turnOnSyncButton() = onView(withText("Sync and save your data"))
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
        const val DEFAULT_APPS_SETTINGS_ACTION = "android.app.role.action.REQUEST_ROLE"
    }
}

fun settingsScreen(interact: SettingsRobot.() -> Unit): SettingsRobot.Transition {
    SettingsRobot().interact()
    return SettingsRobot.Transition()
}

private fun assertSettingsView() {
    // verify that we are in the correct library view
    assertGeneralHeading()
    assertPrivacyHeading()
    assertAdvancedHeading()
    assertAboutHeading()
}

// GENERAL SECTION

private fun assertSettingsToolbar() =
    onView(
        CoreMatchers.allOf(
            withId(R.id.navigationToolbar),
            hasDescendant(ViewMatchers.withContentDescription(R.string.action_bar_up_description)),
            hasDescendant(withText(R.string.settings)),
        ),
    ).check(matches(withEffectiveVisibility(Visibility.VISIBLE)))

private fun assertGeneralHeading() {
    scrollToElementByText("General")
    onView(withText("General"))
        .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))
}

private fun assertSearchButton() {
    mDevice.wait(Until.findObject(By.text("Search")), waitingTime)
    onView(withText(R.string.preferences_search))
        .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))
}

private fun assertHomepageButton() =
    onView(withText(R.string.preferences_home_2)).check(matches(withEffectiveVisibility(Visibility.VISIBLE)))

private fun assertAutofillButton() =
    onView(withText(R.string.preferences_autofill)).check(matches(withEffectiveVisibility(Visibility.VISIBLE)))

private fun assertLanguageButton() =
    onView(withText(R.string.preferences_language)).check(matches(withEffectiveVisibility(Visibility.VISIBLE)))

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

private fun assertTabsButton() {
    mDevice.wait(Until.findObject(By.text("Tabs")), waitingTime)
    onView(withText(R.string.preferences_tabs))
        .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))
}

// PRIVACY SECTION
private fun assertPrivacyHeading() {
    scrollToElementByText("Privacy and security")
    onView(withText("Privacy and security"))
        .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))
}

private fun assertHTTPSOnlyModeButton() {
    scrollToElementByText(getStringResource(R.string.preferences_https_only_title))
    onView(
        withText(R.string.preferences_https_only_title),
    ).check(matches(withEffectiveVisibility(Visibility.VISIBLE)))
}

private fun assertHTTPSOnlyModeState(state: String) {
    onView(
        allOf(
            withText(R.string.preferences_https_only_title),
            hasSibling(withText(state)),
        ),
    ).check(matches(withEffectiveVisibility(Visibility.VISIBLE)))
}

private fun assertEnhancedTrackingProtectionButton() {
    mDevice.wait(Until.findObject(By.text("Privacy and Security")), waitingTime)
    onView(withId(R.id.recycler_view)).perform(
        RecyclerViewActions.scrollTo<RecyclerView.ViewHolder>(
            hasDescendant(withText("Enhanced Tracking Protection")),
        ),
    ).check(matches(withEffectiveVisibility(Visibility.VISIBLE)))
}

private fun assertEnhancedTrackingProtectionState(state: String) {
    mDevice.wait(Until.findObject(By.text("Enhanced Tracking Protection")), waitingTime)
    onView(withText(state)).check(matches(withEffectiveVisibility(Visibility.VISIBLE)))
}

private fun assertLoginsAndPasswordsButton() {
    scrollToElementByText("Logins and passwords")
    onView(withText(R.string.preferences_passwords_logins_and_passwords))
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

private fun assertDeleteBrowsingDataState(state: String) {
    onView(
        allOf(
            withText(R.string.preferences_delete_browsing_data_on_quit),
            hasSibling(withText(state)),
        ),
    ).check(matches(withEffectiveVisibility(Visibility.VISIBLE)))
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

private fun openLinksInAppsButton() = onView(withText(R.string.preferences_open_links_in_apps))

private fun assertOpenLinksInAppsButton() {
    scrollToElementByText("Open links in apps")
    openLinksInAppsButton()
        .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))
}

fun assertOpenLinksInAppsSwitchState(enabled: Boolean) {
    scrollToElementByText("Open links in apps")
    if (enabled) {
        openLinksInAppsButton()
            .check(
                matches(
                    hasCousin(
                        allOf(
                            withClassName(endsWith("Switch")),
                            isChecked(),
                        ),
                    ),
                ),
            )
    } else {
        openLinksInAppsButton()
            .check(
                matches(
                    hasCousin(
                        allOf(
                            withClassName(endsWith("Switch")),
                            isNotChecked(),
                        ),
                    ),
                ),
            )
    }
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
            hasDescendant(withText("Add-ons")),
        ),
    )

    onView(withText("Add-ons"))
        .check(matches(isCompletelyDisplayed()))
}

private fun assertAddonsButton() {
    onView(withId(R.id.recycler_view)).perform(
        RecyclerViewActions.scrollTo<RecyclerView.ViewHolder>(
            hasDescendant(withText("Add-ons")),
        ),
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
    settingsList().scrollToEnd(LISTS_MAXSWIPES)
    return onView(withText("About"))
        .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))
}

private fun rateOnGooglePlayHeading(): UiObject {
    val rateOnGooglePlay = mDevice.findObject(UiSelector().text("Rate on Google Play"))
    settingsList().scrollToEnd(LISTS_MAXSWIPES)
    rateOnGooglePlay.waitForExists(waitingTime)

    return rateOnGooglePlay
}

private fun aboutFirefoxHeading(): UiObject {
    for (i in 1..RETRY_COUNT) {
        try {
            settingsList().scrollToEnd(LISTS_MAXSWIPES)
            assertTrue(
                mDevice.findObject(UiSelector().text("About $appName"))
                    .waitForExists(waitingTime),
            )

            break
        } catch (e: AssertionError) {
            if (i == RETRY_COUNT) {
                throw e
            }
        }
    }
    return mDevice.findObject(UiSelector().text("About $appName"))
}

fun swipeToBottom() = onView(withId(R.id.recycler_view)).perform(ViewActions.swipeUp())

fun clickRateButtonGooglePlay() {
    rateOnGooglePlayHeading().click()
}

private fun assertGooglePlayRedirect() {
    if (isPackageInstalled(GOOGLE_PLAY_SERVICES)) {
        try {
            intended(
                allOf(
                    hasAction(Intent.ACTION_VIEW),
                    hasData(Uri.parse(SupportUtils.RATE_APP_URL)),
                ),
            )
        } catch (e: AssertionFailedError) {
            BrowserRobot().verifyRateOnGooglePlayURL()
        }
    } else {
        BrowserRobot().verifyRateOnGooglePlayURL()
    }
}

private fun addonsManagerButton() = onView(withText(R.string.preferences_addons))

private fun goBackButton() =
    onView(CoreMatchers.allOf(ViewMatchers.withContentDescription("Navigate up")))

private fun settingsList() =
    UiScrollable(UiSelector().resourceId("$packageName:id/recycler_view"))
