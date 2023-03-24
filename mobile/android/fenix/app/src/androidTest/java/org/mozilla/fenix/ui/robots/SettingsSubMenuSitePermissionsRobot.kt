/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ui.robots

import androidx.preference.R
import androidx.recyclerview.widget.RecyclerView
import androidx.test.espresso.Espresso.onView
import androidx.test.espresso.assertion.ViewAssertions.matches
import androidx.test.espresso.contrib.RecyclerViewActions
import androidx.test.espresso.matcher.ViewMatchers.Visibility
import androidx.test.espresso.matcher.ViewMatchers.hasDescendant
import androidx.test.espresso.matcher.ViewMatchers.hasSibling
import androidx.test.espresso.matcher.ViewMatchers.withContentDescription
import androidx.test.espresso.matcher.ViewMatchers.withEffectiveVisibility
import androidx.test.espresso.matcher.ViewMatchers.withId
import androidx.test.espresso.matcher.ViewMatchers.withText
import org.hamcrest.CoreMatchers.allOf
import org.mozilla.fenix.helpers.TestHelper.scrollToElementByText
import org.mozilla.fenix.helpers.click

/**
 * Implementation of Robot Pattern for the settings Site Permissions sub menu.
 */
class SettingsSubMenuSitePermissionsRobot {

    fun verifySitePermissionsToolbarTitle() =
        onView(withText("Site permissions")).check(matches(withEffectiveVisibility(Visibility.VISIBLE)))

    fun verifyToolbarGoBackButton() =
        goBackButton().check(matches(withEffectiveVisibility(Visibility.VISIBLE)))

    fun verifySitePermissionOption(option: String, summary: String = "") {
        scrollToElementByText(option)
        onView(
            allOf(
                withText(option),
                hasSibling(withText(summary)),
            ),
        ).check(matches(withEffectiveVisibility(Visibility.VISIBLE)))
    }

    class Transition {
        fun goBack(interact: SettingsRobot.() -> Unit): SettingsRobot.Transition {
            goBackButton().click()

            SettingsRobot().interact()
            return SettingsRobot.Transition()
        }

        fun openAutoPlay(
            interact: SettingsSubMenuSitePermissionsCommonRobot.() -> Unit,
        ): SettingsSubMenuSitePermissionsCommonRobot.Transition {
            onView(withId(R.id.recycler_view)).perform(
                RecyclerViewActions.scrollTo<RecyclerView.ViewHolder>(
                    hasDescendant(withText("Autoplay")),
                ),
            )

            openAutoPlay().click()

            SettingsSubMenuSitePermissionsCommonRobot().interact()
            return SettingsSubMenuSitePermissionsCommonRobot.Transition()
        }

        fun openCamera(
            interact: SettingsSubMenuSitePermissionsCommonRobot.() -> Unit,
        ): SettingsSubMenuSitePermissionsCommonRobot.Transition {
            onView(withId(R.id.recycler_view)).perform(
                RecyclerViewActions.scrollTo<RecyclerView.ViewHolder>(
                    hasDescendant(withText("Camera")),
                ),
            )

            openCamera().click()

            SettingsSubMenuSitePermissionsCommonRobot().interact()
            return SettingsSubMenuSitePermissionsCommonRobot.Transition()
        }

        fun openLocation(
            interact: SettingsSubMenuSitePermissionsCommonRobot.() -> Unit,
        ): SettingsSubMenuSitePermissionsCommonRobot.Transition {
            onView(withId(R.id.recycler_view)).perform(
                RecyclerViewActions.scrollTo<RecyclerView.ViewHolder>(
                    hasDescendant(withText("Location")),
                ),
            )

            openLocation().click()

            SettingsSubMenuSitePermissionsCommonRobot().interact()
            return SettingsSubMenuSitePermissionsCommonRobot.Transition()
        }

        fun openMicrophone(
            interact: SettingsSubMenuSitePermissionsCommonRobot.() -> Unit,
        ): SettingsSubMenuSitePermissionsCommonRobot.Transition {
            onView(withId(R.id.recycler_view)).perform(
                RecyclerViewActions.scrollTo<RecyclerView.ViewHolder>(
                    hasDescendant(withText("Microphone")),
                ),
            )

            openMicrophone().click()

            SettingsSubMenuSitePermissionsCommonRobot().interact()
            return SettingsSubMenuSitePermissionsCommonRobot.Transition()
        }

        fun openNotification(
            interact: SettingsSubMenuSitePermissionsCommonRobot.() -> Unit,
        ): SettingsSubMenuSitePermissionsCommonRobot.Transition {
            onView(withId(R.id.recycler_view)).perform(
                RecyclerViewActions.scrollTo<RecyclerView.ViewHolder>(
                    hasDescendant(withText("Notification")),
                ),
            )

            openNotification().click()

            SettingsSubMenuSitePermissionsCommonRobot().interact()
            return SettingsSubMenuSitePermissionsCommonRobot.Transition()
        }

        fun openPersistentStorage(
            interact: SettingsSubMenuSitePermissionsCommonRobot.() -> Unit,
        ): SettingsSubMenuSitePermissionsCommonRobot.Transition {
            onView(withId(R.id.recycler_view)).perform(
                RecyclerViewActions.scrollTo<RecyclerView.ViewHolder>(
                    hasDescendant(withText("Persistent Storage")),
                ),
            )

            openPersistentStorage().click()

            SettingsSubMenuSitePermissionsCommonRobot().interact()
            return SettingsSubMenuSitePermissionsCommonRobot.Transition()
        }

        fun openExceptions(
            interact: SettingsSubMenuSitePermissionsExceptionsRobot.() -> Unit,
        ): SettingsSubMenuSitePermissionsExceptionsRobot.Transition {
            onView(withId(R.id.recycler_view)).perform(
                RecyclerViewActions.scrollTo<RecyclerView.ViewHolder>(
                    hasDescendant(withText("Exceptions")),
                ),
            )

            openExceptions().click()

            SettingsSubMenuSitePermissionsExceptionsRobot().interact()
            return SettingsSubMenuSitePermissionsExceptionsRobot.Transition()
        }
    }
}

private fun goBackButton() =
    onView(withContentDescription("Navigate up"))

private fun openAutoPlay() =
    onView(allOf(withText("Autoplay")))

private fun openCamera() =
    onView(allOf(withText("Camera")))

private fun openLocation() =
    onView(allOf(withText("Location")))

private fun openMicrophone() =
    onView(allOf(withText("Microphone")))

private fun openNotification() =
    onView(allOf(withText("Notification")))

private fun openPersistentStorage() =
    onView(allOf(withText("Persistent Storage")))

private fun openExceptions() =
    onView(allOf(withText("Exceptions")))
