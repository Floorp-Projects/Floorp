/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.prompts

import java.util.Date

/**
 * Helper class to identify if a website has shown many dialogs.
 */
internal class PromptAbuserDetector {

    internal var jsAlertCount = 0
    internal var lastDialogShownAt = Date()
    var shouldShowMoreDialogs = true
        private set

    fun resetJSAlertAbuseState() {
        jsAlertCount = 0
        shouldShowMoreDialogs = true
    }

    fun updateJSDialogAbusedState() {
        if (!areDialogsAbusedByTime()) {
            jsAlertCount = 0
        }
        ++jsAlertCount
        lastDialogShownAt = Date()
    }

    fun userWantsMoreDialogs(checkBox: Boolean) {
        shouldShowMoreDialogs = checkBox
    }

    fun areDialogsBeingAbused(): Boolean {
        return areDialogsAbusedByTime() || areDialogsAbusedByCount()
    }

    internal fun areDialogsAbusedByTime(): Boolean {
        return if (jsAlertCount == 0) {
            false
        } else {
            val now = Date()
            val diffInSeconds = (now.time - lastDialogShownAt.time) / SECOND_MS
            diffInSeconds < MAX_SUCCESSIVE_DIALOG_SECONDS_LIMIT
        }
    }

    internal fun areDialogsAbusedByCount(): Boolean {
        return jsAlertCount > MAX_SUCCESSIVE_DIALOG_COUNT
    }

    companion object {
        // Maximum number of successive dialogs before we prompt users to disable dialogs.
        internal const val MAX_SUCCESSIVE_DIALOG_COUNT: Int = 2

        // Minimum time required between dialogs in seconds before enabling the stop dialog.
        internal const val MAX_SUCCESSIVE_DIALOG_SECONDS_LIMIT: Int = 3

        // Number of milliseconds in 1 second.
        internal const val SECOND_MS: Int = 1000
    }
}
