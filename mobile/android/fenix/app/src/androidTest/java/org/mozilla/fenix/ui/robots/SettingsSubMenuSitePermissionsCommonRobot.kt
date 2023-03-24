/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ui.robots

import androidx.test.espresso.Espresso.onView
import androidx.test.espresso.assertion.ViewAssertions.matches
import androidx.test.espresso.matcher.ViewMatchers.Visibility
import androidx.test.espresso.matcher.ViewMatchers.isChecked
import androidx.test.espresso.matcher.ViewMatchers.isDisplayed
import androidx.test.espresso.matcher.ViewMatchers.withContentDescription
import androidx.test.espresso.matcher.ViewMatchers.withEffectiveVisibility
import androidx.test.espresso.matcher.ViewMatchers.withId
import androidx.test.espresso.matcher.ViewMatchers.withText
import androidx.test.uiautomator.UiSelector
import org.hamcrest.CoreMatchers.allOf
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.mozilla.fenix.R
import org.mozilla.fenix.helpers.MatcherHelper.assertItemContainingTextExists
import org.mozilla.fenix.helpers.MatcherHelper.assertItemWithDescriptionExists
import org.mozilla.fenix.helpers.MatcherHelper.itemContainingText
import org.mozilla.fenix.helpers.MatcherHelper.itemWithDescription
import org.mozilla.fenix.helpers.TestAssetHelper.waitingTime
import org.mozilla.fenix.helpers.TestAssetHelper.waitingTimeShort
import org.mozilla.fenix.helpers.TestHelper.getStringResource
import org.mozilla.fenix.helpers.TestHelper.mDevice
import org.mozilla.fenix.helpers.TestHelper.packageName
import org.mozilla.fenix.helpers.assertIsChecked
import org.mozilla.fenix.helpers.click

/**
 * Implementation of Robot Pattern for the settings Site Permissions sub menu.
 */
class SettingsSubMenuSitePermissionsCommonRobot {

    fun verifyNavigationToolBarHeader(header: String) = assertNavigationToolBarHeader(header)

    fun verifyBlockAudioAndVideoOnMobileDataOnlyAudioAndVideoWillPlayOnWiFi() = assertBlockAudioAndVideoOnMobileDataOnlyAudioAndVideoWillPlayOnWiFi()

    fun verifyBlockAudioOnly() = assertBlockAudioOnly()

    fun verifyVideoAndAudioBlockedRecommended() = assertVideoAndAudioBlockedRecommended()

    fun verifyCheckAutoPlayRadioButtonDefault() = assertCheckAutoPayRadioButtonDefault()

    fun verifyAskToAllowButton(isChecked: Boolean = true) =
        onView(withId(R.id.ask_to_allow_radio))
            .check((matches(isDisplayed()))).assertIsChecked(isChecked)

    fun verifyBlockedButton(isChecked: Boolean = false) =
        onView(withId(R.id.block_radio))
            .check((matches(isDisplayed()))).assertIsChecked(isChecked)

    fun verifyBlockedByAndroid() = assertBlockedByAndroid()

    fun verifyUnblockedByAndroid() = assertUnblockedByAndroid()

    fun verifyToAllowIt() = assertToAllowIt()

    fun verifyGotoAndroidSettings() = assertGotoAndroidSettings()

    fun verifyTapPermissions() = assertTapPermissions()

    fun verifyToggleNameToON(name: String) = assertToggleNameToON(name)

    fun verifyGoToSettingsButton() = assertGoToSettingsButton()

    fun verifySitePermissionsAutoPlaySubMenuItems() {
        verifyBlockAudioAndVideoOnMobileDataOnlyAudioAndVideoWillPlayOnWiFi()
        verifyBlockAudioOnly()
        verifyVideoAndAudioBlockedRecommended()
        verifyCheckAutoPlayRadioButtonDefault()
    }

    fun verifySitePermissionsCommonSubMenuItems() {
        verifyAskToAllowButton()
        verifyBlockedButton()
        verifyBlockedByAndroid()
        verifyToAllowIt()
        verifyGotoAndroidSettings()
        verifyTapPermissions()
        verifyGoToSettingsButton()
    }

    fun verifyNotificationSubMenuItems() {
        verifyNotificationToolbar()
        verifyAskToAllowButton()
        verifyBlockedButton()
    }

    fun verifySitePermissionsPersistentStorageSubMenuItems() {
        verifyAskToAllowButton()
        verifyBlockedButton()
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
        assertTrue(
            mDevice.findObject(
                UiSelector().className("android.widget.RelativeLayout"),
            ).getChild(
                UiSelector()
                    .resourceId("android:id/title")
                    .textContains(permissionCategory),
            ).waitForExists(waitingTime),
        )

        assertTrue(
            mDevice.findObject(
                UiSelector().className("android.widget.RelativeLayout"),
            ).getChild(
                UiSelector()
                    .resourceId("android:id/summary")
                    .textContains("Only while app is in use"),
            ).waitForExists(waitingTime),
        )
    }

    fun verifyNotificationToolbar() {
        assertItemContainingTextExists(itemContainingText(getStringResource(R.string.preference_phone_feature_notification)))
        assertItemWithDescriptionExists(itemWithDescription(getStringResource(R.string.action_bar_up_description)))
    }

    class Transition {
        fun goBack(interact: SettingsSubMenuSitePermissionsRobot.() -> Unit): SettingsSubMenuSitePermissionsRobot.Transition {
            goBackButton().click()

            SettingsSubMenuSitePermissionsRobot().interact()
            return SettingsSubMenuSitePermissionsRobot.Transition()
        }
    }
}

private fun assertNavigationToolBarHeader(header: String) = onView(allOf(withContentDescription(header)))

private fun assertBlockAudioAndVideoOnMobileDataOnlyAudioAndVideoWillPlayOnWiFi() =
    onView(withId(R.id.block_radio))
        .check((matches(withEffectiveVisibility(Visibility.VISIBLE))))

private fun assertBlockAudioOnly() = onView(withId(R.id.third_radio))
    .check((matches(withEffectiveVisibility(Visibility.VISIBLE))))

private fun assertVideoAndAudioBlockedRecommended() = onView(withId(R.id.fourth_radio))
    .check((matches(withEffectiveVisibility(Visibility.VISIBLE))))

private fun assertCheckAutoPayRadioButtonDefault() {
    // Allow audio and video
    onView(withId(R.id.block_radio))
        .assertIsChecked(isChecked = false)

    // Block audio and video on cellular data only
    onView(withId(R.id.block_radio))
        .assertIsChecked(isChecked = false)

    // Block audio only
    onView(withId(R.id.third_radio))
        .assertIsChecked(isChecked = true)

    // Block audio and video
    onView(withId(R.id.fourth_radio))
        .assertIsChecked(isChecked = false)
}

private fun assertBlockedByAndroid() {
    blockedByAndroidContainer().waitForExists(waitingTime)
    assertTrue(
        mDevice.findObject(
            UiSelector().textContains(getStringResource(R.string.phone_feature_blocked_by_android)),
        ).waitForExists(waitingTimeShort),
    )
}

private fun assertUnblockedByAndroid() {
    blockedByAndroidContainer().waitUntilGone(waitingTime)
    assertFalse(
        mDevice.findObject(
            UiSelector().textContains(getStringResource(R.string.phone_feature_blocked_by_android)),
        ).waitForExists(waitingTimeShort),
    )
}

private fun blockedByAndroidContainer() = mDevice.findObject(UiSelector().resourceId("$packageName:id/permissions_blocked_container"))

private fun permissionSettingMenu() = mDevice.findObject(UiSelector().resourceId("$packageName:id/container"))

private fun assertToAllowIt() = onView(withText(R.string.phone_feature_blocked_intro))
    .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))

private fun assertGotoAndroidSettings() = onView(withText(R.string.phone_feature_blocked_step_settings))
    .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))

private fun assertTapPermissions() = onView(withText("2. Tap Permissions"))
    .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))

private fun assertToggleNameToON(name: String) = onView(withText(name))
    .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))

private fun assertGoToSettingsButton() =
    goToSettingsButton().check(matches(withEffectiveVisibility(Visibility.VISIBLE)))

private fun goBackButton() =
    onView(allOf(withContentDescription("Navigate up")))

private fun goToSettingsButton() = onView(withId(R.id.settings_button))
