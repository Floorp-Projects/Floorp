/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ui.robots

import androidx.test.espresso.Espresso.onView
import androidx.test.espresso.assertion.ViewAssertions.matches
import androidx.test.espresso.matcher.ViewMatchers.hasSibling
import androidx.test.espresso.matcher.ViewMatchers.isDisplayed
import androidx.test.espresso.matcher.ViewMatchers.withContentDescription
import androidx.test.espresso.matcher.ViewMatchers.withId
import androidx.test.espresso.matcher.ViewMatchers.withText
import androidx.test.uiautomator.UiSelector
import org.hamcrest.CoreMatchers.allOf
import org.hamcrest.CoreMatchers.containsString
import org.mozilla.fenix.R
import org.mozilla.fenix.helpers.DataGenerationHelper.getStringResource
import org.mozilla.fenix.helpers.MatcherHelper.assertUIObjectIsGone
import org.mozilla.fenix.helpers.MatcherHelper.itemContainingText
import org.mozilla.fenix.helpers.TestAssetHelper.waitingTime
import org.mozilla.fenix.helpers.TestHelper.mDevice
import org.mozilla.fenix.helpers.TestHelper.packageName
import org.mozilla.fenix.helpers.click

/**
 * Implementation of Robot Pattern for the settings Site Permissions Notification sub menu.
 */
class SettingsSubMenuSitePermissionsExceptionsRobot {
    fun verifyExceptionsEmptyList() {
        mDevice.findObject(UiSelector().text(getStringResource(R.string.no_site_exceptions)))
            .waitForExists(waitingTime)
        onView(withText(R.string.no_site_exceptions)).check(matches(isDisplayed()))
    }

    fun verifyExceptionCreated(url: String, shouldBeDisplayed: Boolean) {
        if (shouldBeDisplayed) {
            exceptionsList.waitForExists(waitingTime)
            onView(withText(containsString(url))).check(matches(isDisplayed()))
        } else {
            assertUIObjectIsGone(itemContainingText(url))
        }
    }

    fun verifyClearPermissionsDialog() {
        onView(withText(R.string.clear_permissions)).check(matches(isDisplayed()))
        onView(withText(R.string.confirm_clear_permissions_on_all_sites)).check(matches(isDisplayed()))
        onView(withText(R.string.clear_permissions_positive)).check(matches(isDisplayed()))
        onView(withText(R.string.clear_permissions_negative)).check(matches(isDisplayed()))
    }

    // Click button for resetting all of one site's permissions to default
    fun clickClearPermissionsForOneSite() {
        swipeToBottom()
        onView(withText(R.string.clear_permissions))
            .check(matches(isDisplayed()))
            .click()
    }
    fun verifyClearPermissionsForOneSiteDialog() {
        onView(withText(R.string.clear_permissions)).check(matches(isDisplayed()))
        onView(withText(R.string.confirm_clear_permissions_site)).check(matches(isDisplayed()))
        onView(withText(R.string.clear_permissions_positive)).check(matches(isDisplayed()))
        onView(withText(R.string.clear_permissions_negative)).check(matches(isDisplayed()))
    }

    fun openSiteExceptionsDetails(url: String) {
        exceptionsList.waitForExists(waitingTime)
        onView(withText(containsString(url))).click()
    }

    fun verifyPermissionSettingSummary(setting: String, summary: String) {
        onView(
            allOf(
                withText(setting),
                hasSibling(withText(summary)),
            ),
        ).check(matches(isDisplayed()))
    }

    fun openChangePermissionSettingsMenu(permissionSetting: String) {
        onView(withText(containsString(permissionSetting))).click()
    }

    // Click button for resetting all permissions for all websites
    fun clickClearPermissionsOnAllSites() {
        exceptionsList.waitForExists(waitingTime)
        onView(withId(R.id.delete_all_site_permissions_button))
            .check(matches(isDisplayed()))
            .click()
    }

    // Click button for resetting one site permission to default
    fun clickClearOnePermissionForOneSite() {
        onView(withText(R.string.clear_permission))
            .check(matches(isDisplayed()))
            .click()
    }

    fun verifyResetPermissionDefaultForThisSiteDialog() {
        onView(withText(R.string.clear_permission)).check(matches(isDisplayed()))
        onView(withText(R.string.confirm_clear_permission_site)).check(matches(isDisplayed()))
        onView(withText(R.string.clear_permissions_positive)).check(matches(isDisplayed()))
        onView(withText(R.string.clear_permissions_negative)).check(matches(isDisplayed()))
    }

    fun clickOK() = onView(withText(R.string.clear_permissions_positive)).click()

    fun clickCancel() = onView(withText(R.string.clear_permissions_negative)).click()

    class Transition {
        fun goBack(interact: SettingsSubMenuSitePermissionsRobot.() -> Unit): SettingsSubMenuSitePermissionsRobot.Transition {
            goBackButton().click()

            SettingsSubMenuSitePermissionsRobot().interact()
            return SettingsSubMenuSitePermissionsRobot.Transition()
        }
    }
}

private fun goBackButton() =
    onView(allOf(withContentDescription("Navigate up")))

private val exceptionsList =
    mDevice.findObject(UiSelector().resourceId("$packageName:id/exceptions"))
