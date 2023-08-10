package org.mozilla.fenix.ui.robots

import androidx.test.espresso.Espresso.onView
import androidx.test.espresso.matcher.ViewMatchers.withContentDescription
import androidx.test.uiautomator.UiSelector
import org.mozilla.fenix.R
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
}
private fun goBackButton() = onView(withContentDescription(R.string.action_bar_up_description))
