/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

@file:Suppress("TooManyFunctions")

package org.mozilla.fenix.ui.robots

import androidx.core.content.pm.PackageInfoCompat
import androidx.test.espresso.Espresso.onView
import androidx.test.platform.app.InstrumentationRegistry
import androidx.test.uiautomator.UiDevice
import org.hamcrest.CoreMatchers
import androidx.test.espresso.action.ViewActions.click
import androidx.test.espresso.assertion.ViewAssertions.matches
import androidx.test.espresso.matcher.ViewMatchers.withText
import androidx.test.espresso.matcher.ViewMatchers.withId
import androidx.test.espresso.matcher.ViewMatchers.withEffectiveVisibility
import androidx.test.espresso.matcher.ViewMatchers.withContentDescription
import androidx.test.espresso.matcher.ViewMatchers.hasDescendant
import androidx.test.espresso.matcher.ViewMatchers.Visibility
import org.hamcrest.CoreMatchers.containsString
import org.mozilla.fenix.R
import org.mozilla.fenix.helpers.TestHelper

/**
 * Implementation of Robot Pattern for the settings search sub menu.
 */
class SettingsSubMenuAboutRobot {

    fun verifyAboutFirefoxPreview() = assertFirefoxPreviewPage()

    class Transition {
        val mDevice = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation())

        fun goBack(interact: SettingsRobot.() -> Unit): SettingsRobot.Transition {
            goBackButton().perform(click())

            SettingsRobot().interact()
            return SettingsRobot.Transition()
        }
    }
}

private fun assertFirefoxPreviewPage() {
    assertVersionNumber()
    assertProductCompany()
    assertCurrentTimestamp()
    assertWhatIsNewInFirefoxPreview()
    assertSupport()
    assertPrivacyNotice()
    assertKnowYourRights()
    assertLicensingInformation()
    assertLibrariesUsed()
}

private fun assertVersionNumber() {
    val context = InstrumentationRegistry.getInstrumentation().targetContext

    val packageInfo = context.packageManager.getPackageInfo(context.packageName, 0)
    val versionCode = PackageInfoCompat.getLongVersionCode(packageInfo).toString()

    val buildNVersion = "${packageInfo.versionName} (Build #$versionCode)\n"
    val componentsVersion =
        "${mozilla.components.Build.version}, ${mozilla.components.Build.gitHash}"
    val geckoVersion =
        org.mozilla.geckoview.BuildConfig.MOZ_APP_VERSION + "-" + org.mozilla.geckoview.BuildConfig.MOZ_APP_BUILDID

    onView(withId(R.id.about_text))
        .check(matches(withText(containsString(buildNVersion))))
        .check(matches(withText(containsString(componentsVersion))))
        .check(matches(withText(containsString(geckoVersion))))
}

private fun assertProductCompany() {
    onView(withId(R.id.about_content))
        .check(matches(withText(containsString("Firefox Preview is produced by Mozilla."))))
}

private fun assertCurrentTimestamp() {
    onView(withId(R.id.build_date))
        .check(matches(withText(org.mozilla.fenix.BuildConfig.BUILD_DATE)))
}

private fun assertWhatIsNewInFirefoxPreview() {
    onView(withText("What’s new in Firefox Preview"))
        .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))
}

private fun assertSupport() {
    onView(withText("Support"))
        .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))
}

private fun assertPrivacyNotice() {
    onView(withText("Privacy notice"))
        .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))
}

private fun assertKnowYourRights() {
    onView(withText("Know your rights"))
        .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))
}

private fun assertLicensingInformation() {
    onView(withText("Licensing information"))
        .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))
}

private fun assertLibrariesUsed() {
    TestHelper.scrollToElementByText("Libraries that we use")
    onView(withText("Libraries that we use"))
        .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))
        .perform(click())

    onView(withId(R.id.action_bar)).check(matches(hasDescendant(withText(containsString("Firefox Preview | OSS Libraries")))))
}

private fun goBackButton() =
    onView(CoreMatchers.allOf(withContentDescription("Navigate up")))
