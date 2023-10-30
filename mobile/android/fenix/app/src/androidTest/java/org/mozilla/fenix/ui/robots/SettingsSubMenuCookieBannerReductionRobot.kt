/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ui.robots

import org.mozilla.fenix.R
import org.mozilla.fenix.helpers.DataGenerationHelper.getStringResource
import org.mozilla.fenix.helpers.MatcherHelper.assertCheckedItemWithResIdExists
import org.mozilla.fenix.helpers.MatcherHelper.assertItemContainingTextExists
import org.mozilla.fenix.helpers.MatcherHelper.checkedItemWithResId
import org.mozilla.fenix.helpers.MatcherHelper.itemContainingText
import org.mozilla.fenix.helpers.MatcherHelper.itemWithResId
import org.mozilla.fenix.helpers.TestHelper.mDevice
import org.mozilla.fenix.helpers.TestHelper.packageName
import org.mozilla.fenix.helpers.click

/**
 * Implementation of Robot Pattern for the settings Cookie Banner Reduction sub menu.
 */
class SettingsSubMenuCookieBannerReductionRobot {
    fun verifyCookieBannerView(isCookieBannerReductionChecked: Boolean) {
        assertItemContainingTextExists(cookieBannerOptionTitle, cookieBannerOptionDescription)
        assertCheckedItemWithResIdExists(checkedCookieBannerOptionToggle(isCookieBannerReductionChecked))
    }
    fun clickCookieBannerReductionToggle() = cookieBannerOptionToggle.click()
    fun verifyCheckedCookieBannerReductionToggle(isCookieBannerReductionChecked: Boolean) =
        assertCheckedItemWithResIdExists(checkedCookieBannerOptionToggle(isCookieBannerReductionChecked))

    class Transition {
        fun goBack(interact: SettingsRobot.() -> Unit): SettingsRobot.Transition {
            mDevice.pressBack()

            SettingsRobot().interact()
            return SettingsRobot.Transition()
        }
    }
}

private val cookieBannerOptionTitle =
    itemContainingText(getStringResource(R.string.reduce_cookie_banner_option))
private val cookieBannerOptionDescription =
    itemContainingText(getStringResource(R.string.reduce_cookie_banner_summary_1))
private val cookieBannerOptionToggle =
    itemWithResId("$packageName:id/learn_more_switch")
private fun checkedCookieBannerOptionToggle(isChecked: Boolean = false) =
    checkedItemWithResId("$packageName:id/learn_more_switch", isChecked)
