/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ui.robots

import androidx.test.espresso.Espresso.onView
import androidx.test.espresso.assertion.ViewAssertions.matches
import androidx.test.espresso.matcher.RootMatchers
import androidx.test.espresso.matcher.ViewMatchers
import androidx.test.espresso.matcher.ViewMatchers.isChecked
import androidx.test.espresso.matcher.ViewMatchers.isNotChecked
import androidx.test.espresso.matcher.ViewMatchers.withClassName
import androidx.test.espresso.matcher.ViewMatchers.withEffectiveVisibility
import androidx.test.espresso.matcher.ViewMatchers.withId
import androidx.test.espresso.matcher.ViewMatchers.withText
import org.hamcrest.CoreMatchers.allOf
import org.hamcrest.CoreMatchers.endsWith
import org.mozilla.fenix.R
import org.mozilla.fenix.helpers.DataGenerationHelper.getStringResource
import org.mozilla.fenix.helpers.MatcherHelper.assertUIObjectExists
import org.mozilla.fenix.helpers.MatcherHelper.itemContainingText
import org.mozilla.fenix.helpers.MatcherHelper.itemWithDescription
import org.mozilla.fenix.helpers.MatcherHelper.itemWithResId
import org.mozilla.fenix.helpers.TestHelper.hasCousin
import org.mozilla.fenix.helpers.TestHelper.packageName
import org.mozilla.fenix.helpers.click

/**
 * Implementation of Robot Pattern for the settings Data Collection sub menu.
 */
class SettingsSubMenuDataCollectionRobot {

    fun verifyDataCollectionView(
        isUsageAndTechnicalDataEnabled: Boolean,
        isMarketingDataEnabled: Boolean,
        studiesSummary: String,
    ) {
        assertUIObjectExists(
            goBackButton(),
            itemContainingText(getStringResource(R.string.preferences_data_collection)),
            itemContainingText(getStringResource(R.string.preference_usage_data)),
            itemContainingText(getStringResource(R.string.preferences_usage_data_description)),
        )
        verifyUsageAndTechnicalDataToggle(isUsageAndTechnicalDataEnabled)
        assertUIObjectExists(
            itemContainingText(getStringResource(R.string.preferences_marketing_data)),
            itemContainingText(getStringResource(R.string.preferences_marketing_data_description2)),
        )
        verifyMarketingDataToggle(isMarketingDataEnabled)
        assertUIObjectExists(
            itemContainingText(getStringResource(R.string.preference_experiments_2)),
            itemContainingText(studiesSummary),
        )
    }

    fun verifyUsageAndTechnicalDataToggle(enabled: Boolean) =
        onView(withText(R.string.preference_usage_data))
            .check(
                matches(
                    hasCousin(
                        allOf(
                            withClassName(endsWith("Switch")),
                            if (enabled) {
                                isChecked()
                            } else {
                                isNotChecked()
                            },
                        ),
                    ),
                ),
            )

    fun verifyMarketingDataToggle(enabled: Boolean) =
        onView(withText(R.string.preferences_marketing_data))
            .check(
                matches(
                    hasCousin(
                        allOf(
                            withClassName(endsWith("Switch")),
                            if (enabled) {
                                isChecked()
                            } else {
                                isNotChecked()
                            },
                        ),
                    ),
                ),
            )

    fun verifyStudiesToggle(enabled: Boolean) =
        onView(withId(R.id.studies_switch))
            .check(
                matches(
                    if (enabled) {
                        isChecked()
                    } else {
                        isNotChecked()
                    },
                ),
            )

    fun clickUsageAndTechnicalDataToggle() =
        itemContainingText(getStringResource(R.string.preference_usage_data)).click()

    fun clickMarketingDataToggle() =
        itemContainingText(getStringResource(R.string.preferences_marketing_data)).click()

    fun clickStudiesOption() =
        itemContainingText(getStringResource(R.string.preference_experiments_2)).click()

    fun clickStudiesToggle() =
        itemWithResId("$packageName:id/studies_switch").click()

    fun verifyStudiesDialog() {
        assertUIObjectExists(
            itemWithResId("$packageName:id/alertTitle"),
            itemContainingText(getStringResource(R.string.studies_restart_app)),
        )
        studiesDialogOkButton.check(matches(withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)))
        studiesDialogCancelButton.check(matches(withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)))
    }

    fun clickStudiesDialogCancelButton() = studiesDialogCancelButton.click()

    fun clickStudiesDialogOkButton() = studiesDialogOkButton.click()

    class Transition {
        fun goBack(interact: SettingsRobot.() -> Unit): SettingsRobot.Transition {
            goBackButton().click()

            SettingsRobot().interact()
            return SettingsRobot.Transition()
        }
    }
}

private fun goBackButton() = itemWithDescription("Navigate up")
private val studiesDialogOkButton = onView(withId(android.R.id.button1)).inRoot(RootMatchers.isDialog())
private val studiesDialogCancelButton = onView(withId(android.R.id.button2)).inRoot(RootMatchers.isDialog())
