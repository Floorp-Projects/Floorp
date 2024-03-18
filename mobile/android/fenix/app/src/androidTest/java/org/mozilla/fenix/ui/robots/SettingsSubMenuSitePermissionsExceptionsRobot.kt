/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ui.robots

import android.util.Log
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
import org.mozilla.fenix.helpers.Constants.TAG
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
        Log.i(TAG, "verifyExceptionsEmptyList: Waiting for $waitingTime ms for empty exceptions list to exist")
        mDevice.findObject(UiSelector().text(getStringResource(R.string.no_site_exceptions)))
            .waitForExists(waitingTime)
        Log.i(TAG, "verifyExceptionsEmptyList: Waited for $waitingTime ms for empty exceptions list to exist")
        Log.i(TAG, "verifyExceptionsEmptyList: Trying to verify that the empty exceptions list is displayed")
        onView(withText(R.string.no_site_exceptions)).check(matches(isDisplayed()))
        Log.i(TAG, "verifyExceptionsEmptyList: Verified that the empty exceptions list is displayed")
    }

    fun verifyExceptionCreated(url: String, shouldBeDisplayed: Boolean) {
        if (shouldBeDisplayed) {
            Log.i(TAG, "verifyExceptionCreated: Waiting for $waitingTime ms for exceptions list to exist")
            exceptionsList().waitForExists(waitingTime)
            Log.i(TAG, "verifyExceptionCreated: Waited for $waitingTime ms for exceptions list to exist")
            Log.i(TAG, "verifyExceptionCreated: Trying to verify that $url is displayed in the exceptions list")
            onView(withText(containsString(url))).check(matches(isDisplayed()))
            Log.i(TAG, "verifyExceptionCreated: Verified that $url is displayed in the exceptions list")
        } else {
            assertUIObjectIsGone(itemContainingText(url))
        }
    }

    fun verifyClearPermissionsDialog() {
        Log.i(TAG, "verifyClearPermissionsDialog: Trying to verify that the \"Clear permissions\" dialog title is displayed")
        onView(withText(R.string.clear_permissions)).check(matches(isDisplayed()))
        Log.i(TAG, "verifyClearPermissionsDialog: Verified that the \"Clear permissions\" dialog title is displayed")
        Log.i(TAG, "verifyClearPermissionsDialog: Trying to verify that the \"Are you sure that you want to clear all the permissions on all sites?\" dialog message is displayed")
        onView(withText(R.string.confirm_clear_permissions_on_all_sites)).check(matches(isDisplayed()))
        Log.i(TAG, "verifyClearPermissionsDialog: Verified that the \"Are you sure that you want to clear all the permissions on all sites?\" dialog message is displayed")
        Log.i(TAG, "verifyClearPermissionsDialog: Trying to verify that the \"Ok\" dialog button is displayed")
        onView(withText(R.string.clear_permissions_positive)).check(matches(isDisplayed()))
        Log.i(TAG, "verifyClearPermissionsDialog: Verified that the \"Ok\" dialog button is displayed")
        Log.i(TAG, "verifyClearPermissionsDialog: Trying to verify that the \"Cancel\" dialog button is displayed")
        onView(withText(R.string.clear_permissions_negative)).check(matches(isDisplayed()))
        Log.i(TAG, "verifyClearPermissionsDialog: Verified that the \"Cancel\" dialog button is displayed")
    }

    // Click button for resetting all of one site's permissions to default
    fun clickClearPermissionsForOneSite() {
        swipeToBottom()
        Log.i(TAG, "clickClearPermissionsForOneSite: Trying to click the \"Clear permissions\" button")
        onView(withText(R.string.clear_permissions))
            .check(matches(isDisplayed()))
            .click()
        Log.i(TAG, "clickClearPermissionsForOneSite: Clicked the \"Clear permissions\" button")
    }
    fun verifyClearPermissionsForOneSiteDialog() {
        Log.i(TAG, "verifyClearPermissionsForOneSiteDialog: Trying to verify that the \"Clear permissions\" dialog title is displayed")
        onView(withText(R.string.clear_permissions)).check(matches(isDisplayed()))
        Log.i(TAG, "verifyClearPermissionsForOneSiteDialog: Verified that the \"Clear permissions\" dialog title is displayed")
        Log.i(TAG, "verifyClearPermissionsForOneSiteDialog: Trying to verify that the \"Are you sure that you want to clear all the permissions for this site?\" dialog message is displayed")
        onView(withText(R.string.confirm_clear_permissions_site)).check(matches(isDisplayed()))
        Log.i(TAG, "verifyClearPermissionsForOneSiteDialog: Verified that the \"Are you sure that you want to clear all the permissions for this site?\" dialog message is displayed")
        Log.i(TAG, "verifyClearPermissionsForOneSiteDialog: Trying to verify that the \"Ok\" dialog button is displayed")
        onView(withText(R.string.clear_permissions_positive)).check(matches(isDisplayed()))
        Log.i(TAG, "verifyClearPermissionsForOneSiteDialog: Verified that the \"Ok\" dialog button is displayed")
        Log.i(TAG, "verifyClearPermissionsForOneSiteDialog: Trying to verify that the \"Cancel\" dialog button is displayed")
        onView(withText(R.string.clear_permissions_negative)).check(matches(isDisplayed()))
        Log.i(TAG, "verifyClearPermissionsForOneSiteDialog: Verified that the \"Cancel\" dialog button is displayed")
    }

    fun openSiteExceptionsDetails(url: String) {
        Log.i(TAG, "openSiteExceptionsDetails: Waiting for $waitingTime ms for exceptions list to exist")
        exceptionsList().waitForExists(waitingTime)
        Log.i(TAG, "openSiteExceptionsDetails: Waited for $waitingTime ms for exceptions list to exist")
        Log.i(TAG, "openSiteExceptionsDetails: Trying to click $url exception")
        onView(withText(containsString(url))).click()
        Log.i(TAG, "openSiteExceptionsDetails: Clicked $url exception")
    }

    fun verifyPermissionSettingSummary(setting: String, summary: String) {
        Log.i(TAG, "verifyPermissionSettingSummary: Trying to verify that $setting permission is $summary and is displayed")
        onView(
            allOf(
                withText(setting),
                hasSibling(withText(summary)),
            ),
        ).check(matches(isDisplayed()))
        Log.i(TAG, "verifyPermissionSettingSummary: Verified that $setting permission is $summary and is displayed")
    }

    fun openChangePermissionSettingsMenu(permissionSetting: String) {
        Log.i(TAG, "openChangePermissionSettingsMenu: Trying to click $permissionSetting permission button")
        onView(withText(containsString(permissionSetting))).click()
        Log.i(TAG, "openChangePermissionSettingsMenu: Clicked $permissionSetting permission button")
    }

    // Click button for resetting all permissions for all websites
    fun clickClearPermissionsOnAllSites() {
        Log.i(TAG, "clickClearPermissionsOnAllSites: Waiting for $waitingTime ms for exceptions list to exist")
        exceptionsList().waitForExists(waitingTime)
        Log.i(TAG, "clickClearPermissionsOnAllSites: Waited for $waitingTime ms for exceptions list to exist")
        Log.i(TAG, "clickClearPermissionsOnAllSites: Trying to click the \"Clear permissions on all sites\" button")
        onView(withId(R.id.delete_all_site_permissions_button))
            .check(matches(isDisplayed()))
            .click()
        Log.i(TAG, "clickClearPermissionsOnAllSites: Clicked the \"Clear permissions on all sites\" button")
    }

    // Click button for resetting one site permission to default
    fun clickClearOnePermissionForOneSite() {
        Log.i(TAG, "clickClearOnePermissionForOneSite: Trying to click the \"Clear permissions\" button")
        onView(withText(R.string.clear_permission))
            .check(matches(isDisplayed()))
            .click()
        Log.i(TAG, "clickClearOnePermissionForOneSite: Clicked the \"Clear permissions\" button")
    }

    fun verifyResetPermissionDefaultForThisSiteDialog() {
        Log.i(TAG, "verifyResetPermissionDefaultForThisSiteDialog: Trying to verify that the \"Clear permissions\" dialog title is displayed")
        onView(withText(R.string.clear_permission)).check(matches(isDisplayed()))
        Log.i(TAG, "verifyResetPermissionDefaultForThisSiteDialog: Verified that the \"Clear permissions\" dialog title is displayed")
        Log.i(TAG, "verifyResetPermissionDefaultForThisSiteDialog: Trying to verify that the \"Are you sure that you want to clear this permission for this site?\" dialog message is displayed")
        onView(withText(R.string.confirm_clear_permission_site)).check(matches(isDisplayed()))
        Log.i(TAG, "verifyResetPermissionDefaultForThisSiteDialog: Verified that the \"Are you sure that you want to clear this permission for this site?\" dialog message is displayed")
        Log.i(TAG, "verifyResetPermissionDefaultForThisSiteDialog: Trying to verify that the \"Ok\" dialog button is displayed")
        onView(withText(R.string.clear_permissions_positive)).check(matches(isDisplayed()))
        Log.i(TAG, "verifyResetPermissionDefaultForThisSiteDialog: Verified that the \"Ok\" dialog button is displayed")
        Log.i(TAG, "verifyResetPermissionDefaultForThisSiteDialog: Trying to verify that the \"Cancel\" dialog button is displayed")
        onView(withText(R.string.clear_permissions_negative)).check(matches(isDisplayed()))
        Log.i(TAG, "verifyResetPermissionDefaultForThisSiteDialog: Verified that the \"Cancel\" dialog button is displayed")
    }

    fun clickOK() {
        Log.i(TAG, "clickOK: Trying to click the \"Ok\" dialog button")
        onView(withText(R.string.clear_permissions_positive)).click()
        Log.i(TAG, "clickOK: Clicked the \"Ok\" dialog button")
    }

    fun clickCancel() {
        Log.i(TAG, "clickCancel: Trying to click the \"Cancel\" dialog button")
        onView(withText(R.string.clear_permissions_negative)).click()
        Log.i(TAG, "clickCancel: Clicked the \"Cancel\" dialog button")
    }

    class Transition {
        fun goBack(interact: SettingsSubMenuSitePermissionsRobot.() -> Unit): SettingsSubMenuSitePermissionsRobot.Transition {
            Log.i(TAG, "goBack: Trying to click the navigate up button")
            goBackButton().click()
            Log.i(TAG, "goBack: Clicked the navigate up button")

            SettingsSubMenuSitePermissionsRobot().interact()
            return SettingsSubMenuSitePermissionsRobot.Transition()
        }
    }
}

private fun goBackButton() =
    onView(allOf(withContentDescription("Navigate up")))

private fun exceptionsList() =
    mDevice.findObject(UiSelector().resourceId("$packageName:id/exceptions"))
