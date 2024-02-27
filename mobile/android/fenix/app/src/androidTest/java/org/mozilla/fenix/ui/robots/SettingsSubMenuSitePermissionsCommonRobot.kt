/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ui.robots

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

    fun verifyBlockAudioAndVideoOnMobileDataOnlyAudioAndVideoWillPlayOnWiFi() =
        blockRadioButton().check((matches(withEffectiveVisibility(Visibility.VISIBLE))))

    fun verifyBlockAudioOnly() = thirdRadioButton().check((matches(withEffectiveVisibility(Visibility.VISIBLE))))

    fun verifyVideoAndAudioBlockedRecommended() =
        onView(withId(R.id.fourth_radio)).check((matches(withEffectiveVisibility(Visibility.VISIBLE))))

    fun verifyCheckAutoPlayRadioButtonDefault() {
        // Allow audio and video
        askToAllowRadioButton()
            .assertIsChecked(isChecked = false)

        // Block audio and video on cellular data only
        blockRadioButton()
            .assertIsChecked(isChecked = false)

        // Block audio only (default)
        thirdRadioButton()
            .assertIsChecked(isChecked = true)

        // Block audio and video
        fourthRadioButton()
            .assertIsChecked(isChecked = false)
    }

    fun verifyAskToAllowButton(isChecked: Boolean = true) =
        onView(withId(R.id.ask_to_allow_radio))
            .check((matches(isDisplayed()))).assertIsChecked(isChecked)

    fun verifyBlockedButton(isChecked: Boolean = false) =
        onView(withId(R.id.block_radio))
            .check((matches(isDisplayed()))).assertIsChecked(isChecked)

    fun verifyBlockedByAndroid() {
        blockedByAndroidContainer().waitForExists(waitingTime)
        assertUIObjectExists(itemContainingText(getStringResource(R.string.phone_feature_blocked_by_android)))
    }

    fun verifyUnblockedByAndroid() {
        blockedByAndroidContainer().waitUntilGone(waitingTime)
        assertUIObjectExists(itemContainingText(getStringResource(R.string.phone_feature_blocked_by_android)), exists = false)
    }

    fun verifyToAllowIt() =
        onView(withText(R.string.phone_feature_blocked_intro)).check(matches(withEffectiveVisibility(Visibility.VISIBLE)))

    fun verifyGotoAndroidSettings() =
        onView(withText(R.string.phone_feature_blocked_step_settings)).check(matches(withEffectiveVisibility(Visibility.VISIBLE)))

    fun verifyTapPermissions() =
        onView(withText("2. Tap Permissions")).check(matches(withEffectiveVisibility(Visibility.VISIBLE)))

    fun verifyGoToSettingsButton() = goToSettingsButton().check(matches(withEffectiveVisibility(Visibility.VISIBLE)))

    fun verifySitePermissionsAutoPlaySubMenuItems() {
        verifyBlockAudioAndVideoOnMobileDataOnlyAudioAndVideoWillPlayOnWiFi()
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
        thirdRadioButton().check(matches(withText("Allowed")))
    }

    fun clickGoToSettingsButton() {
        goToSettingsButton().click()
        mDevice.findObject(UiSelector().resourceId("com.android.settings:id/list"))
            .waitForExists(waitingTime)
    }

    fun openAppSystemPermissionsSettings() {
        mDevice.findObject(UiSelector().textContains("Permissions")).click()
    }

    fun switchAppPermissionSystemSetting(permissionCategory: String, permission: String) {
        mDevice.findObject(UiSelector().textContains(permissionCategory)).click()

        if (permission == "Allow") {
            mDevice.findObject(UiSelector().textContains("Allow")).click()
        } else {
            mDevice.findObject(UiSelector().textContains("Deny")).click()
        }
    }

    fun goBackToSystemAppPermissionSettings() {
        mDevice.pressBack()
        mDevice.waitForIdle(waitingTime)
    }

    fun goBackToPermissionsSettingsSubMenu() {
        while (!permissionSettingMenu().waitForExists(waitingTimeShort)) {
            mDevice.pressBack()
            mDevice.waitForIdle(waitingTime)
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
        when (text) {
            "Allow audio and video" -> askToAllowRadioButton().click()
            "Block audio and video on cellular data only" -> blockRadioButton().click()
            "Block audio only" -> thirdRadioButton().click()
            "Block audio and video" -> fourthRadioButton().click()
        }
    }

    fun selectPermissionSettingOption(text: String) {
        when (text) {
            "Ask to allow" -> askToAllowRadioButton().click()
            "Blocked" -> blockRadioButton().click()
        }
    }

    fun selectDRMControlledContentPermissionSettingOption(text: String) {
        when (text) {
            "Ask to allow" -> askToAllowRadioButton().click()
            "Blocked" -> blockRadioButton().click()
            "Allowed" -> thirdRadioButton().click()
        }
    }

    class Transition {
        fun goBack(interact: SettingsSubMenuSitePermissionsRobot.() -> Unit): SettingsSubMenuSitePermissionsRobot.Transition {
            goBackButton().click()

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
