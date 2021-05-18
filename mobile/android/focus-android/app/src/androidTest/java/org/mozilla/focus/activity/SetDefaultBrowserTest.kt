/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.focus.activity

import android.os.Build
import org.junit.Rule
import org.junit.Test
import org.mozilla.focus.activity.robots.homeScreen
import org.mozilla.focus.helpers.MainActivityIntentsTestRule

/**
 * Tests to verify the functionality of the Set Default browser setting
 */
class SetDefaultBrowserTest {
    @get: Rule
    var mActivityTestRule = MainActivityIntentsTestRule(showFirstRun = false)

    @Test
    fun changeDefaultBrowserTest() {
        homeScreen {
        }.openMainMenu {
        }.openSettings {
        }.openGeneralSettingsMenu {
            clickSetDefaultBrowser()
            if (Build.VERSION.SDK_INT <= 23) {
                verifyOpenWithDialog()
                selectAlwaysOpenWithFocus()
                verifySwitchIsToggled(true)
            } else {
                verifyAndroidDefaultAppsMenuAppears()
            }
        }
    }
}
