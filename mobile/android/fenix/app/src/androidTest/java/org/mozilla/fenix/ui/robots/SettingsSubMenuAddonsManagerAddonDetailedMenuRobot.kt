/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
@file:Suppress("DEPRECATION")

package org.mozilla.fenix.ui.robots

import android.view.View
import androidx.test.espresso.Espresso.onView
import androidx.test.espresso.assertion.ViewAssertions.matches
import androidx.test.espresso.matcher.ViewMatchers
import androidx.test.espresso.matcher.ViewMatchers.withContentDescription
import androidx.test.espresso.matcher.ViewMatchers.withEffectiveVisibility
import androidx.test.espresso.matcher.ViewMatchers.withId
import androidx.test.rule.ActivityTestRule
import org.hamcrest.CoreMatchers.allOf
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.R
import org.mozilla.fenix.helpers.AppAndSystemHelper.registerAndCleanupIdlingResources
import org.mozilla.fenix.helpers.ViewVisibilityIdlingResource
import org.mozilla.fenix.helpers.click

/**
 * Implementation of Robot Pattern for a single Addon inside of the Addons Management Settings.
 */

class SettingsSubMenuAddonsManagerAddonDetailedMenuRobot {

    class Transition {
        fun goBack(interact: SettingsSubMenuAddonsManagerRobot.() -> Unit): SettingsSubMenuAddonsManagerRobot.Transition {
            fun goBackButton() = onView(allOf(withContentDescription("Navigate up")))
            goBackButton().click()

            SettingsSubMenuAddonsManagerRobot().interact()
            return SettingsSubMenuAddonsManagerRobot.Transition()
        }

        fun removeAddon(activityTestRule: ActivityTestRule<HomeActivity>, interact: SettingsSubMenuAddonsManagerRobot.() -> Unit): SettingsSubMenuAddonsManagerRobot.Transition {
            registerAndCleanupIdlingResources(
                ViewVisibilityIdlingResource(
                    activityTestRule.activity.findViewById(R.id.addon_container),
                    View.VISIBLE,
                ),
            ) {
                removeAddonButton().check(matches(withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)))
                removeAddonButton().click()
            }

            SettingsSubMenuAddonsManagerRobot().interact()
            return SettingsSubMenuAddonsManagerRobot.Transition()
        }
    }
}

private fun removeAddonButton() =
    onView(withId(R.id.remove_add_on))
