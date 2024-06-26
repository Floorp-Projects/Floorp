/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.ktx.util

import androidx.annotation.VisibleForTesting
import java.util.Date

/**
 * Helper class to identify if a website has shown many dialogs.
 *  @param maxSuccessiveDialogSecondsLimit Maximum time required
 *  between dialogs in seconds before not showing more dialog.
 */
class PromptAbuserDetector(private val maxSuccessiveDialogSecondsLimit: Int = MAX_SUCCESSIVE_DIALOG_SECONDS_LIMIT) {

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    var jsAlertCount = 0

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    var lastDialogShownAt = Date()

    var shouldShowMoreDialogs = true
        private set

    /**
     * Updates internal state for alerts counts.
     */
    fun resetJSAlertAbuseState() {
        jsAlertCount = 0
        shouldShowMoreDialogs = true
    }

    /**
     * Updates internal state for last shown and count of dialogs.
     */
    fun updateJSDialogAbusedState() {
        if (!areDialogsAbusedByTime()) {
            jsAlertCount = 0
        }
        ++jsAlertCount
        lastDialogShownAt = Date()
    }

    /**
     * Indicates whether or not user wants to see more dialogs.
     */
    fun userWantsMoreDialogs(checkBox: Boolean) {
        shouldShowMoreDialogs = checkBox
    }

    /**
     * Indicates whether dialogs are being abused or not.
     */
    fun areDialogsBeingAbused(): Boolean {
        return areDialogsAbusedByTime() || areDialogsAbusedByCount()
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    @Suppress("UndocumentedPublicFunction") // this is visible only for tests
    fun now() = Date()

    private fun areDialogsAbusedByTime(): Boolean {
        return if (jsAlertCount == 0) {
            false
        } else {
            val now = now()
            val diffInSeconds = (now.time - lastDialogShownAt.time) / SECOND_MS
            diffInSeconds < maxSuccessiveDialogSecondsLimit
        }
    }

    private fun areDialogsAbusedByCount(): Boolean {
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
