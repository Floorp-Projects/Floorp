/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ui.robots

import androidx.test.espresso.Espresso.onView
import androidx.test.espresso.assertion.ViewAssertions.matches
import androidx.test.espresso.matcher.ViewMatchers.hasSibling
import androidx.test.espresso.matcher.ViewMatchers.withId
import androidx.test.espresso.matcher.ViewMatchers.withText
import org.hamcrest.CoreMatchers.allOf
import org.mozilla.fenix.R
import org.mozilla.fenix.helpers.MatcherHelper.assertItemContainingTextExists
import org.mozilla.fenix.helpers.MatcherHelper.assertItemWithDescriptionExists
import org.mozilla.fenix.helpers.MatcherHelper.itemContainingText
import org.mozilla.fenix.helpers.MatcherHelper.itemWithDescription
import org.mozilla.fenix.helpers.TestHelper.getStringResource
import org.mozilla.fenix.helpers.TestHelper.mDevice
import org.mozilla.fenix.helpers.click
import org.mozilla.fenix.helpers.isChecked

/**
 * Implementation of Robot Pattern for the Open Links In Apps sub menu.
 */
class SettingsSubMenuOpenLinksInAppsRobot {

    fun verifyOpenLinksInAppsView(selectedOpenLinkInAppsOption: String) {
        assertItemWithDescriptionExists(goBackButton)
        assertItemContainingTextExists(
            itemContainingText(getStringResource(R.string.preferences_open_links_in_apps)),
            itemContainingText(getStringResource(R.string.preferences_open_links_in_apps_always)),
            itemContainingText(getStringResource(R.string.preferences_open_links_in_apps_ask)),
            itemContainingText(getStringResource(R.string.preferences_open_links_in_apps_never)),
        )
        verifySelectedOpenLinksInAppOption(selectedOpenLinkInAppsOption)
    }

    fun verifyPrivateOpenLinksInAppsView(selectedOpenLinkInAppsOption: String) {
        assertItemWithDescriptionExists(goBackButton)
        assertItemContainingTextExists(
            itemContainingText(getStringResource(R.string.preferences_open_links_in_apps)),
            itemContainingText(getStringResource(R.string.preferences_open_links_in_apps_ask)),
            itemContainingText(getStringResource(R.string.preferences_open_links_in_apps_never)),
        )
        verifySelectedOpenLinksInAppOption(selectedOpenLinkInAppsOption)
    }

    fun verifySelectedOpenLinksInAppOption(openLinkInAppsOption: String) =
        onView(
            allOf(
                withId(R.id.radio_button),
                hasSibling(withText(openLinkInAppsOption)),
            ),
        ).check(matches(isChecked(true)))

    fun clickOpenLinkInAppOption(openLinkInAppsOption: String) {
        when (openLinkInAppsOption) {
            "Always" -> alwaysOption.click()
            "Ask before opening" -> askBeforeOpeningOption.click()
            "Never" -> neverOption.click()
        }
    }

    class Transition {
        fun goBack(interact: SettingsRobot.() -> Unit): SettingsRobot.Transition {
            mDevice.waitForIdle()
            goBackButton.click()

            SettingsRobot().interact()
            return SettingsRobot.Transition()
        }
    }
}
private val goBackButton = itemWithDescription("Navigate up")
private val alwaysOption =
    itemContainingText(getStringResource(R.string.preferences_open_links_in_apps_always))
private val askBeforeOpeningOption =
    itemContainingText(getStringResource(R.string.preferences_open_links_in_apps_ask))
private val neverOption =
    itemContainingText(getStringResource(R.string.preferences_open_links_in_apps_never))
