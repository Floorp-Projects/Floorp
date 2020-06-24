/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ui.robots

import androidx.test.espresso.intent.Intents
import androidx.test.espresso.intent.matcher.IntentMatchers.hasAction

class SystemSettingsRobot {
    fun verifyNotifications() {
        Intents.intended(hasAction("android.settings.APP_NOTIFICATION_SETTINGS"))
    }

    fun verifyMakeDefaultBrowser() {
        Intents.intended(hasAction(SettingsRobot.DEFAULT_APPS_SETTINGS_ACTION))
    }

    class Transition {
        // Difficult to know where this will go
    }
}

fun systemSettings(interact: SystemSettingsRobot.() -> Unit): SystemSettingsRobot.Transition {
    SystemSettingsRobot().interact()
    return SystemSettingsRobot.Transition()
}
