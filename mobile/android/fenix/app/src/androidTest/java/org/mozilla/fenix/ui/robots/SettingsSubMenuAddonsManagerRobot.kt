/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

@file:Suppress("DEPRECATION")

package org.mozilla.fenix.ui.robots

import android.util.Log
import android.widget.RelativeLayout
import androidx.test.espresso.Espresso.onView
import androidx.test.espresso.action.ViewActions.click
import androidx.test.espresso.assertion.ViewAssertions.matches
import androidx.test.espresso.matcher.RootMatchers.isDialog
import androidx.test.espresso.matcher.ViewMatchers.Visibility
import androidx.test.espresso.matcher.ViewMatchers.hasDescendant
import androidx.test.espresso.matcher.ViewMatchers.hasSibling
import androidx.test.espresso.matcher.ViewMatchers.isAssignableFrom
import androidx.test.espresso.matcher.ViewMatchers.isCompletelyDisplayed
import androidx.test.espresso.matcher.ViewMatchers.isDescendantOfA
import androidx.test.espresso.matcher.ViewMatchers.isDisplayed
import androidx.test.espresso.matcher.ViewMatchers.withContentDescription
import androidx.test.espresso.matcher.ViewMatchers.withEffectiveVisibility
import androidx.test.espresso.matcher.ViewMatchers.withId
import androidx.test.espresso.matcher.ViewMatchers.withParent
import androidx.test.espresso.matcher.ViewMatchers.withText
import androidx.test.uiautomator.By
import androidx.test.uiautomator.UiScrollable
import androidx.test.uiautomator.UiSelector
import androidx.test.uiautomator.Until
import org.hamcrest.CoreMatchers.allOf
import org.hamcrest.CoreMatchers.containsString
import org.hamcrest.CoreMatchers.instanceOf
import org.mozilla.fenix.R
import org.mozilla.fenix.helpers.Constants.RETRY_COUNT
import org.mozilla.fenix.helpers.Constants.TAG
import org.mozilla.fenix.helpers.HomeActivityIntentTestRule
import org.mozilla.fenix.helpers.MatcherHelper.assertUIObjectExists
import org.mozilla.fenix.helpers.MatcherHelper.itemWithText
import org.mozilla.fenix.helpers.TestAssetHelper.waitingTime
import org.mozilla.fenix.helpers.TestAssetHelper.waitingTimeLong
import org.mozilla.fenix.helpers.TestHelper.appName
import org.mozilla.fenix.helpers.TestHelper.mDevice
import org.mozilla.fenix.helpers.TestHelper.packageName
import org.mozilla.fenix.helpers.TestHelper.restartApp
import org.mozilla.fenix.helpers.TestHelper.scrollToElementByText
import org.mozilla.fenix.helpers.click
import org.mozilla.fenix.helpers.ext.waitNotNull

/**
 * Implementation of Robot Pattern for the Addons Management Settings.
 */

class SettingsSubMenuAddonsManagerRobot {
    fun verifyAddonsListIsDisplayed(shouldBeDisplayed: Boolean) =
        assertUIObjectExists(addonsList(), exists = shouldBeDisplayed)

    fun verifyAddonDownloadOverlay() {
        Log.i(TAG, "verifyAddonDownloadOverlay: Trying to verify that the \"Downloading and verifying add-on\" prompt is displayed")
        onView(withText(R.string.mozac_add_on_install_progress_caption)).check(matches(isDisplayed()))
        Log.i(TAG, "verifyAddonDownloadOverlay: Verified that the \"Downloading and verifying add-on\" prompt is displayed")
    }

    fun verifyAddonPermissionPrompt(addonName: String) {
        mDevice.waitNotNull(Until.findObject(By.text("Add $addonName?")), waitingTime)
        Log.i(TAG, "verifyAddonPermissionPrompt: Trying to verify that the add-ons permission prompt items are displayed")
        onView(
            allOf(
                withText("Add $addonName?"),
                hasSibling(withText(containsString("It requires your permission to:"))),
                hasSibling(withText("Add")),
                hasSibling(withText("Cancel")),
            ),
        )
            .inRoot(isDialog())
            .check(matches(isDisplayed()))
        Log.i(TAG, "verifyAddonPermissionPrompt: Verified that the add-ons permission prompt items are displayed")
    }

    fun clickInstallAddon(addonName: String) {
        Log.i(TAG, "clickInstallAddon: Waiting for $waitingTime ms for add-ons list to exist")
        addonsList().waitForExists(waitingTime)
        Log.i(TAG, "clickInstallAddon: Waited for $waitingTime ms for add-ons list to exist")
        Log.i(TAG, "clickInstallAddon: Trying to scroll into view the install $addonName button")
        addonsList().scrollIntoView(
            mDevice.findObject(
                UiSelector()
                    .resourceId("$packageName:id/details_container")
                    .childSelector(UiSelector().text(addonName)),
            ),
        )
        Log.i(TAG, "clickInstallAddon: Scrolled into view the install $addonName button")
        Log.i(TAG, "clickInstallAddon: Trying to ensure the full visibility of the the install $addonName button")
        addonsList().ensureFullyVisible(
            mDevice.findObject(
                UiSelector()
                    .resourceId("$packageName:id/details_container")
                    .childSelector(UiSelector().text(addonName)),
            ),
        )
        Log.i(TAG, "clickInstallAddon: Ensured the full visibility of the the install $addonName button")
        Log.i(TAG, "clickInstallAddon: Trying to click the install $addonName button")
        installButtonForAddon(addonName).click()
        Log.i(TAG, "clickInstallAddon: Clicked the install $addonName button")
    }

    fun verifyAddonInstallCompleted(addonName: String, activityTestRule: HomeActivityIntentTestRule) {
        for (i in 1..RETRY_COUNT) {
            Log.i(TAG, "verifyAddonInstallCompleted: Started try #$i")
            try {
                assertUIObjectExists(itemWithText("OK"), waitingTime = waitingTimeLong)

                break
            } catch (e: AssertionError) {
                Log.i(TAG, "verifyAddonInstallCompleted: AssertionError caught, executing fallback methods")
                if (i == RETRY_COUNT) {
                    throw e
                } else {
                    restartApp(activityTestRule)
                    homeScreen {
                    }.openThreeDotMenu {
                    }.openAddonsManagerMenu {
                        scrollToElementByText(addonName)
                        clickInstallAddon(addonName)
                        verifyAddonPermissionPrompt(addonName)
                        acceptPermissionToInstallAddon()
                    }
                }
            }
        }
    }

    fun verifyAddonInstallCompletedPrompt(addonName: String) {
        Log.i(TAG, "verifyAddonInstallCompletedPrompt: Trying to verify that completed add-on install prompt items are visible")
        onView(
            allOf(
                withText("OK"),
                withParent(instanceOf(RelativeLayout::class.java)),
                hasSibling(withText("$addonName has been added to $appName")),
                hasSibling(withText("Access $addonName from the $appName menu.")),
                hasSibling(withText("Allow in private browsing")),
            ),
        )
            .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))
        Log.i(TAG, "verifyAddonInstallCompletedPrompt: Verified that completed add-on install prompt items are visible")
    }

    fun closeAddonInstallCompletePrompt() {
        Log.i(TAG, "closeAddonInstallCompletePrompt: Trying to click the \"OK\" button from the completed add-on install prompt")
        onView(withText("OK")).click()
        Log.i(TAG, "closeAddonInstallCompletePrompt: Clicked the \"OK\" button from the completed add-on install prompt")
    }

    fun verifyAddonIsInstalled(addonName: String) {
        scrollToElementByText(addonName)
        Log.i(TAG, "verifyAddonIsInstalled: Trying to verify that the $addonName add-on was installed")
        onView(
            allOf(
                withId(R.id.add_button),
                isDescendantOfA(withId(R.id.add_on_item)),
                hasSibling(hasDescendant(withText(addonName))),
            ),
        ).check(matches(withEffectiveVisibility(Visibility.INVISIBLE)))
        Log.i(TAG, "verifyAddonIsInstalled: Verified that the $addonName add-on was installed")
    }

    fun verifyEnabledTitleDisplayed() {
        Log.i(TAG, "verifyEnabledTitleDisplayed: Trying to verify that the \"Enabled\" heading is displayed")
        onView(withText("Enabled"))
            .check(matches(isCompletelyDisplayed()))
        Log.i(TAG, "verifyEnabledTitleDisplayed: Verified that the \"Enabled\" heading is displayed")
    }

    fun cancelInstallAddon() = cancelInstall()
    fun acceptPermissionToInstallAddon() = allowPermissionToInstall()
    fun verifyAddonsItems() {
        Log.i(TAG, "verifyAddonsItems: Trying to verify that the \"Recommended\" heading is visible")
        onView(allOf(withId(R.id.title), withText("Recommended")))
            .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))
        Log.i(TAG, "verifyAddonsItems: Verified that the \"Recommended\" heading is visible")
        Log.i(TAG, "verifyAddonsItems: Trying to verify that all uBlock Origin items are completely displayed")
        onView(
            allOf(
                isAssignableFrom(RelativeLayout::class.java),
                withId(R.id.add_on_item),
                hasDescendant(allOf(withId(R.id.add_on_icon), isCompletelyDisplayed())),
                hasDescendant(
                    allOf(
                        withId(R.id.details_container),
                        hasDescendant(withText("uBlock Origin")),
                        hasDescendant(withText("Finally, an efficient wide-spectrum content blocker. Easy on CPU and memory.")),
                        hasDescendant(withId(R.id.rating)),
                        hasDescendant(withId(R.id.review_count)),
                    ),
                ),
                hasDescendant(withId(R.id.add_button)),
            ),
        ).check(matches(isCompletelyDisplayed()))
        Log.i(TAG, "verifyAddonsItems: Verified that all uBlock Origin items are completely displayed")
    }
    fun verifyAddonCanBeInstalled(addonName: String) {
        scrollToElementByText(addonName)
        mDevice.waitNotNull(Until.findObject(By.text(addonName)), waitingTime)
        Log.i(TAG, "verifyAddonCanBeInstalled: Trying to verify that the install $addonName button is visible")
        onView(
            allOf(
                withId(R.id.add_button),
                hasSibling(
                    hasDescendant(
                        allOf(
                            withId(R.id.add_on_name),
                            withText(addonName),
                        ),
                    ),
                ),
            ),
        ).check(matches(withEffectiveVisibility(Visibility.VISIBLE)))
        Log.i(TAG, "verifyAddonCanBeInstalled: Verified that the install $addonName button is visible")
    }

    fun selectAllowInPrivateBrowsing() {
        assertUIObjectExists(itemWithText("Allow in private browsing"), waitingTime = waitingTimeLong)
        Log.i(TAG, "selectAllowInPrivateBrowsing: Trying to click the \"Allow in private browsing\" check box")
        onView(withId(R.id.allow_in_private_browsing)).click()
        Log.i(TAG, "selectAllowInPrivateBrowsing: Clicked the \"Allow in private browsing\" check box")
    }

    fun installAddon(addonName: String, activityTestRule: HomeActivityIntentTestRule) {
        homeScreen {
        }.openThreeDotMenu {
        }.openAddonsManagerMenu {
            clickInstallAddon(addonName)
            verifyAddonPermissionPrompt(addonName)
            acceptPermissionToInstallAddon()
            verifyAddonInstallCompleted(addonName, activityTestRule)
        }
    }

    class Transition {
        fun goBack(interact: HomeScreenRobot.() -> Unit): HomeScreenRobot.Transition {
            Log.i(TAG, "goBack: Trying to click navigate up toolbar button")
            onView(allOf(withContentDescription("Navigate up"))).click()
            Log.i(TAG, "goBack: Clicked the navigate up toolbar button")

            HomeScreenRobot().interact()
            return HomeScreenRobot.Transition()
        }

        fun openDetailedMenuForAddon(
            addonName: String,
            interact: SettingsSubMenuAddonsManagerAddonDetailedMenuRobot.() -> Unit,
        ): SettingsSubMenuAddonsManagerAddonDetailedMenuRobot.Transition {
            scrollToElementByText(addonName)
            Log.i(TAG, "openDetailedMenuForAddon: Trying to verify that the $addonName add-on is visible")
            addonItem(addonName).check(matches(withEffectiveVisibility(Visibility.VISIBLE)))
            Log.i(TAG, "openDetailedMenuForAddon: Verified that the $addonName add-on is visible")
            Log.i(TAG, "openDetailedMenuForAddon: Trying to click the $addonName add-on")
            addonItem(addonName).perform(click())
            Log.i(TAG, "openDetailedMenuForAddon: Clicked the $addonName add-on")

            SettingsSubMenuAddonsManagerAddonDetailedMenuRobot().interact()
            return SettingsSubMenuAddonsManagerAddonDetailedMenuRobot.Transition()
        }
    }

    private fun installButtonForAddon(addonName: String) =
        onView(
            allOf(
                withContentDescription("Install $addonName"),
                isDescendantOfA(withId(R.id.add_on_item)),
                hasSibling(hasDescendant(withText(addonName))),
            ),
        )

    private fun cancelInstall() {
        Log.i(TAG, "cancelInstall: Trying to verify that the \"Cancel\" button is completely displayed")
        onView(allOf(withId(R.id.deny_button), withText("Cancel"))).check(matches(isCompletelyDisplayed()))
        Log.i(TAG, "cancelInstall: Verified that the \"Cancel\" button is completely displayed")
        Log.i(TAG, "cancelInstall: Trying to click the \"Cancel\" button")
        onView(allOf(withId(R.id.deny_button), withText("Cancel"))).perform(click())
        Log.i(TAG, "cancelInstall: Clicked the \"Cancel\" button")
    }

    private fun allowPermissionToInstall() {
        mDevice.waitNotNull(Until.findObject(By.text("Add")), waitingTime)
        Log.i(TAG, "allowPermissionToInstall: Trying to verify that the \"Add\" button is completely displayed")
        onView(allOf(withId(R.id.allow_button), withText("Add"))).check(matches(isCompletelyDisplayed()))
        Log.i(TAG, "allowPermissionToInstall: Verified that the \"Add\" button is completely displayed")
        Log.i(TAG, "allowPermissionToInstall: Trying to click the \"Add\" button")
        onView(allOf(withId(R.id.allow_button), withText("Add"))).perform(click())
        Log.i(TAG, "allowPermissionToInstall: Clicked the \"Add\" button")
    }
}

fun addonsMenu(interact: SettingsSubMenuAddonsManagerRobot.() -> Unit): SettingsSubMenuAddonsManagerRobot.Transition {
    SettingsSubMenuAddonsManagerRobot().interact()
    return SettingsSubMenuAddonsManagerRobot.Transition()
}

private fun addonItem(addonName: String) =
    onView(
        allOf(
            withId(R.id.add_on_item),
            hasDescendant(
                allOf(
                    withId(R.id.add_on_name),
                    withText(addonName),
                ),
            ),
        ),
    )

private fun addonsList() =
    UiScrollable(UiSelector().resourceId("$packageName:id/add_ons_list")).setAsVerticalList()
