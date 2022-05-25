/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

@file:Suppress("TooManyFunctions")
package org.mozilla.focus.activity.robots

import androidx.test.espresso.Espresso.onView
import androidx.test.espresso.Espresso.openActionBarOverflowOrOptionsMenu
import androidx.test.espresso.ViewInteraction
import androidx.test.espresso.action.ViewActions.click
import androidx.test.espresso.assertion.ViewAssertions.matches
import androidx.test.espresso.matcher.ViewMatchers.isChecked
import androidx.test.espresso.matcher.ViewMatchers.isDisplayed
import androidx.test.espresso.matcher.ViewMatchers.isEnabled
import androidx.test.espresso.matcher.ViewMatchers.isNotChecked
import androidx.test.espresso.matcher.ViewMatchers.withId
import androidx.test.espresso.matcher.ViewMatchers.withText
import androidx.test.uiautomator.UiScrollable
import androidx.test.uiautomator.UiSelector
import org.hamcrest.CoreMatchers.allOf
import org.hamcrest.CoreMatchers.containsString
import org.hamcrest.Matchers
import org.junit.Assert.assertTrue
import org.mozilla.focus.R
import org.mozilla.focus.helpers.EspressoHelper.hasCousin
import org.mozilla.focus.helpers.TestHelper.getStringResource
import org.mozilla.focus.helpers.TestHelper.getTargetContext
import org.mozilla.focus.helpers.TestHelper.mDevice
import org.mozilla.focus.helpers.TestHelper.packageName
import org.mozilla.focus.helpers.TestHelper.waitingTime
import org.mozilla.focus.helpers.TestHelper.waitingTimeShort

class SettingsPrivacyMenuRobot {

    fun verifyPrivacySettingsItems() {
        privacySettingsList.waitForExists(waitingTime)
        adTrackersBlockSwitch().check(matches(isDisplayed()))
        assertAdTrackersBlockSwitchState()
        analyticTrackersBlockSwitch().check(matches(isDisplayed()))
        assertAnalyticTrackersBlockSwitchState()
        socialTrackersBlockSwitch().check(matches(isDisplayed()))
        assertSocialTrackersBlockSwitchState()
        otherContentTrackersBlockSwitch().check(matches(isDisplayed()))
        assertOtherContentTrackersBlockSwitchState()
        blockWebFontsSwitch().check(matches(isDisplayed()))
        assertBlockWebFontsSwitchState()
        blockJavaScriptSwitch().check(matches(isDisplayed()))
        assertBlockJavaScriptSwitchState()
        cookiesAndSiteDataSection().check(matches(isDisplayed()))
        blockCookies().check(matches(isDisplayed()))
        blockCookiesDefaultOption().check(matches(isDisplayed()))
        assertTrue(sitePermissions().exists())
        verifyExceptionsListDisabled()
        useFingerprintSwitch().check(matches(isDisplayed()))
        assertUseFingerprintSwitchState()
        stealthModeSwitch().check(matches(isDisplayed()))
        assertStealthModeSwitchState()
        safeBrowsingSwitch().check(matches(isDisplayed()))
        assertSafeBrowsingSwitchState()
        httpsOnlyModeSwitch().check(matches(isDisplayed()))
        assertHttpsOnlyModeSwitchState()
        sendDataSwitch().check(matches(isDisplayed()))
        if (packageName != "org.mozilla.focus.debug") {
            assertSendDataSwitchState(true)
        } else {
            assertSendDataSwitchState()
        }
        studiesOption().check(matches(isDisplayed()))
        studiesDefaultOption().check(matches(isDisplayed()))
    }

    fun verifyCookiesAndSiteDataSection() {
        privacySettingsList.waitForExists(waitingTime)
        cookiesAndSiteDataSection().check(matches(isDisplayed()))
        blockCookies().check(matches(isDisplayed()))
        blockCookiesDefaultOption().check(matches(isDisplayed()))
        assertTrue(sitePermissions().exists())
    }

    fun verifyBlockCookiesPrompt() {
        assertTrue(blockCookiesPromptHeading.waitForExists(waitingTimeShort))
        assertTrue(blockCookiesYesPleaseOption.waitForExists(waitingTimeShort))
        assertTrue(block3rdPartyCookiesOnlyOption.waitForExists(waitingTimeShort))
        assertTrue(block3rdPartyTrackerCookiesOnlyOption.waitForExists(waitingTimeShort))
        assertTrue(blockCrossSiteCookiesOption.waitForExists(waitingTimeShort))
        assertTrue(noThanksOption.waitForExists(waitingTimeShort))
        assertTrue(cancelBlockCookiesPrompt.waitForExists(waitingTimeShort))
    }

    fun verifyBlockAdTrackersEnabled(enabled: Boolean) {
        if (enabled) {
            adTrackersBlockSwitch()
                .check(
                    matches(
                        hasCousin(
                            allOf(
                                withId(R.id.switchWidget),
                                isChecked()
                            )
                        )
                    )
                )
        } else {
            adTrackersBlockSwitch()
                .check(
                    matches(
                        hasCousin(
                            allOf(
                                withId(R.id.switchWidget),
                                isNotChecked()
                            )
                        )
                    )
                )
        }
    }

    fun verifyBlockAnalyticTrackersEnabled(enabled: Boolean) {
        if (enabled) {
            analyticTrackersBlockSwitch()
                .check(
                    matches(
                        hasCousin(
                            allOf(
                                withId(R.id.switchWidget),
                                isChecked()
                            )
                        )
                    )
                )
        } else {
            analyticTrackersBlockSwitch()
                .check(
                    matches(
                        hasCousin(
                            allOf(
                                withId(R.id.switchWidget),
                                isNotChecked()
                            )
                        )
                    )
                )
        }
    }

    fun verifyBlockSocialTrackersEnabled(enabled: Boolean) {
        if (enabled) {
            socialTrackersBlockSwitch()
                .check(
                    matches(
                        hasCousin(
                            allOf(
                                withId(R.id.switchWidget),
                                isChecked()
                            )
                        )
                    )
                )
        } else {
            socialTrackersBlockSwitch()
                .check(
                    matches(
                        hasCousin(
                            allOf(
                                withId(R.id.switchWidget),
                                isNotChecked()
                            )
                        )
                    )
                )
        }
    }

    fun verifyBlockOtherTrackersEnabled(enabled: Boolean) {
        if (enabled) {
            otherContentTrackersBlockSwitch()
                .check(
                    matches(
                        hasCousin(
                            allOf(
                                withId(R.id.switchWidget),
                                isChecked()
                            )
                        )
                    )
                )
        } else {
            otherContentTrackersBlockSwitch()
                .check(
                    matches(
                        hasCousin(
                            allOf(
                                withId(R.id.switchWidget),
                                isNotChecked()
                            )
                        )
                    )
                )
        }
    }

    fun clickAdTrackersBlockSwitch() = adTrackersBlockSwitch().perform(click())

    fun clickAnalyticsTrackersBlockSwitch() = analyticTrackersBlockSwitch().perform(click())

    fun clickSocialTrackersBlockSwitch() = socialTrackersBlockSwitch().perform(click())

    fun clickOtherContentTrackersBlockSwitch() = otherContentTrackersBlockSwitch().perform(click())

    fun clickBlockCookies() = blockCookies().perform(click())

    fun clickCancelBlockCookiesPrompt() {
        cancelBlockCookiesPrompt.click()
        mDevice.waitForIdle(waitingTimeShort)
    }

    fun clickYesPleaseOption() = blockCookiesYesPleaseOption.click()

    fun switchSafeBrowsingToggle(): ViewInteraction = safeBrowsingSwitch().perform(click())

    fun verifyExceptionsListDisabled() {
        exceptionsList()
            .check(matches(Matchers.not(isEnabled())))
    }

    fun openExceptionsList() {
        exceptionsList()
            .check(matches(isEnabled()))
            .perform(click())
    }

    fun verifyExceptionURL(url: String) {
        onView(withId(R.id.domainView)).check(matches(withText(containsString(url))))
    }

    fun removeException() {
        openActionBarOverflowOrOptionsMenu(getTargetContext)
        onView(withText("Remove"))
            .perform(click())
        onView(withId(R.id.checkbox))
            .perform(click())
        onView(withId(R.id.remove))
            .perform(click())
    }

    fun removeAllExceptions() {
        onView(withId(R.id.removeAllExceptions))
            .perform(click())
    }

    class Transition {
        fun goBackToSettings(interact: SettingsRobot.() -> Unit): SettingsRobot.Transition {
            mDevice.pressBack()

            SettingsRobot().interact()
            return SettingsRobot.Transition()
        }

        fun clickSitePermissionsSettings(interact: SettingsSitePermissionsRobot.() -> Unit): SettingsSitePermissionsRobot.Transition {
            sitePermissions().waitForExists(waitingTime)
            sitePermissions().click()

            SettingsSitePermissionsRobot().interact()
            return SettingsSitePermissionsRobot.Transition()
        }
    }
}

private val privacySettingsList =
    UiScrollable(UiSelector().resourceId("$packageName:id/recycler_view"))

private fun adTrackersBlockSwitch(): ViewInteraction {
    privacySettingsList
        .scrollTextIntoView("Block ad trackers")
    return onView(withText("Block ad trackers"))
}

private fun assertAdTrackersBlockSwitchState(enabled: Boolean = true) {
    if (enabled) {
        adTrackersBlockSwitch()
            .check(
                matches(
                    hasCousin(
                        allOf(
                            withId(R.id.switchWidget),
                            isChecked()
                        )
                    )
                )
            )
    } else {
        adTrackersBlockSwitch()
            .check(
                matches(
                    hasCousin(
                        allOf(
                            withId(R.id.switchWidget),
                            isNotChecked()
                        )
                    )
                )
            )
    }
}

private fun analyticTrackersBlockSwitch(): ViewInteraction {
    privacySettingsList
        .scrollTextIntoView("Block analytic trackers")
    return onView(withText("Block analytic trackers"))
}

private fun assertAnalyticTrackersBlockSwitchState(enabled: Boolean = true) {
    if (enabled) {
        analyticTrackersBlockSwitch()
            .check(
                matches(
                    hasCousin(
                        allOf(
                            withId(R.id.switchWidget),
                            isChecked()
                        )
                    )
                )
            )
    } else {
        analyticTrackersBlockSwitch()
            .check(
                matches(
                    hasCousin(
                        allOf(
                            withId(R.id.switchWidget),
                            isNotChecked()
                        )
                    )
                )
            )
    }
}

private fun socialTrackersBlockSwitch(): ViewInteraction {
    privacySettingsList
        .scrollTextIntoView("Block social trackers")
    return onView(withText("Block social trackers"))
}

private fun assertSocialTrackersBlockSwitchState(enabled: Boolean = true) {
    if (enabled) {
        socialTrackersBlockSwitch()
            .check(
                matches(
                    hasCousin(
                        allOf(
                            withId(R.id.switchWidget),
                            isChecked()
                        )
                    )
                )
            )
    } else {
        socialTrackersBlockSwitch()
            .check(
                matches(
                    hasCousin(
                        allOf(
                            withId(R.id.switchWidget),
                            isNotChecked()
                        )
                    )
                )
            )
    }
}

private fun otherContentTrackersBlockSwitch(): ViewInteraction {
    privacySettingsList
        .scrollTextIntoView("Block other content trackers")
    return onView(withText("Block other content trackers"))
}

private fun assertOtherContentTrackersBlockSwitchState(enabled: Boolean = false) {
    if (enabled) {
        otherContentTrackersBlockSwitch()
            .check(
                matches(
                    hasCousin(
                        allOf(
                            withId(R.id.switchWidget),
                            isChecked()
                        )
                    )
                )
            )
    } else {
        otherContentTrackersBlockSwitch()
            .check(
                matches(
                    hasCousin(
                        allOf(
                            withId(R.id.switchWidget),
                            isNotChecked()
                        )
                    )
                )
            )
    }
}

private fun blockWebFontsSwitch(): ViewInteraction {
    privacySettingsList
        .scrollTextIntoView("Block web fonts")
    return onView(withText("Block web fonts"))
}

private fun assertBlockWebFontsSwitchState(enabled: Boolean = false) {
    if (enabled) {
        blockWebFontsSwitch()
            .check(
                matches(
                    hasCousin(
                        allOf(
                            withId(R.id.switchWidget),
                            isChecked()
                        )
                    )
                )
            )
    } else {
        blockWebFontsSwitch()
            .check(
                matches(
                    hasCousin(
                        allOf(
                            withId(R.id.switchWidget),
                            isNotChecked()
                        )
                    )
                )
            )
    }
}

private fun blockJavaScriptSwitch(): ViewInteraction {
    privacySettingsList
        .scrollTextIntoView("Block JavaScript")
    return onView(withText("Block JavaScript"))
}

private fun assertBlockJavaScriptSwitchState(enabled: Boolean = false) {
    if (enabled) {
        blockJavaScriptSwitch()
            .check(
                matches(
                    hasCousin(
                        allOf(
                            withId(R.id.switchWidget),
                            isChecked()
                        )
                    )
                )
            )
    } else {
        blockJavaScriptSwitch()
            .check(
                matches(
                    hasCousin(
                        allOf(
                            withId(R.id.switchWidget),
                            isNotChecked()
                        )
                    )
                )
            )
    }
}

private fun cookiesAndSiteDataSection(): ViewInteraction {
    privacySettingsList
        .scrollTextIntoView("Cookies and Site Data")
    return onView(withText(R.string.preference_category_cookies))
}

private fun blockCookies(): ViewInteraction {
    privacySettingsList
        .scrollTextIntoView("Cookies and Site Data")
    return onView(withText(R.string.preference_privacy_category_cookies))
}

private fun blockCookiesDefaultOption(): ViewInteraction {
    privacySettingsList
        .scrollTextIntoView("Cookies and Site Data")
    return onView(withText(R.string.preference_privacy_should_block_cookies_cross_site_option))
}

private fun sitePermissions() =
    privacySettingsList
        .getChildByText(UiSelector().text("Site permissions"), "Site permissions", true)

private fun useFingerprintSwitch(): ViewInteraction {
    val useFingerprintSwitchSummary = getStringResource(R.string.preference_security_biometric_summary2)
    privacySettingsList.scrollTextIntoView(useFingerprintSwitchSummary)
    return onView(withText(useFingerprintSwitchSummary))
}

private fun assertUseFingerprintSwitchState(enabled: Boolean = false) {
    if (enabled) {
        useFingerprintSwitch()
            .check(
                matches(
                    hasCousin(
                        allOf(
                            withId(R.id.switchWidget),
                            isChecked()
                        )
                    )
                )
            )
    } else {
        useFingerprintSwitch()
            .check(
                matches(
                    hasCousin(
                        allOf(
                            withId(R.id.switchWidget),
                            isNotChecked()
                        )
                    )
                )
            )
    }
}

private fun stealthModeSwitch(): ViewInteraction {
    val stealthModeSwitchSummary = getStringResource(R.string.preference_privacy_stealth_summary)
    privacySettingsList.scrollTextIntoView(stealthModeSwitchSummary)
    return onView(withText(stealthModeSwitchSummary))
}

private fun assertStealthModeSwitchState(enabled: Boolean = false) {
    if (enabled) {
        stealthModeSwitch()
            .check(
                matches(
                    hasCousin(
                        allOf(
                            withId(R.id.switchWidget),
                            isChecked()
                        )
                    )
                )
            )
    } else {
        stealthModeSwitch()
            .check(
                matches(
                    hasCousin(
                        allOf(
                            withId(R.id.switchWidget),
                            isNotChecked()
                        )
                    )
                )
            )
    }
}

private fun safeBrowsingSwitch(): ViewInteraction {
    val safeBrowsingSwitchText =
        mDevice.findObject(
            UiSelector().text(
                getStringResource(R.string.preference_safe_browsing_summary)
            )
        )
    privacySettingsList.scrollToEnd(3)
    privacySettingsList.scrollIntoView(safeBrowsingSwitchText)
    return onView(withText(getStringResource(R.string.preference_safe_browsing_summary)))
}

private fun assertSafeBrowsingSwitchState(enabled: Boolean = true) {
    if (enabled) {
        safeBrowsingSwitch()
            .check(
                matches(
                    hasCousin(
                        allOf(
                            withId(R.id.switchWidget),
                            isChecked()
                        )
                    )
                )
            )
    } else {
        safeBrowsingSwitch()
            .check(
                matches(
                    hasCousin(
                        allOf(
                            withId(R.id.switchWidget),
                            isNotChecked()
                        )
                    )
                )
            )
    }
}

private fun httpsOnlyModeSwitch(): ViewInteraction {
    val httpsOnlyModeSwitchText = getStringResource(R.string.preference_https_only_title)
    privacySettingsList.scrollTextIntoView(httpsOnlyModeSwitchText)
    return onView(withText(httpsOnlyModeSwitchText))
}

private fun assertHttpsOnlyModeSwitchState(enabled: Boolean = true) {
    if (enabled) {
        httpsOnlyModeSwitch()
            .check(
                matches(
                    hasCousin(
                        allOf(
                            withId(R.id.switchWidget),
                            isChecked()
                        )
                    )
                )
            )
    } else {
        httpsOnlyModeSwitch()
            .check(
                matches(
                    hasCousin(
                        allOf(
                            withId(R.id.switchWidget),
                            isNotChecked()
                        )
                    )
                )
            )
    }
}

private fun sendDataSwitch(): ViewInteraction {
    val sendDataSwitchSummary = getStringResource(R.string.preference_mozilla_telemetry_summary2)
    privacySettingsList.scrollTextIntoView(sendDataSwitchSummary)
    return onView(withText(sendDataSwitchSummary))
}

private fun assertSendDataSwitchState(enabled: Boolean = false) {
    if (enabled) {
        sendDataSwitch()
            .check(
                matches(
                    hasCousin(
                        allOf(
                            withId(R.id.switchWidget),
                            isChecked()
                        )
                    )
                )
            )
    } else {
        sendDataSwitch()
            .check(
                matches(
                    hasCousin(
                        allOf(
                            withId(R.id.switchWidget),
                            isNotChecked()
                        )
                    )
                )
            )
    }
}

private fun studiesOption(): ViewInteraction {
    val studies = getStringResource(R.string.preference_studies)
    privacySettingsList.scrollTextIntoView(studies)
    return onView(withText(R.string.preference_studies))
}

private fun studiesDefaultOption(): ViewInteraction {
    privacySettingsList.scrollToEnd(3)
    return onView(withText(R.string.preference_state_on))
}

private fun exceptionsList(): ViewInteraction {
    val exceptionsTitle = getStringResource(R.string.preference_exceptions)
    privacySettingsList.scrollTextIntoView(exceptionsTitle)
    return onView(withText(exceptionsTitle))
}

private val blockCookiesPromptHeading =
    mDevice.findObject(
        UiSelector()
            .resourceId("$packageName:id/alertTitle")
            .textContains(getStringResource(R.string.preference_block_cookies_title))
    )

private val blockCookiesYesPleaseOption =
    mDevice.findObject(
        UiSelector()
            .textContains(getStringResource(R.string.preference_privacy_should_block_cookies_yes_option2))
    )

private val block3rdPartyCookiesOnlyOption =
    mDevice.findObject(
        UiSelector()
            .textContains(getStringResource(R.string.preference_privacy_should_block_cookies_yes_option2))
    )

private val block3rdPartyTrackerCookiesOnlyOption =
    mDevice.findObject(
        UiSelector()
            .textContains(getStringResource(R.string.preference_privacy_should_block_cookies_third_party_tracker_cookies_option))
    )

private val blockCrossSiteCookiesOption =
    mDevice.findObject(
        UiSelector()
            .textContains(getStringResource(R.string.preference_privacy_should_block_cookies_cross_site_option))
    )

private val noThanksOption =
    mDevice.findObject(
        UiSelector()
            .textContains(getStringResource(R.string.preference_privacy_should_block_cookies_no_option2))
    )

private val cancelBlockCookiesPrompt =
    mDevice.findObject(
        UiSelector()
            .textContains(getStringResource(R.string.action_cancel))
    )
