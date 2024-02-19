/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ui.robots

import android.util.Log
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
import org.mozilla.fenix.helpers.Constants.TAG
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

    fun verifyUsageAndTechnicalDataToggle(enabled: Boolean) {
        Log.i(TAG, "verifyUsageAndTechnicalDataToggle: Trying to verify that the \"Usage and technical data\" toggle is checked: $enabled")
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
        Log.i(TAG, "verifyUsageAndTechnicalDataToggle: Verified that the \"Usage and technical data\" toggle is checked: $enabled")
    }

    fun verifyMarketingDataToggle(enabled: Boolean) {
        Log.i(TAG, "verifyMarketingDataToggle: Trying to verify that the \"Marketing data\" toggle is checked: $enabled")
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
        Log.i(TAG, "verifyMarketingDataToggle: Verified that the \"Marketing data\" toggle is checked: $enabled")
    }

    fun verifyStudiesToggle(enabled: Boolean) {
        Log.i(TAG, "verifyStudiesToggle: Trying to verify that the \"Studies\" toggle is checked: $enabled")
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
        Log.i(TAG, "verifyStudiesToggle: Verified that the \"Studies\" toggle is checked: $enabled")
    }

    fun clickUsageAndTechnicalDataToggle() {
        Log.i(TAG, "clickUsageAndTechnicalDataToggle: Trying to click the \"Usage and technical data\" toggle")
        itemContainingText(getStringResource(R.string.preference_usage_data)).click()
        Log.i(TAG, "clickUsageAndTechnicalDataToggle: Clicked the \"Usage and technical data\" toggle")
    }

    fun clickMarketingDataToggle() {
        Log.i(TAG, "clickUsageAndTechnicalDataToggle: Trying to click the \"Marketing data\" toggle")
        itemContainingText(getStringResource(R.string.preferences_marketing_data)).click()
        Log.i(TAG, "clickUsageAndTechnicalDataToggle: Clicked the \"Marketing data\" toggle")
    }

    fun clickStudiesOption() {
        Log.i(TAG, "clickStudiesOption: Trying to click the \"Studies\" option")
        itemContainingText(getStringResource(R.string.preference_experiments_2)).click()
        Log.i(TAG, "clickStudiesOption: Clicked the \"Studies\" option")
    }

    fun clickStudiesToggle() {
        Log.i(TAG, "clickStudiesToggle: Trying to click the \"Studies\" toggle")
        itemWithResId("$packageName:id/studies_switch").click()
        Log.i(TAG, "clickStudiesToggle: Clicked the \"Studies\" toggle")
    }

    fun verifyStudiesDialog() {
        assertUIObjectExists(
            itemWithResId("$packageName:id/alertTitle"),
            itemContainingText(getStringResource(R.string.studies_restart_app)),
        )
        Log.i(TAG, "verifyStudiesDialog: Trying to verify that the \"Studies\" dialog \"Ok\" button is visible")
        studiesDialogOkButton().check(matches(withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)))
        Log.i(TAG, "verifyStudiesDialog: Verified that the \"Studies\" dialog \"Ok\" button is visible")
        Log.i(TAG, "verifyStudiesDialog: Trying to verify that the \"Studies\" dialog \"Cancel\" button is visible")
        studiesDialogCancelButton().check(matches(withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)))
        Log.i(TAG, "verifyStudiesDialog: Verified that the \"Studies\" dialog \"Cancel\" button is visible")
    }

    fun clickStudiesDialogCancelButton() {
        Log.i(TAG, "clickStudiesDialogCancelButton: Trying to click the \"Studies\" dialog \"Cancel\" button")
        studiesDialogCancelButton().click()
        Log.i(TAG, "clickStudiesDialogCancelButton: Clicked the \"Studies\" dialog \"Cancel\" button")
    }

    fun clickStudiesDialogOkButton() {
        Log.i(TAG, "clickStudiesDialogOkButton: Trying to click the \"Studies\" dialog \"Ok\" button")
        studiesDialogOkButton().click()
        Log.i(TAG, "clickStudiesDialogOkButton: Clicked the \"Studies\" dialog \"Ok\" button")
    }

    class Transition {
        fun goBack(interact: SettingsRobot.() -> Unit): SettingsRobot.Transition {
            Log.i(TAG, "goBack: Trying to click the navigate up toolbar button")
            goBackButton().click()
            Log.i(TAG, "goBack: Clicked the navigate up toolbar button")

            SettingsRobot().interact()
            return SettingsRobot.Transition()
        }
    }
}

private fun goBackButton() = itemWithDescription("Navigate up")
private fun studiesDialogOkButton() = onView(withId(android.R.id.button1)).inRoot(RootMatchers.isDialog())
private fun studiesDialogCancelButton() = onView(withId(android.R.id.button2)).inRoot(RootMatchers.isDialog())
