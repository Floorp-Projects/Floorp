/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

@file:Suppress("TooManyFunctions")
package org.mozilla.focus.activity.robots

import androidx.test.espresso.Espresso.onView
import androidx.test.espresso.ViewInteraction
import androidx.test.espresso.action.ViewActions.click
import androidx.test.espresso.assertion.ViewAssertions.matches
import androidx.test.espresso.matcher.ViewMatchers.isDisplayed
import androidx.test.espresso.matcher.ViewMatchers.withText
import androidx.test.uiautomator.UiScrollable
import androidx.test.uiautomator.UiSelector
import org.mozilla.focus.R
import org.mozilla.focus.helpers.TestHelper.getStringResource
import org.mozilla.focus.helpers.TestHelper.packageName
import org.mozilla.focus.helpers.TestHelper.waitingTime

class SettingsPrivacyMenuRobot {

    fun verifyPrivacySettingsItems() {
        privacySettingsList.waitForExists(waitingTime)
        adTrackersBlockSwitch().check(matches(isDisplayed()))
        analyticTrackersBlockSwitch().check(matches(isDisplayed()))
        socialTrackersBlockSwitch().check(matches(isDisplayed()))
        otherContentTrackersBlockSwitch().check(matches(isDisplayed()))
        blockWebFontsSwitch().check(matches(isDisplayed()))
        blockJavaScriptSwitch().check(matches(isDisplayed()))
        blockCookiesMenu().check(matches(isDisplayed()))
        useFingerprintSwitch().check(matches(isDisplayed()))
        stealthModeSwitch().check(matches(isDisplayed()))
        safeBrowsingSwitch().check(matches(isDisplayed()))
        sendDataSwitch().check(matches(isDisplayed()))
    }

    fun switchSafeBrowsingToggle(): ViewInteraction = safeBrowsingSwitch().perform(click())

    class Transition
}

private val privacySettingsList =
    UiScrollable(UiSelector().resourceId("$packageName:id/recycler_view"))

private fun adTrackersBlockSwitch(): ViewInteraction {
    privacySettingsList
        .scrollTextIntoView("Block ad trackers")
    return onView(withText("Block ad trackers"))
}

private fun analyticTrackersBlockSwitch(): ViewInteraction {
    privacySettingsList
        .scrollTextIntoView("Block analytic trackers")
    return onView(withText("Block analytic trackers"))
}

private fun socialTrackersBlockSwitch(): ViewInteraction {
    privacySettingsList
        .scrollTextIntoView("Block social trackers")
    return onView(withText("Block social trackers"))
}

private fun otherContentTrackersBlockSwitch(): ViewInteraction {
    privacySettingsList
        .scrollTextIntoView("Block other content trackers")
    return onView(withText("Block other content trackers"))
}

private fun blockWebFontsSwitch(): ViewInteraction {
    privacySettingsList
        .scrollTextIntoView("Block web fonts")
    return onView(withText("Block web fonts"))
}

private fun blockJavaScriptSwitch(): ViewInteraction {
    privacySettingsList
        .scrollTextIntoView("Block JavaScript")
    return onView(withText("Block JavaScript"))
}

private fun blockCookiesMenu(): ViewInteraction {
    privacySettingsList
        .scrollTextIntoView("Block cookies")
    return onView(withText("Block cookies"))
}

private fun useFingerprintSwitch(): ViewInteraction {
    val useFingerprintSwitchSummary = getStringResource(R.string.preference_security_biometric_summary)
    privacySettingsList.scrollTextIntoView(useFingerprintSwitchSummary)
    return onView(withText(useFingerprintSwitchSummary))
}

private fun stealthModeSwitch(): ViewInteraction {
    val stealthModeSwitchSummary = getStringResource(R.string.preference_privacy_stealth_summary)
    privacySettingsList.scrollTextIntoView(stealthModeSwitchSummary)
    return onView(withText(stealthModeSwitchSummary))
}

private fun safeBrowsingSwitch(): ViewInteraction {
    val safeBrowsingSwitchText = getStringResource(R.string.preference_safe_browsing_summary)
    privacySettingsList.scrollTextIntoView("Data Choices")
    return onView(withText(safeBrowsingSwitchText))
}

private fun sendDataSwitch(): ViewInteraction {
    val sendDataSwitchSummary = getStringResource(R.string.preference_mozilla_telemetry_summary2)
    privacySettingsList.scrollTextIntoView(sendDataSwitchSummary)
    return onView(withText(sendDataSwitchSummary))
}
