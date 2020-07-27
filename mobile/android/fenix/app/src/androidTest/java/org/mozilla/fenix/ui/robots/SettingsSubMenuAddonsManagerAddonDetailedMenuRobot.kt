package org.mozilla.fenix.ui.robots

import androidx.test.espresso.Espresso.onView
import androidx.test.espresso.assertion.ViewAssertions.matches
import androidx.test.espresso.matcher.ViewMatchers.isCompletelyDisplayed
import androidx.test.espresso.matcher.ViewMatchers.withContentDescription
import androidx.test.espresso.matcher.ViewMatchers.withId
import org.hamcrest.CoreMatchers.allOf
import org.mozilla.fenix.R
import org.mozilla.fenix.helpers.click

/**
 * Implementation of Robot Pattern for a single Addon inside of the Addons Management Settings.
 */

class SettingsSubMenuAddonsManagerAddonDetailedMenuRobot {

    fun verifyCurrentAddonMenu() = assertAddonMenuItems()

    class Transition {
        fun goBack(interact: SettingsSubMenuAddonsManagerRobot.() -> Unit): SettingsSubMenuAddonsManagerRobot.Transition {
            fun goBackButton() = onView(allOf(withContentDescription("Navigate up")))
            goBackButton().click()

            SettingsSubMenuAddonsManagerRobot().interact()
            return SettingsSubMenuAddonsManagerRobot.Transition()
        }

        fun removeAddon(interact: SettingsSubMenuAddonsManagerRobot.() -> Unit): SettingsSubMenuAddonsManagerRobot.Transition {
            removeAddonButton().click()

            SettingsSubMenuAddonsManagerRobot().interact()
            return SettingsSubMenuAddonsManagerRobot.Transition()
        }
    }

    private fun assertAddonMenuItems() {
        enableSwitchButton().check(matches(isCompletelyDisplayed()))
        settingsButton().check(matches(isCompletelyDisplayed()))
        detailsButton().check(matches(isCompletelyDisplayed()))
        permissionsButton().check(matches(isCompletelyDisplayed()))
        removeAddonButton().check(matches(isCompletelyDisplayed()))
    }
}

private fun enableSwitchButton() =
    onView(withId(R.id.enable_switch))

private fun settingsButton() =
    onView(withId(R.id.settings))

private fun detailsButton() =
    onView(withId(R.id.details))

private fun permissionsButton() =
    onView(withId(R.id.permissions))

private fun removeAddonButton() =
    onView(withId(R.id.remove_add_on))
