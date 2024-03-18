/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

@file:Suppress("TooManyFunctions")

package org.mozilla.fenix.ui.robots

import android.util.Log
import androidx.test.espresso.Espresso.onView
import androidx.test.espresso.assertion.ViewAssertions.matches
import androidx.test.espresso.matcher.RootMatchers
import androidx.test.espresso.matcher.ViewMatchers.isDisplayed
import androidx.test.espresso.matcher.ViewMatchers.withId
import androidx.test.uiautomator.UiSelector
import mozilla.components.support.ktx.kotlin.tryGetHostFromUrl
import org.mozilla.fenix.R
import org.mozilla.fenix.helpers.Constants.TAG
import org.mozilla.fenix.helpers.DataGenerationHelper.getStringResource
import org.mozilla.fenix.helpers.MatcherHelper.assertUIObjectExists
import org.mozilla.fenix.helpers.TestAssetHelper.waitingTime
import org.mozilla.fenix.helpers.TestAssetHelper.waitingTimeShort
import org.mozilla.fenix.helpers.TestHelper.mDevice
import org.mozilla.fenix.helpers.TestHelper.packageName

/**
 * Implementation of Robot Pattern for Site Security UI.
 */
class SiteSecurityRobot {

    fun verifyQuickActionSheet(url: String = "", isConnectionSecure: Boolean) {
        Log.i(TAG, "verifyQuickActionSheet: Waiting for $waitingTime ms for quick action sheet to exist")
        quickActionSheet().waitForExists(waitingTime)
        Log.i(TAG, "verifyQuickActionSheet: Waited for $waitingTime ms for quick action sheet to exist")
        assertUIObjectExists(
            quickActionSheetUrl(url.tryGetHostFromUrl()),
            quickActionSheetSecurityInfo(isConnectionSecure),
            quickActionSheetTrackingProtectionSwitch(),
            quickActionSheetClearSiteData(),
        )
    }
    fun openSecureConnectionSubMenu(isConnectionSecure: Boolean) {
        Log.i(TAG, "openSecureConnectionSubMenu: Trying to click the security info button while connection is secure: $isConnectionSecure")
        quickActionSheetSecurityInfo(isConnectionSecure).click()
        Log.i(TAG, "openSecureConnectionSubMenu: Clicked the security info button while connection is secure: $isConnectionSecure")
        Log.i(TAG, "openSecureConnectionSubMenu: Trying to click the security info button and wait for $waitingTimeShort ms for a new window")
        mDevice.waitForWindowUpdate(packageName, waitingTimeShort)
        Log.i(TAG, "openSecureConnectionSubMenu: Clicked the security info button and waited for $waitingTimeShort ms for a new window")
    }
    fun verifySecureConnectionSubMenu(pageTitle: String = "", url: String = "", isConnectionSecure: Boolean) {
        Log.i(TAG, "verifySecureConnectionSubMenu: Waiting for $waitingTime ms for secure connection submenu to exist")
        secureConnectionSubMenu().waitForExists(waitingTime)
        Log.i(TAG, "verifySecureConnectionSubMenu: Waited for $waitingTime ms for secure connection submenu to exist")
        assertUIObjectExists(
            secureConnectionSubMenuPageTitle(pageTitle),
            secureConnectionSubMenuPageUrl(url),
            secureConnectionSubMenuSecurityInfo(isConnectionSecure),
            secureConnectionSubMenuLockIcon(),
            secureConnectionSubMenuCertificateInfo(),
        )
    }
    fun clickQuickActionSheetClearSiteData() {
        Log.i(TAG, "clickQuickActionSheetClearSiteData: Trying to click the \"Clear cookies and site data\" button")
        quickActionSheetClearSiteData().click()
        Log.i(TAG, "clickQuickActionSheetClearSiteData: Clicked the \"Clear cookies and site data\" button")
    }
    fun verifyClearSiteDataPrompt(url: String) {
        assertUIObjectExists(clearSiteDataPrompt(url))
        Log.i(TAG, "verifyClearSiteDataPrompt: Trying to verify that the \"Cancel\" dialog button is displayed")
        cancelClearSiteDataButton().check(matches(isDisplayed()))
        Log.i(TAG, "verifyClearSiteDataPrompt: Verified that the \"Cancel\" dialog button is displayed")
        Log.i(TAG, "verifyClearSiteDataPrompt: Trying to verify that the \"Delete\" dialog button is displayed")
        deleteSiteDataButton().check(matches(isDisplayed()))
        Log.i(TAG, "verifyClearSiteDataPrompt: Verified that the \"Delete\" dialog button is displayed")
    }

    class Transition
}

private fun quickActionSheet() =
    mDevice.findObject(UiSelector().resourceId("$packageName:id/quick_action_sheet"))

private fun quickActionSheetUrl(url: String) =
    mDevice.findObject(
        UiSelector()
            .resourceId("$packageName:id/url")
            .textContains(url),
    )

private fun quickActionSheetSecurityInfo(isConnectionSecure: Boolean) =
    if (isConnectionSecure) {
        mDevice.findObject(
            UiSelector()
                .resourceId("$packageName:id/securityInfo")
                .textContains(getStringResource(R.string.quick_settings_sheet_secure_connection_2)),
        )
    } else {
        mDevice.findObject(
            UiSelector()
                .resourceId("$packageName:id/securityInfo")
                .textContains(getStringResource(R.string.quick_settings_sheet_insecure_connection_2)),
        )
    }

private fun quickActionSheetTrackingProtectionSwitch() =
    mDevice.findObject(
        UiSelector()
            .resourceId("$packageName:id/trackingProtectionSwitch"),
    )

private fun quickActionSheetClearSiteData() =
    mDevice.findObject(
        UiSelector()
            .resourceId("$packageName:id/clearSiteData"),
    )

private fun secureConnectionSubMenu() =
    mDevice.findObject(
        UiSelector()
            .resourceId("$packageName:id/design_bottom_sheet"),
    )

private fun secureConnectionSubMenuPageTitle(pageTitle: String) =
    mDevice.findObject(
        UiSelector()
            .resourceId("$packageName:id/title")
            .textContains(pageTitle),
    )

private fun secureConnectionSubMenuPageUrl(url: String) =
    mDevice.findObject(
        UiSelector()
            .resourceId("$packageName:id/url")
            .textContains(url),
    )

private fun secureConnectionSubMenuLockIcon() =
    mDevice.findObject(
        UiSelector()
            .resourceId("$packageName:id/securityInfoIcon"),
    )

private fun secureConnectionSubMenuSecurityInfo(isConnectionSecure: Boolean) =
    if (isConnectionSecure) {
        mDevice.findObject(
            UiSelector()
                .resourceId("$packageName:id/securityInfo")
                .textContains(getStringResource(R.string.quick_settings_sheet_secure_connection_2)),
        )
    } else {
        mDevice.findObject(
            UiSelector()
                .resourceId("$packageName:id/securityInfo")
                .textContains(getStringResource(R.string.quick_settings_sheet_insecure_connection_2)),
        )
    }

private fun secureConnectionSubMenuCertificateInfo() =
    mDevice.findObject(
        UiSelector()
            .resourceId("$packageName:id/securityInfo"),
    )

private fun clearSiteDataPrompt(url: String) =
    mDevice.findObject(
        UiSelector()
            .resourceId("android:id/message")
            .textContains(url),
    )

private fun cancelClearSiteDataButton() = onView(withId(android.R.id.button2)).inRoot(RootMatchers.isDialog())
private fun deleteSiteDataButton() = onView(withId(android.R.id.button1)).inRoot(RootMatchers.isDialog())
