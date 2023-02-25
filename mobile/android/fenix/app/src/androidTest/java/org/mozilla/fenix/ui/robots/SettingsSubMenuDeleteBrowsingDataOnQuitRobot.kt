/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ui.robots

import androidx.test.espresso.Espresso.onView
import androidx.test.espresso.assertion.ViewAssertions.matches
import androidx.test.espresso.matcher.ViewMatchers.Visibility
import androidx.test.espresso.matcher.ViewMatchers.hasDescendant
import androidx.test.espresso.matcher.ViewMatchers.withChild
import androidx.test.espresso.matcher.ViewMatchers.withClassName
import androidx.test.espresso.matcher.ViewMatchers.withContentDescription
import androidx.test.espresso.matcher.ViewMatchers.withEffectiveVisibility
import androidx.test.espresso.matcher.ViewMatchers.withId
import androidx.test.espresso.matcher.ViewMatchers.withResourceName
import androidx.test.espresso.matcher.ViewMatchers.withText
import org.hamcrest.CoreMatchers.allOf
import org.hamcrest.CoreMatchers.containsString
import org.mozilla.fenix.R
import org.mozilla.fenix.helpers.assertIsChecked
import org.mozilla.fenix.helpers.atPosition
import org.mozilla.fenix.helpers.click
import org.mozilla.fenix.helpers.isChecked

/**
 * Implementation of Robot Pattern for the settings Delete Browsing Data On Quit sub menu.
 */
class SettingsSubMenuDeleteBrowsingDataOnQuitRobot {

    fun verifyNavigationToolBarHeader() =
        onView(
            allOf(
                withId(R.id.navigationToolbar),
                withChild(withText(R.string.preferences_delete_browsing_data_on_quit)),
            ),
        )
            .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))

    fun verifyDeleteBrowsingOnQuitEnabled(enabled: Boolean) =
        deleteBrowsingOnQuitButton.assertIsChecked(enabled)

    fun verifyDeleteBrowsingOnQuitButtonSummary() =
        onView(
            withText(R.string.preference_summary_delete_browsing_data_on_quit_2),
        ).check(matches(withEffectiveVisibility(Visibility.VISIBLE)))

    fun clickDeleteBrowsingOnQuitButtonSwitch() = onView(withResourceName("switch_widget")).click()

    fun verifyAllTheCheckBoxesText() {
        openTabsCheckbox
            .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))

        browsingHistoryCheckbox
            .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))

        cookiesAndSiteDataCheckbox
            .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))

        onView(withText(R.string.preferences_delete_browsing_data_cookies_subtitle))
            .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))

        cachedFilesCheckbox
            .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))

        onView(withText(R.string.preferences_delete_browsing_data_cached_files_subtitle))
            .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))

        sitePermissionsCheckbox
            .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))
    }

    fun verifyAllTheCheckBoxesChecked(checked: Boolean) {
        for (index in 2..7) {
            onView(withId(R.id.recycler_view))
                .check(
                    matches(
                        atPosition(
                            index,
                            hasDescendant(
                                allOf(
                                    withResourceName(containsString("checkbox")),
                                    isChecked(checked),
                                ),
                            ),
                        ),
                    ),
                )
        }
    }

    class Transition {
        fun goBack(interact: SettingsRobot.() -> Unit): SettingsRobot.Transition {
            goBackButton.click()

            SettingsRobot().interact()
            return SettingsRobot.Transition()
        }
    }
}

private val goBackButton = onView(withContentDescription("Navigate up"))

private val deleteBrowsingOnQuitButton =
    onView(withClassName(containsString("android.widget.Switch")))

private val openTabsCheckbox =
    onView(withText(R.string.preferences_delete_browsing_data_tabs_title_2))

private val browsingHistoryCheckbox =
    onView(withText(R.string.preferences_delete_browsing_data_browsing_history_title))

private val cookiesAndSiteDataCheckbox = onView(withText(R.string.preferences_delete_browsing_data_cookies_and_site_data))

private val cachedFilesCheckbox =
    onView(withText(R.string.preferences_delete_browsing_data_cached_files))

private val sitePermissionsCheckbox =
    onView(withText(R.string.preferences_delete_browsing_data_site_permissions))
