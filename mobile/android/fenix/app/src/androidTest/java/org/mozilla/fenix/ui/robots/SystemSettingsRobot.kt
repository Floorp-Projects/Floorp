/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ui.robots

import androidx.test.espresso.intent.Intents
import androidx.test.espresso.intent.matcher.IntentMatchers.hasAction
import androidx.test.uiautomator.UiSelector
import org.junit.Assert.assertTrue
import org.mozilla.fenix.helpers.TestAssetHelper.waitingTime
import org.mozilla.fenix.helpers.TestHelper

class SystemSettingsRobot {
    fun verifySystemNotificationsView() = assertSystemNotificationsView()
    fun verifyNotifications() {
        Intents.intended(hasAction("android.settings.APP_NOTIFICATION_SETTINGS"))
    }

    fun verifyMakeDefaultBrowser() {
        Intents.intended(hasAction(SettingsRobot.DEFAULT_APPS_SETTINGS_ACTION))
    }

    class Transition {
        // Difficult to know where this will go
        fun goBack(interact: SettingsRobot.() -> Unit): SettingsRobot.Transition {
            mDevice.pressBack()

            SettingsRobot().interact()
            return SettingsRobot.Transition()
        }
    }
}

fun systemSettings(interact: SystemSettingsRobot.() -> Unit): SystemSettingsRobot.Transition {
    SystemSettingsRobot().interact()
    return SystemSettingsRobot.Transition()
}

private fun assertSystemNotificationsView() {
    mDevice.findObject(UiSelector().resourceId("com.android.settings:id/list"))
        .waitForExists(waitingTime)
    assertTrue(
        mDevice.findObject(UiSelector().textContains("All ${TestHelper.appName} notifications"))
            .waitForExists(waitingTime)
    )
}
