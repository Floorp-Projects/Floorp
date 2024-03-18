/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ui.robots

import android.util.Log
import org.mozilla.fenix.helpers.Constants.TAG
import org.mozilla.fenix.helpers.TestHelper.mDevice

class SettingsSubMenuSetDefaultBrowserRobot {
    class Transition {
        fun goBack(interact: SettingsRobot.() -> Unit): SettingsRobot.Transition {
            Log.i(TAG, "goBack: Waiting for device to be idle")
            mDevice.waitForIdle()
            Log.i(TAG, "goBack: Waited for device to be idle")

            // We are now in system settings / showing a default browser dialog.
            // Really want to go back to the app. Not interested in up navigation like in other robots.
            Log.i(TAG, "clearNotifications: Trying to click the device back button")
            mDevice.pressBack()
            Log.i(TAG, "clearNotifications: Clicked the device back button")

            SettingsRobot().interact()
            return SettingsRobot.Transition()
        }
    }
}
