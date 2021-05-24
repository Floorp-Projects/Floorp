/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ui.robots

import androidx.test.platform.app.InstrumentationRegistry
import androidx.test.uiautomator.UiDevice

class SettingsSubMenuSetDefaultBrowserRobot {
    class Transition {
        val mDevice = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation())

        fun goBack(interact: SettingsRobot.() -> Unit): SettingsRobot.Transition {
            mDevice.waitForIdle()

            // We are now in system settings / showing a default browser dialog.
            // Really want to go back to the app. Not interested in up navigation like in other robots.
            mDevice.pressBack()

            SettingsRobot().interact()
            return SettingsRobot.Transition()
        }
    }
}
