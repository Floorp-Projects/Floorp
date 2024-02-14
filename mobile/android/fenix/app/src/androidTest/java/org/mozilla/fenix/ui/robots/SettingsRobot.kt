/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

@file:Suppress("TooManyFunctions")

package org.mozilla.fenix.ui.robots

import android.content.Intent
import android.net.Uri
import android.util.Log
import androidx.recyclerview.widget.RecyclerView
import androidx.test.espresso.Espresso.onView
import androidx.test.espresso.action.ViewActions
import androidx.test.espresso.assertion.ViewAssertions.matches
import androidx.test.espresso.contrib.RecyclerViewActions
import androidx.test.espresso.intent.Intents.intended
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
import org.hamcrest.CoreMatchers.endsWith
import org.hamcrest.Matchers.allOf
import org.mozilla.fenix.R
import org.mozilla.fenix.helpers.AppAndSystemHelper.isPackageInstalled
import org.mozilla.fenix.helpers.Constants.LISTS_MAXSWIPES
import org.mozilla.fenix.helpers.Constants.PackageName.GOOGLE_PLAY_SERVICES
import org.mozilla.fenix.helpers.Constants.RETRY_COUNT
import org.mozilla.fenix.helpers.Constants.TAG
import org.mozilla.fenix.helpers.DataGenerationHelper.getStringResource
import org.mozilla.fenix.helpers.MatcherHelper.assertUIObjectExists
import org.mozilla.fenix.helpers.MatcherHelper.itemContainingText
import org.mozilla.fenix.helpers.MatcherHelper.itemWithText
import org.mozilla.fenix.helpers.TestAssetHelper.waitingTime
import org.mozilla.fenix.helpers.TestAssetHelper.waitingTimeShort
import org.mozilla.fenix.helpers.TestHelper.appName
import org.mozilla.fenix.helpers.TestHelper.hasCousin
import org.mozilla.fenix.helpers.TestHelper.mDevice
import org.mozilla.fenix.helpers.TestHelper.packageName
import org.mozilla.fenix.helpers.TestHelper.scrollToElementByText
import org.mozilla.fenix.helpers.click
import org.mozilla.fenix.helpers.ext.waitNotNull
import org.mozilla.fenix.settings.SupportUtils
import org.mozilla.fenix.ui.robots.SettingsRobot.Companion.DEFAULT_APPS_SETTINGS_ACTION

/**
 * Implementation of Robot Pattern for the settings menu.
 */
class SettingsRobot {

    // BASICS SECTION
    fun verifyGeneralHeading() {
        scrollToElementByText("General")
        onView(withText("General"))
            .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))
    }

    fun verifySearchButton() {
        mDevice.wait(Until.findObject(By.text("Search")), waitingTime)
        onView(withText(R.string.preferences_search))
            .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))
    }
    fun verifyCustomizeButton() {
        onView(withText("Customize"))
            .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))
    }

    fun verifyAccessibilityButton() {
        onView(withText("Accessibility"))
            .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))
    }
    fun verifySetAsDefaultBrowserButton() {
        scrollToElementByText("Set as default browser")
        onView(withText("Set as default browser"))
            .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))
    }
    fun verifyTabsButton() =
        assertUIObjectExists(itemContainingText(getStringResource(R.string.preferences_tabs)))
    fun verifyHomepageButton() {
        onView(withText(R.string.preferences_home_2)).check(matches(withEffectiveVisibility(Visibility.VISIBLE)))
    }
    fun verifyAutofillButton() {
        onView(withText(R.string.preferences_autofill)).check(matches(withEffectiveVisibility(Visibility.VISIBLE)))
    }
    fun verifyLanguageButton() {
        scrollToElementByText(getStringResource(R.string.preferences_language))
        onView(withText(R.string.preferences_language)).check(matches(withEffectiveVisibility(Visibility.VISIBLE)))
    }
    fun verifyDefaultBrowserToggle(isEnabled: Boolean) {
        scrollToElementByText(getStringResource(R.string.preferences_set_as_default_browser))
        onView(withText(R.string.preferences_set_as_default_browser))
            .check(
                matches(
                    hasCousin(
                        allOf(
                            withId(R.id.switch_widget),
                            if (isEnabled) {
                                isChecked()
                            } else {
                                isNotChecked()
                            },
                        ),
                    ),
                ),
            )
    }

    fun clickDefaultBrowserSwitch() = toggleDefaultBrowserSwitch()
    fun verifyAndroidDefaultAppsMenuAppears() {
        intended(hasAction(DEFAULT_APPS_SETTINGS_ACTION))
    }

    // PRIVACY SECTION
    fun verifyPrivacyHeading() {
        scrollToElementByText("Privacy and security")
        onView(withText("Privacy and security"))
            .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))
    }
    fun verifyHTTPSOnlyModeButton() {
        scrollToElementByText(getStringResource(R.string.preferences_https_only_title))
        onView(
            withText(R.string.preferences_https_only_title),
        ).check(matches(withEffectiveVisibility(Visibility.VISIBLE)))
    }

    fun verifyCookieBannerBlockerButton(enabled: Boolean) {
        scrollToElementByText(getStringResource(R.string.preferences_cookie_banner_reduction_private_mode))
        onView(withText(R.string.preferences_cookie_banner_reduction_private_mode))
            .check(
                matches(
                    hasCousin(
                        CoreMatchers.allOf(
                            withClassName(endsWith("Switch")),
                            if (enabled) {
                                isChecked()
                            } else {
                                isNotChecked()
                            },
                        ),
                    ),
                ),
            )
        Log.i(TAG, "verifyCookieBannerBlockerButton: Verified if cookie banner blocker toggle is enabled: $enabled")
    }

    fun verifyEnhancedTrackingProtectionButton() {
        mDevice.wait(Until.findObject(By.text("Privacy and Security")), waitingTime)
        onView(withId(R.id.recycler_view)).perform(
            RecyclerViewActions.scrollTo<RecyclerView.ViewHolder>(
                hasDescendant(withText("Enhanced Tracking Protection")),
            ),
        ).check(matches(withEffectiveVisibility(Visibility.VISIBLE)))
    }
    fun verifyLoginsAndPasswordsButton() {
        scrollToElementByText("Logins and passwords")
        onView(withText(R.string.preferences_passwords_logins_and_passwords))
            .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))
    }
    fun verifyPrivateBrowsingButton() {
        scrollToElementByText("Private browsing")
        mDevice.wait(Until.findObject(By.text("Private browsing")), waitingTime)
        onView(withText("Private browsing"))
            .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))
    }
    fun verifySitePermissionsButton() {
        scrollToElementByText("Site permissions")
        onView(withText("Site permissions"))
            .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))
    }
    fun verifyDeleteBrowsingDataButton() {
        scrollToElementByText("Delete browsing data")
        onView(withText("Delete browsing data"))
            .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))
    }
    fun verifyDeleteBrowsingDataOnQuitButton() {
        scrollToElementByText("Delete browsing data on quit")
        onView(withText("Delete browsing data on quit"))
            .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))
    }
    fun verifyNotificationsButton() {
        scrollToElementByText("Notifications")
        onView(withText("Notifications"))
            .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))
    }
    fun verifyDataCollectionButton() {
        scrollToElementByText("Data collection")
        onView(withText("Data collection"))
            .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))
    }
    fun verifyOpenLinksInAppsButton() {
        scrollToElementByText("Open links in apps")
        openLinksInAppsButton()
            .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))
        Log.i(TAG, "clickOpenLinksInAppsGoToSettingsCFRButton: Verified \"Open links in apps\" setting option")
    }
    fun verifySettingsView() {
        scrollToElementByText("General")
        onView(withText("General"))
            .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))
        scrollToElementByText("Privacy and security")
        onView(withText("Privacy and security"))
            .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))
        onView(withId(R.id.recycler_view)).perform(
            RecyclerViewActions.scrollTo<RecyclerView.ViewHolder>(
                hasDescendant(withText("Add-ons")),
            ),
        )
        onView(withText("Add-ons"))
            .check(matches(isCompletelyDisplayed()))
        settingsList().scrollToEnd(LISTS_MAXSWIPES)
        onView(withText("About"))
            .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))
    }
    fun verifySettingsToolbar() =
        onView(
            CoreMatchers.allOf(
                withId(R.id.navigationToolbar),
                hasDescendant(ViewMatchers.withContentDescription(R.string.action_bar_up_description)),
                hasDescendant(withText(R.string.settings)),
            ),
        ).check(matches(withEffectiveVisibility(Visibility.VISIBLE)))

    // ADVANCED SECTION
    fun verifyAdvancedHeading() {
        onView(withId(R.id.recycler_view)).perform(
            RecyclerViewActions.scrollTo<RecyclerView.ViewHolder>(
                hasDescendant(withText("Add-ons")),
            ),
        )

        onView(withText("Add-ons"))
            .check(matches(isCompletelyDisplayed()))
    }
    fun verifyAddons() {
        onView(withId(R.id.recycler_view)).perform(
            RecyclerViewActions.scrollTo<RecyclerView.ViewHolder>(
                hasDescendant(withText("Add-ons")),
            ),
        )

        addonsManagerButton()
            .check(matches(isCompletelyDisplayed()))
    }

    fun verifyExternalDownloadManagerButton() =
        onView(
            withText(R.string.preferences_external_download_manager),
        ).check(matches(withEffectiveVisibility(Visibility.VISIBLE)))

    fun verifyExternalDownloadManagerToggle(enabled: Boolean) =
        onView(withText(R.string.preferences_external_download_manager))
            .check(
                matches(
                    hasCousin(
                        allOf(
                            withClassName(endsWith("Switch")),
                            if (enabled) {
                                isChecked()
                            } else {
                                isNotChecked()
                            },
                        ),
                    ),
                ),
            )

    fun verifyLeakCanaryToggle(enabled: Boolean) =
        onView(withText(R.string.preference_leakcanary))
            .check(
                matches(
                    hasCousin(
                        allOf(
                            withClassName(endsWith("Switch")),
                            if (enabled) {
                                isChecked()
                            } else {
                                isNotChecked()
                            },
                        ),
                    ),
                ),
            )

    fun verifyRemoteDebuggingToggle(enabled: Boolean) =
        onView(withText(R.string.preferences_remote_debugging))
            .check(
                matches(
                    hasCousin(
                        allOf(
                            withClassName(endsWith("Switch")),
                            if (enabled) {
                                isChecked()
                            } else {
                                isNotChecked()
                            },
                        ),
                    ),
                ),
            )

    // DEVELOPER TOOLS SECTION
    fun verifyRemoteDebuggingButton() {
        scrollToElementByText("Remote debugging via USB")
        onView(withText("Remote debugging via USB"))
            .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))
    }
    fun verifyLeakCanaryButton() {
        scrollToElementByText("LeakCanary")
        onView(withText("LeakCanary"))
            .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))
    }

    // ABOUT SECTION
    fun verifyAboutHeading() {
        settingsList().scrollToEnd(LISTS_MAXSWIPES)
        onView(withText("About"))
            .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))
    }

    fun verifyRateOnGooglePlay() = assertUIObjectExists(rateOnGooglePlayHeading())
    fun verifyAboutFirefoxPreview() = assertUIObjectExists(aboutFirefoxHeading())
    fun verifyGooglePlayRedirect() {
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

    fun verifySettingsOptionSummary(setting: String, summary: String) {
        scrollToElementByText(setting)
        onView(
            allOf(
                withText(setting),
                hasSibling(withText(summary)),
            ),
        ).check(matches(withEffectiveVisibility(Visibility.VISIBLE)))
    }

    class Transition {
        fun goBack(interact: HomeScreenRobot.() -> Unit): HomeScreenRobot.Transition {
            goBackButton().click()

            HomeScreenRobot().interact()
            return HomeScreenRobot.Transition()
        }

        fun goBackToOnboardingScreen(interact: HomeScreenRobot.() -> Unit): HomeScreenRobot.Transition {
            mDevice.pressBack()
            mDevice.waitForIdle(waitingTimeShort)

            HomeScreenRobot().interact()
            return HomeScreenRobot.Transition()
        }

        fun goBackToBrowser(interact: BrowserRobot.() -> Unit): BrowserRobot.Transition {
            goBackButton().click()

            BrowserRobot().interact()
            return BrowserRobot.Transition()
        }

        fun openAboutFirefoxPreview(interact: SettingsSubMenuAboutRobot.() -> Unit): SettingsSubMenuAboutRobot.Transition {
            aboutFirefoxHeading().click()
            SettingsSubMenuAboutRobot().interact()
            return SettingsSubMenuAboutRobot.Transition()
        }

        fun openSearchSubMenu(interact: SettingsSubMenuSearchRobot.() -> Unit): SettingsSubMenuSearchRobot.Transition {
            itemWithText(getStringResource(R.string.preferences_search))
                .also {
                    it.waitForExists(waitingTimeShort)
                    it.click()
                }

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
            itemWithText(getStringResource(R.string.preferences_tabs))
                .also {
                    it.waitForExists(waitingTime)
                    it.clickAndWaitForNewWindow(waitingTimeShort)
                }

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
            mDevice.findObject(UiSelector().textContains(getStringResource(R.string.preferences_autofill)))
                .also {
                    Log.i(TAG, "openAutofillSubMenu: Looking for \"Autofill\" settings button")
                    it.waitForExists(waitingTime)
                    it.click()
                    Log.i(TAG, "openAutofillSubMenu: Clicked \"Autofill\" settings button")
                }

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

        fun openOpenLinksInAppsMenu(interact: SettingsSubMenuOpenLinksInAppsRobot.() -> Unit): SettingsSubMenuOpenLinksInAppsRobot.Transition {
            openLinksInAppsButton().click()

            SettingsSubMenuOpenLinksInAppsRobot().interact()
            return SettingsSubMenuOpenLinksInAppsRobot.Transition()
        }

        fun openHttpsOnlyModeMenu(interact: SettingsSubMenuHttpsOnlyModeRobot.() -> Unit): SettingsSubMenuHttpsOnlyModeRobot.Transition {
            scrollToElementByText("HTTPS-Only Mode")
            onView(withText(getStringResource(R.string.preferences_https_only_title))).click()
            mDevice.waitNotNull(
                Until.findObjects(By.res("$packageName:id/https_only_switch")),
                waitingTime,
            )

            SettingsSubMenuHttpsOnlyModeRobot().interact()
            return SettingsSubMenuHttpsOnlyModeRobot.Transition()
        }

        fun openExperimentsMenu(interact: SettingsSubMenuExperimentsRobot.() -> Unit): SettingsSubMenuExperimentsRobot.Transition {
            scrollToElementByText("Nimbus Experiments")
            onView(withText(getStringResource(R.string.preferences_nimbus_experiments))).click()

            SettingsSubMenuExperimentsRobot().interact()
            return SettingsSubMenuExperimentsRobot.Transition()
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

private fun toggleDefaultBrowserSwitch() {
    scrollToElementByText("Privacy and security")
    onView(withText("Set as default browser")).perform(ViewActions.click())
}

private fun openLinksInAppsButton() = onView(withText(R.string.preferences_open_links_in_apps))

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
            assertUIObjectExists(itemWithText("About $appName"))

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

private fun addonsManagerButton() = onView(withText(R.string.preferences_addons))

private fun goBackButton() =
    onView(CoreMatchers.allOf(ViewMatchers.withContentDescription("Navigate up")))

private fun settingsList() =
    UiScrollable(UiSelector().resourceId("$packageName:id/recycler_view"))
