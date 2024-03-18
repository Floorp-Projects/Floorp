/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ui.robots

import androidx.test.espresso.Espresso.onView
import androidx.test.espresso.matcher.ViewMatchers.withContentDescription
import androidx.test.uiautomator.UiSelector
import org.mozilla.fenix.R
import org.mozilla.fenix.helpers.MatcherHelper.assertUIObjectExists
import org.mozilla.fenix.helpers.MatcherHelper.itemContainingText
import org.mozilla.fenix.helpers.MatcherHelper.itemWithResId
import org.mozilla.fenix.helpers.TestHelper.packageName
import org.mozilla.fenix.helpers.click

/**
 * Implementation of Robot Pattern for the experiments sub menu.
 */
class SettingsSubMenuExperimentsRobot {

    class Transition {

        fun goBackToHomeScreen(interact: HomeScreenRobot.() -> Unit): HomeScreenRobot.Transition {
            goBackButton().click()

            HomeScreenRobot().interact()
            return HomeScreenRobot.Transition()
        }

        fun goBack(interact: SettingsRobot.() -> Unit): SettingsRobot.Transition {
            goBackButton().click()

            SettingsRobot().interact()
            return SettingsRobot.Transition()
        }
    }

    private fun getExperiment(experimentName: String): UiSelector? {
        return UiSelector().textContains(experimentName)
    }

    fun verifyExperimentExists(title: String) {
        val experiment = getExperiment(title)

        checkNotNull(experiment)
    }

    fun verifyExperimentEnrolled(title: String) {
        itemContainingText(title).click()
        assertUIObjectExists(checkIcon())
        goBackButton().click()
    }

    fun verifyExperimentNotEnrolled(title: String) {
        itemContainingText(title).click()
        assertUIObjectExists(checkIcon(), exists = false)
        goBackButton().click()
    }

    fun unenrollfromExperiment(title: String) {
        val branch = itemWithResId("$packageName:id/nimbus_branch_name")

        itemContainingText(title).click()
        assertUIObjectExists(checkIcon())
        branch.click()
        assertUIObjectExists(checkIcon(), exists = false)
    }
}
private fun goBackButton() = onView(withContentDescription(R.string.action_bar_up_description))
private fun checkIcon() = itemWithResId("$packageName:id/selected_icon")
