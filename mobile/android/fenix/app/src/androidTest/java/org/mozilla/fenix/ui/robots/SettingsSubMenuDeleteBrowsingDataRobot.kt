/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ui.robots

import androidx.test.espresso.Espresso.onView
import androidx.test.espresso.assertion.ViewAssertions.matches
import androidx.test.espresso.matcher.RootMatchers.isDialog
import androidx.test.espresso.matcher.ViewMatchers.Visibility
import androidx.test.espresso.matcher.ViewMatchers.hasSibling
import androidx.test.espresso.matcher.ViewMatchers.withChild
import androidx.test.espresso.matcher.ViewMatchers.withContentDescription
import androidx.test.espresso.matcher.ViewMatchers.withEffectiveVisibility
import androidx.test.espresso.matcher.ViewMatchers.withId
import androidx.test.espresso.matcher.ViewMatchers.withText
import androidx.test.platform.app.InstrumentationRegistry
import androidx.test.uiautomator.By
import androidx.test.uiautomator.UiDevice
import androidx.test.uiautomator.UiSelector
import androidx.test.uiautomator.Until
import org.hamcrest.CoreMatchers.allOf
import org.mozilla.fenix.R
import org.mozilla.fenix.helpers.TestAssetHelper
import org.mozilla.fenix.helpers.TestHelper.appName
import org.mozilla.fenix.helpers.assertIsChecked
import org.mozilla.fenix.helpers.click

/**
 * Implementation of Robot Pattern for the settings Delete Browsing Data sub menu.
 */
class SettingsSubMenuDeleteBrowsingDataRobot {

    fun verifyNavigationToolBarHeader() = assertNavigationToolBarHeader()

    fun verifyDeleteBrowsingDataButton() = assertDeleteBrowsingDataButton()

    fun verifyClickDeleteBrowsingDataButton() = assertClickDeleteBrowsingDataButton()

    fun verifyMessageInDialogBox() = assertMessageInDialogBox()

    fun verifyDeleteButtonInDialogBox() = assertDeleteButtonInDialogBox()

    fun verifyCancelButtonInDialogBox() = assertCancelButtonInDialogBox()

    fun verifyAllTheCheckBoxesText() = assertAllTheCheckBoxesText()

    fun verifyAllTheCheckBoxesChecked() = assertAllTheCheckBoxesChecked()

    fun verifyContentsInDialogBox() {
        verifyMessageInDialogBox()
        verifyDeleteButtonInDialogBox()
        verifyCancelButtonInDialogBox()
    }

    fun clickCancelButtonInDialogBoxAndVerifyContentsInDialogBox() {
        mDevice.wait(
            Until.findObject(By.text("Delete browsing data")),
            TestAssetHelper.waitingTime
        )
        verifyClickDeleteBrowsingDataButton()
        verifyContentsInDialogBox()
        cancelButton().click()
    }

    fun verifyDeleteBrowsingDataSubMenuItems() {
        verifyDeleteBrowsingDataButton()
        clickCancelButtonInDialogBoxAndVerifyContentsInDialogBox()
        verifyAllTheCheckBoxesText()
        verifyAllTheCheckBoxesChecked()
    }

    class Transition {
        val mDevice = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation())!!

        fun goBack(interact: SettingsRobot.() -> Unit): SettingsRobot.Transition {
            goBackButton().click()

            SettingsRobot().interact()
            return SettingsRobot.Transition()
        }
    }
}

private fun goBackButton() =
    onView(allOf(withContentDescription("Navigate up")))

private fun assertNavigationToolBarHeader() {
    onView(allOf(withId(R.id.navigationToolbar),
        withChild(withText(R.string.preferences_delete_browsing_data))))
        .check((matches(withEffectiveVisibility(Visibility.VISIBLE))))
}

private fun assertDeleteBrowsingDataButton() {
    onView(withId(R.id.delete_data))
        .check((matches(withEffectiveVisibility(Visibility.VISIBLE))))
}

private fun assertClickDeleteBrowsingDataButton() {
    onView(withId(R.id.delete_data))
        .check((matches(withEffectiveVisibility(Visibility.VISIBLE)))).click()
}

private fun cancelButton() =
    mDevice.findObject(UiSelector().textStartsWith("CANCEL"))

private fun assertMessageInDialogBox() =
    onView(withText("$appName will delete the selected browsing data."))
        .inRoot(isDialog())
        .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))

private fun assertDeleteButtonInDialogBox() =
    onView(withText("Delete"))
        .inRoot(isDialog())
        .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))

private fun assertCancelButtonInDialogBox() =
    onView(withText("Cancel"))
        .inRoot(isDialog())
        .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))

private fun assertAllTheCheckBoxesText() {

    onView(withText(R.string.preferences_delete_browsing_data_tabs_title_2))
        .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))

    onView(withText("0 tabs"))
        .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))

    onView(withText(R.string.preferences_delete_browsing_data_browsing_data_title))
        .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))

    onView(withText("0 addresses"))
        .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))

    onView(withText(R.string.preferences_delete_browsing_data_cookies))
        .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))

    onView(withText(R.string.preferences_delete_browsing_data_cookies_subtitle))
        .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))

    onView(withText(R.string.preferences_delete_browsing_data_cached_files))
        .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))

    onView(withText(R.string.preferences_delete_browsing_data_cached_files_subtitle))
        .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))

    onView(withText(R.string.preferences_delete_browsing_data_site_permissions))
        .check(matches(withEffectiveVisibility(Visibility.VISIBLE)))
}

private fun assertAllTheCheckBoxesChecked() {
    onView(allOf(withId(R.id.checkbox), hasSibling(withText("Open tabs")))).assertIsChecked(true)

    onView(allOf(withId(R.id.checkbox), hasSibling(withText("Browsing history and site data")))).assertIsChecked(true)

    onView(allOf(withId(R.id.checkbox), hasSibling(withText("Cookies")))).assertIsChecked(true)

    onView(allOf(withId(R.id.checkbox), hasSibling(withText("Cached images and files")))).assertIsChecked(true)

    onView(allOf(withId(R.id.checkbox), hasSibling(withText("Site permissions")))).assertIsChecked(true)
}
