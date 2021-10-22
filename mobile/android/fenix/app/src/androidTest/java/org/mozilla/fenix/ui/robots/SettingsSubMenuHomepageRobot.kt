package org.mozilla.fenix.ui.robots

import androidx.test.espresso.Espresso.onView
import androidx.test.espresso.matcher.ViewMatchers.hasSibling
import androidx.test.espresso.matcher.ViewMatchers.withId
import androidx.test.espresso.matcher.ViewMatchers.withText
import org.hamcrest.CoreMatchers.allOf
import org.mozilla.fenix.R
import org.mozilla.fenix.helpers.click

/**
 * Implementation of Robot Pattern for the settings Homepage sub menu.
 */
class SettingsSubMenuHomepageRobot {

    fun clickStartOnHomepageButton() = homepageButton().click()

    class Transition
}

private fun openingScreenHeading() = onView(withText("Opening screen"))

private fun homepageButton() =
    onView(
        allOf(
            withId(R.id.title),
            withText("Homepage"),
            hasSibling(withId(R.id.radio_button))
        )
    )

private fun lastTabButton() = onView(withText("Last tab"))

private fun homepageAfterFourHoursButton() =
    onView(withText("Homepage after four hours of inactivity"))
