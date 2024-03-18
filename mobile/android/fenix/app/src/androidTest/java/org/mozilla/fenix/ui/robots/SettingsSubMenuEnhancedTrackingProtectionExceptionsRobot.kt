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

    fun verifyTPExceptionsDefaultView() {
        assertUIObjectExists(
            itemWithText("Exceptions let you disable tracking protection for selected sites."),
        )
        Log.i(TAG, "verifyTPExceptionsDefaultView: Trying to verify that the ETP exceptions learn more link is displayed")
        learnMoreLink().check(matches(isDisplayed()))
        Log.i(TAG, "verifyTPExceptionsDefaultView: Verified that the ETP exceptions learn more link is displayed")
    }

    fun openExceptionsLearnMoreLink() {
        Log.i(TAG, "openExceptionsLearnMoreLink: Trying to click the ETP exceptions learn more link")
        learnMoreLink().click()
        Log.i(TAG, "openExceptionsLearnMoreLink: Clicked the ETP exceptions learn more link")
    }

    fun removeOneSiteException(siteHost: String) {
        Log.i(TAG, "removeOneSiteException: Waiting for $waitingTime ms for exceptions list to exist to exist")
        exceptionsList().waitForExists(waitingTime)
        Log.i(TAG, "removeOneSiteException: Waited for $waitingTime ms for exceptions list to exist to exist")
        Log.i(TAG, "removeOneSiteException: Trying to click the delete site exception button")
        removeSiteExceptionButton(siteHost).click()
        Log.i(TAG, "removeOneSiteException: Clicked the delete site exception button")
    }

    fun verifySiteExceptionExists(siteUrl: String, shouldExist: Boolean) {
        Log.i(TAG, "verifySiteExceptionExists: Waiting for $waitingTime ms for exceptions list to exist to exist")
        exceptionsList().waitForExists(waitingTime)
        Log.i(TAG, "verifySiteExceptionExists: Waited for $waitingTime ms for exceptions list to exist to exist")
        assertUIObjectExists(itemContainingText(siteUrl), exists = shouldExist)
    }

    class Transition {
        fun goBack(interact: SettingsSubMenuEnhancedTrackingProtectionRobot.() -> Unit): SettingsSubMenuEnhancedTrackingProtectionRobot.Transition {
            Log.i(TAG, "goBack: Trying to click the navigate up toolbar button")
            goBackButton().click()
            Log.i(TAG, "goBack: Clicked the navigate up toolbar button")

            SettingsSubMenuEnhancedTrackingProtectionRobot().interact()
            return SettingsSubMenuEnhancedTrackingProtectionRobot.Transition()
        }

        fun disableExceptions(interact: SettingsSubMenuEnhancedTrackingProtectionExceptionsRobot.() -> Unit): Transition {
            Log.i(TAG, "disableExceptions: Trying to click the \"Turn on for all sites\" button")
            disableAllExceptionsButton().click()
            Log.i(TAG, "disableExceptions: Clicked the \"Turn on for all sites\" button")

            SettingsSubMenuEnhancedTrackingProtectionExceptionsRobot().interact()
            return Transition()
        }
    }
}

private fun goBackButton() =
    onView(allOf(withContentDescription("Navigate up")))

private fun learnMoreLink() = onView(withText("Learn more"))

private fun disableAllExceptionsButton() = onView(withId(R.id.removeAllExceptions))

private fun removeSiteExceptionButton(siteHost: String) =
    onView(
        allOf(
            withContentDescription("Delete"),
            hasSibling(withText(containsString(siteHost))),
        ),
    )

private fun exceptionsList() =
    mDevice.findObject(UiSelector().resourceId("$packageName:id/exceptions_list"))
