/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ui.robots

import android.util.Log
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
import org.mozilla.fenix.helpers.Constants.TAG
import org.mozilla.fenix.helpers.TestHelper.scrollToElementByText
import org.mozilla.fenix.helpers.click

/**
 * Implementation of Robot Pattern for the settings Site Permissions sub menu.
 */
class SettingsSubMenuSitePermissionsRobot {

    fun verifySitePermissionsToolbarTitle() {
        Log.i(TAG, "verifySitePermissionsToolbarTitle: Trying to verify that the \"Site permissions\" toolbar title is visible")
        onView(withText("Site permissions")).check(matches(withEffectiveVisibility(Visibility.VISIBLE)))
        Log.i(TAG, "verifySitePermissionsToolbarTitle: Verified that the \"Site permissions\" toolbar title is visible")
    }

    fun verifyToolbarGoBackButton() {
        Log.i(TAG, "verifyToolbarGoBackButton: Trying to verify that the navigate up toolbar button is visible")
        goBackButton().check(matches(withEffectiveVisibility(Visibility.VISIBLE)))
        Log.i(TAG, "verifyToolbarGoBackButton: Verified that the navigate up toolbar button is visible")
    }

    fun verifySitePermissionOption(option: String, summary: String = "") {
        Log.i(TAG, "verifySitePermissionOption: Trying to verify that the $option option with $summary summary is visible")
        scrollToElementByText(option)
        onView(
            allOf(
                withText(option),
                hasSibling(withText(summary)),
            ),
        ).check(matches(withEffectiveVisibility(Visibility.VISIBLE)))
        Log.i(TAG, "verifySitePermissionOption: Trying to verify that the $option option with $summary summary is visible")
    }

    class Transition {
        fun goBack(interact: SettingsRobot.() -> Unit): SettingsRobot.Transition {
            Log.i(TAG, "goBack: Trying to click navigate up toolbar button")
            goBackButton().click()
            Log.i(TAG, "goBack: Clicked the navigate up toolbar button")

            SettingsRobot().interact()
            return SettingsRobot.Transition()
        }

        fun openAutoPlay(
            interact: SettingsSubMenuSitePermissionsCommonRobot.() -> Unit,
        ): SettingsSubMenuSitePermissionsCommonRobot.Transition {
            Log.i(TAG, "openAutoPlay: Trying to perform scroll action to the \"Autoplay\" button")
            onView(withId(R.id.recycler_view)).perform(
                RecyclerViewActions.scrollTo<RecyclerView.ViewHolder>(
                    hasDescendant(withText("Autoplay")),
                ),
            )
            Log.i(TAG, "openAutoPlay: Performed scroll action to the \"Autoplay\" button")
            Log.i(TAG, "openAutoPlay: Trying to click the \"Autoplay\" button")
            openAutoPlay().click()
            Log.i(TAG, "openAutoPlay: Clicked the \"Autoplay\" button")

            SettingsSubMenuSitePermissionsCommonRobot().interact()
            return SettingsSubMenuSitePermissionsCommonRobot.Transition()
        }

        fun openCamera(
            interact: SettingsSubMenuSitePermissionsCommonRobot.() -> Unit,
        ): SettingsSubMenuSitePermissionsCommonRobot.Transition {
            Log.i(TAG, "openCamera: Trying to perform scroll action to the \"Camera\" button")
            onView(withId(R.id.recycler_view)).perform(
                RecyclerViewActions.scrollTo<RecyclerView.ViewHolder>(
                    hasDescendant(withText("Camera")),
                ),
            )
            Log.i(TAG, "openCamera: Performed scroll action to the \"Camera\" button")
            Log.i(TAG, "openCamera: Trying to click the \"Camera\" button")
            openCamera().click()
            Log.i(TAG, "openCamera: Clicked the \"Camera\" button")

            SettingsSubMenuSitePermissionsCommonRobot().interact()
            return SettingsSubMenuSitePermissionsCommonRobot.Transition()
        }

        fun openLocation(
            interact: SettingsSubMenuSitePermissionsCommonRobot.() -> Unit,
        ): SettingsSubMenuSitePermissionsCommonRobot.Transition {
            Log.i(TAG, "openLocation: Trying to perform scroll action to the \"Location\" button")
            onView(withId(R.id.recycler_view)).perform(
                RecyclerViewActions.scrollTo<RecyclerView.ViewHolder>(
                    hasDescendant(withText("Location")),
                ),
            )
            Log.i(TAG, "openLocation: Performed scroll action to the \"Location\" button")
            Log.i(TAG, "openLocation: Trying to click the \"Location\" button")
            openLocation().click()
            Log.i(TAG, "openLocation: Clicked the \"Location\" button")

            SettingsSubMenuSitePermissionsCommonRobot().interact()
            return SettingsSubMenuSitePermissionsCommonRobot.Transition()
        }

        fun openMicrophone(
            interact: SettingsSubMenuSitePermissionsCommonRobot.() -> Unit,
        ): SettingsSubMenuSitePermissionsCommonRobot.Transition {
            Log.i(TAG, "openMicrophone: Trying to perform scroll action to the \"Microphone\" button")
            onView(withId(R.id.recycler_view)).perform(
                RecyclerViewActions.scrollTo<RecyclerView.ViewHolder>(
                    hasDescendant(withText("Microphone")),
                ),
            )
            Log.i(TAG, "openMicrophone: Performed scroll action to the \"Microphone\" button")
            Log.i(TAG, "openMicrophone: Trying to click the \"Microphone\" button")
            openMicrophone().click()
            Log.i(TAG, "openMicrophone: Clicked the \"Microphone\" button")

            SettingsSubMenuSitePermissionsCommonRobot().interact()
            return SettingsSubMenuSitePermissionsCommonRobot.Transition()
        }

        fun openNotification(
            interact: SettingsSubMenuSitePermissionsCommonRobot.() -> Unit,
        ): SettingsSubMenuSitePermissionsCommonRobot.Transition {
            Log.i(TAG, "openNotification: Trying to perform scroll action to the \"Notification\" button")
            onView(withId(R.id.recycler_view)).perform(
                RecyclerViewActions.scrollTo<RecyclerView.ViewHolder>(
                    hasDescendant(withText("Notification")),
                ),
            )
            Log.i(TAG, "openNotification: Performed scroll action to the \"Notification\" button")
            Log.i(TAG, "openNotification: Trying to click the \"Notification\" button")
            openNotification().click()
            Log.i(TAG, "openNotification: Clicked the \"Notification\" button")

            SettingsSubMenuSitePermissionsCommonRobot().interact()
            return SettingsSubMenuSitePermissionsCommonRobot.Transition()
        }

        fun openPersistentStorage(
            interact: SettingsSubMenuSitePermissionsCommonRobot.() -> Unit,
        ): SettingsSubMenuSitePermissionsCommonRobot.Transition {
            Log.i(TAG, "openPersistentStorage: Trying to perform scroll action to the \"Persistent Storage\" button")
            onView(withId(R.id.recycler_view)).perform(
                RecyclerViewActions.scrollTo<RecyclerView.ViewHolder>(
                    hasDescendant(withText("Persistent Storage")),
                ),
            )
            Log.i(TAG, "openPersistentStorage: Performed scroll action to the \"Persistent Storage\" button")
            Log.i(TAG, "openPersistentStorage: Trying to click the \"Persistent Storage\" button")
            openPersistentStorage().click()
            Log.i(TAG, "openPersistentStorage: Clicked the \"Persistent Storage\" button")

            SettingsSubMenuSitePermissionsCommonRobot().interact()
            return SettingsSubMenuSitePermissionsCommonRobot.Transition()
        }

        fun openDRMControlledContent(
            interact: SettingsSubMenuSitePermissionsCommonRobot.() -> Unit,
        ): SettingsSubMenuSitePermissionsCommonRobot.Transition {
            Log.i(TAG, "openDRMControlledContent: Trying to perform scroll action to the \"DRM-controlled content\" button")
            onView(withId(R.id.recycler_view)).perform(
                RecyclerViewActions.scrollTo<RecyclerView.ViewHolder>(
                    hasDescendant(withText("DRM-controlled content")),
                ),
            )
            Log.i(TAG, "openDRMControlledContent: Performed scroll action to the \"DRM-controlled content\" button")
            Log.i(TAG, "openDRMControlledContent: Trying to click the \"DRM-controlled content\" button")
            openDrmControlledContent().click()
            Log.i(TAG, "openDRMControlledContent: Clicked the \"DRM-controlled content\" button")

            SettingsSubMenuSitePermissionsCommonRobot().interact()
            return SettingsSubMenuSitePermissionsCommonRobot.Transition()
        }

        fun openExceptions(
            interact: SettingsSubMenuSitePermissionsExceptionsRobot.() -> Unit,
        ): SettingsSubMenuSitePermissionsExceptionsRobot.Transition {
            Log.i(TAG, "openExceptions: Trying to perform scroll action to the \"Exceptions\" button")
            onView(withId(R.id.recycler_view)).perform(
                RecyclerViewActions.scrollTo<RecyclerView.ViewHolder>(
                    hasDescendant(withText("Exceptions")),
                ),
            )
            Log.i(TAG, "openExceptions: Performed scroll action to the \"Exceptions\" button")
            Log.i(TAG, "openExceptions: Trying to click the \"Exceptions\" button")
            openExceptions().click()
            Log.i(TAG, "openExceptions: Clicked the \"Exceptions\" button")

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

private fun openDrmControlledContent() =
    onView(allOf(withText("DRM-controlled content")))

private fun openExceptions() =
    onView(allOf(withText("Exceptions")))
