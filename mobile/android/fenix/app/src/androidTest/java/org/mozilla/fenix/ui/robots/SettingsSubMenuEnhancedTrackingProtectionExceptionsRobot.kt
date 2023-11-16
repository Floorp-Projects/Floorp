/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ui.robots

import androidx.test.espresso.Espresso.onView
import androidx.test.espresso.assertion.ViewAssertions.matches
import androidx.test.espresso.matcher.ViewMatchers.Visibility
import androidx.test.espresso.matcher.ViewMatchers.hasSibling
import androidx.test.espresso.matcher.ViewMatchers.isDisplayed
import androidx.test.espresso.matcher.ViewMatchers.withContentDescription
import androidx.test.espresso.matcher.ViewMatchers.withEffectiveVisibility
import androidx.test.espresso.matcher.ViewMatchers.withId
import androidx.test.espresso.matcher.ViewMatchers.withText
import androidx.test.uiautomator.UiSelector
import org.hamcrest.CoreMatchers.allOf
import org.hamcrest.CoreMatchers.containsString
import org.mozilla.fenix.R
import org.mozilla.fenix.helpers.MatcherHelper.assertUIObjectExists
import org.mozilla.fenix.helpers.MatcherHelper.itemContainingText
import org.mozilla.fenix.helpers.MatcherHelper.itemWithText
import org.mozilla.fenix.helpers.TestAssetHelper.waitingTime
import org.mozilla.fenix.helpers.TestHelper.mDevice
import org.mozilla.fenix.helpers.TestHelper.packageName
import org.mozilla.fenix.helpers.click

/**
 * Implementation of Robot Pattern for the settings Enhanced Tracking Protection Exceptions sub menu.
 */
class SettingsSubMenuEnhancedTrackingProtectionExceptionsRobot {

    fun verifyNavigationToolBarHeader() = assertNavigationToolBarHeader()

    fun verifyTPExceptionsDefaultView() {
        assertUIObjectExists(
            itemWithText("Exceptions let you disable tracking protection for selected sites."),
        )
        learnMoreLink.check(matches(isDisplayed()))
    }

    fun openExceptionsLearnMoreLink() = learnMoreLink.click()

    fun removeOneSiteException(siteHost: String) {
        exceptionsList.waitForExists(waitingTime)
        removeSiteExceptionButton(siteHost).click()
    }

    fun verifySiteExceptionExists(siteUrl: String, shouldExist: Boolean) {
        exceptionsList.waitForExists(waitingTime)
        assertUIObjectExists(itemContainingText(siteUrl), exists = shouldExist)
    }

    class Transition {
        fun goBack(interact: SettingsSubMenuEnhancedTrackingProtectionRobot.() -> Unit): SettingsSubMenuEnhancedTrackingProtectionRobot.Transition {
            goBackButton().click()

            SettingsSubMenuEnhancedTrackingProtectionRobot().interact()
            return SettingsSubMenuEnhancedTrackingProtectionRobot.Transition()
        }

        fun disableExceptions(interact: SettingsSubMenuEnhancedTrackingProtectionExceptionsRobot.() -> Unit): Transition {
            disableAllExceptionsButton().click()

            SettingsSubMenuEnhancedTrackingProtectionExceptionsRobot().interact()
            return Transition()
        }
    }
}

private fun goBackButton() =
    onView(allOf(withContentDescription("Navigate up")))

private fun assertNavigationToolBarHeader() {
    onView(withText(R.string.preferences_tracking_protection_exceptions))
        .check((matches(withEffectiveVisibility(Visibility.VISIBLE))))
}

private val learnMoreLink = onView(withText("Learn more"))

private fun disableAllExceptionsButton() =
    onView(withId(R.id.removeAllExceptions)).click()

private fun removeSiteExceptionButton(siteHost: String) =
    onView(
        allOf(
            withContentDescription("Delete"),
            hasSibling(withText(containsString(siteHost))),
        ),
    )

private val exceptionsList =
    mDevice.findObject(UiSelector().resourceId("$packageName:id/exceptions_list"))
