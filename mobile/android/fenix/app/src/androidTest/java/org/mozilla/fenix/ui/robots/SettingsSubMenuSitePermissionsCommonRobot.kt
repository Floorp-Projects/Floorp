/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ui.robots

import android.util.Log
import androidx.test.espresso.Espresso.onView
import androidx.test.espresso.assertion.ViewAssertions.matches
import androidx.test.espresso.matcher.ViewMatchers.Visibility
import androidx.test.espresso.matcher.ViewMatchers.isDisplayed
import androidx.test.espresso.matcher.ViewMatchers.withContentDescription
import androidx.test.espresso.matcher.ViewMatchers.withEffectiveVisibility
import androidx.test.espresso.matcher.ViewMatchers.withId
import androidx.test.espresso.matcher.ViewMatchers.withText
import androidx.test.uiautomator.UiSelector
import org.hamcrest.CoreMatchers.allOf
import org.mozilla.fenix.R
import org.mozilla.fenix.helpers.Constants.TAG
import org.mozilla.fenix.helpers.DataGenerationHelper.getStringResource
import org.mozilla.fenix.helpers.MatcherHelper.assertUIObjectExists
import org.mozilla.fenix.helpers.MatcherHelper.itemContainingText
import org.mozilla.fenix.helpers.MatcherHelper.itemWithClassName
import org.mozilla.fenix.helpers.MatcherHelper.itemWithDescription
import org.mozilla.fenix.helpers.TestAssetHelper.waitingTime
import org.mozilla.fenix.helpers.TestAssetHelper.waitingTimeShort
import org.mozilla.fenix.helpers.TestHelper.mDevice
import org.mozilla.fenix.helpers.TestHelper.packageName
import org.mozilla.fenix.helpers.assertIsChecked
import org.mozilla.fenix.helpers.click

/**
 * Implementation of Robot Pattern for the settings Site Permissions sub menu.
 */
class SettingsSubMenuSitePermissionsCommonRobot {

    fun verifyBlockAudioAndVideoOnMobileDataOnly() {
        Log.i(TAG, "verifyBlockAudioAndVideoOnMobileDataOnly: Trying to verify that the \"Block audio and video on cellular data only\" option is visible")
        blockRadioButton().check((matches(withEffectiveVisibility(Visibility.VISIBLE))))
        Log.i(TAG, "verifyBlockAudioAndVideoOnMobileDataOnly: Verified that the \"Block audio and video on cellular data only\" option is visible")
    }

    fun verifyBlockAudioOnly() {
        Log.i(TAG, "verifyBlockAudioOnly: Trying to verify that the \"Block audio only\" option is visible")
        thirdRadioButton().check((matches(withEffectiveVisibility(Visibility.VISIBLE))))
        Log.i(TAG, "verifyBlockAudioOnly: Verified that the \"Block audio only\" option is visible")
    }

    fun verifyVideoAndAudioBlockedRecommended() {
        Log.i(TAG, "verifyVideoAndAudioBlockedRecommended: Trying to verify that the \"Block audio and video\" option is visible")
        onView(withId(R.id.fourth_radio)).check((matches(withEffectiveVisibility(Visibility.VISIBLE))))
        Log.i(TAG, "verifyVideoAndAudioBlockedRecommended: Verified that the \"Block audio and video\" option is visible")
    }

    fun verifyCheckAutoPlayRadioButtonDefault() {
        // Allow audio and video
        Log.i(TAG, "verifyCheckAutoPlayRadioButtonDefault: Trying to verify that the \"Allow audio and video\" radio button is not checked")
        askToAllowRadioButton()
            .assertIsChecked(isChecked = false)
        Log.i(TAG, "verifyCheckAutoPlayRadioButtonDefault: Verified that the \"Allow audio and video\" radio button is not checked")
        Log.i(TAG, "verifyCheckAutoPlayRadioButtonDefault: Trying to verify that the \"Block audio and video on cellular data only\" radio button is not checked")
        // Block audio and video on cellular data only
        blockRadioButton()
            .assertIsChecked(isChecked = false)
        Log.i(TAG, "verifyCheckAutoPlayRadioButtonDefault: Verified that the \"Block audio and video on cellular data only\" radio button is not checked")
        Log.i(TAG, "verifyCheckAutoPlayRadioButtonDefault: Trying to verify that the \"Block audio only\" radio button is checked")
        // Block audio only (default)
        thirdRadioButton()
            .assertIsChecked(isChecked = true)
        Log.i(TAG, "verifyCheckAutoPlayRadioButtonDefault: Verified that the \"Block audio only\" radio button is checked")
        Log.i(TAG, "verifyCheckAutoPlayRadioButtonDefault: Trying to verify that the \"Block audio and video\" radio button is not checked")
        // Block audio and video
        fourthRadioButton()
            .assertIsChecked(isChecked = false)
        Log.i(TAG, "verifyCheckAutoPlayRadioButtonDefault: Verified that the \"Block audio and video\" radio button is not checked")
    }

    fun verifyAskToAllowButton(isChecked: Boolean = true) {
        Log.i(TAG, "verifyAskToAllowButton: Trying to verify that the \"Ask to allow\" radio button is checked: $isChecked")
        onView(withId(R.id.ask_to_allow_radio))
            .check((matches(isDisplayed()))).assertIsChecked(isChecked)
        Log.i(TAG, "verifyAskToAllowButton: Verified that the \"Ask to allow\" radio button is checked: $isChecked")
    }

    fun verifyBlockedButton(isChecked: Boolean = false) {
        Log.i(TAG, "verifyBlockedButton: Trying to verify that the \"Blocked\" radio button is checked: $isChecked")
        onView(withId(R.id.block_radio))
            .check((matches(isDisplayed()))).assertIsChecked(isChecked)
        Log.i(TAG, "verifyBlockedButton: Verified that the \"Blocked\" radio button is checked: $isChecked")
    }

    fun verifyBlockedByAndroid() {
        Log.i(TAG, "verifyBlockedByAndroid: Waiting for $waitingTime ms for the \"Blocked by Android\" heading to exist")
        blockedByAndroidContainer().waitForExists(waitingTime)
        Log.i(TAG, "verifyBlockedByAndroid: Waited for $waitingTime ms for the \"Blocked by Android\" heading to exist")
        assertUIObjectExists(itemContainingText(getStringResource(R.string.phone_feature_blocked_by_android)))
    }

    fun verifyUnblockedByAndroid() {
        Log.i(TAG, "verifyUnblockedByAndroid: Waiting for $waitingTime ms for the \"Blocked by Android\" heading to be gone")
        blockedByAndroidContainer().waitUntilGone(waitingTime)
        Log.i(TAG, "verifyUnblockedByAndroid: Waited for $waitingTime ms for the \"Blocked by Android\" heading to be gone")
        assertUIObjectExists(itemContainingText(getStringResource(R.string.phone_feature_blocked_by_android)), exists = false)
    }

    fun verifyToAllowIt() {
        Log.i(TAG, "verifyToAllowIt: Trying to verify that the \"To allow it:\" instruction is visible")
        onView(withText(R.string.phone_feature_blocked_intro)).check(
            matches(
                withEffectiveVisibility(
                    Visibility.VISIBLE,
                ),
            ),
        )
        Log.i(TAG, "verifyToAllowIt: Verified that the \"To allow it:\" instruction is visible")
    }

    fun verifyGotoAndroidSettings() {
        Log.i(TAG, "verifyGotoAndroidSettings: Trying to verify that the \"1. Go to Android Settings\" step is visible")
        onView(withText(R.string.phone_feature_blocked_step_settings)).check(
            matches(
                withEffectiveVisibility(Visibility.VISIBLE),
            ),
        )
        Log.i(TAG, "verifyGotoAndroidSettings: Verified that the \"1. Go to Android Settings\" step is visible")
    }

    fun verifyTapPermissions() {
        Log.i(TAG, "verifyTapPermissions: Trying to verify that the \"2. Tap Permissions\" step is visible")
        onView(withText("2. Tap Permissions")).check(matches(withEffectiveVisibility(Visibility.VISIBLE)))
        Log.i(TAG, "verifyTapPermissions: Verified that the \"2. Tap Permissions\" step is visible")
    }

    fun verifyGoToSettingsButton() {
        Log.i(TAG, "verifyGoToSettingsButton: Trying to verify that the \"Go to settings\" button is visible")
        goToSettingsButton().check(matches(withEffectiveVisibility(Visibility.VISIBLE)))
        Log.i(TAG, "verifyGoToSettingsButton: Verified that the \"Go to settings\" button is visible")
    }

    fun verifySitePermissionsAutoPlaySubMenuItems() {
        verifyBlockAudioAndVideoOnMobileDataOnly()
        verifyBlockAudioOnly()
        verifyVideoAndAudioBlockedRecommended()
        verifyCheckAutoPlayRadioButtonDefault()
    }

    fun verifySitePermissionsCommonSubMenuItems() {
        verifyAskToAllowButton()
        verifyBlockedButton()
    }

    fun verifyBlockedByAndroidSection() {
        verifyBlockedByAndroid()
        verifyToAllowIt()
        verifyGotoAndroidSettings()
        verifyTapPermissions()
        verifyGoToSettingsButton()
    }

    fun verifyNotificationSubMenuItems() {
        verifyNotificationToolbar()
        verifySitePermissionsCommonSubMenuItems()
    }

    fun verifySitePermissionsPersistentStorageSubMenuItems() {
        verifyAskToAllowButton()
        verifyBlockedButton()
    }

    fun verifyDRMControlledContentSubMenuItems() {
        verifyAskToAllowButton()
        verifyBlockedButton()
        // Third option is "Allowed"
        Log.i(TAG, "verifyDRMControlledContentSubMenuItems: Trying to verify that \"Allowed\" is the third radio button")
        thirdRadioButton().check(matches(withText("Allowed")))
        Log.i(TAG, "verifyDRMControlledContentSubMenuItems: Verified that \"Allowed\" is the third radio button")
    }

    fun clickGoToSettingsButton() {
        Log.i(TAG, "clickGoToSettingsButton: Trying to click the \"Go to settings\" button")
        goToSettingsButton().click()
        Log.i(TAG, "clickGoToSettingsButton: Clicked the \"Go to settings\" button")
        Log.i(TAG, "clickGoToSettingsButton: Waiting for $waitingTime ms for system app info list to exist")
        mDevice.findObject(UiSelector().resourceId("com.android.settings:id/list"))
            .waitForExists(waitingTime)
        Log.i(TAG, "clickGoToSettingsButton: Waited for $waitingTime ms for system app info list to exist")
    }

    fun openAppSystemPermissionsSettings() {
        Log.i(TAG, "openAppSystemPermissionsSettings: Trying to click the system \"Permissions\" button")
        mDevice.findObject(UiSelector().textContains("Permissions")).click()
        Log.i(TAG, "openAppSystemPermissionsSettings: Clicked the system \"Permissions\" button")
    }

    fun switchAppPermissionSystemSetting(permissionCategory: String, permission: String) {
        Log.i(TAG, "switchAppPermissionSystemSetting: Trying to click the system permission category: $permissionCategory button")
        mDevice.findObject(UiSelector().textContains(permissionCategory)).click()
        Log.i(TAG, "switchAppPermissionSystemSetting: Clicked the system permission category: $permissionCategory button")

        if (permission == "Allow") {
            Log.i(TAG, "switchAppPermissionSystemSetting: Trying to click the system permission option: $permission")
            mDevice.findObject(UiSelector().textContains("Allow")).click()
            Log.i(TAG, "switchAppPermissionSystemSetting: Clicked the system permission option: $permission")
        } else {
            Log.i(TAG, "switchAppPermissionSystemSetting: Trying to click the system permission option: $permission")
            mDevice.findObject(UiSelector().textContains("Deny")).click()
            Log.i(TAG, "switchAppPermissionSystemSetting: Clicked the system permission option: $permission")
        }
    }

    fun goBackToSystemAppPermissionSettings() {
        Log.i(TAG, "goBackToSystemAppPermissionSettings: Trying to click the device back button")
        mDevice.pressBack()
        Log.i(TAG, "goBackToSystemAppPermissionSettings: Clicked the device back button")
        Log.i(TAG, "goBackToSystemAppPermissionSettings: Waiting for device to be idle for $waitingTime ms")
        mDevice.waitForIdle(waitingTime)
        Log.i(TAG, "goBackToSystemAppPermissionSettings: Waited for device to be idle for $waitingTime ms")
    }

    fun goBackToPermissionsSettingsSubMenu() {
        while (!permissionSettingMenu().waitForExists(waitingTimeShort)) {
            Log.i(TAG, "goBackToPermissionsSettingsSubMenu: The permissions settings menu does not exist")
            Log.i(TAG, "goBackToPermissionsSettingsSubMenu: Trying to click the device back button")
            mDevice.pressBack()
            Log.i(TAG, "goBackToPermissionsSettingsSubMenu: Clicked the device back button")
            Log.i(TAG, "goBackToPermissionsSettingsSubMenu: Waiting for device to be idle for $waitingTime ms")
            mDevice.waitForIdle(waitingTime)
            Log.i(TAG, "goBackToPermissionsSettingsSubMenu: Waited for device to be idle for $waitingTime ms")
        }
    }

    fun verifySystemGrantedPermission(permissionCategory: String) {
        assertUIObjectExists(
            itemWithClassName("android.widget.RelativeLayout")
                .getChild(
                    UiSelector()
                        .resourceId("android:id/title")
                        .textContains(permissionCategory),
                ),
            itemWithClassName("android.widget.RelativeLayout")
                .getChild(
                    UiSelector()
                        .resourceId("android:id/summary")
                        .textContains("Only while app is in use"),
                ),
        )
    }

    fun verifyNotificationToolbar() =
        assertUIObjectExists(
            itemContainingText(getStringResource(R.string.preference_phone_feature_notification)),
            itemWithDescription(getStringResource(R.string.action_bar_up_description)),
        )

    fun selectAutoplayOption(text: String) {
        Log.i(TAG, "selectAutoplayOption: Trying to click the $text radio button")
        when (text) {
            "Allow audio and video" -> askToAllowRadioButton().click()
            "Block audio and video on cellular data only" -> blockRadioButton().click()
            "Block audio only" -> thirdRadioButton().click()
            "Block audio and video" -> fourthRadioButton().click()
        }
        Log.i(TAG, "selectAutoplayOption: Clicked the $text radio button button")
    }

    fun selectPermissionSettingOption(text: String) {
        Log.i(TAG, "selectPermissionSettingOption: Trying to click the $text radio button")
        when (text) {
            "Ask to allow" -> askToAllowRadioButton().click()
            "Blocked" -> blockRadioButton().click()
        }
        Log.i(TAG, "selectPermissionSettingOption: Clicked the $text radio button")
    }

    fun selectDRMControlledContentPermissionSettingOption(text: String) {
        Log.i(TAG, "selectDRMControlledContentPermissionSettingOption: Trying to click the $text radio button")
        when (text) {
            "Ask to allow" -> askToAllowRadioButton().click()
            "Blocked" -> blockRadioButton().click()
            "Allowed" -> thirdRadioButton().click()
        }
        Log.i(TAG, "selectDRMControlledContentPermissionSettingOption: Clicked the $text radio button")
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

// common Blocked radio button for all settings
private fun blockRadioButton() = onView(withId(R.id.block_radio))

// common Ask to Allow radio button for all settings
private fun askToAllowRadioButton() = onView(withId(R.id.ask_to_allow_radio))

// common extra 3rd radio button for all settings
private fun thirdRadioButton() = onView(withId(R.id.third_radio))

// common extra 4th radio button for all settings
private fun fourthRadioButton() = onView(withId(R.id.fourth_radio))

private fun blockedByAndroidContainer() = mDevice.findObject(UiSelector().resourceId("$packageName:id/permissions_blocked_container"))

private fun permissionSettingMenu() = mDevice.findObject(UiSelector().resourceId("$packageName:id/container"))

private fun goBackButton() =
    onView(allOf(withContentDescription("Navigate up")))

private fun goToSettingsButton() = onView(withId(R.id.settings_button))
