/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ui.robots

import android.util.Log
import androidx.test.espresso.Espresso.onView
import androidx.test.espresso.assertion.ViewAssertions.matches
import androidx.test.espresso.matcher.ViewMatchers.hasSibling
import androidx.test.espresso.matcher.ViewMatchers.withId
import androidx.test.espresso.matcher.ViewMatchers.withText
import org.hamcrest.CoreMatchers.allOf
import org.mozilla.fenix.R
import org.mozilla.fenix.helpers.Constants.TAG
import org.mozilla.fenix.helpers.DataGenerationHelper.getStringResource
import org.mozilla.fenix.helpers.MatcherHelper.assertUIObjectExists
import org.mozilla.fenix.helpers.MatcherHelper.itemContainingText
import org.mozilla.fenix.helpers.MatcherHelper.itemWithDescription
import org.mozilla.fenix.helpers.TestHelper.mDevice
import org.mozilla.fenix.helpers.isChecked

/**
 * Implementation of Robot Pattern for the Open Links In Apps sub menu.
 */
class SettingsSubMenuOpenLinksInAppsRobot {

    fun verifyOpenLinksInAppsView(selectedOpenLinkInAppsOption: String) {
        assertUIObjectExists(
            goBackButton(),
            itemContainingText(getStringResource(R.string.preferences_open_links_in_apps)),
            itemContainingText(getStringResource(R.string.preferences_open_links_in_apps_always)),
            itemContainingText(getStringResource(R.string.preferences_open_links_in_apps_ask)),
            itemContainingText(getStringResource(R.string.preferences_open_links_in_apps_never)),
        )
        verifySelectedOpenLinksInAppOption(selectedOpenLinkInAppsOption)
    }

    fun verifyPrivateOpenLinksInAppsView(selectedOpenLinkInAppsOption: String) {
        assertUIObjectExists(
            goBackButton(),
            itemContainingText(getStringResource(R.string.preferences_open_links_in_apps)),
            itemContainingText(getStringResource(R.string.preferences_open_links_in_apps_ask)),
            itemContainingText(getStringResource(R.string.preferences_open_links_in_apps_never)),
        )
        verifySelectedOpenLinksInAppOption(selectedOpenLinkInAppsOption)
    }

    fun verifySelectedOpenLinksInAppOption(openLinkInAppsOption: String) {
        Log.i(TAG, "verifySelectedOpenLinksInAppOption: Trying to verify that the $openLinkInAppsOption option is checked")
        onView(
            allOf(
                withId(R.id.radio_button),
                hasSibling(withText(openLinkInAppsOption)),
            ),
        ).check(matches(isChecked(true)))
        Log.i(TAG, "verifySelectedOpenLinksInAppOption: Verified that the $openLinkInAppsOption option is checked")
    }

    fun clickOpenLinkInAppOption(openLinkInAppsOption: String) {
        Log.i(TAG, "clickOpenLinkInAppOption: Trying to click the $openLinkInAppsOption option")
        when (openLinkInAppsOption) {
            "Always" -> alwaysOption().click()
            "Ask before opening" -> askBeforeOpeningOption().click()
            "Never" -> neverOption().click()
        }
        Log.i(TAG, "clickOpenLinkInAppOption: Clicked the $openLinkInAppsOption option")
    }

    class Transition {
        fun goBack(interact: SettingsRobot.() -> Unit): SettingsRobot.Transition {
            Log.i(TAG, "goBack: Waiting for device to be idle")
            mDevice.waitForIdle()
            Log.i(TAG, "goBack: Waited for device to be idle")
            Log.i(TAG, "goBack: Trying to click the navigate up button")
            goBackButton().click()
            Log.i(TAG, "goBack: Clicked the navigate up button")

            SettingsRobot().interact()
            return SettingsRobot.Transition()
        }
    }
}
private fun goBackButton() = itemWithDescription("Navigate up")
private fun alwaysOption() =
    itemContainingText(getStringResource(R.string.preferences_open_links_in_apps_always))
private fun askBeforeOpeningOption() =
    itemContainingText(getStringResource(R.string.preferences_open_links_in_apps_ask))
private fun neverOption() =
    itemContainingText(getStringResource(R.string.preferences_open_links_in_apps_never))
